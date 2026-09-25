#pragma once

#include <Arduino.h>
#include <functional>

// TTP223 Capacitive Touch Sensor Handler
// Default TTP223 module outputs HIGH when touched, LOW when idle.
class Touch {
 public:
  using Callback = std::function<void()>;

  void begin(uint8_t headPin, uint8_t cheekPin);
  void onHeadTouch(Callback cb) { onHead_ = std::move(cb); }
  void onCheekTouch(Callback cb) { onCheek_ = std::move(cb); }

  bool headPressed() const { return headState_; }
  bool cheekPressed() const { return cheekState_; }

  void update(uint32_t nowMs);

 private:
  uint8_t headPin_ = 255;
  uint8_t cheekPin_ = 255;

  bool headState_ = false;
  bool cheekState_ = false;
  bool lastHeadRaw_ = false;
  bool lastCheekRaw_ = false;

  uint32_t headDebounceMs_ = 0;
  uint32_t cheekDebounceMs_ = 0;

  Callback onHead_;
  Callback onCheek_;
};
