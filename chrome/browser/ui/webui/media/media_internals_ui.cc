// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/media/media_internals_ui.h"

#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/media/media_internals_handler.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "grit/browser_resources.h"
#include "ui/base/resource/resource_bundle.h"

using content::WebContents;

namespace {

content::WebUIDataSource* CreateMediaInternalsHTMLSource() {
  content::WebUIDataSource* source =
      content::WebUIDataSource::Create(chrome::kChromeUIMediaInternalsHost);

  source->SetJsonPath("strings.js");
  source->AddResourcePath("media_internals.js", IDR_MEDIA_INTERNALS_JS);
  source->SetDefaultResource(IDR_MEDIA_INTERNALS_HTML);
  return source;
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
//
// MediaInternalsUI
//
////////////////////////////////////////////////////////////////////////////////

MediaInternalsUI::MediaInternalsUI(content::WebUI* web_ui)
    : WebUIController(web_ui) {
  web_ui->AddMessageHandler(new MediaInternalsMessageHandler());

  Profile* profile = Profile::FromWebUI(web_ui);
  content::WebUIDataSource::Add(profile, CreateMediaInternalsHTMLSource());
}
