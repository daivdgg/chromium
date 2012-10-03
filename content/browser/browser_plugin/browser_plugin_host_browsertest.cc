// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/singleton.h"
#include "base/run_loop.h"
#include "base/test/test_timeouts.h"
#include "base/utf_string_conversions.h"
#include "content/browser/browser_plugin/browser_plugin_guest.h"
#include "content/browser/browser_plugin/browser_plugin_host_factory.h"
#include "content/browser/browser_plugin/test_browser_plugin_embedder.h"
#include "content/browser/browser_plugin/test_browser_plugin_guest.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/common/view_messages.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_view_host_observer.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "content/shell/shell.h"
#include "content/test/content_browser_test_utils.h"
#include "content/test/content_browser_test.h"
#include "net/base/net_util.h"
#include "net/test/test_server.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebInputEvent.h"

using WebKit::WebInputEvent;
using WebKit::WebMouseEvent;
using content::BrowserPluginEmbedder;
using content::BrowserPluginGuest;
using content::BrowserPluginHostFactory;

namespace {

const char* kHTMLForGuest =
    "data:text/html,<html><body>hello world</body></html>";
const char* kHTMLForGuestInfiniteLoop =
    "data:text/html,<html><head><script type=\"text/javascript\">"
    "function StartInfiniteLoop() {"
    "  setTimeout(function () {while (true) {} }, 0);"
    "}"
    "</script></head><body></body></html>";
const char kHTMLForGuestTouchHandler[] =
    "data:text/html,<html><body><div id=\"touch\">With touch</div></body>"
    "<script type=\"text/javascript\">"
    "function handler() {}"
    "function InstallTouchHandler() { "
    "  document.getElementById(\"touch\").addEventListener(\"touchstart\", "
    "     handler);"
    "}"
    "function UninstallTouchHandler() { "
    "  document.getElementById(\"touch\").removeEventListener(\"touchstart\", "
    "     handler);"
    "}"
    "</script></html>";
const char* kHTMLForGuestWithTitle =
    "data:text/html,"
    "<html><head><title>%s</title></head>"
    "<body>hello world</body>"
    "</html>";

std::string GetHTMLForGuestWithTitle(const std::string& title) {
  return StringPrintf(kHTMLForGuestWithTitle, title.c_str());
}

}  // namespace

namespace content {

// Test factory for creating test instances of BrowserPluginEmbedder and
// BrowserPluginGuest.
class TestBrowserPluginHostFactory : public BrowserPluginHostFactory {
 public:
  virtual BrowserPluginGuest* CreateBrowserPluginGuest(
      int instance_id,
      WebContentsImpl* web_contents,
      RenderViewHost* render_view_host) OVERRIDE {
    return new TestBrowserPluginGuest(instance_id,
                                      web_contents,
                                      render_view_host);
  }

  // Also keeps track of number of instances created.
  virtual BrowserPluginEmbedder* CreateBrowserPluginEmbedder(
      WebContentsImpl* web_contents,
      RenderViewHost* render_view_host) OVERRIDE {
    embedder_instance_count_++;
    if (message_loop_runner_)
      message_loop_runner_->Quit();

    return new TestBrowserPluginEmbedder(web_contents, render_view_host);
  }

  // Singleton getter.
  static TestBrowserPluginHostFactory* GetInstance() {
    return Singleton<TestBrowserPluginHostFactory>::get();
  }

  // Waits for at least one embedder to be created in the test. Returns true if
  // we have a guest, false if waiting times out.
  void WaitForEmbedderCreation() {
    // Check if already have created instance.
    if (embedder_instance_count_ > 0)
      return;
    // Wait otherwise.
    message_loop_runner_ = new MessageLoopRunner();
    message_loop_runner_->Run();
  }

 protected:
  TestBrowserPluginHostFactory() : embedder_instance_count_(0) {}
  virtual ~TestBrowserPluginHostFactory() {}

 private:
  // For Singleton.
  friend struct DefaultSingletonTraits<TestBrowserPluginHostFactory>;

  scoped_refptr<MessageLoopRunner> message_loop_runner_;
  int embedder_instance_count_;

  DISALLOW_COPY_AND_ASSIGN(TestBrowserPluginHostFactory);
};

// Test factory class for browser plugin that creates guests with short hang
// timeout.
class TestShortHangTimeoutGuestFactory : public TestBrowserPluginHostFactory {
 public:
  virtual BrowserPluginGuest* CreateBrowserPluginGuest(
      int instance_id,
      WebContentsImpl* web_contents,
      RenderViewHost* render_view_host) OVERRIDE {
    BrowserPluginGuest* guest = new TestBrowserPluginGuest(instance_id,
                                                         web_contents,
                                                         render_view_host);
    guest->set_guest_hang_timeout_for_testing(TestTimeouts::tiny_timeout());
    return guest;
  }

  // Singleton getter.
  static TestShortHangTimeoutGuestFactory* GetInstance() {
    return Singleton<TestShortHangTimeoutGuestFactory>::get();
  }

 protected:
  TestShortHangTimeoutGuestFactory() {}
  virtual ~TestShortHangTimeoutGuestFactory() {}

 private:
  // For Singleton.
  friend struct DefaultSingletonTraits<TestShortHangTimeoutGuestFactory>;

  DISALLOW_COPY_AND_ASSIGN(TestShortHangTimeoutGuestFactory);
};

// A transparent observer that can be used to verify that a RenderViewHost
// received a specific message.
class RenderViewHostMessageObserver : public RenderViewHostObserver {
 public:
  RenderViewHostMessageObserver(RenderViewHost* host,
                                uint32 message_id)
      : RenderViewHostObserver(host),
        message_id_(message_id),
        message_received_(false) {
  }

  virtual ~RenderViewHostMessageObserver() {}

  void WaitUntilMessageReceived() {
    if (message_received_)
      return;
    message_loop_runner_ = new MessageLoopRunner();
    message_loop_runner_->Run();
  }

  void ResetState() {
    message_received_ = false;
  }

  // IPC::Listener implementation.
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE {
    if (message.type() == message_id_) {
      message_received_ = true;
      if (message_loop_runner_)
        message_loop_runner_->Quit();
    }
    return false;
  }

 private:
  scoped_refptr<MessageLoopRunner> message_loop_runner_;
  uint32 message_id_;
  bool message_received_;

  DISALLOW_COPY_AND_ASSIGN(RenderViewHostMessageObserver);
};

class BrowserPluginHostTest : public ContentBrowserTest {
 public:
  BrowserPluginHostTest() {}

  virtual void SetUp() OVERRIDE {
    // Override factory to create tests instances of BrowserPlugin*.
    content::BrowserPluginEmbedder::set_factory_for_testing(
        TestBrowserPluginHostFactory::GetInstance());
    content::BrowserPluginGuest::set_factory_for_testing(
        TestBrowserPluginHostFactory::GetInstance());

    ContentBrowserTest::SetUp();
  }
  virtual void TearDown() OVERRIDE {
    content::BrowserPluginEmbedder::set_factory_for_testing(NULL);
    content::BrowserPluginGuest::set_factory_for_testing(NULL);

    ContentBrowserTest::TearDown();
  }

  static void SimulateTabKeyPress(WebContents* web_contents) {
    SimulateKeyPress(web_contents,
                     ui::VKEY_TAB,
                     false,   // control.
                     false,   // shift.
                     false,   // alt.
                     false);  // command.
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(BrowserPluginHostTest);
};

// This test loads a guest that has infinite loop, therefore it hangs the guest
// and eventually gets killed.
// TODO(lazyboy): This test is flaky on Windows, since this relies on
// RenderViewGone to be called and times out. http://crbug.com/151190.
#if defined(OS_WIN)
#define MAYBE_NavigateGuest DISABLED_NavigateGuest
#else
#define MAYBE_NavigateGuest NavigateGuest
#endif
IN_PROC_BROWSER_TEST_F(BrowserPluginHostTest, MAYBE_NavigateGuest) {
  // Override the hang timeout for guest to be very small.
  content::BrowserPluginGuest::set_factory_for_testing(
      TestShortHangTimeoutGuestFactory::GetInstance());
  ASSERT_TRUE(test_server()->Start());
  GURL test_url(test_server()->GetURL(
      "files/browser_plugin_embedder_crash.html"));
  NavigateToURL(shell(), test_url);

  WebContentsImpl* embedder_web_contents = static_cast<WebContentsImpl*>(
      shell()->web_contents());
  RenderViewHostImpl* rvh = static_cast<RenderViewHostImpl*>(
      embedder_web_contents->GetRenderViewHost());

  rvh->ExecuteJavascriptAndGetValue(string16(), ASCIIToUTF16(
      StringPrintf("SetSrc('%s');", kHTMLForGuestInfiniteLoop)));

  // Wait to make sure embedder is created/attached to WebContents.
  TestBrowserPluginHostFactory::GetInstance()->WaitForEmbedderCreation();

  TestBrowserPluginEmbedder* test_embedder =
      static_cast<TestBrowserPluginEmbedder*>(
          embedder_web_contents->GetBrowserPluginEmbedder());
  ASSERT_TRUE(test_embedder);
  test_embedder->WaitForGuestAdded();

  // Verify that we have exactly one guest.
  const BrowserPluginEmbedder::ContainerInstanceMap& instance_map =
      test_embedder->guest_web_contents_for_testing();
  EXPECT_EQ(1u, instance_map.size());

  WebContentsImpl* test_guest_web_contents = static_cast<WebContentsImpl*>(
      instance_map.begin()->second);
  TestBrowserPluginGuest* test_guest = static_cast<TestBrowserPluginGuest*>(
      test_guest_web_contents->GetBrowserPluginGuest());

  // Wait for the guest to send an UpdateRectMsg, meaning it is ready.
  test_guest->WaitForUpdateRectMsg();

  test_guest_web_contents->GetRenderViewHost()->ExecuteJavascriptAndGetValue(
      string16(), ASCIIToUTF16("StartInfiniteLoop();"));

  // Send a mouse event to the guest.
  SimulateMouseClick(embedder_web_contents);

  // Expect the guest to crash.
  test_guest->WaitForCrashed();
}

// This test ensures that if guest isn't there and we resize the guest (from
// js), it remembers the size correctly.
//
// Initially we load an embedder with a guest without a src attribute (which has
// dimension 640x480), resize it to 100x200, and then we set the source to a
// sample guest. In the end we verify that the correct size has been set.
IN_PROC_BROWSER_TEST_F(BrowserPluginHostTest, NavigateAfterResize) {
  ASSERT_TRUE(test_server()->Start());
  GURL test_url(test_server()->GetURL(
      "files/browser_plugin_embedder.html"));
  NavigateToURL(shell(), test_url);

  WebContentsImpl* embedder_web_contents = static_cast<WebContentsImpl*>(
      shell()->web_contents());
  RenderViewHostImpl* rvh = static_cast<RenderViewHostImpl*>(
      embedder_web_contents->GetRenderViewHost());

  const gfx::Size nxt_size = gfx::Size(100, 200);
  rvh->ExecuteJavascriptAndGetValue(string16(), ASCIIToUTF16(
      StringPrintf("SetSize(%d, %d);", nxt_size.width(), nxt_size.height())));

  rvh->ExecuteJavascriptAndGetValue(string16(), ASCIIToUTF16(
      StringPrintf("SetSrc('%s');", kHTMLForGuest)));

  // Wait to make sure embedder is created/attached to WebContents.
  TestBrowserPluginHostFactory::GetInstance()->WaitForEmbedderCreation();

  TestBrowserPluginEmbedder* test_embedder =
      static_cast<TestBrowserPluginEmbedder*>(
          embedder_web_contents->GetBrowserPluginEmbedder());
  ASSERT_TRUE(test_embedder);
  test_embedder->WaitForGuestAdded();

  // Verify that we have exactly one guest.
  const BrowserPluginEmbedder::ContainerInstanceMap& instance_map =
      test_embedder->guest_web_contents_for_testing();
  EXPECT_EQ(1u, instance_map.size());

  WebContentsImpl* test_guest_web_contents = static_cast<WebContentsImpl*>(
      instance_map.begin()->second);
  TestBrowserPluginGuest* test_guest = static_cast<TestBrowserPluginGuest*>(
      test_guest_web_contents->GetBrowserPluginGuest());

  // Wait for the guest to receive a damage buffer of size 100x200.
  // This means the guest will be painted properly at that size.
  test_guest->WaitForDamageBufferWithSize(nxt_size);
}

IN_PROC_BROWSER_TEST_F(BrowserPluginHostTest, AdvanceFocus) {
  ASSERT_TRUE(test_server()->Start());
  GURL test_url(test_server()->GetURL(
      "files/browser_plugin_focus.html"));
  NavigateToURL(shell(), test_url);

  WebContentsImpl* embedder_web_contents = static_cast<WebContentsImpl*>(
      shell()->web_contents());
  RenderViewHostImpl* rvh = static_cast<RenderViewHostImpl*>(
      embedder_web_contents->GetRenderViewHost());

  test_url = test_server()->GetURL(
      "files/browser_plugin_focus_child.html");
  rvh->ExecuteJavascriptAndGetValue(string16(), ASCIIToUTF16(
      StringPrintf("SetSrc('%s');", test_url.spec().c_str())));

  // Wait to make sure embedder is created/attached to WebContents.
  TestBrowserPluginHostFactory::GetInstance()->WaitForEmbedderCreation();

  TestBrowserPluginEmbedder* test_embedder =
      static_cast<TestBrowserPluginEmbedder*>(
          embedder_web_contents->GetBrowserPluginEmbedder());
  ASSERT_TRUE(test_embedder);
  test_embedder->WaitForGuestAdded();

  // Verify that we have exactly one guest.
  const BrowserPluginEmbedder::ContainerInstanceMap& instance_map =
      test_embedder->guest_web_contents_for_testing();
  EXPECT_EQ(1u, instance_map.size());

  WebContentsImpl* test_guest_web_contents = static_cast<WebContentsImpl*>(
      instance_map.begin()->second);
  TestBrowserPluginGuest* test_guest = static_cast<TestBrowserPluginGuest*>(
      test_guest_web_contents->GetBrowserPluginGuest());
  test_guest->WaitForUpdateRectMsg();

  SimulateMouseClick(embedder_web_contents);
  BrowserPluginHostTest::SimulateTabKeyPress(embedder_web_contents);
  // Wait until we focus into the guest.
  test_guest->WaitForFocus();

  // TODO(fsamuel): A third Tab key press should not be necessary.
  // The browser plugin will take keyboard focus but it will not
  // focus an initial element. The initial element is dependent
  // upon tab direction which WebKit does not propagate to the plugin.
  // See http://crbug.com/147644.
  BrowserPluginHostTest::SimulateTabKeyPress(embedder_web_contents);
  BrowserPluginHostTest::SimulateTabKeyPress(embedder_web_contents);
  BrowserPluginHostTest::SimulateTabKeyPress(embedder_web_contents);
  test_guest->WaitForAdvanceFocus();
}

// This test opens a page in http and then opens another page in https, forcing
// a RenderViewHost swap in the web_contents. We verify that the embedder in the
// web_contents gets cleared properly.
IN_PROC_BROWSER_TEST_F(BrowserPluginHostTest, EmbedderChangedAfterSwap) {
  ASSERT_TRUE(test_server()->Start());
  net::TestServer https_server(
      net::TestServer::TYPE_HTTPS,
      net::TestServer::kLocalhost,
      FilePath(FILE_PATH_LITERAL("content/test/data")));
  ASSERT_TRUE(https_server.Start());

  // 1. Load an embedder page with one guest in it.
  GURL test_url(test_server()->GetURL("files/browser_plugin_embedder.html"));
  NavigateToURL(shell(), test_url);

  WebContentsImpl* embedder_web_contents = static_cast<WebContentsImpl*>(
      shell()->web_contents());
  RenderViewHostImpl* rvh = static_cast<RenderViewHostImpl*>(
      embedder_web_contents->GetRenderViewHost());
  rvh->ExecuteJavascriptAndGetValue(string16(), ASCIIToUTF16(
      StringPrintf("SetSrc('%s');", kHTMLForGuest)));

  // Wait to make sure embedder is created/attached to WebContents.
  TestBrowserPluginHostFactory::GetInstance()->WaitForEmbedderCreation();

  TestBrowserPluginEmbedder* test_embedder_before_swap =
      static_cast<TestBrowserPluginEmbedder*>(
          embedder_web_contents->GetBrowserPluginEmbedder());
  ASSERT_TRUE(test_embedder_before_swap);
  test_embedder_before_swap->WaitForGuestAdded();

  // Verify that we have exactly one guest.
  const BrowserPluginEmbedder::ContainerInstanceMap& instance_map =
      test_embedder_before_swap->guest_web_contents_for_testing();
  EXPECT_EQ(1u, instance_map.size());

  WebContentsImpl* test_guest_web_contents = static_cast<WebContentsImpl*>(
      instance_map.begin()->second);
  TestBrowserPluginGuest* test_guest = static_cast<TestBrowserPluginGuest*>(
      test_guest_web_contents->GetBrowserPluginGuest());

  // Wait for the guest to send an UpdateRectMsg, which means the guest is
  // ready.
  test_guest->WaitForUpdateRectMsg();

  // 2. Navigate to a URL in https, so we trigger a RenderViewHost swap.
  GURL test_https_url(https_server.GetURL(
      "files/browser_plugin_title_change.html"));
  content::WindowedNotificationObserver swap_observer(
      content::NOTIFICATION_WEB_CONTENTS_SWAPPED,
      content::Source<WebContents>(embedder_web_contents));
  NavigateToURL(shell(), test_https_url);
  swap_observer.Wait();

  TestBrowserPluginEmbedder* test_embedder_after_swap =
      static_cast<TestBrowserPluginEmbedder*>(
          static_cast<WebContentsImpl*>(shell()->web_contents())->
              GetBrowserPluginEmbedder());
  // Verify we have a no embedder in web_contents (since the new page doesn't
  // have any browser plugin).
  ASSERT_TRUE(!test_embedder_after_swap);
  ASSERT_NE(test_embedder_before_swap, test_embedder_after_swap);
}

// This test opens two pages in http and there is no RenderViewHost swap,
// therefore the embedder created on first page navigation stays the same in
// web_contents.
IN_PROC_BROWSER_TEST_F(BrowserPluginHostTest, EmbedderSameAfterNav) {
  ASSERT_TRUE(test_server()->Start());

  GURL test_url(test_server()->GetURL("files/browser_plugin_embedder.html"));
  NavigateToURL(shell(), test_url);

  WebContentsImpl* embedder_web_contents = static_cast<WebContentsImpl*>(
      shell()->web_contents());
  RenderViewHostImpl* rvh = static_cast<RenderViewHostImpl*>(
      embedder_web_contents->GetRenderViewHost());

  rvh->ExecuteJavascriptAndGetValue(string16(), ASCIIToUTF16(
      StringPrintf("SetSrc('%s');", kHTMLForGuest)));

  // Wait to make sure embedder is created/attached to WebContents.
  TestBrowserPluginHostFactory::GetInstance()->WaitForEmbedderCreation();

  TestBrowserPluginEmbedder* test_embedder =
      static_cast<TestBrowserPluginEmbedder*>(
          embedder_web_contents->GetBrowserPluginEmbedder());
  ASSERT_TRUE(test_embedder);
  test_embedder->WaitForGuestAdded();

  // Verify that we have exactly one guest.
  const BrowserPluginEmbedder::ContainerInstanceMap& instance_map =
      test_embedder->guest_web_contents_for_testing();
  EXPECT_EQ(1u, instance_map.size());

  WebContentsImpl* test_guest_web_contents = static_cast<WebContentsImpl*>(
      instance_map.begin()->second);
  TestBrowserPluginGuest* test_guest = static_cast<TestBrowserPluginGuest*>(
      test_guest_web_contents->GetBrowserPluginGuest());

  // Wait for the guest to send an UpdateRectMsg, which means the guest is
  // ready.
  test_guest->WaitForUpdateRectMsg();

  // Navigate to another page in same host and port, so RenderViewHost swap
  // does not happen and existing embedder doesn't change in web_contents.
  GURL test_url_new(test_server()->GetURL(
      "files/browser_plugin_title_change.html"));
  const string16 expected_title = ASCIIToUTF16("done");
  content::TitleWatcher title_watcher(shell()->web_contents(), expected_title);
  NavigateToURL(shell(), test_url_new);
  LOG(INFO) << "Start waiting for title";
  string16 actual_title = title_watcher.WaitAndGetTitle();
  EXPECT_EQ(expected_title, actual_title);
  LOG(INFO) << "Done navigating to second page";

  TestBrowserPluginEmbedder* test_embedder_after_nav =
      static_cast<TestBrowserPluginEmbedder*>(
          embedder_web_contents->GetBrowserPluginEmbedder());
  // Embedder must not change in web_contents.
  ASSERT_EQ(test_embedder_after_nav, test_embedder);
}

IN_PROC_BROWSER_TEST_F(BrowserPluginHostTest, VisibilityChanged) {
  ASSERT_TRUE(test_server()->Start());
  GURL test_url(test_server()->GetURL(
      "files/browser_plugin_focus.html"));
  NavigateToURL(shell(), test_url);

  WebContentsImpl* embedder_web_contents = static_cast<WebContentsImpl*>(
      shell()->web_contents());
  RenderViewHostImpl* rvh = static_cast<RenderViewHostImpl*>(
      embedder_web_contents->GetRenderViewHost());

  test_url = test_server()->GetURL(
      "files/browser_plugin_focus_child.html");
  rvh->ExecuteJavascriptAndGetValue(string16(), ASCIIToUTF16(
      StringPrintf("SetSrc('%s');", test_url.spec().c_str())));

  // Wait to make sure embedder is created/attached to WebContents.
  TestBrowserPluginHostFactory::GetInstance()->WaitForEmbedderCreation();

  TestBrowserPluginEmbedder* test_embedder =
      static_cast<TestBrowserPluginEmbedder*>(
          embedder_web_contents->GetBrowserPluginEmbedder());
  ASSERT_TRUE(test_embedder);
  test_embedder->WaitForGuestAdded();

  // Verify that we have exactly one guest.
  const BrowserPluginEmbedder::ContainerInstanceMap& instance_map =
      test_embedder->guest_web_contents_for_testing();
  EXPECT_EQ(1u, instance_map.size());

  WebContentsImpl* test_guest_web_contents = static_cast<WebContentsImpl*>(
      instance_map.begin()->second);
  TestBrowserPluginGuest* test_guest = static_cast<TestBrowserPluginGuest*>(
      test_guest_web_contents->GetBrowserPluginGuest());

  // Wait for the guest to send an UpdateRectMsg, meaning it is ready.
  test_guest->WaitForUpdateRectMsg();

  // Hide the embedder.
  embedder_web_contents->WasHidden();

  // Make sure that hiding the embedder also hides the guest.
  test_guest->WaitUntilHidden();
}

// This test verifies that calling the reload method reloads the guest.
IN_PROC_BROWSER_TEST_F(BrowserPluginHostTest, ReloadGuest) {
  ASSERT_TRUE(test_server()->Start());
  GURL test_url(test_server()->GetURL(
      "files/browser_plugin_embedder.html"));
  NavigateToURL(shell(), test_url);

  WebContentsImpl* embedder_web_contents = static_cast<WebContentsImpl*>(
      shell()->web_contents());
  RenderViewHostImpl* rvh = static_cast<RenderViewHostImpl*>(
      embedder_web_contents->GetRenderViewHost());

  rvh->ExecuteJavascriptAndGetValue(string16(), ASCIIToUTF16(
      StringPrintf("SetSrc('%s');", kHTMLForGuest)));

  // Wait to make sure embedder is created/attached to WebContents.
  TestBrowserPluginHostFactory::GetInstance()->WaitForEmbedderCreation();

  TestBrowserPluginEmbedder* test_embedder =
      static_cast<TestBrowserPluginEmbedder*>(
          embedder_web_contents->GetBrowserPluginEmbedder());
  ASSERT_TRUE(test_embedder);
  test_embedder->WaitForGuestAdded();

  // Verify that we have exactly one guest.
  const BrowserPluginEmbedder::ContainerInstanceMap& instance_map =
      test_embedder->guest_web_contents_for_testing();
  EXPECT_EQ(1u, instance_map.size());

  WebContentsImpl* test_guest_web_contents = static_cast<WebContentsImpl*>(
      instance_map.begin()->second);
  TestBrowserPluginGuest* test_guest = static_cast<TestBrowserPluginGuest*>(
      test_guest_web_contents->GetBrowserPluginGuest());
  test_guest->WaitForUpdateRectMsg();
  test_guest->ResetUpdateRectCount();

  rvh->ExecuteJavascriptAndGetValue(string16(), ASCIIToUTF16(
      "document.getElementById('plugin').reload()"));
  test_guest->WaitForReload();
}

// This test verifies that calling the stop method forwards the stop request
// to the guest's WebContents.
IN_PROC_BROWSER_TEST_F(BrowserPluginHostTest, StopGuest) {
  ASSERT_TRUE(test_server()->Start());
  GURL test_url(test_server()->GetURL(
      "files/browser_plugin_embedder.html"));
  NavigateToURL(shell(), test_url);

  WebContentsImpl* embedder_web_contents = static_cast<WebContentsImpl*>(
      shell()->web_contents());
  RenderViewHostImpl* rvh = static_cast<RenderViewHostImpl*>(
      embedder_web_contents->GetRenderViewHost());

  rvh->ExecuteJavascriptAndGetValue(string16(), ASCIIToUTF16(
      StringPrintf("SetSrc('%s');", kHTMLForGuest)));

  // Wait to make sure embedder is created/attached to WebContents.
  TestBrowserPluginHostFactory::GetInstance()->WaitForEmbedderCreation();

  TestBrowserPluginEmbedder* test_embedder =
      static_cast<TestBrowserPluginEmbedder*>(
          embedder_web_contents->GetBrowserPluginEmbedder());
  ASSERT_TRUE(test_embedder);
  test_embedder->WaitForGuestAdded();

  // Verify that we have exactly one guest.
  const BrowserPluginEmbedder::ContainerInstanceMap& instance_map =
      test_embedder->guest_web_contents_for_testing();
  EXPECT_EQ(1u, instance_map.size());

  WebContentsImpl* test_guest_web_contents = static_cast<WebContentsImpl*>(
      instance_map.begin()->second);
  TestBrowserPluginGuest* test_guest = static_cast<TestBrowserPluginGuest*>(
      test_guest_web_contents->GetBrowserPluginGuest());
  test_guest->WaitForUpdateRectMsg();

  rvh->ExecuteJavascriptAndGetValue(string16(), ASCIIToUTF16(
      "document.getElementById('plugin').stop()"));
  test_guest->WaitForStop();
}

// Verifies that installing/uninstalling touch-event handlers in the guest
// plugin correctly updates the touch-event handling state in the embedder.
IN_PROC_BROWSER_TEST_F(BrowserPluginHostTest, AcceptTouchEvents) {
  ASSERT_TRUE(test_server()->Start());
  GURL test_url(test_server()->GetURL(
      "files/browser_plugin_embedder.html"));
  NavigateToURL(shell(), test_url);

  WebContentsImpl* embedder_web_contents = static_cast<WebContentsImpl*>(
      shell()->web_contents());
  RenderViewHostImpl* rvh = static_cast<RenderViewHostImpl*>(
      embedder_web_contents->GetRenderViewHost());

  rvh->ExecuteJavascriptAndGetValue(string16(), ASCIIToUTF16(
      StringPrintf("SetSrc('%s');", kHTMLForGuestTouchHandler)));

  // Wait to make sure embedder is created/attached to WebContents.
  TestBrowserPluginHostFactory::GetInstance()->WaitForEmbedderCreation();

  TestBrowserPluginEmbedder* test_embedder =
      static_cast<TestBrowserPluginEmbedder*>(
          embedder_web_contents->GetBrowserPluginEmbedder());
  ASSERT_TRUE(test_embedder);
  test_embedder->WaitForGuestAdded();

  // Verify that we have exactly one guest.
  const BrowserPluginEmbedder::ContainerInstanceMap& instance_map =
      test_embedder->guest_web_contents_for_testing();
  EXPECT_EQ(1u, instance_map.size());

  WebContentsImpl* test_guest_web_contents = static_cast<WebContentsImpl*>(
      instance_map.begin()->second);
  TestBrowserPluginGuest* test_guest = static_cast<TestBrowserPluginGuest*>(
      test_guest_web_contents->GetBrowserPluginGuest());

  // Wait for the guest to send an UpdateRectMsg, which means the guest is
  // ready.
  test_guest->WaitForUpdateRectMsg();

  // The embedder should not have any touch event handlers at this point.
  EXPECT_FALSE(rvh->has_touch_handler());

  // Install the touch handler in the guest. This should cause the embedder to
  // start listening for touch events too.
  RenderViewHostMessageObserver observer(rvh,
      ViewHostMsg_HasTouchEventHandlers::ID);
  test_guest_web_contents->GetRenderViewHost()->ExecuteJavascriptAndGetValue(
      string16(), ASCIIToUTF16("InstallTouchHandler();"));
  observer.WaitUntilMessageReceived();
  EXPECT_TRUE(rvh->has_touch_handler());

  // Uninstalling the touch-handler in guest should cause the embedder to stop
  // listening for touch events.
  observer.ResetState();
  test_guest_web_contents->GetRenderViewHost()->ExecuteJavascriptAndGetValue(
      string16(), ASCIIToUTF16("UninstallTouchHandler();"));
  observer.WaitUntilMessageReceived();
  EXPECT_FALSE(rvh->has_touch_handler());
}

IN_PROC_BROWSER_TEST_F(BrowserPluginHostTest, Renavigate) {
  ASSERT_TRUE(test_server()->Start());
  GURL test_url(test_server()->GetURL(
      "files/browser_plugin_embedder.html"));
  NavigateToURL(shell(), test_url);

  WebContentsImpl* embedder_web_contents = static_cast<WebContentsImpl*>(
      shell()->web_contents());
  RenderViewHostImpl* rvh = static_cast<RenderViewHostImpl*>(
      embedder_web_contents->GetRenderViewHost());

  rvh->ExecuteJavascriptAndGetValue(string16(), ASCIIToUTF16(
      StringPrintf("SetSrc('%s');", GetHTMLForGuestWithTitle("P1").c_str())));

  // Wait to make sure embedder is created/attached to WebContents.
  TestBrowserPluginHostFactory::GetInstance()->WaitForEmbedderCreation();

  TestBrowserPluginEmbedder* test_embedder =
      static_cast<TestBrowserPluginEmbedder*>(
          embedder_web_contents->GetBrowserPluginEmbedder());
  ASSERT_TRUE(test_embedder);
  test_embedder->WaitForGuestAdded();

  // Verify that we have exactly one guest.
  const BrowserPluginEmbedder::ContainerInstanceMap& instance_map =
      test_embedder->guest_web_contents_for_testing();
  EXPECT_EQ(1u, instance_map.size());

  WebContentsImpl* test_guest_web_contents = static_cast<WebContentsImpl*>(
      instance_map.begin()->second);
  TestBrowserPluginGuest* test_guest = static_cast<TestBrowserPluginGuest*>(
      test_guest_web_contents->GetBrowserPluginGuest());

  // Wait for the guest to send an UpdateRectMsg, meaning it is ready.
  test_guest->WaitForUpdateRectMsg();

  // Navigate to P2 and verify that the navigation occurred.
  {
    const string16 expected_title = ASCIIToUTF16("P2");
    content::TitleWatcher title_watcher(test_guest_web_contents,
                                        expected_title);

    rvh->ExecuteJavascriptAndGetValue(string16(), ASCIIToUTF16(
        StringPrintf("SetSrc('%s');", GetHTMLForGuestWithTitle("P2").c_str())));

    string16 actual_title = title_watcher.WaitAndGetTitle();
    EXPECT_EQ(expected_title, actual_title);
  }

  // Navigate to P3 and verify that the navigation occurred.
  {
    const string16 expected_title = ASCIIToUTF16("P3");
    content::TitleWatcher title_watcher(test_guest_web_contents,
                                        expected_title);

    rvh->ExecuteJavascriptAndGetValue(string16(), ASCIIToUTF16(
        StringPrintf("SetSrc('%s');", GetHTMLForGuestWithTitle("P3").c_str())));

    string16 actual_title = title_watcher.WaitAndGetTitle();
    EXPECT_EQ(expected_title, actual_title);
  }

  // Go back and verify that we're back at P2.
  {
    const string16 expected_title = ASCIIToUTF16("P2");
    content::TitleWatcher title_watcher(test_guest_web_contents,
                                        expected_title);

    rvh->ExecuteJavascriptAndGetValue(string16(), ASCIIToUTF16("Back();"));

    string16 actual_title = title_watcher.WaitAndGetTitle();
    EXPECT_EQ(expected_title, actual_title);
  }

  // Go forward and verify that we're back at P3.
  {
    const string16 expected_title = ASCIIToUTF16("P3");
    content::TitleWatcher title_watcher(test_guest_web_contents,
                                        expected_title);

    rvh->ExecuteJavascriptAndGetValue(string16(), ASCIIToUTF16("Forward();"));

    string16 actual_title = title_watcher.WaitAndGetTitle();
    EXPECT_EQ(expected_title, actual_title);
  }

  // Go back two entries and verify that we're back at P1.
  {
    const string16 expected_title = ASCIIToUTF16("P1");
    content::TitleWatcher title_watcher(test_guest_web_contents,
                                        expected_title);

    rvh->ExecuteJavascriptAndGetValue(string16(), ASCIIToUTF16("Go(-2);"));

    string16 actual_title = title_watcher.WaitAndGetTitle();
    EXPECT_EQ(expected_title, actual_title);
  }
}

// This tests verifies that reloading the embedder does not crash the browser
// and that the guest is reset.
IN_PROC_BROWSER_TEST_F(BrowserPluginHostTest, ReloadEmbedder) {
  ASSERT_TRUE(test_server()->Start());
  GURL test_url(test_server()->GetURL(
      "files/browser_plugin_embedder.html"));
  NavigateToURL(shell(), test_url);

  WebContentsImpl* embedder_web_contents = static_cast<WebContentsImpl*>(
      shell()->web_contents());
  RenderViewHostImpl* rvh = static_cast<RenderViewHostImpl*>(
      embedder_web_contents->GetRenderViewHost());

  rvh->ExecuteJavascriptAndGetValue(string16(), ASCIIToUTF16(
      StringPrintf("SetSrc('%s');", kHTMLForGuest)));

  // Wait to make sure embedder is created/attached to WebContents.
  TestBrowserPluginHostFactory::GetInstance()->WaitForEmbedderCreation();

  TestBrowserPluginEmbedder* test_embedder =
      static_cast<TestBrowserPluginEmbedder*>(
          embedder_web_contents->GetBrowserPluginEmbedder());
  ASSERT_TRUE(test_embedder);
  test_embedder->WaitForGuestAdded();

  // Verify that we have exactly one guest.
  const BrowserPluginEmbedder::ContainerInstanceMap& instance_map =
      test_embedder->guest_web_contents_for_testing();
  EXPECT_EQ(1u, instance_map.size());

  WebContentsImpl* test_guest_web_contents = static_cast<WebContentsImpl*>(
      instance_map.begin()->second);
  TestBrowserPluginGuest* test_guest = static_cast<TestBrowserPluginGuest*>(
      test_guest_web_contents->GetBrowserPluginGuest());

  // Wait for the guest to send an UpdateRectMsg, meaning it is ready.
  test_guest->WaitForUpdateRectMsg();

  // Change the title of the page to 'modified' so that we know that
  // the page has successfully reloaded when it goes back to 'embedder'
  // in the next step.
  {
    const string16 expected_title = ASCIIToUTF16("modified");
    content::TitleWatcher title_watcher(embedder_web_contents,
                                        expected_title);

    rvh->ExecuteJavascriptAndGetValue(string16(), ASCIIToUTF16(
        StringPrintf("SetTitle('%s');", "modified")));

    string16 actual_title = title_watcher.WaitAndGetTitle();
    EXPECT_EQ(expected_title, actual_title);
  }

  // Reload the embedder page, and verify that the reload was successful.
  // Then navigate the guest to verify that the browser process does not crash.
  {
    const string16 expected_title = ASCIIToUTF16("embedder");
    content::TitleWatcher title_watcher(embedder_web_contents,
                                        expected_title);

    embedder_web_contents->GetController().Reload(false);
    string16 actual_title = title_watcher.WaitAndGetTitle();
    EXPECT_EQ(expected_title, actual_title);

    embedder_web_contents->GetRenderViewHost()->
        ExecuteJavascriptAndGetValue(string16(), ASCIIToUTF16(
            StringPrintf("SetSrc('%s');", kHTMLForGuest)));

    WebContentsImpl* test_guest_web_contents = static_cast<WebContentsImpl*>(
        instance_map.begin()->second);
    TestBrowserPluginGuest* test_guest = static_cast<TestBrowserPluginGuest*>(
        test_guest_web_contents->GetBrowserPluginGuest());

    // Wait for the guest to send an UpdateRectMsg, meaning it is ready.
    test_guest->WaitForUpdateRectMsg();
  }
}


}  // namespace content
