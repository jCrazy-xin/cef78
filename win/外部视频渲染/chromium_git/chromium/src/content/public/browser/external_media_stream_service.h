// Copyright (c) 2017 The TAL Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_EXTERNAL_MEDIA_STREAM_SERVICE_H_
#define CONTENT_PUBLIC_BROWSER_EXTERNAL_MEDIA_STREAM_SERVICE_H_

#include <stddef.h>
#include <stdint.h>

#include "content/common/content_export.h"

namespace content {

class ExternalMediaStreamObserver;

// This class servce for external devices
class CONTENT_EXPORT ExternalMediaStreamService {
 public:
  // Runs on Device Task IOThread
  virtual bool AddObserver(
      const std::string& provider,
      const std::string& path,
      ExternalMediaStreamObserver* observer) = 0;

  // Runs on Device Task IOThread
  virtual bool RemoveObserver(
      ExternalMediaStreamObserver* observer) = 0;

  virtual bool isStreamValid(
      const std::string& provider,
      const std::string& path) = 0;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_EXTERNAL_MEDIA_STREAM_SERVICE_H_
