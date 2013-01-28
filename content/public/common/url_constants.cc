// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/common/url_constants.h"

#include "content/common/savable_url_schemes.h"
#include "googleurl/src/gurl.h"

namespace chrome {

const char kAboutScheme[] = "about";
const char kBlobScheme[] = "blob";

// Before adding new chrome schemes please check with security@chromium.org.
// There are security implications associated with introducing new schemes.
const char kChromeDevToolsScheme[] = "chrome-devtools";
const char kChromeInternalScheme[] = "chrome-internal";
const char kChromeSearchScheme[] = "chrome-search";
const char kChromeUIScheme[] = "chrome";
const char kDataScheme[] = "data";
const char kFileScheme[] = "file";
const char kFileSystemScheme[] = "filesystem";
const char kFtpScheme[] = "ftp";
const char kGuestScheme[] = "chrome-guest";
const char kHttpScheme[] = "http";
const char kHttpsScheme[] = "https";
const char kJavaScriptScheme[] = "javascript";
const char kMailToScheme[] = "mailto";
const char kMetadataScheme[] = "metadata";
const char kSwappedOutScheme[] = "swappedout";
const char kViewSourceScheme[] = "view-source";

const char kAboutBlankURL[] = "about:blank";
const char kAboutSrcDocURL[] = "about:srcdoc";
const char kChromeUIAppCacheInternalsHost[] = "appcache-internals";
const char kChromeUIBlobInternalsHost[] = "blob-internals";
const char kChromeUIBrowserCrashHost[] = "inducebrowsercrashforrealz";
const char kChromeUIDevToolsHost[] = "devtools";
const char kChromeUIHistogramHost[] = "histograms";
const char kChromeUINetworkViewCacheHost[] = "view-http-cache";
const char kChromeUIResourcesHost[] = "resources";
const char kChromeUITcmallocHost[] = "tcmalloc";

const char kChromeUICrashURL[] = "chrome://crash";
const char kChromeUIGpuCleanURL[] = "chrome://gpuclean";
const char kChromeUIGpuCrashURL[] = "chrome://gpucrash";
const char kChromeUIGpuHangURL[] = "chrome://gpuhang";
const char kChromeUIHangURL[] = "chrome://hang";
const char kChromeUIKillURL[] = "chrome://kill";
const char kChromeUIPpapiFlashCrashURL[] = "chrome://ppapiflashcrash";
const char kChromeUIPpapiFlashHangURL[] = "chrome://ppapiflashhang";

}  // namespace chrome

namespace content {

const char kStandardSchemeSeparator[] = "://";

// This error URL is loaded in normal web renderer processes, so it should not
// have a chrome:// scheme that might let it be confused with a WebUI page.
const char kUnreachableWebDataURL[] = "data:text/html,chromewebdata";

const char kChromeUINetworkViewCacheURL[] = "chrome://view-http-cache/";
const char kChromeUIShorthangURL[] = "chrome://shorthang";

// This URL is loaded when a page is swapped out and replaced by a page in a
// different renderer process.  It must have a unique origin that cannot be
// scripted by other pages in the process.
const char kSwappedOutURL[] = "swappedout://";

const char* const* GetSavableSchemes() {
  return GetSavableSchemesInternal();
}

bool HasWebUIScheme(const GURL& url) {
  return
#if !defined(OS_IOS)
         url.SchemeIs(chrome::kChromeDevToolsScheme) ||
         url.SchemeIs(chrome::kChromeInternalScheme) ||
#endif
         url.SchemeIs(chrome::kChromeUIScheme);
}

}  // namespace content
