//========================================================================================
//
//  FontRegSweepIdleTask.cpp
//
//  The self-cleaning backstop. Fonts are copied out of the job directory at
//  registration, so a registration outlives its source; when the proxy
//  deletes a finished job's directory, this sweep notices and unregisters.
//  Idle tasks never run mid-script, so a sweep cannot remove fonts while the
//  job that registered them is still executing.
//
//========================================================================================

#include "VCPlugInHeaders.h"

#include "CIdleTask.h"

#include "FontRegID.h"
#include "FontRegRegistry.h"

// How often to check for vanished job directories. A constant until someone
// needs a configuration surface.
const static uint32 kFontRegSweepIntervalMs = 60 * 1000;

class FontRegSweepIdleTask : public CIdleTask
{
public:
	FontRegSweepIdleTask(IPMUnknown* boss) : CIdleTask(boss) {}
	virtual ~FontRegSweepIdleTask() {}

	virtual uint32 RunTask(uint32 appFlags, IdleTimer* timeCheck);
	virtual const char* TaskName();
};

CREATE_PMINTERFACE(FontRegSweepIdleTask, kFontRegSweepIdleTaskImpl)

uint32 FontRegSweepIdleTask::RunTask(uint32 appFlags, IdleTimer* timeCheck)
{
	FontRegRegistry::Instance().SweepStale();
	return kFontRegSweepIntervalMs;
}

const char* FontRegSweepIdleTask::TaskName()
{
	return "FontRegister stale-registration sweep";
}
