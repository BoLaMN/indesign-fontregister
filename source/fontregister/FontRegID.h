//========================================================================================
//
//  FontRegID.h
//
//  IDs for the FontRegister plug-in.
//
//========================================================================================

#ifndef __FontRegID_h__
#define __FontRegID_h__

#include "SDKDef.h"

// Company:
#define kFontRegCompanyKey	kSDKDefPlugInCompanyKey
#define kFontRegCompanyValue	kSDKDefPlugInCompanyValue

// Plug-in:
#define kFontRegPluginName	"FontReg.sdk"
// PLACEHOLDER -- needs a real prefix from Adobe Developer Support before
// shipping. Deliberately distinct from indesign-httplink's 0x1DE000 so the two
// plug-ins can load side by side even with placeholder prefixes.
#define kFontRegPrefixNumber	0x1DF000
#define kFontRegVersion		kSDKDefPluginVersionString
#define kFontRegAuthor		"Adobe Developer Technologies"

#define kFontRegPrefix		RezLong(kFontRegPrefixNumber)
#define kFontRegStringPrefix	SDK_DEF_STRINGIZE(kFontRegPrefixNumber)

// Missing plug-in: (see ExtraPluginInfo resource)
#define kFontRegMissingPluginURLValue		kSDKDefPartnersStandardValue_enUS
#define kFontRegMissingPluginAlertValue	kSDKDefMissingPluginAlertValue

// PluginID:
DECLARE_PMID(kPlugInIDSpace, kFontRegPluginID, kFontRegPrefix + 0)

// ClassIDs:
DECLARE_PMID(kClassIDSpace, kFontRegAppScriptProviderBoss,    kFontRegPrefix + 0)
DECLARE_PMID(kClassIDSpace, kFontRegRegistrationProxyBoss,    kFontRegPrefix + 1)
DECLARE_PMID(kClassIDSpace, kFontRegStartupShutdownBoss,      kFontRegPrefix + 2)

// InterfaceIDs:
DECLARE_PMID(kInterfaceIDSpace, IID_IFONTREGSWEEPIDLETASK, kFontRegPrefix + 0)

// ImplementationIDs:
DECLARE_PMID(kImplementationIDSpace, kFontRegAppScriptProviderImpl,    kFontRegPrefix + 0)
DECLARE_PMID(kImplementationIDSpace, kFontRegProxyScriptImpl,          kFontRegPrefix + 1)
DECLARE_PMID(kImplementationIDSpace, kFontRegSweepIdleTaskImpl,        kFontRegPrefix + 2)
DECLARE_PMID(kImplementationIDSpace, kFontRegStartupShutdownImpl,      kFontRegPrefix + 3)

// ScriptInfo IDs
DECLARE_PMID(kScriptInfoIDSpace, kFontRegObjectScriptElement,             kFontRegPrefix + 0)
DECLARE_PMID(kScriptInfoIDSpace, kFontRegRegisterFontMethodScriptElement, kFontRegPrefix + 1)
DECLARE_PMID(kScriptInfoIDSpace, kFontRegRegisterFolderMethodScriptElement, kFontRegPrefix + 2)
DECLARE_PMID(kScriptInfoIDSpace, kFontRegUnregisterMethodScriptElement,   kFontRegPrefix + 3)
DECLARE_PMID(kScriptInfoIDSpace, kFontRegFontNamesPropertyScriptElement,  kFontRegPrefix + 4)
DECLARE_PMID(kScriptInfoIDSpace, kFontRegSourcePathPropertyScriptElement, kFontRegPrefix + 5)
DECLARE_PMID(kScriptInfoIDSpace, kFontRegIsValidPropertyScriptElement,    kFontRegPrefix + 6)
DECLARE_PMID(kScriptInfoIDSpace, kFontRegPathParamScriptElement,          kFontRegPrefix + 7)

// Four-char script IDs. Must be unique across the whole DOM.
enum FontRegScriptIDs
{
	c_FontRegistration      = 'fRgO',	// the object
	c_FontRegistrations     = 'fRgC',	// its collection
	e_FontRegRegisterFont   = 'fRgF',
	e_FontRegRegisterFolder = 'fRgD',
	e_FontRegUnregister     = 'fRgU',
	p_FontRegFontNames      = 'fRgN',
	p_FontRegSourcePath     = 'fRgS',
	p_FontRegIsValid        = 'fRgV',
	p_FontRegPathParam      = 'fRgP',
};

// Initial data format version numbers
#define kFontRegFirstMajorFormatNumber  RezLong(1)
#define kFontRegFirstMinorFormatNumber  RezLong(0)

#define kFontRegCurrentMajorFormatNumber kFontRegFirstMajorFormatNumber
#define kFontRegCurrentMinorFormatNumber kFontRegFirstMinorFormatNumber

#endif // __FontRegID_h__
