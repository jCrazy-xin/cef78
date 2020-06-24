// Copyright (c) 2017 The TAL Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_EXTERNAL_MEDIA_STREAM_OBSERVER_H_
#define CONTENT_PUBLIC_BROWSER_EXTERNAL_MEDIA_STREAM_OBSERVER_H_

#include <stddef.h>
#include <stdint.h>

#include "base/location.h"
#include "base/time/time.h"

#include "content/common/content_export.h"
#include "media/capture/video_capture_types.h"

namespace content {

// This class servce for external devices
class CONTENT_EXPORT ExternalMediaStreamObserver {
 public:

    virtual void GetObserverParams(std::string& provider,
                                   std::string& path) = 0;

    // Captured a new video frame, data for which is pointed to by |data|.
    //
    // The format of the frame is described by |frame_format|, and is assumed to
    // be tightly packed. This method will try to reserve an output buffer and
    // copy from |data| into the output buffer. If no output buffer is
    // available, the frame will be silently dropped. |reference_time| is
    // system clock time when we detect the capture happens, it is used for
    // Audio/Video sync, not an exact presentation time for playout, because it
    // could contain noise. |timestamp| measures the ideal time span between the
    // first frame in the stream and the current frame; however, the time source
    // is determined by the platform's device driver and is often not the system
    // clock, or even has a drift with respect to system clock.
    // |frame_feedback_id| is an identifier that allows clients to refer back to
    // this particular frame when reporting consumer feedback via
    // OnConsumerReportingUtilization(). This identifier is needed because
    // frames are consumed asynchronously and multiple frames can be "in flight"
    // at the same time.
    virtual void OnIncomingCapturedData(const uint8_t* data,
                                        int length,
                                        const media::VideoCaptureFormat& frame_format,
                                        int clockwise_rotation,
                                        base::TimeTicks reference_time,
                                        base::TimeDelta timestamp,
                                        int frame_feedback_id = 0) = 0;

    virtual void OnIncomingCapturedData(const uint8_t* y_data,
                                        const uint8_t* u_data,
                                        const uint8_t* v_data,
                                        unsigned int y_stride,
                                        unsigned int u_stride,
                                        unsigned int v_stride,
                                        const media::VideoCaptureFormat& frame_format,
                                        int clockwise_rotation,
                                        base::TimeTicks reference_time,
                                        base::TimeDelta timestamp,
                                        int frame_feedback_id = 0) = 0;

    // An error has occurred that cannot be handled and VideoCaptureDevice must
    // be StopAndDeAllocate()-ed. |reason| is a text description of the error.
    virtual void OnError(const base::Location& from_here,
                         const std::string& reason) = 0;

    // VideoCaptureDevice requests the |message| to be logged.
    virtual void OnLog(const std::string& message) {}

    // VideoCaptureDevice reports it's successfully started.
    virtual void OnStarted() = 0;
    virtual ~ExternalMediaStreamObserver() {}
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_EXTERNAL_MEDIA_STREAM_OBSERVER_H_
