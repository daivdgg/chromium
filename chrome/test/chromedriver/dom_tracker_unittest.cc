// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/json/json_reader.h"
#include "base/values.h"
#include "chrome/test/chromedriver/dom_tracker.h"
#include "chrome/test/chromedriver/status.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(DomTracker, GetFrameIdForNode) {
  DomTracker tracker;
  std::string frame_id;
  ASSERT_TRUE(tracker.GetFrameIdForNode(101, &frame_id).IsError());
  ASSERT_TRUE(frame_id.empty());

  const char nodes[] =
      "[{\"nodeId\":100,\"children\":"
      "    [{\"nodeId\":101},"
      "     {\"nodeId\":102,\"frameId\":\"f\"}]"
      "}]";
  base::DictionaryValue params;
  params.Set("nodes", base::JSONReader::Read(nodes));
  tracker.OnEvent("DOM.setChildNodes", params);
  ASSERT_TRUE(tracker.GetFrameIdForNode(101, &frame_id).IsError());
  ASSERT_TRUE(frame_id.empty());
  ASSERT_TRUE(tracker.GetFrameIdForNode(102, &frame_id).IsOk());
  ASSERT_STREQ("f", frame_id.c_str());

  tracker.OnEvent("DOM.documentUpdated", params);
  ASSERT_TRUE(tracker.GetFrameIdForNode(102, &frame_id).IsError());
}
