#include "libcef/browser/ems/atom_external_media_stream_manager.h"

#include "content/public/browser/external_media_stream_observer.h"

#include "include/cef_ems.h"

int CefEmsProviderManager::OnIncomingCapturedI420Data(
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
  int frame_feedback_id) {
  return atom::AtomExternalMediaStreamManager::GetInstance()
    ->NotifyIncomingCapturedI420Data(provider_name,
      provider_name_length,
      path,
      path_length,
      y_data,
      u_data,
      v_data,
      y_stride,
      u_stride,
      v_stride,
      frame_width,
      frame_height,
      frame_rate,
      clockwise_rotation,
      timestamp,
      frame_feedback_id);
}

int CefEmsProviderManager::OnIncomingCapturedRGBAData(
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
  int frame_feedback_id) {
  return atom::AtomExternalMediaStreamManager::GetInstance()
    ->NotifyIncomingCapturedI420Data(provider_name,
      provider_name_length,
      path,
      path_length,
      data,
      data_length,
      frame_width,
      frame_height,
      frame_rate,
      clockwise_rotation,
      timestamp,
      pixel_format,
      frame_feedback_id);
}

void CefEmsProviderManager::OnError(
  const char* provider_name,
  const char* provider_name_length,
  const char* reason,
  int reason_length) {
  atom::AtomExternalMediaStreamManager::GetInstance()
    ->NotifyError(provider_name,
      provider_name_length,
      reason,
      reason_length);
}

namespace atom {

// static
AtomExternalMediaStreamManager* AtomExternalMediaStreamManager::GetInstance() {
  return base::Singleton<AtomExternalMediaStreamManager>::get();
}

AtomExternalMediaStreamManager::AtomExternalMediaStreamManager() {}

AtomExternalMediaStreamManager::~AtomExternalMediaStreamManager() {}


void AtomExternalMediaStreamManager::Init() {
}

void AtomExternalMediaStreamManager::UnInit() {

}

bool AtomExternalMediaStreamManager::AddObserver(
  const std::string& provider,
  const std::string& path,
  content::ExternalMediaStreamObserver* observer) {
  base::AutoLock auto_lock(engine_map_lock_);
  std::string copy_provider(provider);
  std::string key = copy_provider.append("#").append(path);

  if (engine_map_.find(key) == engine_map_.end()) {
    ProviderObservers observers;
    engine_map_[key] = observers;
  }

  ProviderObservers& os = engine_map_[key];
  auto i = os.begin();
  while (i != os.end()) {
    auto o = *i;
    if (o == observer) {
      return false;
    }
    ++i;
  }

  os.push_back(observer);
  return true;
}

bool AtomExternalMediaStreamManager::RemoveObserver(
  content::ExternalMediaStreamObserver* observer) {
  base::AutoLock auto_lock(engine_map_lock_);
  auto keys = engine_map_.begin();
  while (keys != engine_map_.end()) {
    ProviderObservers& os = keys->second;
    ProviderObservers::iterator i = os.begin();
    while (i != os.end()) {
      ProviderObservers::iterator currnet = i++;
      auto o = *currnet;
      if (o == observer) {
        os.erase(currnet);
      }
    }
    ++keys;
  }
  return true;
}

bool AtomExternalMediaStreamManager::isStreamValid(
  const std::string& provider,
  const std::string& path) {
  return true;
}

int AtomExternalMediaStreamManager::NotifyIncomingCapturedI420Data(
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
  int frame_feedback_id) {
  base::AutoLock auto_lock(engine_map_lock_);

  base::TimeTicks frame_timestamp = base::TimeTicks::Now();
  if(timestamp != 0) {
    frame_timestamp = base::TimeTicks::FromInternalValue(timestamp);
  }
  if (first_frame_time_.is_null()) {
    first_frame_time_ = frame_timestamp;
  }

  std::string data_path(path);
  std::string provider(provider_name);

  std::string data_key = provider.append("#").append(path);

  if (engine_map_.find(data_key) == engine_map_.end()) {
    return -1;
  }

  media::VideoCaptureFormat format;
  format.frame_size.set_width(frame_width);
  format.frame_size.set_height(frame_height);
  format.frame_rate = frame_rate;
  //wanghongliang 201808182057 START
  //format.pixel_storage = media::PIXEL_STORAGE_CPU;
  //wanghongliang 201808182057 START
  format.pixel_format = media::PIXEL_FORMAT_I420;
  ProviderObservers& observers = engine_map_[data_key];
  auto i = observers.begin();
  while (i != observers.end()) {
    auto o = *i;
    o->OnIncomingCapturedData(y_data,
      u_data,
      v_data,
      y_stride,
      u_stride,
      v_stride,
      format,
      clockwise_rotation,
      first_frame_time_,
      frame_timestamp - first_frame_time_,
      frame_feedback_id
    );
    ++i;
  }
  return 0;
}


int AtomExternalMediaStreamManager::NotifyIncomingCapturedI420Data(
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
  int frame_feedback_id) {

  base::AutoLock auto_lock(engine_map_lock_);
  base::TimeTicks frame_timestamp = base::TimeTicks::Now();
  if(timestamp != 0) {
    frame_timestamp = base::TimeTicks::FromInternalValue(timestamp);
  }
  if (first_frame_time_.is_null()) {
    first_frame_time_ = frame_timestamp;
  }

  std::string data_path(path);
  std::string provider(provider_name);

  std::string data_key = provider.append("#").append(path);

  if (engine_map_.find(data_key) == engine_map_.end()) {
    return -1;
  }

  media::VideoCaptureFormat format;
  format.frame_size.set_width(frame_width);
  format.frame_size.set_height(frame_height);
  format.frame_rate = frame_rate;
  //wanghongliang 201808182057 START
  //format.pixel_storage = media::VideoPixelStorage(pixel_storage_type);
  //wanghongliang 201808182057 END
  format.pixel_format = media::VideoPixelFormat(pixel_format);
  ProviderObservers& observers = engine_map_[data_key];
  auto i = observers.begin();
  while (i != observers.end()) {
    auto o = *i;
    o->OnIncomingCapturedData(data,
      data_length,
      format,
      clockwise_rotation,
      first_frame_time_,
      frame_timestamp - first_frame_time_,
      frame_feedback_id);
    ++i;
  }
  return 0;
}

void AtomExternalMediaStreamManager::NotifyError(
  const char* provider_name,
  const char* provider_name_length,
  const char* reason,
  int reason_length) {
  base::AutoLock auto_lock(engine_map_lock_);
  std::string provider(provider_name);
  std::string data_key_prefix = provider.append("#");

  auto i = engine_map_.begin();
  while (i != engine_map_.end()) {
    const std::string& key = i->first;
    if (key.find(data_key_prefix) != 0)
      continue;

    // notify error
    auto os = i->second;
    auto oi = os.begin();
    while (oi != os.end()) {
      auto o = *oi;
      o->OnError(FROM_HERE, std::string(reason));
      ++oi;
    }
    ++i;
  }
}

} // namespace atom
