// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/top_controls_manager.h"

#include <algorithm>

#include "base/logging.h"
#include "base/time.h"
#include "cc/keyframed_animation_curve.h"
#include "cc/layer_tree_impl.h"
#include "cc/timing_function.h"
#include "cc/top_controls_manager_client.h"
#include "ui/gfx/transform.h"
#include "ui/gfx/vector2d_f.h"

namespace cc {
namespace {
// These constants were chosen empirically for their visually pleasant behavior.
// Contact tedchoc@chromium.org for questions about changing these values.
const float kShowHideThreshold = 0.75f;
const int64 kShowHideMaxDurationMs = 250;
}

// static
scoped_ptr<TopControlsManager> TopControlsManager::Create(
    TopControlsManagerClient* client, float top_controls_height) {
  return make_scoped_ptr(new TopControlsManager(client, top_controls_height));
}

TopControlsManager::TopControlsManager(TopControlsManagerClient* client,
                                       float top_controls_height)
    : client_(client),
      is_overlay_mode_(false),
      top_controls_height_(top_controls_height),
      controls_top_offset_(0),
      content_top_offset_(top_controls_height),
      previous_root_scroll_offset_(0.f),
      scroll_readjustment_enabled_(false),
      is_showing_animation_(false) {
  CHECK(client_);
}

TopControlsManager::~TopControlsManager() {
}

void TopControlsManager::UpdateDrawPositions() {
  if (!RootScrollLayer())
    return;

  // If the scroll position has changed underneath us (i.e. a javascript
  // scroll), then simulate a scroll that covers the delta.
  float scroll_total_y = RootScrollLayerTotalScrollY();
  if (scroll_readjustment_enabled_
      && scroll_total_y != previous_root_scroll_offset_) {
    ScrollBy(gfx::Vector2dF(0, scroll_total_y - previous_root_scroll_offset_));
    StartAnimationIfNecessary();
    previous_root_scroll_offset_ = RootScrollLayerTotalScrollY();
  }
}

void TopControlsManager::ScrollBegin() {
  ResetAnimations();
  scroll_readjustment_enabled_ = false;
}

gfx::Vector2dF TopControlsManager::ScrollBy(
    const gfx::Vector2dF pending_delta) {
  ResetAnimations();
  return ScrollInternal(pending_delta);
}

gfx::Vector2dF TopControlsManager::ScrollInternal(
    const gfx::Vector2dF pending_delta) {
  float scroll_total_y = RootScrollLayerTotalScrollY();
  float scroll_delta_y = pending_delta.y();

  float previous_controls_offset = controls_top_offset_;
  float previous_content_offset = content_top_offset_;
  bool previous_was_overlay = is_overlay_mode_;

  controls_top_offset_ -= scroll_delta_y;
  controls_top_offset_ = std::min(
      std::max(controls_top_offset_, -top_controls_height_), 0.f);

  if (scroll_total_y > 0 || (scroll_total_y == 0
      && content_top_offset_ < scroll_delta_y)) {
    is_overlay_mode_ = true;
    content_top_offset_ = 0;
  } else if (scroll_total_y <= 0 && (scroll_delta_y < 0
      || (scroll_delta_y > 0 && content_top_offset_ > 0))) {
    is_overlay_mode_ = false;
    content_top_offset_ -= scroll_delta_y;
  }
  content_top_offset_ = std::max(
      std::min(content_top_offset_,
               controls_top_offset_ + top_controls_height_), 0.f);

  gfx::Vector2dF applied_delta;
  if (!previous_was_overlay)
    applied_delta.set_y(previous_content_offset - content_top_offset_);

  if (is_overlay_mode_ != previous_was_overlay
      || previous_controls_offset != controls_top_offset_
      || previous_content_offset != content_top_offset_) {
    client_->setNeedsRedraw();
    client_->setNeedsUpdateDrawProperties();
  }

  return pending_delta - applied_delta;
}

void TopControlsManager::ScrollEnd() {
  StartAnimationIfNecessary();
  previous_root_scroll_offset_ = RootScrollLayerTotalScrollY();
  scroll_readjustment_enabled_ = true;
}

void TopControlsManager::Animate(base::TimeTicks monotonic_time) {
  if (!top_controls_animation_ || !RootScrollLayer())
    return;

  double time = (monotonic_time - base::TimeTicks()).InMillisecondsF();
  float new_offset = top_controls_animation_->getValue(time);
  gfx::Vector2dF scroll_vector(0.f, -(new_offset - controls_top_offset_));
  ScrollInternal(scroll_vector);
  client_->setNeedsRedraw();

  if ((is_showing_animation_ && new_offset >= 0) ||
      (!is_showing_animation_ && new_offset <= -top_controls_height_)) {
    top_controls_animation_.reset();
    StartAnimationIfNecessary();
  }
}

void TopControlsManager::ResetAnimations() {
  if (top_controls_animation_)
    top_controls_animation_.reset();
}

LayerImpl* TopControlsManager::RootScrollLayer() {
  return client_->activeTree()->root_scroll_layer();
}

float TopControlsManager::RootScrollLayerTotalScrollY() {
  LayerImpl* layer = RootScrollLayer();
  if (!layer)
    return 0;
  gfx::Vector2dF scroll_total = layer->scrollOffset() + layer->scrollDelta();
  return scroll_total.y();
}

void TopControlsManager::SetupAnimation(bool show_controls) {
  top_controls_animation_ = KeyframedFloatAnimationCurve::create();
  double start_time =
      (base::TimeTicks::Now() - base::TimeTicks()).InMillisecondsF();
  top_controls_animation_->addKeyframe(
      FloatKeyframe::create(start_time, controls_top_offset_,
                            scoped_ptr<TimingFunction>()));
  float max_ending_offset = (show_controls ? 1 : -1) * top_controls_height_;
  top_controls_animation_->addKeyframe(
      FloatKeyframe::create(start_time + kShowHideMaxDurationMs,
                            controls_top_offset_ + max_ending_offset,
                            EaseTimingFunction::create()));
  is_showing_animation_ = show_controls;
}

void TopControlsManager::StartAnimationIfNecessary() {
  float scroll_total_y = RootScrollLayerTotalScrollY();

  if (controls_top_offset_ != 0
      && controls_top_offset_ != -top_controls_height_) {
    SetupAnimation(
        controls_top_offset_ >= -(top_controls_height_ * kShowHideThreshold));
    client_->setNeedsRedraw();
  }
}

}  // namespace cc
