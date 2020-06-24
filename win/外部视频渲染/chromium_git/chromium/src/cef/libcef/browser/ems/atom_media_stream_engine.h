// Copyright (c) 2013 GitHub, Inc.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

#ifndef ATOM_BROWSER_EMS_ATOM_EXTERNAL_MEDIA_STREAM_ENGINE_H_
#define ATOM_BROWSER_EMS_ATOM_EXTERNAL_MEDIA_STREAM_ENGINE_H_

#include <string>
#include <vector>
#include <map>

#include "base/macros.h"
#include "build/build_config.h"

namespace content {
  class ExternalMediaStreamObserver;
}

namespace atom {

using EMSObservers = std::vector<content::ExternalMediaStreamObserver*>;
using EMSObserverMap = std::map<std::string, EMSObservers>;

class AtomMediaStreamEngine {
public:
  AtomMediaStreamEngine() {};
  virtual ~AtomMediaStreamEngine() {};

  virtual std::string Provider() = 0;

  virtual void Initialize() = 0;
  virtual void UnInitialize() = 0;

  virtual void StartPlay(const std::string& stream_id) = 0;
  virtual void StopPlay(const std::string& stream_id) = 0;

  virtual bool AddObserver( 
      const std::string& path,
      content::ExternalMediaStreamObserver* observer);

  virtual bool RemoveObserver(
      content::ExternalMediaStreamObserver* observer);

  virtual bool IsStreamValid(
      const std::string& path) = 0;

protected:
  EMSObserverMap observers_map_;

  DISALLOW_COPY_AND_ASSIGN(AtomMediaStreamEngine);
};

}  // namespace atom

#endif  // ATOM_BROWSER_EMS_ATOM_EXTERNAL_MEDIA_STREAM_ENGINE_H_
