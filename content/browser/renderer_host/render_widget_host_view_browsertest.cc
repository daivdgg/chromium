// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "content/port/browser/render_widget_host_view_port.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_paths.h"
#include "content/shell/shell.h"
#include "content/test/content_browser_test.h"
#include "content/test/content_browser_test_utils.h"
#include "net/base/net_util.h"
#include "skia/ext/platform_canvas.h"
#include "ui/compositor/compositor_setup.h"
#if defined(OS_MACOSX)
#include "ui/surface/io_surface_support_mac.h"
#endif

namespace content {

class RenderWidgetHostViewBrowserTest : public ContentBrowserTest {
 public:
  RenderWidgetHostViewBrowserTest() : finish_called_(false) {}

  virtual void SetUpInProcessBrowserTestFixture() {
    ASSERT_TRUE(PathService::Get(DIR_TEST_DATA, &test_dir_));
  }

  virtual void SetUpCommandLine(CommandLine* command_line) {
    ui::DisableTestCompositor();
  }

  void FinishCopyFromBackingStore(bool expected_result, bool result) {
    ASSERT_EQ(expected_result, result);
    finish_called_ = true;
  }

 protected:
  FilePath test_dir_;
  bool finish_called_;
};

#if defined(OS_MACOSX)
// Tests that the callback passed to CopyFromBackingStore is always called, even
// when the RenderWidgetHost is deleting in the middle of an async copy.
IN_PROC_BROWSER_TEST_F(RenderWidgetHostViewBrowserTest,
                       MacAsyncCopyFromBackingStoreCallbackTest) {
  if (!IOSurfaceSupport::Initialize())
    return;

  NavigateToURL(shell(), net::FilePathToFileURL(
      test_dir_.AppendASCII("rwhv_compositing_static.html")));

  RenderViewHost* const rwh =
      shell()->web_contents()->GetRenderViewHost();
  RenderWidgetHostViewPort* rwhvp =
      static_cast<RenderWidgetHostViewPort*>(rwh->GetView());

  // Wait until an IoSurface is created by repeatedly resizing the window.
  // TODO(justinlin): Find a better way to force an IoSurface when possible.
  gfx::Size size(400, 300);
  int increment = 0;
  while (!rwhvp->HasAcceleratedSurface(gfx::Size())) {
    base::RunLoop run_loop;
    SetWindowBounds(shell()->window(), gfx::Rect(size.width() + increment,
                                                 size.height()));
    // Wait for any ViewHostMsg_CompositorSurfaceBuffersSwapped message to post.
    run_loop.RunUntilIdle();
    increment++;
    ASSERT_LT(increment, 50);
  }

  skia::PlatformBitmap bitmap;
  rwh->CopyFromBackingStore(
      gfx::Rect(),
      size,
      base::Bind(&RenderWidgetHostViewBrowserTest::FinishCopyFromBackingStore,
                 base::Unretained(this), false),
      &bitmap);

  // Delete the surface before the callback is run. This is synchronous until
  // we get to the copy_timer_, so we will always end up in the destructor
  // before the timer fires.
  rwhvp->AcceleratedSurfaceRelease();
  base::RunLoop run_loop;
  run_loop.RunUntilIdle();

  ASSERT_TRUE(finish_called_);
}
#endif

}  // namespace content
