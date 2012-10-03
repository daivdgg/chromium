// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/browser_plugin/browser_plugin_embedder_helper.h"

#include "content/browser/browser_plugin/browser_plugin_embedder.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/common/browser_plugin_messages.h"
#include "content/common/view_messages.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "ui/gfx/size.h"

namespace content {

BrowserPluginEmbedderHelper::BrowserPluginEmbedderHelper(
    BrowserPluginEmbedder* embedder,
    RenderViewHost* render_view_host)
    : RenderViewHostObserver(render_view_host),
      embedder_(embedder) {
}

BrowserPluginEmbedderHelper::~BrowserPluginEmbedderHelper() {
}

bool BrowserPluginEmbedderHelper::Send(IPC::Message* message) {
  return RenderViewHostObserver::Send(message);
}

bool BrowserPluginEmbedderHelper::OnMessageReceived(
    const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(BrowserPluginEmbedderHelper, message)
    IPC_MESSAGE_HANDLER(BrowserPluginHostMsg_CreateGuest,
                        OnCreateGuest);
    IPC_MESSAGE_HANDLER(BrowserPluginHostMsg_NavigateGuest,
                        OnNavigateGuest);
    IPC_MESSAGE_HANDLER(BrowserPluginHostMsg_ResizeGuest, OnResizeGuest)
    IPC_MESSAGE_HANDLER(BrowserPluginHostMsg_UpdateRect_ACK, OnUpdateRectACK);
    IPC_MESSAGE_HANDLER(BrowserPluginHostMsg_SetFocus, OnSetFocus);
    IPC_MESSAGE_HANDLER_GENERIC(BrowserPluginHostMsg_HandleInputEvent,
        OnHandleInputEvent(*static_cast<const IPC::SyncMessage*>(&message),
                           &handled))
    IPC_MESSAGE_HANDLER(BrowserPluginHostMsg_PluginDestroyed,
                        OnPluginDestroyed);
    IPC_MESSAGE_HANDLER(BrowserPluginHostMsg_Go, OnGo)
    IPC_MESSAGE_HANDLER(BrowserPluginHostMsg_Stop, OnStop)
    IPC_MESSAGE_HANDLER(BrowserPluginHostMsg_Reload, OnReload)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void BrowserPluginEmbedderHelper::OnResizeGuest(
    int instance_id,
    const BrowserPluginHostMsg_ResizeGuest_Params& params) {
  embedder_->ResizeGuest(render_view_host(), instance_id, params);
}

void BrowserPluginEmbedderHelper::OnHandleInputEvent(
    const IPC::SyncMessage& message,
    bool* handled) {
  *handled = true;
  PickleIterator iter(message);

  // TODO(fsamuel): This appears to be a monotonically increasing value.
  int instance_id = -1;
  const char* guest_rect_data = NULL;
  int guest_rect_data_length = -1;
  const char* input_event_data = NULL;
  int input_event_data_length = -1;
  if (!iter.SkipBytes(4) ||
      !message.ReadInt(&iter, &instance_id) ||
      !message.ReadData(&iter, &guest_rect_data, &guest_rect_data_length) ||
      !message.ReadData(&iter, &input_event_data, &input_event_data_length)) {
    *handled = false;
    return;
  }
  const gfx::Rect* guest_rect =
      reinterpret_cast<const gfx::Rect*>(guest_rect_data);
  const WebKit::WebInputEvent* input_event =
      reinterpret_cast<const WebKit::WebInputEvent*>(input_event_data);
  RenderViewHostImpl* rvh = static_cast<RenderViewHostImpl*>(
      render_view_host());

  // Convert the window coordinates into screen coordinates.
  gfx::Rect guest_screen_rect(*guest_rect);
  if (rvh->GetView())
    guest_screen_rect.Offset(rvh->GetView()->GetViewBounds().origin());

  IPC::Message* reply_message =
      IPC::SyncMessage::GenerateReply(&message);
  embedder_->HandleInputEvent(instance_id,
                              rvh,
                              guest_screen_rect,
                              *input_event,
                              reply_message);
}

void BrowserPluginEmbedderHelper::OnCreateGuest(
    int instance_id,
    const std::string& storage_partition_id,
    bool persist_storage) {
  // The first BrowserPluginHostMsg_CreateGuest message is handled in
  // WebContentsImpl. All subsequent BrowserPluginHostMsg_CreateGuest
  // messages are handled here.
  embedder_->CreateGuest(render_view_host(),
                         instance_id,
                         storage_partition_id,
                         persist_storage);
}

void BrowserPluginEmbedderHelper::OnNavigateGuest(
    int instance_id,
    const std::string& src,
    const BrowserPluginHostMsg_ResizeGuest_Params& resize_params) {
  embedder_->NavigateGuest(render_view_host(),
                           instance_id,
                           src,
                           resize_params);
}

void BrowserPluginEmbedderHelper::OnUpdateRectACK(int instance_id,
                                                  int message_id,
                                                  const gfx::Size& size) {
  embedder_->UpdateRectACK(instance_id, message_id, size);
}

void BrowserPluginEmbedderHelper::OnSetFocus(int instance_id, bool focused) {
  embedder_->SetFocus(instance_id, focused);
}

void BrowserPluginEmbedderHelper::OnPluginDestroyed(int instance_id) {
  embedder_->PluginDestroyed(instance_id);
}

void BrowserPluginEmbedderHelper::OnGo(int instance_id, int relative_index) {
  embedder_->Go(instance_id, relative_index);
}

void BrowserPluginEmbedderHelper::OnStop(int instance_id) {
  embedder_->Stop(instance_id);
}

void BrowserPluginEmbedderHelper::OnReload(int instance_id) {
  embedder_->Reload(instance_id);
}

}  // namespace content
