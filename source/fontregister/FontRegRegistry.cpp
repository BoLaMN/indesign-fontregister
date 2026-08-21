//========================================================================================
//
//  FontRegRegistry.cpp
//
//  Mechanism: fonts copied into a session subfolder of InDesign's per-user
//  CompositeFont folder (ICompositeFontMgr::GetCompositeFontFolder) become
//  installable SYNCHRONOUSLY -- that folder is one of the few the font
//  system's incremental seed scanner (CurrentFontSystemSeed(kTrue)) actually
//  checksums. Nothing else works mid-script: AddDirectory'd folders, OS font
//  folders, and CoreText process registration are all invisible until event-
//  loop idle, which never happens while a script runs. (All verified
//  empirically against InDesign 21.5; app.updateFonts() uses the same seed
//  machinery.)
//
//========================================================================================

#include "VCPlugInHeaders.h"

#include "ICompositeFontMgr.h"
#include "IFontMgr.h"
#include "IFontGroup.h"
#include "IPMFont.h"
#include "ISession.h"
#include "IWorkspace.h"

#include "CompositeFontMgrID.h"
#include "FileUtils.h"

#include "FontRegRegistry.h"

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <set>
#include <system_error>

#ifdef WINDOWS
#include <windows.h>
#else
#include <signal.h>
#include <cerrno>
#endif

namespace fs = std::filesystem;

namespace
{

// C++17: u8path/u8string speak std::string carrying UTF-8.
fs::path ToPath(const std::string& utf8)
{
	return fs::u8path(utf8);
}

std::string FromPath(const fs::path& p)
{
	return p.u8string();
}

bool LooksLikeFontFile(const fs::path& p)
{
	std::string ext = FromPath(p.extension());
	std::transform(ext.begin(), ext.end(), ext.begin(),
	               [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
	return ext == ".otf" || ext == ".ttf" || ext == ".ttc" || ext == ".otc";
}

/** Every "Family\tStyle" the font system knows right now. The set before and
    after a rescan is how we learn what a registration actually installed --
    CoolType doesn't report per-file. */
void SnapshotFontNames(IFontMgr* fontMgr, std::set<std::string>& outNames)
{
	const int32 numGroups = fontMgr->GetNumFontGroups();
	for (int32 g = 0; g < numGroups; ++g)
	{
		InterfacePtr<IFontGroup> group(fontMgr->QueryFontGroup(g));
		if (group == nil)
			continue;
		const int32 numFonts = group->GetNumFonts();
		for (int32 f = 0; f < numFonts; ++f)
		{
			InterfacePtr<IPMFont> font(fontMgr->QueryFont(group, f));
			if (font == nil)
				continue;
			PMString name;
			font->AppendFamilyName(name);
			name.Append("\t");
			font->AppendStyleName(name);
			outNames.insert(std::string(name.GetUTF8String()));
		}
	}
}

bool ProcessIsAlive(unsigned long pid)
{
#ifdef WINDOWS
	HANDLE h = ::OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, (DWORD)pid);
	if (h == NULL)
		return false;
	DWORD code = 0;
	const bool alive = ::GetExitCodeProcess(h, &code) && code == STILL_ACTIVE;
	::CloseHandle(h);
	return alive;
#else
	return kill((pid_t)pid, 0) == 0 || errno == EPERM;
#endif
}

/** Make the font system notice folder changes, synchronously. The seed scan
    is incremental (one folder per call), so poll until the seed moves or a
    bounded number of calls pass, then force the list rebuild. */
void RescanFonts(IFontMgr* fontMgr)
{
	const int32 oldSeed = fontMgr->CurrentFontSystemSeed(kFalse);
	int32 newSeed = oldSeed;
	for (int i = 0; i < 50 && newSeed == oldSeed; ++i)
		newSeed = fontMgr->CurrentFontSystemSeed(kTrue);
	fontMgr->ForceUpdateFontSystem();
}

} // anonymous namespace

FontRegRegistry& FontRegRegistry::Instance()
{
	static FontRegRegistry sInstance;
	return sInstance;
}

bool FontRegRegistry::QueryCompositeFontFolder(std::string& outUtf8Path) const
{
	InterfacePtr<ICompositeFontMgr> compFontMgr(GetExecutionContextSession(), IID_ICOMPOSITEFONTMGR);
	if (compFontMgr == nil)
	{
		InterfacePtr<IWorkspace> workspace(GetExecutionContextSession()->QueryWorkspace());
		if (workspace != nil)
			compFontMgr.reset((ICompositeFontMgr*)workspace->QueryInterface(IID_ICOMPOSITEFONTMGR));
	}
	if (compFontMgr == nil)
		return false;

	IDFile folder;
	compFontMgr->GetCompositeFontFolder(&folder);
	const PMString folderPath = FileUtils::SysFileToPMString(folder);
	outUtf8Path = std::string(folderPath.GetUTF8String());
	return !outUtf8Path.empty();
}

bool FontRegRegistry::EnsureTempDir(PMString& outError)
{
	if (fDirRegistered)
		return true;

	std::string compFolder;
	if (!QueryCompositeFontFolder(compFolder))
	{
		outError = "FontRegister: the composite font manager is unavailable.";
		return false;
	}

	fs::path dir = ToPath(compFolder);
	dir /= "FontRegister-" + std::to_string(
		static_cast<unsigned long>(
#ifdef WINDOWS
			::GetCurrentProcessId()
#else
			getpid()
#endif
		));
	std::error_code ec;
	fs::create_directories(dir, ec);
	if (ec)
	{
		outError = "FontRegister: could not create the session font directory.";
		return false;
	}
	fTempDir = FromPath(dir);

	// The trash lives OUTSIDE the CompositeFont tree (which is scanned
	// recursively) but on the same volume, so a locked file can be renamed
	// into it without a cross-volume copy.
	fs::path trash = dir.parent_path().parent_path() /
		("FontRegister-trash-" + std::string(FromPath(dir.filename())).substr(13));
	fs::create_directories(trash, ec);
	if (!ec)
		fTrashDir = FromPath(trash);

	fDirRegistered = true;
	return true;
}

bool FontRegRegistry::RemoveBackingFile(const std::string& utf8Path)
{
	const fs::path p = ToPath(utf8Path);
	std::error_code ec;

	fs::remove(p, ec);
	if (!fs::exists(p, ec))
		return true;

	// Windows refuses to delete a font file the font engine has mapped, but
	// usually allows a rename. Moving it out of the scanned tree changes the
	// folder checksum, so the rescan still uninstalls the font.
	if (!fTrashDir.empty())
	{
		const fs::path dst = ToPath(fTrashDir) / p.filename();
		fs::rename(p, dst, ec);
		if (!ec && !fs::exists(p, ec))
			return true;
	}

	fPendingRemovals.push_back(utf8Path);
	return false;
}

int32 FontRegRegistry::FindActiveBySource(const PMString& sourcePath) const
{
	for (const FontRegRegistration& reg : fRegs)
		if (reg.fValid && reg.fSourcePath == sourcePath)
			return reg.fId;
	return 0;
}

int32 FontRegRegistry::Register(const PMString& sourcePath, bool isFolder, PMString& outError)
{
	const int32 existing = FindActiveBySource(sourcePath);
	if (existing != 0)
		return existing;

	const std::string sourceUtf8(sourcePath.GetUTF8String());
	const fs::path source = ToPath(sourceUtf8);

	std::error_code ec;
	if (isFolder ? !fs::is_directory(source, ec) : !fs::is_regular_file(source, ec))
	{
		outError = isFolder ? "FontRegister: the folder does not exist: "
		                    : "FontRegister: the font file does not exist: ";
		outError.Append(sourcePath);
		return 0;
	}

	std::vector<fs::path> candidates;
	if (isFolder)
	{
		for (const fs::directory_entry& entry : fs::directory_iterator(source, ec))
			if (entry.is_regular_file() && LooksLikeFontFile(entry.path()))
				candidates.push_back(entry.path());
		std::sort(candidates.begin(), candidates.end());
	}
	else
	{
		candidates.push_back(source);
	}

	FontRegRegistration reg;
	reg.fId = fNextId++;
	reg.fSourcePath = sourcePath;
	reg.fValid = true;

	// A folder with no font files is a legitimate empty registration; a single
	// file, though, was named explicitly, so it must produce fonts (checked
	// against the rescan diff below).
	if (!candidates.empty())
	{
		if (!EnsureTempDir(outError))
			return 0;

		InterfacePtr<IFontMgr> fontMgr(GetExecutionContextSession(), UseDefaultIID());
		if (fontMgr == nil)
		{
			outError = "FontRegister: the font manager is unavailable.";
			return 0;
		}

		std::set<std::string> before;
		SnapshotFontNames(fontMgr, before);

		const fs::path tempDir = ToPath(fTempDir);
		for (const fs::path& src : candidates)
		{
			// Prefix with the registration id so two jobs' identically named
			// files never collide.
			const fs::path dst = tempDir /
				(std::string("reg") + std::to_string(reg.fId) + "_" + FromPath(src.filename()));
			fs::copy_file(src, dst, fs::copy_options::overwrite_existing, ec);
			if (ec)
			{
				for (const std::string& f : reg.fTempFiles)
					fs::remove(ToPath(f), ec);
				outError = "FontRegister: could not copy a font into the session directory: ";
				outError.Append(PMString(FromPath(src).c_str()));
				return 0;
			}
			reg.fTempFiles.push_back(FromPath(dst));
		}

		RescanFonts(fontMgr);

		std::set<std::string> after;
		SnapshotFontNames(fontMgr, after);
		for (const std::string& name : after)
			if (before.find(name) == before.end())
				reg.fFontNames.push_back(PMString(name.c_str()));

		if (!isFolder && reg.fFontNames.empty())
		{
			// The copy landed but the font system took nothing from it: not a
			// usable font file. Clean up rather than leaving a dud in the dir.
			for (const std::string& f : reg.fTempFiles)
				fs::remove(ToPath(f), ec);
			RescanFonts(fontMgr);
			outError = "FontRegister: not a usable font file: ";
			outError.Append(sourcePath);
			return 0;
		}
	}

	fRegs.push_back(reg);
	return reg.fId;
}

bool FontRegRegistry::Unregister(int32 id, PMString& outError)
{
	for (FontRegRegistration& reg : fRegs)
	{
		if (reg.fId != id)
			continue;
		if (!reg.fValid)
			return true;	// double unregister() is a no-op

		std::vector<std::string> stuck;
		for (const std::string& f : reg.fTempFiles)
			if (!RemoveBackingFile(f))
				stuck.push_back(f);
		reg.fValid = false;

		if (!reg.fTempFiles.empty())
		{
			InterfacePtr<IFontMgr> fontMgr(GetExecutionContextSession(), UseDefaultIID());
			if (fontMgr != nil)
			{
				RescanFonts(fontMgr);

				// A font file the engine had mapped (because something
				// composed with it) refuses both delete and rename; the
				// rescan re-initializes the font manager, which is what
				// releases the mapping -- so retry the stragglers now.
				if (!stuck.empty())
				{
					std::vector<std::string> still;
					for (const std::string& f : stuck)
					{
						// RemoveBackingFile queued it once already; drop the
						// duplicate before retrying.
						fPendingRemovals.erase(
							std::remove(fPendingRemovals.begin(), fPendingRemovals.end(), f),
							fPendingRemovals.end());
						if (!RemoveBackingFile(f))
							still.push_back(f);
					}
					if (still.size() < stuck.size())
						RescanFonts(fontMgr);
					stuck.swap(still);
				}
			}
		}

		if (!stuck.empty())
		{
			outError = "FontRegister: the OS is still holding ";
			outError.Append(PMString(std::to_string(stuck.size()).c_str()));
			outError.Append(" font file(s); they will be removed by the background sweep. First: ");
			outError.Append(PMString(stuck.front().c_str()));
		}
		return true;
	}
	return false;
}

void FontRegRegistry::CleanupStaleSessionDirs()
{
	std::string compFolder;
	if (!QueryCompositeFontFolder(compFolder))
		return;

	const fs::path comp = ToPath(compFolder);
	std::error_code ec;
	std::vector<fs::path> parents = { comp, comp.parent_path().parent_path() };
	for (const fs::path& parent : parents)
	{
		for (const fs::directory_entry& entry : fs::directory_iterator(parent, ec))
		{
			if (!entry.is_directory())
				continue;
			const std::string name = FromPath(entry.path().filename());
			unsigned long pid = 0;
			if (name.rfind("FontRegister-trash-", 0) == 0)
				pid = std::strtoul(name.c_str() + 19, nullptr, 10);
			else if (name.rfind("FontRegister-", 0) == 0)
				pid = std::strtoul(name.c_str() + 13, nullptr, 10);
			else
				continue;
			if (pid == 0 || ProcessIsAlive(pid))
				continue;
			fs::remove_all(entry.path(), ec);
		}
	}
}

void FontRegRegistry::SweepStale()
{
	std::error_code ec;
	for (const FontRegRegistration& reg : fRegs)
	{
		if (!reg.fValid)
			continue;
		const fs::path source = ToPath(std::string(reg.fSourcePath.GetUTF8String()));
		if (!fs::exists(source, ec))
		{
			PMString ignored;	// stragglers land on fPendingRemovals below
			Unregister(reg.fId, ignored);
		}
	}

	// Retry files the OS refused to release earlier; the font engine unmaps
	// eventually (e.g. once nothing composes with the font any more).
	if (!fPendingRemovals.empty())
	{
		std::vector<std::string> stillPending;
		stillPending.swap(fPendingRemovals);
		bool progressed = false;
		for (const std::string& f : stillPending)
			progressed = RemoveBackingFile(f) || progressed;
		if (progressed)
		{
			InterfacePtr<IFontMgr> fontMgr(GetExecutionContextSession(), UseDefaultIID());
			if (fontMgr != nil)
				RescanFonts(fontMgr);
		}
	}

	// Empty the trash opportunistically; renamed-but-locked files unlock over
	// time and this is the only place that reaps them.
	if (!fTrashDir.empty())
	{
		for (const fs::directory_entry& entry : fs::directory_iterator(ToPath(fTrashDir), ec))
			fs::remove(entry.path(), ec);
	}
}

void FontRegRegistry::Shutdown()
{
	for (FontRegRegistration& reg : fRegs)
		reg.fValid = false;
	std::error_code ec;
	if (!fTempDir.empty())
		fs::remove_all(ToPath(fTempDir), ec);
	if (!fTrashDir.empty())
		fs::remove_all(ToPath(fTrashDir), ec);
}

int32 FontRegRegistry::CountActive() const
{
	int32 n = 0;
	for (const FontRegRegistration& reg : fRegs)
		if (reg.fValid)
			++n;
	return n;
}

const FontRegRegistration* FontRegRegistry::GetNthActive(int32 n) const
{
	for (const FontRegRegistration& reg : fRegs)
		if (reg.fValid && n-- == 0)
			return &reg;
	return nil;
}

const FontRegRegistration* FontRegRegistry::Find(int32 id) const
{
	for (const FontRegRegistration& reg : fRegs)
		if (reg.fId == id)
			return &reg;
	return nil;
}
