//========================================================================================
//
//  FontRegScriptProvider.cpp
//
//  Scripting surface: app.registerFont(), app.registerFontFolder(),
//  app.fontRegistrations, and the FontRegistration object's fontNames /
//  sourcePath / isValid properties and unregister() method.
//
//  One provider serves both sides: it is registered against the Application
//  object for the two register methods, and as the represent-provider for the
//  FontRegistration object. Lives in the model plug-in so ExtendScript, UXP
//  and InDesign Server all reach it.
//
//========================================================================================

#include "VCPlugInHeaders.h"

#include "CScriptProvider.h"
#include "IIntData.h"
#include "IScript.h"
#include "IScriptRequestData.h"
#include "IScriptErrorUtils.h"
#include "IScriptUtils.h"

#include "ErrorUtils.h"
#include "ScriptData.h"
#include "Utils.h"

#include "FontRegID.h"
#include "FontRegRegistry.h"

/** Provider for the FontRegistration collection on app, the register methods
    on app, and the properties/unregister method on the object itself. Which
    role a call is in falls out of the method/property ID and the script
    object it arrives on.
*/
class FontRegScriptProvider : public RepresentScriptProvider
{
public:
	FontRegScriptProvider(IPMUnknown* boss) : RepresentScriptProvider(boss) {}
	virtual ~FontRegScriptProvider() {}

	virtual ErrorCode HandleMethod(ScriptID methodID, IScriptRequestData* data, IScript* script);
	virtual ErrorCode AccessProperty(ScriptID propID, IScriptRequestData* data, IScript* script);

protected:
	// The fontRegistrations collection.
	virtual int32     GetNumObjects(const IScriptRequestData* data, IScript* parent);
	virtual ErrorCode AppendNthObject(const IScriptRequestData* data, IScript* parent, int32 n, ScriptList& objectList);
	virtual ErrorCode GetObjectByID(IScriptRequestData* data, IScript* parent);

private:
	ErrorCode RegisterCommon(IScriptRequestData* data, IScript* parent, bool isFolder);
	ErrorCode Unregister(IScriptRequestData* data, IScript* script);
	ErrorCode AccessRegistrationProperty(ScriptID propID, IScriptRequestData* data, IScript* script);

	/** Build a FontRegistration proxy for a registration id. The proxy is a
	    kBaseProxyScriptObjectBoss subclass whose IIntData carries the id (not
	    a collection index -- ids stay stable as registrations come and go). */
	IScript* QueryRegistrationProxy(const IScriptRequestData* data, IScript* parent, int32 registrationId);

	/** The registration behind a FontRegistration script object. */
	static const FontRegRegistration* FindFromScript(IScript* script);

	static ErrorCode Fail(const PMString& message);
};

CREATE_PMINTERFACE(FontRegScriptProvider, kFontRegAppScriptProviderImpl)

ErrorCode FontRegScriptProvider::Fail(const PMString& message)
{
	PMString msg(message);
	msg.SetTranslatable(kFalse);
	ErrorUtils::PMSetGlobalErrorCode(kFailure, &msg);
	return kFailure;
}

const FontRegRegistration* FontRegScriptProvider::FindFromScript(IScript* script)
{
	if (script == nil)
		return nil;
	InterfacePtr<IIntData> idData(script, UseDefaultIID());
	if (idData == nil)
		return nil;
	return FontRegRegistry::Instance().Find(idData->GetInt());
}

IScript* FontRegScriptProvider::QueryRegistrationProxy(const IScriptRequestData* data, IScript* parent, int32 registrationId)
{
	return Utils<IScriptUtils>()->CreateProxyScriptObject(
		data->GetRequestContext(), kFontRegRegistrationProxyBoss,
		c_FontRegistration, parent, registrationId);
}

//========================================================================================
// Collection plumbing
//========================================================================================
int32 FontRegScriptProvider::GetNumObjects(const IScriptRequestData* data, IScript* parent)
{
	return FontRegRegistry::Instance().CountActive();
}

ErrorCode FontRegScriptProvider::AppendNthObject(const IScriptRequestData* data, IScript* parent, int32 n, ScriptList& objectList)
{
	const FontRegRegistration* reg = FontRegRegistry::Instance().GetNthActive(n);
	if (reg == nil)
		return kFailure;
	InterfacePtr<IScript> proxy(QueryRegistrationProxy(data, parent, reg->fId));
	if (proxy == nil)
		return kFailure;
	objectList.push_back(proxy);
	return kSuccess;
}

ErrorCode FontRegScriptProvider::GetObjectByID(IScriptRequestData* data, IScript* parent)
{
	// The default implementation resolves UIDs in a database; our objects are
	// session state keyed by registration id, so resolve against the registry.
	int32 id = 0;
	if (data->GetAccessorData().GetInt32(&id) != kSuccess)
		return kFailure;

	ScriptList objectList;
	const FontRegRegistration* reg = FontRegRegistry::Instance().Find(id);
	if (reg != nil)
	{
		InterfacePtr<IScript> proxy(QueryRegistrationProxy(data, parent, reg->fId));
		if (proxy == nil)
			return kFailure;
		objectList.push_back(proxy);
	}
	data->AppendReturnData(parent, data->GetRequestInfo()->GetScriptID(), ScriptData(objectList));
	return kSuccess;
}

//========================================================================================
// Methods
//========================================================================================
ErrorCode FontRegScriptProvider::HandleMethod(ScriptID methodID, IScriptRequestData* data, IScript* script)
{
	switch (methodID.Get())
	{
		case e_FontRegRegisterFont:   return RegisterCommon(data, script, false);
		case e_FontRegRegisterFolder: return RegisterCommon(data, script, true);
		case e_FontRegUnregister:     return Unregister(data, script);
		default:                      return RepresentScriptProvider::HandleMethod(methodID, data, script);
	}
}

ErrorCode FontRegScriptProvider::RegisterCommon(IScriptRequestData* data, IScript* parent, bool isFolder)
{
	ScriptData pathData;
	PMString path;
	if (data->ExtractRequestData(p_FontRegPathParam, pathData) != kSuccess
		|| pathData.GetPMString(path) != kSuccess || path.empty())
	{
		return Utils<IScriptErrorUtils>()->SetInvalidParameterErrorData(data, p_FontRegPathParam);
	}

	PMString error;
	const int32 id = FontRegRegistry::Instance().Register(path, isFolder, error);
	if (id == 0)
		return Fail(error);

	InterfacePtr<IScript> proxy(QueryRegistrationProxy(data, parent, id));
	if (proxy == nil)
		return kFailure;

	ScriptData out;
	out.SetObject(proxy);
	data->AppendReturnData(parent, isFolder ? e_FontRegRegisterFolder : e_FontRegRegisterFont, out);
	return kSuccess;
}

ErrorCode FontRegScriptProvider::Unregister(IScriptRequestData* data, IScript* script)
{
	const FontRegRegistration* reg = FindFromScript(script);
	if (reg == nil)
		return Fail(PMString("FontRegister: this FontRegistration no longer exists."));
	FontRegRegistry::Instance().Unregister(reg->fId);	// already-invalid is a no-op
	return kSuccess;
}

//========================================================================================
// Properties
//========================================================================================
ErrorCode FontRegScriptProvider::AccessProperty(ScriptID propID, IScriptRequestData* data, IScript* script)
{
	switch (propID.Get())
	{
		case p_FontRegFontNames:
		case p_FontRegSourcePath:
		case p_FontRegIsValid:
			return AccessRegistrationProperty(propID, data, script);
		default:
			return RepresentScriptProvider::AccessProperty(propID, data, script);
	}
}

ErrorCode FontRegScriptProvider::AccessRegistrationProperty(ScriptID propID, IScriptRequestData* data, IScript* script)
{
	if (!data->IsPropertyGet())
		return Utils<IScriptErrorUtils>()->SetReadOnlyPropertyErrorData(data, propID);

	const FontRegRegistration* reg = FindFromScript(script);
	if (reg == nil)
		return Fail(PMString("FontRegister: this FontRegistration no longer exists."));

	ScriptData out;
	switch (propID.Get())
	{
		case p_FontRegFontNames:
		{
			ScriptListData names;
			for (const PMString& name : reg->fFontNames)
			{
				PMString value(name);
				value.SetTranslatable(kFalse);
				names.push_back(ScriptData(value));
			}
			out.SetList(names);
			break;
		}
		case p_FontRegSourcePath:
		{
			PMString value(reg->fSourcePath);
			value.SetTranslatable(kFalse);
			out.SetPMString(value);
			break;
		}
		case p_FontRegIsValid:
			out.SetBoolean(reg->fValid ? kTrue : kFalse);
			break;
	}
	data->AppendReturnData(script, propID, out);
	return kSuccess;
}
