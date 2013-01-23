// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_FUNCTION_HISTOGRAM_VALUE_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_FUNCTION_HISTOGRAM_VALUE_H_


namespace extensions {
namespace functions {

// Do not reorder this enumeration. If you need to add a new entry, add it just
// prior to ENUM_BOUNDARY.
enum HistogramValue {
  UNKNOWN = 0,
  WEBNAVIGATION_GETALLFRAMES,
  BROWSINGDATA_REMOVEWEBSQL,
  ALARMS_CREATE,
  FILEBROWSERPRIVATE_REMOVEFILEWATCH,
  COOKIES_GET,
  FONTSETTINGS_GETMINIMUMFONTSIZE,
  CHROMEOSINFOPRIVATE_GET,
  BOOKMARKMANAGERPRIVATE_CUT,
  TABS_CAPTUREVISIBLETAB,
  MANAGEMENT_SETENABLED,
  HISTORY_DELETEALL,
  STORAGE_GET,
  SOCKET_SETKEEPALIVE,
  DOWNLOADS_CANCEL,
  BOOKMARKS_CREATE,
  BOOKMARKS_UPDATE,
  FILEBROWSERPRIVATE_GETDRIVEFILES,
  TERMINALPRIVATE_ONTERMINALRESIZE,
  FILEBROWSERPRIVATE_REQUESTDIRECTORYREFRESH,
  BLUETOOTH_GETADAPTERSTATE,
  FILEBROWSERPRIVATE_CANCELFILETRANSFERS,
  FILEBROWSERPRIVATE_PINDRIVEFILE,
  SOCKET_WRITE,
  OMNIBOX_SETDEFAULTSUGGESTION,
  TTS_SPEAK,
  WALLPAPERPRIVATE_RESTOREMINIMIZEDWINDOWS,
  BROWSINGDATA_REMOVEHISTORY,
  FILEBROWSERPRIVATE_ISFULLSCREEN,
  AUTOTESTPRIVATE_LOGOUT,
  EXPERIMENTAL_HISTORY_GETMOSTVISITED,
  BLUETOOTH_DISCONNECT,
  BLUETOOTH_SETOUTOFBANDPAIRINGDATA,
  BOOKMARKMANAGERPRIVATE_CANPASTE,
  AUTOTESTPRIVATE_RESTART,
  USB_CLAIMINTERFACE,
  MEDIAPLAYERPRIVATE_SETWINDOWHEIGHT,
  EXPERIMENTAL_PROCESSES_GETPROCESSINFO,
  HISTORY_GETVISITS,
  SOCKET_BIND,
  TABS_MOVE,
  SOCKET_DISCONNECT,
  FILESYSTEM_GETWRITABLEENTRY,
  SYNCFILESYSTEM_REQUESTFILESYSTEM,
  COMMANDS_GETALL,
  EXPERIMENTAL_DISCOVERY_REMOVESUGGESTION,
  EXPERIMENTAL_INPUT_VIRTUALKEYBOARD_SENDKEYBOARDEVENT,
  BOOKMARKMANAGERPRIVATE_GETSUBTREE,
  EXPERIMENTAL_RLZ_RECORDPRODUCTEVENT,
  BOOKMARKS_GETRECENT,
  APP_CURRENTWINDOWINTERNAL_SETBOUNDS,
  CLOUDPRINTPRIVATE_SETUPCONNECTOR,
  SERIAL_SETCONTROLSIGNALS,
  FILEBROWSERPRIVATE_SETLASTMODIFIED,
  IDLE_SETDETECTIONINTERVAL,
  FILEBROWSERPRIVATE_GETFILETASKS,
  WEBSTOREPRIVATE_GETSTORELOGIN,
  SYSTEMPRIVATE_GETINCOGNITOMODEAVAILABILITY,
  EXPERIMENTAL_IDLTEST_SENDARRAYBUFFERVIEW,
  SOCKET_SETNODELAY,
  APP_CURRENTWINDOWINTERNAL_SHOW,
  WEBSTOREPRIVATE_GETBROWSERLOGIN,
  EXPERIMENTAL_IDENTITY_GETAUTHTOKEN,
  EXPERIMENTAL_SYSTEMINFO_DISPLAY_GET,
  BROWSINGDATA_REMOVEPLUGINDATA,
  SOCKET_LISTEN,
  MEDIAGALLERIES_GETMEDIAFILESYSTEMS,
  DOWNLOADS_OPEN,
  TABS_EXECUTESCRIPT,
  SYNCFILESYSTEM_GETUSAGEANDQUOTA,
  INPUTMETHODPRIVATE_GET,
  USB_CLOSEDEVICE,
  TTS_STOP,
  SERIAL_GETPORTS,
  FILEBROWSERPRIVATE_CLEARDRIVECACHE,
  SERIAL_GETCONTROLSIGNALS,
  DEVELOPERPRIVATE_ENABLE,
  FILEBROWSERPRIVATE_GETDRIVEFILEPROPERTIES,
  USB_FINDDEVICES,
  BOOKMARKMANAGERPRIVATE_DROP,
  FILEBROWSERPRIVATE_GETFILETRANSFERS,
  INPUT_IME_SETMENUITEMS,
  BOOKMARKS_EXPORT,
  HISTORY_SEARCH,
  TTSENGINE_SENDTTSEVENT,
  EXPERIMENTAL_ACCESSIBILITY_GETALERTSFORTAB,
  BOOKMARKS_IMPORT,
  SYNCFILESYSTEM_DELETEFILESYSTEM,
  DEBUGGER_SENDCOMMAND,
  DEBUGGER_DETACH,
  METRICSPRIVATE_RECORDSMALLCOUNT,
  APP_CURRENTWINDOWINTERNAL_MINIMIZE,
  DEVELOPERPRIVATE_AUTOUPDATE,
  EXPERIMENTAL_DNS_RESOLVE,
  EXPERIMENTAL_SYSTEMINFO_MEMORY_GET,
  HISTORY_ADDURL,
  TABS_GET,
  BROWSERACTION_SETBADGETEXT,
  TABS_RELOAD,
  WINDOWS_CREATE,
  DEVELOPERPRIVATE_LOADUNPACKED,
  DOWNLOADS_SETDESTINATION,
  EXPERIMENTAL_PROCESSES_GETPROCESSIDFORTAB,
  BOOKMARKS_GETCHILDREN,
  BROWSERACTION_GETTITLE,
  TERMINALPRIVATE_OPENTERMINALPROCESS,
  SERIAL_CLOSE,
  CONTEXTMENUS_REMOVE,
  FILEBROWSERPRIVATE_REQUESTLOCALFILESYSTEM,
  ECHOPRIVATE_GETREGISTRATIONCODE,
  TABS_GETCURRENT,
  FONTSETTINGS_CLEARDEFAULTFIXEDFONTSIZE,
  MEDIAPLAYERPRIVATE_CLOSEWINDOW,
  WEBREQUESTINTERNAL_ADDEVENTLISTENER,
  CLOUDPRINTPRIVATE_GETPRINTERS,
  STORAGE_SET,
  FONTSETTINGS_GETDEFAULTFONTSIZE,
  EXTENSION_SETUPDATEURLDATA,
  SERIAL_WRITE,
  IDLE_QUERYSTATE,
  EXPERIMENTAL_RLZ_GETACCESSPOINTRLZ,
  WEBSTOREPRIVATE_SETSTORELOGIN,
  PAGEACTIONS_ENABLEFORTAB,
  COOKIES_SET,
  CONTENTSETTINGS_SET,
  CONTEXTMENUS_REMOVEALL,
  TABS_INSERTCSS,
  WEBREQUEST_HANDLERBEHAVIORCHANGED,
  INPUT_IME_SETCURSORPOSITION,
  OMNIBOX_SENDSUGGESTIONS,
  SYSTEMINDICATOR_ENABLE,
  EVENTS_GETRULES,
  BOOKMARKMANAGERPRIVATE_COPY,
  SOCKET_RECVFROM,
  TABS_GETALLINWINDOW,
  CONTEXTMENUS_UPDATE,
  BOOKMARKS_SEARCH,
  EXPERIMENTAL_APP_CLEARALLNOTIFICATIONS,
  BLUETOOTH_GETLOCALOUTOFBANDPAIRINGDATA,
  SYSTEMPRIVATE_GETUPDATESTATUS,
  FONTSETTINGS_CLEARMINIMUMFONTSIZE,
  FILEBROWSERPRIVATE_GETFILELOCATIONS,
  EXPERIMENTAL_DISCOVERY_SUGGEST,
  FILEBROWSERPRIVATE_SETDEFAULTTASK,
  BROWSERACTION_GETBADGETEXT,
  APP_CURRENTWINDOWINTERNAL_HIDE,
  SOCKET_CONNECT,
  BOOKMARKS_GETSUBTREE,
  HISTORY_DELETEURL,
  EXPERIMENTAL_MEDIAGALLERIES_ASSEMBLEMEDIAFILE,
  BOOKMARKMANAGERPRIVATE_STARTDRAG,
  BROWSINGDATA_REMOVEPASSWORDS,
  DOWNLOADS_DRAG,
  INPUT_IME_SETCOMPOSITION,
  METRICSPRIVATE_RECORDUSERACTION,
  USB_RELEASEINTERFACE,
  PAGEACTION_GETPOPUP,
  SCRIPTBADGE_GETATTENTION,
  FONTSETTINGS_GETFONTLIST,
  PERMISSIONS_CONTAINS,
  SCRIPTBADGE_GETPOPUP,
  EXPERIMENTAL_ACCESSIBILITY_GETFOCUSEDCONTROL,
  DEVELOPERPRIVATE_GETSTRINGS,
  METRICSPRIVATE_RECORDMEDIUMCOUNT,
  MANAGEMENT_GET,
  PERMISSIONS_GETALL,
  DOWNLOADS_SHOW,
  EXPERIMENTAL_RLZ_CLEARPRODUCTSTATE,
  TABS_REMOVE,
  MANAGEMENT_GETPERMISSIONWARNINGSBYID,
  WINDOWS_GET,
  FILEBROWSERPRIVATE_EXECUTETASK,
  TTS_GETVOICES,
  MANAGEMENT_GETALL,
  MANAGEMENT_GETPERMISSIONWARNINGSBYMANIFEST,
  APP_CURRENTWINDOWINTERNAL_CLEARATTENTION,
  AUTOTESTPRIVATE_SHUTDOWN,
  FONTSETTINGS_CLEARDEFAULTFONTSIZE,
  BOOKMARKS_GETTREE,
  FILEBROWSERPRIVATE_SELECTFILES,
  RUNTIME_GETBACKGROUNDPAGE,
  EXPERIMENTAL_RECORD_REPLAYURLS,
  WEBSTOREPRIVATE_COMPLETEINSTALL,
  EXPERIMENTAL_SPEECHINPUT_START,
  COOKIES_GETALL,
  DOWNLOADS_GETFILEICON,
  PAGEACTION_GETTITLE,
  BROWSINGDATA_REMOVE,
  SERIAL_OPEN,
  FILESYSTEM_GETDISPLAYPATH,
  FILEBROWSERPRIVATE_FORMATDEVICE,
  BOOKMARKS_GET,
  MANAGEDMODEPRIVATE_GET,
  ALARMS_CLEAR,
  SYNCFILESYSTEM_GETFILESYNCSTATUS,
  SOCKET_GETINFO,
  WEBSTOREPRIVATE_INSTALLBUNDLE,
  BROWSERACTION_ENABLE,
  METRICSPRIVATE_RECORDMEDIUMTIME,
  PAGEACTION_SETTITLE,
  CLOUDPRINTPRIVATE_GETHOSTNAME,
  CONTENTSETTINGS_GETRESOURCEIDENTIFIERS,
  SOCKET_CREATE,
  DEVELOPERPRIVATE_RELOAD,
  FILEBROWSERPRIVATE_GETMOUNTPOINTS,
  APP_RUNTIME_POSTINTENTRESPONSE,
  MANAGEDMODEPRIVATE_SETPOLICY,
  WEBSTOREPRIVATE_BEGININSTALLWITHMANIFEST3,
  WALLPAPERPRIVATE_SETWALLPAPER,
  USB_CONTROLTRANSFER,
  EXPERIMENTAL_SPEECHINPUT_STOP,
  USB_BULKTRANSFER,
  FILEBROWSERPRIVATE_GETVOLUMEMETADATA,
  PAGECAPTURE_SAVEASMHTML,
  EXTENSION_ISALLOWEDINCOGNITOACCESS,
  BROWSINGDATA_REMOVEAPPCACHE,
  APP_CURRENTWINDOWINTERNAL_DRAWATTENTION,
  METRICSPRIVATE_RECORDCOUNT,
  USB_INTERRUPTTRANSFER,
  TYPES_CHROMESETTING_CLEAR,
  INPUT_IME_COMMITTEXT,
  EXPERIMENTAL_IDLTEST_SENDARRAYBUFFER,
  WALLPAPERPRIVATE_SETWALLPAPERIFEXIST,
  SOCKET_ACCEPT,
  WEBNAVIGATION_GETFRAME,
  EXPERIMENTAL_POWER_RELEASEKEEPAWAKE,
  APP_CURRENTWINDOWINTERNAL_SETICON,
  PUSHMESSAGING_GETCHANNELID,
  EXPERIMENTAL_INFOBARS_SHOW,
  INPUT_IME_SETCANDIDATEWINDOWPROPERTIES,
  METRICSPRIVATE_RECORDPERCENTAGE,
  TYPES_CHROMESETTING_GET,
  WINDOWS_GETLASTFOCUSED,
  MANAGEDMODEPRIVATE_GETPOLICY,
  STORAGE_CLEAR,
  STORAGE_GETBYTESINUSE,
  TABS_QUERY,
  PAGEACTION_SETPOPUP,
  DEVELOPERPRIVATE_INSPECT,
  DOWNLOADS_SEARCH,
  FONTSETTINGS_CLEARFONT,
  WINDOWS_UPDATE,
  BOOKMARKMANAGERPRIVATE_CANOPENNEWWINDOWS,
  SERIAL_FLUSH,
  BROWSERACTION_SETTITLE,
  BOOKMARKMANAGERPRIVATE_CANEDIT,
  WALLPAPERPRIVATE_SETCUSTOMWALLPAPER,
  BOOKMARKS_REMOVE,
  INPUT_IME_SETCANDIDATES,
  TERMINALPRIVATE_CLOSETERMINALPROCESS,
  HISTORY_DELETERANGE,
  EXPERIMENTAL_IDLTEST_GETARRAYBUFFER,
  TERMINALPRIVATE_SENDINPUT,
  TABS_HIGHLIGHT,
  BLUETOOTH_STARTDISCOVERY,
  FILEBROWSERPRIVATE_SELECTFILE,
  WINDOWS_GETCURRENT,
  DEBUGGER_ATTACH,
  WALLPAPERPRIVATE_SAVETHUMBNAIL,
  INPUT_IME_KEYEVENTHANDLED,
  FONTSETTINGS_SETDEFAULTFONTSIZE,
  RUNTIME_REQUESTUPDATECHECK,
  PAGEACTION_SETICON,
  BROWSERACTION_SETBADGEBACKGROUNDCOLOR,
  DEVELOPERPRIVATE_GETITEMSINFO,
  BLUETOOTH_STOPDISCOVERY,
  COOKIES_REMOVE,
  EXPERIMENTAL_RLZ_SENDFINANCIALPING,
  TABCAPTURE_GETCAPTUREDTABS,
  WINDOWS_REMOVE,
  WALLPAPERPRIVATE_GETOFFLINEWALLPAPERLIST,
  BROWSERACTION_GETBADGEBACKGROUNDCOLOR,
  PAGEACTIONS_DISABLEFORTAB,
  DEVELOPERPRIVATE_ALLOWFILEACCESS,
  FILEBROWSERPRIVATE_REMOVEMOUNT,
  BLUETOOTH_CONNECT,
  TABCAPTURE_CAPTURE,
  EXPERIMENTAL_NOTIFICATION_SHOW,
  TABS_DUPLICATE,
  BLUETOOTH_WRITE,
  PAGEACTION_SHOW,
  WALLPAPERPRIVATE_GETTHUMBNAIL,
  DOWNLOADS_PAUSE,
  PERMISSIONS_REQUEST,
  TOPSITES_GET,
  BROWSINGDATA_REMOVEDOWNLOADS,
  BROWSINGDATA_REMOVELOCALSTORAGE,
  FILEBROWSERHANDLERINTERNAL_SELECTFILE,
  INPUT_IME_UPDATEMENUITEMS,
  FILEBROWSERPRIVATE_GETSTRINGS,
  CONTENTSETTINGS_GET,
  FONTSETTINGS_SETDEFAULTFIXEDFONTSIZE,
  EXPERIMENTAL_APP_NOTIFY,
  METRICSPRIVATE_RECORDLONGTIME,
  SOCKET_READ,
  EXPERIMENTAL_PROCESSES_TERMINATE,
  METRICSPRIVATE_RECORDTIME,
  BOOKMARKMANAGERPRIVATE_GETSTRINGS,
  USB_ISOCHRONOUSTRANSFER,
  PERMISSIONS_REMOVE,
  MANAGEMENT_UNINSTALL,
  I18N_GETACCEPTLANGUAGES,
  MANAGEMENT_LAUNCHAPP,
  INPUT_IME_CLEARCOMPOSITION,
  ALARMS_GETALL,
  DIAL_DISCOVERNOW,
  TYPES_CHROMESETTING_SET,
  BROWSERACTION_SETICON,
  EXPERIMENTAL_ACCESSIBILITY_SETACCESSIBILITYENABLED,
  FILEBROWSERPRIVATE_VIEWFILES,
  BLUETOOTH_GETSERVICES,
  TABS_UPDATE,
  BROWSINGDATA_REMOVEFORMDATA,
  FILEBROWSERPRIVATE_RELOADDRIVE,
  ALARMS_GET,
  BROWSINGDATA_REMOVEINDEXEDDB,
  FILEBROWSERPRIVATE_ADDFILEWATCH,
  CONTENTSETTINGS_CLEAR,
  FILEBROWSERPRIVATE_GETPREFERENCES,
  BOOKMARKMANAGERPRIVATE_PASTE,
  FILESYSTEM_ISWRITABLEENTRY,
  USB_SETINTERFACEALTERNATESETTING,
  FONTSETTINGS_SETMINIMUMFONTSIZE,
  BROWSERACTION_GETPOPUP,
  SOCKET_DESTROY,
  BLUETOOTH_GETDEVICES,
  ALARMS_CLEARALL,
  FONTSETTINGS_GETDEFAULTFIXEDFONTSIZE,
  FILEBROWSERPRIVATE_ZIPSELECTION,
  SYSTEMINDICATOR_DISABLE,
  SCRIPTBADGE_SETPOPUP,
  EXTENSION_ISALLOWEDFILESCHEMEACCESS,
  EXPERIMENTAL_IDENTITY_LAUNCHWEBAUTHFLOW,
  FILEBROWSERPRIVATE_GETNETWORKCONNECTIONSTATE,
  TABS_DETECTLANGUAGE,
  METRICSPRIVATE_RECORDVALUE,
  BOOKMARKMANAGERPRIVATE_SORTCHILDREN,
  SERIAL_READ,
  APP_CURRENTWINDOWINTERNAL_MAXIMIZE,
  EXPERIMENTAL_DISCOVERY_CLEARALLSUGGESTIONS,
  MANAGEDMODEPRIVATE_ENTER,
  FILEBROWSERPRIVATE_TRANSFERFILE,
  BROWSERACTION_SETPOPUP,
  TABS_GETSELECTED,
  FONTSETTINGS_GETFONT,
  BLUETOOTH_READ,
  WEBREQUESTINTERNAL_EVENTHANDLED,
  EVENTS_ADDRULES,
  CONTEXTMENUS_CREATE,
  MEDIAPLAYERPRIVATE_GETPLAYLIST,
  DOWNLOADS_ERASE,
  EXPERIMENTAL_RECORD_CAPTUREURLS,
  TTS_ISSPEAKING,
  BOOKMARKS_REMOVETREE,
  FILEBROWSERPRIVATE_SEARCHDRIVE,
  EXPERIMENTAL_SYSTEMINFO_CPU_GET,
  FILEBROWSERPRIVATE_SETPREFERENCES,
  FONTSETTINGS_SETFONT,
  SOCKET_GETNETWORKLIST,
  BOOKMARKS_MOVE,
  WALLPAPERPRIVATE_MINIMIZEINACTIVEWINDOWS,
  STORAGE_REMOVE,
  AUTOTESTPRIVATE_LOGINSTATUS,
  TABS_CREATE,
  FILEBROWSERPRIVATE_CANCELDIALOG,
  BROWSINGDATA_REMOVECOOKIES,
  FILESYSTEM_CHOOSEENTRY,
  MEDIAPLAYERPRIVATE_PLAY,
  WEBSTOREPRIVATE_GETWEBGLSTATUS,
  SOCKET_SENDTO,
  BROWSINGDATA_REMOVEFILESYSTEMS,
  WALLPAPERPRIVATE_GETSTRINGS,
  BROWSINGDATA_REMOVECACHE,
  BOOKMARKMANAGERPRIVATE_RECORDLAUNCH,
  BROWSERACTION_DISABLE,
  EXPERIMENTAL_SPEECHINPUT_ISRECORDING,
  APP_WINDOW_CREATE,
  RUNTIME_RELOAD,
  EXPERIMENTAL_POWER_REQUESTKEEPAWAKE,
  SYSTEMINDICATOR_SETICON,
  FILEBROWSERPRIVATE_ADDMOUNT,
  APP_CURRENTWINDOWINTERNAL_FOCUS,
  EVENTS_REMOVERULES,
  DOWNLOADS_DOWNLOAD,
  WINDOWS_GETALL,
  FILEBROWSERPRIVATE_TOGGLEFULLSCREEN,
  APP_CURRENTWINDOWINTERNAL_RESTORE,
  WEBSOCKETPROXYPRIVATE_GETPASSPORTFORTCP,
  PAGEACTION_HIDE,
  EXPERIMENTAL_SYSTEMINFO_STORAGE_GET,
  DOWNLOADS_ACCEPTDANGER,
  WEBSOCKETPROXYPRIVATE_GETURLFORTCP,
  FILEBROWSERPRIVATE_GETSIZESTATS,
  DOWNLOADS_RESUME,
  COOKIES_GETALLCOOKIESTORES,
  MEDIAGALLERIESPRIVATE_ADDGALLERYWATCH,
  MEDIAGALLERIESPRIVATE_REMOVEGALLERYWATCH,
  ENUM_BOUNDARY // Last entry: Add new entries above.
};

}  // namespace functions
}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_FUNCTION_HISTOGRAM_VALUE_H_
