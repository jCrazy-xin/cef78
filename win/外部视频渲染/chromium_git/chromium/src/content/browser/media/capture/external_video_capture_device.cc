// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/capture/external_video_capture_device.h"

#include <stdint.h>

#include <algorithm>
#include <memory>
#include <string>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/common/content_client.h"
#include "content/public/browser/external_media_stream_observer.h"
#include "content/public/browser/external_media_stream_service.h"
#include "media/base/video_frame_metadata.h"
//wanghongliang 201808181543 start
#include "media/capture/video_capture_types.h"
//wanghongliang 201808181543 end
//#include "media/capture/content/thread_safe_capture_oracle.h"
#include "media/capture/content/video_capture_oracle.h"
#include "ui/base/layout.h"
#include "ui/gfx/geometry/dip_util.h"
#include "ui/gfx/geometry/size_conversions.h"


namespace content {
constexpr char kExternalCaptureScheme[] = "ems://";

class ExternalMediaStreamObserverImpl
    : public ExternalMediaStreamObserver {
public:
  static ExternalMediaStreamObserver* Start(
    const std::string& provider,
    const std::string& path,
    std::unique_ptr<media::VideoCaptureDevice::Client> client);

  virtual void GetObserverParams(std::string& provider,
                                 std::string& path);

  virtual void OnIncomingCapturedData(const uint8_t* data,
                                    int length,
                                    const media::VideoCaptureFormat& frame_format,
                                    int clockwise_rotation,
                                    base::TimeTicks reference_time,
                                    base::TimeDelta timestamp,
                                    int frame_feedback_id = 0);
  virtual void OnIncomingCapturedData(
    const uint8_t* y_data,
    const uint8_t* u_data,
    const uint8_t* v_data,
    unsigned int y_stride,
    unsigned int u_stride,
    unsigned int v_stride,
    const media::VideoCaptureFormat& frame_format,
    int clockwise_rotation,
    base::TimeTicks reference_time,
    base::TimeDelta timestamp,
    int frame_feedback_id = 0);

  virtual void OnError(const base::Location& from_here,
                    const std::string& reason);

  virtual void OnLog(const std::string& message);

  virtual void OnStarted();

private:
  ExternalMediaStreamObserverImpl(
    const std::string& provicer,
    const std::string& path,
    std::unique_ptr<media::VideoCaptureDevice::Client> client);
  virtual ~ExternalMediaStreamObserverImpl();

  std::string provider_;
  std::string path_;
  std::unique_ptr<media::VideoCaptureDevice::Client> client_;
};

ExternalMediaStreamObserver*
  ExternalMediaStreamObserverImpl::Start(
    const std::string& provider,
    const std::string& path,
    std::unique_ptr<media::VideoCaptureDevice::Client> client) {
  ExternalMediaStreamService* service =
   GetContentClient()->browser()->GetExternalMediaStreamService();
  if (!service) {
    client.release();
    return nullptr;
  }
  ExternalMediaStreamObserver* observer =
    new ExternalMediaStreamObserverImpl(provider, path, std::move(client));
  service->AddObserver(provider, path, observer);
  return observer;
}

ExternalMediaStreamObserverImpl::ExternalMediaStreamObserverImpl(
  const std::string& provider,
  const std::string& path,
  std::unique_ptr<media::VideoCaptureDevice::Client> client)
  : provider_(provider)
  , path_(path)
  , client_(std::move(client)) {
}

ExternalMediaStreamObserverImpl::~ExternalMediaStreamObserverImpl() {
  ExternalMediaStreamService* service =
    GetContentClient()->browser()->GetExternalMediaStreamService();
  if (!service)
    return;
  service->RemoveObserver(this);
}

void ExternalMediaStreamObserverImpl::GetObserverParams(
  std::string& provider,
  std::string& path) {
  provider = provider_;
  path = path_;
}

void ExternalMediaStreamObserverImpl::OnIncomingCapturedData(
  const uint8_t* data,
  int length,
  const media::VideoCaptureFormat& frame_format,
  int clockwise_rotation,
  base::TimeTicks reference_time,
  base::TimeDelta timestamp,
  int frame_feedback_id) {
  if (client_) {
    client_->OnIncomingCapturedData(data,
                length,
                frame_format,
                gfx::ColorSpace(),
                clockwise_rotation,
                true,
                reference_time,
                timestamp,
                frame_feedback_id);
  }
}

void ExternalMediaStreamObserverImpl::OnIncomingCapturedData(
  const uint8_t* y_data,
  const uint8_t* u_data,
  const uint8_t* v_data,
  unsigned int y_stride,
  unsigned int u_stride,
  unsigned int v_stride,
  const media::VideoCaptureFormat& frame_format,
  int clockwise_rotation,
  base::TimeTicks reference_time,
  base::TimeDelta timestamp,
  int frame_feedback_id) {
  if (client_) {
   client_->OnIncomingCapturedYuvData(y_data,
     u_data,
     v_data,
     y_stride,
     u_stride,
     v_stride,
     frame_format,
     clockwise_rotation,
     reference_time,
     timestamp,
     frame_feedback_id);
  }
}

void ExternalMediaStreamObserverImpl::OnError(
  const base::Location& from_here,
  const std::string& reason) {
  if (client_) {
    client_->OnError(media::VideoCaptureError::kNone, from_here, reason);
  }
}

void ExternalMediaStreamObserverImpl::OnLog(
  const std::string& message) {
  if (client_) {
    client_->OnLog(message);
  }
}

void ExternalMediaStreamObserverImpl::OnStarted() {
  if (client_) {
    client_->OnStarted();
  }
}

ExternalVideoCaptureDevice::ExternalVideoCaptureDevice(
    const std::string& provider,
    const std::string& stream_id)
    : provider_(provider)
    , stream_id_(stream_id) {}

ExternalVideoCaptureDevice::~ExternalVideoCaptureDevice() {
  DVLOG(2) << "ExternalVideoCaptureDevice@" << this << " destroying.";
}

// static
std::unique_ptr<media::VideoCaptureDevice>
ExternalVideoCaptureDevice::Create(const std::string& device_id) {
  std::string provider;
  std::string stream_id;
  if (!ParseExternalInfoFromDeviceId(device_id, provider, stream_id))
    return nullptr;
  //wanghongliang 201808181537 START
  std::unique_ptr<media::VideoCaptureDevice> video_capture_device;
  video_capture_device.reset(new ExternalVideoCaptureDevice(provider,
    stream_id));
  return video_capture_device;
  //wanghongliang 201808181537 END
  //return new ExternalVideoCaptureDevice(provider, stream_id);
}

// static
// device id rule:
// ems://provider/stream_id
bool ExternalVideoCaptureDevice::ParseExternalInfoFromDeviceId(
    const std::string& device_id_param,
    std::string& provider,
    std::string& stream_id) {
  const std::string device_scheme = kExternalCaptureScheme;
  if (!base::StartsWith(device_id_param, device_scheme,
                        base::CompareCase::SENSITIVE))
    return false;

  const std::string device_id = device_id_param.substr(device_scheme.size());

  const size_t sep_pos = device_id.find('/');
  if (sep_pos == std::string::npos)
    return false;

  const std::string provider_param = device_id.substr(0, sep_pos);
  if (!provider_param.size())
    return false;

  const std::string stream_id_param = device_id.substr(sep_pos + 1);
  if (!stream_id_param.size())
    return false;

  provider = provider_param;
  stream_id = stream_id_param;
  return true;
}

void ExternalVideoCaptureDevice::AllocateAndStart(
    const media::VideoCaptureParams& params,
    std::unique_ptr<Client> client) {
  DVLOG(1) << "Allocating " << params.requested_format.frame_size.ToString();
  observer_.reset(ExternalMediaStreamObserverImpl::Start(provider_,
    stream_id_,
    std::move(client)));
  if (!observer_) {
    client->OnError(
        media::VideoCaptureError::kNone,
        FROM_HERE,
        base::StringPrintf(
            "device error ems://%s/%s",
            provider_.c_str(), stream_id_.c_str()));
    return;
  }
  observer_->OnStarted();
}

void ExternalVideoCaptureDevice::StopAndDeAllocate() {
  if (observer_) {
    ExternalMediaStreamService* service =
      GetContentClient()->browser()->GetExternalMediaStreamService();
    if (service)
      service->RemoveObserver(observer_.get());
  }
  observer_.reset();
}

}  // namespace content
