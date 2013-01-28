// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CAPTURER_MAC_DESKTOP_CONFIGURATION_H_
#define REMOTING_CAPTURER_MAC_DESKTOP_CONFIGURATION_H_

#include <ApplicationServices/ApplicationServices.h>
#include <Carbon/Carbon.h>
#include <vector>

#include "base/basictypes.h"
#include "third_party/skia/include/core/SkPoint.h"
#include "third_party/skia/include/core/SkRect.h"

namespace remoting {

// Describes the configuration of a specific display.
struct MacDisplayConfiguration {
  MacDisplayConfiguration();

  // Returns the current configuration of the specified display.
  static MacDisplayConfiguration ForDisplay(CGDirectDisplayID display_id);

  // Cocoa identifier for this display.
  CGDirectDisplayID id;

  // Bounds of this display in Density-Independent Pixels (DIPs).
  SkIRect bounds;

  // Bounds of this display in physical pixels.
  SkIRect pixel_bounds;

  // Scale factor from DIPs to physical pixels.
  float dip_to_pixel_scale;
};

typedef std::vector<MacDisplayConfiguration> MacDisplayConfigurations;

// Describes the configuration of the whole desktop.
struct MacDesktopConfiguration {
  MacDesktopConfiguration();
  ~MacDesktopConfiguration();

  // Returns the current configuration of the desktop.
  static MacDesktopConfiguration GetCurrent();

  // Bounds of the desktop in Density-Independent Pixels (DIPs).
  SkIRect bounds;

  // Bounds of the desktop in physical pixels.
  SkIRect pixel_bounds;

  // Scale factor from DIPs to physical pixels.
  float dip_to_pixel_scale;

  // Configurations of the displays making up the desktop area.
  MacDisplayConfigurations displays;
};

}  // namespace remoting

#endif  // REMOTING_CAPTURER_MAC_DESKTOP_CONFIGURATION_H_
