// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_TAB_MODAL_CONFIRM_DIALOG_MAC_H_
#define CHROME_BROWSER_UI_COCOA_TAB_MODAL_CONFIRM_DIALOG_MAC_H_

#import <Cocoa/Cocoa.h>

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/ui/cocoa/constrained_window/constrained_window_mac.h"
#include "chrome/browser/ui/tab_modal_confirm_dialog.h"

@class ConstrainedWindowAlert;

namespace content {
class WebContents;
}

class TabModalConfirmDialogDelegate;
@class TabModalConfirmDialogMacBridge;

// Displays a tab-modal dialog, i.e. a dialog that will block the current page
// but still allow the user to switch to a different page.
// To display the dialog, allocate this object on the heap. It will open the
// dialog from its constructor and then delete itself when the user dismisses
// the dialog.
class TabModalConfirmDialogMac : public TabModalConfirmDialog,
                                 public ConstrainedWindowMacDelegate {
 public:
  TabModalConfirmDialogMac(TabModalConfirmDialogDelegate* delegate,
                           content::WebContents* web_contents);

 private:
  virtual ~TabModalConfirmDialogMac();

  // TabModalConfirmDialog:
  virtual void AcceptTabModalDialog() OVERRIDE;
  virtual void CancelTabModalDialog() OVERRIDE;

  // TabModalConfirmDialogCloseDelegate:
  virtual void CloseDialog() OVERRIDE;

  // ConstrainedWindowMacDelegate:
  virtual void OnConstrainedWindowClosed(
      ConstrainedWindowMac* window) OVERRIDE;

  scoped_ptr<ConstrainedWindowMac> window_;
  scoped_ptr<TabModalConfirmDialogDelegate> delegate_;
  scoped_nsobject<ConstrainedWindowAlert> alert_;
  scoped_nsobject<TabModalConfirmDialogMacBridge> bridge_;

  DISALLOW_COPY_AND_ASSIGN(TabModalConfirmDialogMac);
};

#endif  // CHROME_BROWSER_UI_COCOA_TAB_MODAL_CONFIRM_DIALOG_MAC_H_
