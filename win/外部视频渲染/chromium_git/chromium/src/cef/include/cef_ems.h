// tools directory for more information.
//

#ifndef CEF_INCLUDE_CEF_EMS_H_
#define CEF_INCLUDE_CEF_EMS_H_
#pragma once

#include "include/cef_base.h"

class CefEmsProviderManager : public virtual CefBaseRefCounted {
 public:
  /*--cef()--*/
  static int OnIncomingCapturedI420Data(
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

  static int OnIncomingCapturedRGBAData(
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

  static void OnError(
    const char* provider_name,
    const char* provider_name_length,
    const char* reason,
    int reason_length);
};


#endif // CEF_INCLUDE_CEF_EMS_H_
