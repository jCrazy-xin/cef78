// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEDIA_CAPTURE_EXTERNAL_VIDEO_CAPTURE_DEVICE_H_
#define CONTENT_BROWSER_MEDIA_CAPTURE_EXTERNAL_VIDEO_CAPTURE_DEVICE_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "content/common/content_export.h"
#include "media/capture/video/video_capture_device.h"

namespace content {

class ExternalMediaStreamObserver;

// A virtualized VideoCaptureDevice that captures the displayed contents of a
// external media engine (i.e., agora, zego, rtc ...)
// An instance is created by providing a device_id.
// -- added by xiongzefa@100tal.com 20171031
class CONTENT_EXPORT ExternalVideoCaptureDevice
    : public media::VideoCaptureDevice {
 public:
  // Create a ExternalVideoCaptureDevice instance from the given
  // |device_id|.  Returns NULL if |device_id| is invalid.
  static std::unique_ptr<media::VideoCaptureDevice> Create(
      const std::string& device_id);

  ~ExternalVideoCaptureDevice() override;

  // VideoCaptureDevice implementation.
  void AllocateAndStart(const media::VideoCaptureParams& params,
                        std::unique_ptr<Client> client) override;
  void StopAndDeAllocate() override;

 private:
  ExternalVideoCaptureDevice(
      const std::string& provider,
      const std::string& stream_id);

  static bool ParseExternalInfoFromDeviceId(
      const std::string& device_id,
      std::string& provider,
      std::string& stream_id);

  std::string provider_;
  std::string stream_id_;

  std::unique_ptr<ExternalMediaStreamObserver> observer_;

  DISALLOW_COPY_AND_ASSIGN(ExternalVideoCaptureDevice);
};


}  // namespace content

#endif  // CONTENT_BROWSER_MEDIA_CAPTURE_EXTERNAL_VIDEO_CAPTURE_DEVICE_H_
