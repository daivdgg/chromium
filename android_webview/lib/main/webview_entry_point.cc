// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/lib/main/aw_main_delegate.h"
#include "android_webview/native/android_webview_jni_registrar.h"
#include "base/android/jni_android.h"
#include "base/command_line.h"
#include "content/public/app/android_library_loader_hooks.h"
#include "content/public/app/content_main.h"
#include "content/public/browser/android/compositor.h"
#include "content/public/common/content_switches.h"
#include "ui/base/ui_base_switches.h"

// This is called by the VM when the shared library is first loaded.
// Most of the initialization is done in LibraryLoadedOnMainThread(), not here.
JNI_EXPORT jint JNI_OnLoad(JavaVM* vm, void* reserved) {
  base::android::InitVM(vm);
  JNIEnv* env = base::android::AttachCurrentThread();
  if (!content::RegisterLibraryLoaderEntryHook(env))
    return -1;

  if (!android_webview::RegisterJni(env))
    return -1;

  // Set the command line to enable synchronous API compatibility.
  CommandLine::Init(0, NULL);
  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableWebViewSynchronousAPIs);

  // TODO: The default locale needs to be set in order to prevent assertion
  // failures in WebKit. However, this is really a single process mode issue
  // and should be properly fixed. See bug 153758.
  CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kLang, "en-US");

  // TODO: The next two lines are temporarily required for the renderer
  // initialization to not crash.
  // See BUG 152904.
  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kSingleProcess);
  content::Compositor::Initialize();

  content::SetContentMainDelegate(new android_webview::AwMainDelegate());

  return JNI_VERSION_1_4;
}
