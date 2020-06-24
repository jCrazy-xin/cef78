#include "libcef/browser/ems/atom_media_stream_engine.h"

#include "content/public/browser/external_media_stream_observer.h"

namespace atom {

bool AtomMediaStreamEngine::AddObserver( 
    const std::string& path,
    content::ExternalMediaStreamObserver* observer) {
  if (observers_map_.find(path) == observers_map_.end()) {
    EMSObservers observers;
    observers.push_back(observer);
    observers_map_[path] = observers;
    observer->OnStarted();
    return true;
  }

  EMSObservers& observers = observers_map_[path];
  EMSObservers::iterator i = observers.begin();
  while (i != observers.end()) {
    content::ExternalMediaStreamObserver* o = (*i);
    if (o == observer)
      return false;
    i++;
  }
  observers.push_back(observer);
  observer->OnStarted();
  return true;
}

bool AtomMediaStreamEngine::RemoveObserver(
    content::ExternalMediaStreamObserver* observer) {
  EMSObserverMap::iterator map_it = observers_map_.begin();
  while(map_it != observers_map_.end()) {
    EMSObservers& observers = map_it->second;
    EMSObservers::iterator i = observers.begin();
    while (i != observers.end()) {
      content::ExternalMediaStreamObserver* o = (*i);
      if (o == observer) {
        observers.erase(i);
        continue;
      } 
      i++;
    }
    map_it++;
  }
  return true;
}

} // namespace atom