//========================================================================================
//
//  FontRegStartupShutdown.cpp
//
//  Installs the sweep idle task at startup, and tears the session down at
//  shutdown: uninstall the task, then best-effort delete the temp font
//  directory (the OS would eventually reclaim it, but be tidy).
//
//========================================================================================

#include "VCPlugInHeaders.h"

#include "IIdleTask.h"
#include "ISession.h"
#include "IStartupShutdownService.h"

#include "FontRegID.h"
#include "FontRegRegistry.h"

class FontRegStartupShutdown : public CPMUnknown<IStartupShutdownService>
{
public:
	FontRegStartupShutdown(IPMUnknown* boss) : CPMUnknown<IStartupShutdownService>(boss) {}
	virtual ~FontRegStartupShutdown() {}

	virtual void Startup();
	virtual void Shutdown();
};

CREATE_PMINTERFACE(FontRegStartupShutdown, kFontRegStartupShutdownImpl)

void FontRegStartupShutdown::Startup()
{
	// A crashed engine never runs Shutdown, so reap dead sessions' dirs here.
	FontRegRegistry::Instance().CleanupStaleSessionDirs();

	InterfacePtr<IIdleTask> task(GetExecutionContextSession(), IID_IFONTREGSWEEPIDLETASK);
	ASSERT(task);
	if (task != nil)
		task->InstallTask(60 * 1000);	// first sweep one interval in, not at launch
}

void FontRegStartupShutdown::Shutdown()
{
	InterfacePtr<IIdleTask> task(GetExecutionContextSession(), IID_IFONTREGSWEEPIDLETASK);
	if (task != nil)
		task->UninstallTask();
	FontRegRegistry::Instance().Shutdown();
}
