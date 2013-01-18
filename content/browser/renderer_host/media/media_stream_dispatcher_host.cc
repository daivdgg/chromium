// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/media/media_stream_dispatcher_host.h"

#include "content/browser/browser_main_loop.h"
#include "content/browser/renderer_host/media/web_contents_capture_util.h"
#include "content/common/media/media_stream_messages.h"
#include "content/common/media/media_stream_options.h"
#include "googleurl/src/gurl.h"

namespace content {

struct MediaStreamDispatcherHost::StreamRequest {
  StreamRequest() : render_view_id(0), page_request_id(0) {}
  StreamRequest(int render_view_id, int page_request_id)
      : render_view_id(render_view_id),
        page_request_id(page_request_id ) {
  }
  int render_view_id;
  // Id of the request generated by MediaStreamDispatcher.
  int page_request_id;
};

MediaStreamDispatcherHost::MediaStreamDispatcherHost(int render_process_id)
    : render_process_id_(render_process_id) {
}

void MediaStreamDispatcherHost::StreamGenerated(
    const std::string& label,
    const StreamDeviceInfoArray& audio_devices,
    const StreamDeviceInfoArray& video_devices) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DVLOG(1) << "MediaStreamDispatcherHost::StreamGenerated("
           << ", {label = " << label <<  "})";

  StreamMap::iterator it = streams_.find(label);
  DCHECK(it != streams_.end());
  StreamRequest request = it->second;

  Send(new MediaStreamMsg_StreamGenerated(
      request.render_view_id, request.page_request_id, label, audio_devices,
      video_devices));
}

void MediaStreamDispatcherHost::StreamGenerationFailed(
    const std::string& label) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DVLOG(1) << "MediaStreamDispatcherHost::StreamGenerationFailed("
           << ", {label = " << label <<  "})";

  StreamMap::iterator it = streams_.find(label);
  DCHECK(it != streams_.end());
  StreamRequest request = it->second;
  streams_.erase(it);

  Send(new MediaStreamMsg_StreamGenerationFailed(request.render_view_id,
                                                 request.page_request_id));
}

void MediaStreamDispatcherHost::DevicesEnumerated(
    const std::string& label,
    const StreamDeviceInfoArray& devices) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DVLOG(1) << "MediaStreamDispatcherHost::DevicesEnumerated("
           << ", {label = " << label <<  "})";

  StreamMap::iterator it = streams_.find(label);
  DCHECK(it != streams_.end());
  StreamRequest request = it->second;

  Send(new MediaStreamMsg_DevicesEnumerated(
      request.render_view_id, request.page_request_id, label, devices));
}

void MediaStreamDispatcherHost::DeviceOpened(
    const std::string& label,
    const StreamDeviceInfo& video_device) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DVLOG(1) << "MediaStreamDispatcherHost::DeviceOpened("
           << ", {label = " << label <<  "})";

  StreamMap::iterator it = streams_.find(label);
  DCHECK(it != streams_.end());
  StreamRequest request = it->second;

  Send(new MediaStreamMsg_DeviceOpened(
      request.render_view_id, request.page_request_id, label, video_device));
}

bool MediaStreamDispatcherHost::OnMessageReceived(
    const IPC::Message& message, bool* message_was_ok) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP_EX(MediaStreamDispatcherHost, message, *message_was_ok)
    IPC_MESSAGE_HANDLER(MediaStreamHostMsg_GenerateStream, OnGenerateStream)
    IPC_MESSAGE_HANDLER(MediaStreamHostMsg_CancelGenerateStream,
                        OnCancelGenerateStream)
    IPC_MESSAGE_HANDLER(MediaStreamHostMsg_StopGeneratedStream,
                        OnStopGeneratedStream)
    IPC_MESSAGE_HANDLER(MediaStreamHostMsg_EnumerateDevices,
                        OnEnumerateDevices)
    IPC_MESSAGE_HANDLER(MediaStreamHostMsg_OpenDevice,
                        OnOpenDevice)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP_EX()
  return handled;
}

void MediaStreamDispatcherHost::OnChannelClosing() {
  BrowserMessageFilter::OnChannelClosing();
  DVLOG(1) << "MediaStreamDispatcherHost::OnChannelClosing";

  // Since the IPC channel is gone, close all requesting/requested streams.
  for (StreamMap::iterator it = streams_.begin();
       it != streams_.end();
       ++it) {
    std::string label = it->first;
    GetManager()->StopGeneratedStream(label);
  }
  // Clear the map after we have stopped all the streams.
  streams_.clear();
}

MediaStreamDispatcherHost::~MediaStreamDispatcherHost() {
  DCHECK(streams_.empty());
}

void MediaStreamDispatcherHost::OnGenerateStream(
    int render_view_id,
    int page_request_id,
    const StreamOptions& components,
    const GURL& security_origin) {
  DVLOG(1) << "MediaStreamDispatcherHost::OnGenerateStream("
           << render_view_id << ", "
           << page_request_id << ", ["
           << " audio:" << components.audio_type
           << " video:" << components.video_type
           << " ], "
           << security_origin.spec() << ")";

  std::string label;
  if (components.audio_type == MEDIA_TAB_AUDIO_CAPTURE ||
      components.video_type == MEDIA_TAB_VIDEO_CAPTURE) {
    // Append our tab capture device id scheme. It's OK if both device_id's
    // are empty since we check their validity in GenerateStreamForDevice.
    // TODO(justinlin): This is kind of a hack, but the plumbing for audio
    // streams is too complicated to plumb in by type. Will revisit once it's
    // refactored. http://crbug.com/163100
    const std::string& device_id =
        WebContentsCaptureUtil::AppendWebContentsDeviceScheme(
            !components.video_device_id.empty() ?
            components.video_device_id : components.audio_device_id);

    // TODO(justinlin): Cleanup/get rid of GenerateStreamForDevice and merge
    // with the regular GenerateStream.
    label = GetManager()->GenerateStreamForDevice(
        this, render_process_id_, render_view_id,
        components, device_id, security_origin);
  } else {
    label = GetManager()->GenerateStream(this, render_process_id_,
                                         render_view_id,
                                         components, security_origin);
    DCHECK(!label.empty());
  }

  if (label.empty()) {
    Send(new MediaStreamMsg_StreamGenerationFailed(render_view_id,
                                                   page_request_id));
  } else {
    streams_[label] = StreamRequest(render_view_id, page_request_id);
  }
}

void MediaStreamDispatcherHost::OnCancelGenerateStream(int render_view_id,
                                                       int page_request_id) {
  DVLOG(1) << "MediaStreamDispatcherHost::OnCancelGenerateStream("
           << render_view_id << ", "
           << page_request_id << ")";

  for (StreamMap::iterator it = streams_.begin(); it != streams_.end(); ++it) {
    if (it->second.render_view_id == render_view_id &&
        it->second.page_request_id == page_request_id) {
      GetManager()->CancelRequest(it->first);
    }
  }
}

void MediaStreamDispatcherHost::OnStopGeneratedStream(
    int render_view_id, const std::string& label) {
  DVLOG(1) << "MediaStreamDispatcherHost::OnStopGeneratedStream("
           << ", {label = " << label <<  "})";

  StreamMap::iterator it = streams_.find(label);
  if (it == streams_.end())
    return;

  GetManager()->StopGeneratedStream(label);
  streams_.erase(it);
}

void MediaStreamDispatcherHost::OnEnumerateDevices(
    int render_view_id,
    int page_request_id,
    MediaStreamType type,
    const GURL& security_origin) {
  DVLOG(1) << "MediaStreamDispatcherHost::OnEnumerateDevices("
           << render_view_id << ", "
           << page_request_id << ", "
           << type << ", "
           << security_origin.spec() << ")";

  const std::string& label = GetManager()->EnumerateDevices(
      this, render_process_id_, render_view_id, type, security_origin);
  DCHECK(!label.empty());
  streams_[label] = StreamRequest(render_view_id, page_request_id);
}

void MediaStreamDispatcherHost::OnOpenDevice(
    int render_view_id,
    int page_request_id,
    const std::string& device_id,
    MediaStreamType type,
    const GURL& security_origin) {
  DVLOG(1) << "MediaStreamDispatcherHost::OnOpenDevice("
           << render_view_id << ", "
           << page_request_id << ", device_id: "
           << device_id.c_str() << ", type: "
           << type << ", "
           << security_origin.spec() << ")";

  const std::string& label = GetManager()->OpenDevice(
      this, render_process_id_, render_view_id,
      device_id, type, security_origin);
  DCHECK(!label.empty());
  streams_[label] = StreamRequest(render_view_id, page_request_id);
}

MediaStreamManager* MediaStreamDispatcherHost::GetManager() {
  return BrowserMainLoop::GetMediaStreamManager();
}

}  // namespace content
