//========================================================================================
//
//  FontRegRegistry.h
//
//  Session-scoped font registration. One private temp directory is handed to
//  InDesign's font system (IFontMgr::AddDirectory) the first time anything is
//  registered; every registration copies its font files in and forces a
//  synchronous rescan, so fonts are usable on the next script line. Removal
//  deletes the copies and rescans again. Nothing outlives the process.
//
//========================================================================================

#ifndef __FontRegRegistry_h__
#define __FontRegRegistry_h__

#include "PMString.h"

#include <vector>

/** One registerFont/registerFontFolder call. `id` is what the FontRegistration
    script object carries; everything else is resolved through it. */
struct FontRegRegistration
{
	int32 fId = 0;
	PMString fSourcePath;					// what the script passed in
	std::vector<PMString> fFontNames;		// "Family\tStyle", itemByName form
	std::vector<std::string> fTempFiles;	// UTF-8 paths of our copies
	bool fValid = false;
};

/** Plain singleton rather than a boss: no persistence, no notifications, and
    the script providers and the idle task are the only callers. */
class FontRegRegistry
{
public:
	static FontRegRegistry& Instance();

	/** Copy the font file(s) at sourcePath (a file, or every font file in a
	    folder when isFolder) into the temp dir and rescan. Returns the
	    registration id, or 0 with outError set. Re-registering an active
	    sourcePath returns the existing id. */
	int32 Register(const PMString& sourcePath, bool isFolder, PMString& outError);

	/** Delete the registration's copies and rescan. False if the id is
	    unknown; an already-invalid id is a no-op returning true. */
	bool Unregister(int32 id);

	/** Unregister every active registration whose sourcePath no longer
	    exists (the proxy deleting a job dir is the signal the job is done),
	    and retry files whose removal was blocked by the OS. */
	void SweepStale();

	/** Best-effort removal of the temp dir at shutdown. */
	void Shutdown();

	int32 CountActive() const;
	const FontRegRegistration* GetNthActive(int32 n) const;	// 0-based
	const FontRegRegistration* Find(int32 id) const;

private:
	FontRegRegistry() = default;

	bool EnsureTempDir(PMString& outError);
	int32 FindActiveBySource(const PMString& sourcePath) const;

	/** Delete a backing copy, verified: on Windows a font file CoolType has
	    mapped can refuse deletion, so fall back to moving it out of the
	    scanned tree; a file that resists both lands on fPendingRemovals. */
	bool RemoveBackingFile(const std::string& utf8Path);

	std::vector<FontRegRegistration> fRegs;
	std::vector<std::string> fPendingRemovals;	// UTF-8 paths still to delete
	std::string fTempDir;		// UTF-8; empty until first use
	std::string fTrashDir;		// UTF-8; outside the scanned tree
	bool fDirRegistered = false;
	int32 fNextId = 1;
};

#endif // __FontRegRegistry_h__
