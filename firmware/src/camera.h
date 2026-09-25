#pragma once

#include <Arduino.h>
#include "config.h"

// Camera: When HAVE_CAMERA is 0 (SuperMini), stubs out gracefully.
class Camera {
 public:
  bool begin();
  bool ok() const { return ok_; }

  void setStreaming(bool on, float fps = 10.0f);
  bool streaming() const { return streaming_; }

  size_t takeFrame(uint8_t* out, size_t cap);
  uint32_t framesCaptured() const { return captured_; }

 private:
#if HAVE_CAMERA
  static void taskEntry(void* self);
  void task();

  uint8_t* latest_ = nullptr;
  size_t latestCap_ = 0;
  volatile size_t latestLen_ = 0;
  volatile bool fresh_ = false;
  portMUX_TYPE lock_ = portMUX_INITIALIZER_UNLOCKED;
#endif

  bool ok_ = false;
  volatile bool streaming_ = false;
  volatile uint32_t intervalMs_ = 100;
  volatile uint32_t captured_ = 0;
};
