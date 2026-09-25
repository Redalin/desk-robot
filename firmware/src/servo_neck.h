#pragma once

#include <Arduino.h>
#include "config.h"

#if HAVE_SERVOS
#include <ESP32Servo.h>
#endif

// One head axis (pan or tilt): smooth easing, speed limiting, auto-relax.
// When HAVE_SERVOS is 0, functions as a virtual neck (smooth simulation for gaze & state).
class ServoNeck {
 public:
  void begin(uint8_t pin, float minDeg, float maxDeg, float maxSpeedDegPerSec,
             uint32_t relaxAfterMs, float trimDeg = 0.0f,
             float glanceRangeDeg = 25.0f, bool invert = false);

  void setTarget(float deg);
  void setRaw(float deg);
  void hold();
  bool moving() const { return fabsf(targetDeg_ - currentDeg_) > 0.25f; }
  float current() const { return currentDeg_; }
  float target() const { return targetDeg_; }

  void setIdleGlances(bool on) { idleGlances_ = on; }
  void update(uint32_t nowMs);

 private:
  void writeAngle(float deg);
  void attachIfNeeded();

#if HAVE_SERVOS
  Servo servo_;
#endif
  uint8_t pin_ = 255;
  float minDeg_ = -60, maxDeg_ = 60;
  float maxSpeed_ = 180; // deg/sec
  uint32_t relaxAfterMs_ = 1500;
  float trimDeg_ = 0;
  float glanceRange_ = 25;
  bool invert_ = false;

  float currentDeg_ = 0;
  float targetDeg_ = 0;
  bool attached_ = false;
  uint32_t settledSinceMs_ = 0;
  uint32_t lastUpdateMs_ = 0;
  uint32_t nextGlanceMs_ = 0;
  bool idleGlances_ = true;
};
