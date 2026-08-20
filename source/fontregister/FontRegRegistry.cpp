//========================================================================================
//
//  FontRegRegistry.cpp
//
//========================================================================================

#include "VCPlugInHeaders.h"

#include "IFontMgr.h"
#include "IFontGroup.h"
#include "IPMFont.h"
#include "ISession.h"

#include "FileUtils.h"
#include "StringUtils.h"

#include "FontRegRegistry.h"

#include <algorithm>
#include <filesystem>
#include <set>
#include <system_error>

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

} // anonymous namespace

FontRegRegistry& FontRegRegistry::Instance()
{
	static FontRegRegistry sInstance;
	return sInstance;
}

bool FontRegRegistry::EnsureTempDir(PMString& outError)
{
	if (fDirRegistered)
		return true;

	std::error_code ec;
	fs::path dir = fs::temp_directory_path(ec);
	if (ec)
	{
		outError = "FontRegister: no usable temp directory.";
		return false;
	}
	dir /= "FontRegister-" + std::to_string(
		static_cast<unsigned long>(
#ifdef WINDOWS
			::GetCurrentProcessId()
#else
			getpid()
#endif
		));
	fs::create_directories(dir, ec);
	if (ec)
	{
		outError = "FontRegister: could not create the session font directory.";
		return false;
	}
	fTempDir = FromPath(dir);

	InterfacePtr<IFontMgr> fontMgr(GetExecutionContextSession(), UseDefaultIID());
	if (fontMgr == nil)
	{
		outError = "FontRegister: the font manager is unavailable.";
		return false;
	}
	fontMgr->AddDirectory(FileUtils::PMStringToSysFile(PMString(fTempDir.c_str())));
	fDirRegistered = true;
	return true;
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

		fontMgr->ForceUpdateFontSystem();

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
			fontMgr->ForceUpdateFontSystem();
			outError = "FontRegister: not a usable font file: ";
			outError.Append(sourcePath);
			return 0;
		}
	}

	fRegs.push_back(reg);
	return reg.fId;
}

bool FontRegRegistry::Unregister(int32 id)
{
	for (FontRegRegistration& reg : fRegs)
	{
		if (reg.fId != id)
			continue;
		if (!reg.fValid)
			return true;	// double unregister() is a no-op

		std::error_code ec;
		for (const std::string& f : reg.fTempFiles)
			fs::remove(ToPath(f), ec);
		reg.fValid = false;

		if (!reg.fTempFiles.empty())
		{
			InterfacePtr<IFontMgr> fontMgr(GetExecutionContextSession(), UseDefaultIID());
			if (fontMgr != nil)
				fontMgr->ForceUpdateFontSystem();
		}
		return true;
	}
	return false;
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
			Unregister(reg.fId);
	}
}

void FontRegRegistry::Shutdown()
{
	for (FontRegRegistration& reg : fRegs)
		reg.fValid = false;
	if (!fTempDir.empty())
	{
		std::error_code ec;
		fs::remove_all(ToPath(fTempDir), ec);
	}
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
