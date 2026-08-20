//========================================================================================
//
//  FontRegProxyScript.cpp
//
//  The FontRegistration script object. A proxy (non-persistent) script object
//  whose IIntData -- supplied by the kBaseProxyScriptObjectBoss parent --
//  carries the registration id. Ids are stable for the life of the session,
//  unlike collection indices, so a held handle keeps meaning the same
//  registration as others come and go.
//
//========================================================================================

#include "VCPlugInHeaders.h"

#include "CProxyScript.h"
#include "IIntData.h"

#include "FontRegID.h"
#include "FontRegRegistry.h"

class FontRegProxyScript : public CProxyScript
{
public:
	FontRegProxyScript(IPMUnknown* boss) : CProxyScript(boss) {}

	/** Id-form specifier (the base class assumes IIntData is a collection
	    index and returns index form). */
	virtual ScriptObject GetScriptObject(const RequestContext& context) const;

	/** Registrations are never destroyed while the process lives -- an
	    unregistered one just reports isValid == false -- so the object exists
	    as long as the registry knows the id. */
	virtual bool16 HasBeenDeleted(const RequestContext& context);
};

CREATE_PMINTERFACE(FontRegProxyScript, kFontRegProxyScriptImpl)

ScriptObject FontRegProxyScript::GetScriptObject(const RequestContext& context) const
{
	int32 id = 0;
	InterfacePtr<const IIntData> idData(this, UseDefaultIID());
	if (idData != nil)
		id = idData->GetInt();
	return ScriptObject(ScriptData(id), GetObjectType(context), kFormUniqueID);
}

bool16 FontRegProxyScript::HasBeenDeleted(const RequestContext& context)
{
	InterfacePtr<const IIntData> idData(this, UseDefaultIID());
	if (idData == nil)
		return kTrue;
	return FontRegRegistry::Instance().Find(idData->GetInt()) == nil ? kTrue : kFalse;
}
