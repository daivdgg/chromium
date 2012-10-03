// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/command_line.h"
#include "base/file_path.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "chrome/browser/extensions/component_loader.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/extensions/script_bubble_controller.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "chrome/browser/ui/tab_contents/tab_contents.h"
#include "chrome/browser/ui/tab_contents/test_tab_contents.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_builder.h"
#include "chrome/common/extensions/feature_switch.h"
#include "chrome/common/extensions/value_builder.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/test/test_browser_thread.h"

using content::BrowserThread;

namespace extensions {
namespace {

class ScriptBubbleControllerTest : public TabContentsTestHarness {
 public:
  ScriptBubbleControllerTest()
      : ui_thread_(BrowserThread::UI, MessageLoop::current()),
        file_thread_(BrowserThread::FILE, MessageLoop::current()),
        enable_script_bubble_(FeatureSwitch::GetScriptBubble(), true) {
  }

  virtual void SetUp() OVERRIDE {
    TabContentsTestHarness::SetUp();
    CommandLine command_line(CommandLine::NO_PROGRAM);
    extension_service_ =
        static_cast<TestExtensionSystem*>(
            ExtensionSystem::Get(tab_contents()->profile()))->
        CreateExtensionService(
            &command_line, FilePath(), false);
    extension_service_->component_loader()->AddScriptBubble();
    extension_service_->Init();

    TabHelper::CreateForWebContents(web_contents());

    script_bubble_controller_ =
        TabHelper::FromWebContents(web_contents())->script_bubble_controller();
  }

 protected:
  int tab_id() {
    return ExtensionTabUtil::GetTabId(web_contents());
  }

  ExtensionService* extension_service_;
  ScriptBubbleController* script_bubble_controller_;

 private:
  content::TestBrowserThread ui_thread_;
  content::TestBrowserThread file_thread_;
  FeatureSwitch::ScopedOverride enable_script_bubble_;
};

TEST_F(ScriptBubbleControllerTest, Basics) {
#if defined(OS_WIN)
  FilePath root(FILE_PATH_LITERAL("c:\\"));
#else
  FilePath root(FILE_PATH_LITERAL("/root"));
#endif
  scoped_refptr<const Extension> extension1 =
      ExtensionBuilder()
      .SetPath(root.AppendASCII("f1"))
      .SetManifest(DictionaryBuilder()
                   .Set("name", "ex1")
                   .Set("version", "1")
                   .Set("manifest_version", 2))
      .Build();

  scoped_refptr<const Extension> extension2 =
      ExtensionBuilder()
      .SetPath(root.AppendASCII("f2"))
      .SetManifest(DictionaryBuilder()
                   .Set("name", "ex2")
                   .Set("version", "1")
                   .Set("manifest_version", 2))
      .Build();

  extension_service_->AddExtension(extension1);
  extension_service_->AddExtension(extension2);

  const Extension* script_bubble =
      extension_service_->component_loader()->GetScriptBubble();
  ExtensionAction* script_bubble_action = script_bubble->page_action();
  ASSERT_TRUE(script_bubble_action);

  // By default, the bubble should be invisible.
  NavigateAndCommit(GURL("http://www.google.com"));
  EXPECT_FALSE(script_bubble_action->GetIsVisible(tab_id()));
  EXPECT_EQ("", script_bubble_action->GetBadgeText(tab_id()));
  EXPECT_EQ(GURL(), script_bubble_action->GetPopupUrl(tab_id()));

  // Running a script on the tab causes the bubble to be visible.
  TabHelper::ContentScriptObserver::ExecutingScriptsMap executing_scripts;
  executing_scripts[extension1->id()].insert("script1");
  script_bubble_controller_->OnContentScriptsExecuting(
      web_contents(),
      executing_scripts,
      web_contents()->GetController().GetActiveEntry()->GetPageID(),
      web_contents()->GetController().GetActiveEntry()->GetURL());
  EXPECT_TRUE(script_bubble_action->GetIsVisible(tab_id()));
  EXPECT_EQ("1", script_bubble_action->GetBadgeText(tab_id()));
  std::set<std::string> extension_ids;
  extension_ids.insert(extension1->id());
  EXPECT_EQ(ScriptBubbleController::GetPopupUrl(script_bubble, extension_ids),
            script_bubble_action->GetPopupUrl(tab_id()));

  // Running a script from another extension increments the count.
  executing_scripts.clear();
  executing_scripts[extension2->id()].insert("script2");
  script_bubble_controller_->OnContentScriptsExecuting(
      web_contents(),
      executing_scripts,
      web_contents()->GetController().GetActiveEntry()->GetPageID(),
      web_contents()->GetController().GetActiveEntry()->GetURL());
  EXPECT_TRUE(script_bubble_action->GetIsVisible(tab_id()));
  EXPECT_EQ("2", script_bubble_action->GetBadgeText(tab_id()));
  extension_ids.insert(extension2->id());
  EXPECT_EQ(ScriptBubbleController::GetPopupUrl(script_bubble, extension_ids),
            script_bubble_action->GetPopupUrl(tab_id()));

  // Running another script from an already-seen extension does not affect
  // count.
  executing_scripts.clear();
  executing_scripts[extension2->id()].insert("script3");
  script_bubble_controller_->OnContentScriptsExecuting(
      web_contents(),
      executing_scripts,
      web_contents()->GetController().GetActiveEntry()->GetPageID(),
      web_contents()->GetController().GetActiveEntry()->GetURL());
  EXPECT_TRUE(script_bubble_action->GetIsVisible(tab_id()));
  EXPECT_EQ("2", script_bubble_action->GetBadgeText(tab_id()));
  EXPECT_EQ(ScriptBubbleController::GetPopupUrl(script_bubble, extension_ids),
            script_bubble_action->GetPopupUrl(tab_id()));

  // Navigating away resets the badge.
  NavigateAndCommit(GURL("http://www.google.com"));
  EXPECT_FALSE(script_bubble_action->GetIsVisible(tab_id()));
  EXPECT_EQ("", script_bubble_action->GetBadgeText(tab_id()));
  EXPECT_EQ(GURL(), script_bubble_action->GetPopupUrl(tab_id()));
};

}  // namespace
}  // namespace extensions
