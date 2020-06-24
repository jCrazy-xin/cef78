// Copyright (c) 2013 GitHub, Inc.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

#ifndef ATOM_EXTERNAL_MEDIA_STREAM_MANAGER_H_
#define ATOM_EXTERNAL_MEDIA_STREAM_MANAGER_H_

#include <map>
#include <string>
#include <memory>
#include <list>

#include "base/macros.h"
#include "base/memory/singleton.h"
#include "base/synchronization/lock.h"
#include "build/build_config.h"

#include "content/public/browser/external_media_stream_service.h"

namespace atom {

class AtomMediaStreamEngine;

using ProviderObservers = std::list<content::ExternalMediaStreamObserver*>;

// key = provider_name + '#' + path
using ProviderMap = std::map<std::string, ProviderObservers>;

class AtomExternalMediaStreamManager :
              public content::ExternalMediaStreamService {
public:
  static AtomExternalMediaStreamManager* GetInstance();

  void Init();
  void UnInit();

  AtomExternalMediaStreamManager();
  virtual ~AtomExternalMediaStreamManager();

  virtual bool AddObserver(
      const std::string& provider,
      const std::string& path,
      content::ExternalMediaStreamObserver* observer);

  virtual bool RemoveObserver(
      content::ExternalMediaStreamObserver* observer);

  virtual bool isStreamValid(
      const std::string& provider,
      const std::string& path);

  // CefEmsProviderManager
  int NotifyIncomingCapturedI420Data(
    const char* provider_name,
    int provider_name_length,
    const char* path,
    int path_length,
    const unsigned char* y_data,
    const unsigned char* u_data,
    const unsigned char* v_data,
    unsigned int y_stride,
    unsigned int u_stride,
    unsigned int v_stride,
    int frame_width,
    int frame_height,
    float frame_rate,
    int clockwise_rotation,
    long long timestamp,
    int frame_feedback_id = 0);

  int NotifyIncomingCapturedI420Data(
    const char* provider_name,
    int provider_name_length,
    const char* path,
    int path_length,
    const unsigned char* data,
    int data_length,
    int frame_width,
    int frame_height,
    float frame_rate,
    int clockwise_rotation,
    long long timestamp,
    int pixel_format,
    int frame_feedback_id = 0);

  void NotifyError(
    const char* provider_name,
    const char* provider_name_length,
    const char* reason,
    int reason_length);

private:
  friend struct base::DefaultSingletonTraits<AtomExternalMediaStreamManager>;

  ProviderMap engine_map_;
  base::Lock engine_map_lock_;
  base::TimeTicks first_frame_time_;

  DISALLOW_COPY_AND_ASSIGN(AtomExternalMediaStreamManager);
};

}  // namespace atom

#endif  // ATOM_EXTERNAL_MEDIA_STREAM_MANAGER_H_
