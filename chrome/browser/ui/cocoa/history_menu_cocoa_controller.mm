// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/history_menu_cocoa_controller.h"

#include "base/memory/scoped_vector.h"
#include "chrome/app/chrome_command_ids.h"  // IDC_HISTORY_MENU
#import "chrome/browser/app_controller_mac.h"
#include "chrome/browser/history/history.h"
#include "chrome/browser/history/history_types.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/tab_restore_service.h"
#include "chrome/browser/sessions/tab_restore_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_tab_restore_service_delegate.h"
#include "chrome/browser/ui/cocoa/event_utils.h"
#include "chrome/browser/ui/host_desktop.h"
#include "webkit/glue/window_open_disposition.h"

using content::OpenURLParams;
using content::Referrer;

@implementation HistoryMenuCocoaController

- (id)initWithBridge:(HistoryMenuBridge*)bridge {
  if ((self = [super init])) {
    bridge_ = bridge;
    DCHECK(bridge_);
  }
  return self;
}

- (BOOL)validateMenuItem:(NSMenuItem*)menuItem {
  AppController* controller = [NSApp delegate];
  return [controller keyWindowIsNotModal];
}

// Open the URL of the given history item in the current tab.
- (void)openURLForItem:(const HistoryMenuBridge::HistoryItem*)node {
  Browser* browser =
      chrome::FindOrCreateTabbedBrowser(bridge_->profile(),
                                        chrome::HOST_DESKTOP_TYPE_NATIVE);
  WindowOpenDisposition disposition =
      event_utils::WindowOpenDispositionFromNSEvent([NSApp currentEvent]);

  // If this item can be restored using TabRestoreService, do so. Otherwise,
  // just load the URL.
  TabRestoreService* service =
      TabRestoreServiceFactory::GetForProfile(bridge_->profile());
  if (node->session_id && service) {
    service->RestoreEntryById(
        browser->tab_restore_service_delegate(), node->session_id,
        browser->host_desktop_type(), UNKNOWN);
  } else {
    DCHECK(node->url.is_valid());
    OpenURLParams params(
        node->url, Referrer(), disposition,
        content::PAGE_TRANSITION_AUTO_BOOKMARK, false);
    browser->OpenURL(params);
  }
}

- (IBAction)openHistoryMenuItem:(id)sender {
  const HistoryMenuBridge::HistoryItem* item =
      bridge_->HistoryItemForMenuItem(sender);
  [self openURLForItem:item];
}

@end  // HistoryMenuCocoaController
