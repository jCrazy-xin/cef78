#include "libcef_dll/ctocpp/ems_ctocpp.h"

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
  return cef_ems_provider_manager_onincomingcapturedi420data(
    provider_name,
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
    frame_feedback_id
    );
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
  return cef_ems_provider_manager_onincomingcapturedRGBAdata(provider_name,
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
  cef_ems_provider_manager_onerror(
    provider_name,
    provider_name_length,
    reason,
    reason_length);
}
