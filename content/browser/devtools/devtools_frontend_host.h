// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_DEVTOOLS_FRONTEND_HOST_H_
#define CONTENT_BROWSER_DEVTOOLS_DEVTOOLS_FRONTEND_HOST_H_

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "content/public/browser/devtools_client_host.h"
#include "content/public/browser/web_contents_observer.h"

namespace content {

class DevToolsFrontendHostDelegate;
class WebContentsImpl;

// This class handles messages from DevToolsClient and calls corresponding
// methods on DevToolsFrontendHostDelegate which is implemented by the
// embedder. This allows us to avoid exposing DevTools client messages through
// the content public API.
class DevToolsFrontendHost : public DevToolsClientHost,
                             public WebContentsObserver {
 public:
  DevToolsFrontendHost(WebContentsImpl* web_contents,
                       DevToolsFrontendHostDelegate* delegate);

 private:
  virtual ~DevToolsFrontendHost();

  // DevToolsClientHost implementation.
  virtual void DispatchOnInspectorFrontend(const std::string& message) OVERRIDE;
  virtual void InspectedContentsClosing() OVERRIDE;
  virtual void ReplacedWithAnotherClient() OVERRIDE;

  // WebContentsObserver overrides.
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

  void OnDispatchOnInspectorBackend(const std::string& message);
  void OnActivateWindow();
  void OnCloseWindow();
  void OnMoveWindow(int x, int y);
  void OnRequestSetDockSide(const std::string& side);
  void OnOpenInNewTab(const std::string& url);
  void OnSave(const std::string& url, const std::string& content, bool save_as);
  void OnAppend(const std::string& url, const std::string& content);
  void OnRequestFileSystems();
  void OnAddFileSystem();
  void OnRemoveFileSystem(const std::string& file_system_path);

  DevToolsFrontendHostDelegate* delegate_;
  DISALLOW_COPY_AND_ASSIGN(DevToolsFrontendHost);
};

}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_DEVTOOLS_FRONTEND_HOST_H_
