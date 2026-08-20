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

	/** An unregistered registration reports deleted. ExtendScript's built-in
	    isValid property (which shadows any same-named plug-in property) then
	    returns false for held handles, which is exactly the contract. */
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
	const FontRegRegistration* reg = FontRegRegistry::Instance().Find(idData->GetInt());
	return (reg != nil && reg->fValid) ? kFalse : kTrue;
}
