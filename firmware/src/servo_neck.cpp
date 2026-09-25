#include "servo_neck.h"

void ServoNeck::begin(uint8_t pin, float minDeg, float maxDeg,
                      float maxSpeedDegPerSec, uint32_t relaxAfterMs,
                      float trimDeg, float glanceRangeDeg, bool invert) {
  pin_ = pin;
  minDeg_ = minDeg;
  maxDeg_ = maxDeg;
  maxSpeed_ = maxSpeedDegPerSec;
  relaxAfterMs_ = relaxAfterMs;
  trimDeg_ = trimDeg;
  glanceRange_ = glanceRangeDeg;
  invert_ = invert;
  currentDeg_ = 0;
  targetDeg_ = 0;
  lastUpdateMs_ = millis();
  nextGlanceMs_ = millis() + 5000;
  attachIfNeeded();
  writeAngle(0);
}

void ServoNeck::attachIfNeeded() {
#if HAVE_SERVOS
  if (pin_ == 255 || attached_) return;
  servo_.setPeriodHertz(50);
  servo_.attach(pin_, 500, 2400);
  attached_ = true;
#endif
}

void ServoNeck::writeAngle(float deg) {
#if HAVE_SERVOS
  if (pin_ == 255) return;
  float servoDeg = (invert_ ? -deg : deg) + trimDeg_;
  float t = (servoDeg + 90.0f) / 180.0f;
  int us = 500 + static_cast<int>(t * 1900.0f);
  servo_.writeMicroseconds(constrain(us, 500, 2400));
#else
  (void)deg;
#endif
}

void ServoNeck::setRaw(float deg) {
  deg = constrain(deg, -60.0f, 60.0f);
  attachIfNeeded();
  currentDeg_ = targetDeg_ = deg;
  settledSinceMs_ = 0;
  writeAngle(deg);
}

void ServoNeck::hold() {
  bool wasAttached = attached_;
  attachIfNeeded();
  if (!wasAttached) writeAngle(currentDeg_);
  settledSinceMs_ = 0;
}

void ServoNeck::setTarget(float deg) {
  targetDeg_ = constrain(deg, minDeg_, maxDeg_);
  settledSinceMs_ = 0;
  attachIfNeeded();
}

void ServoNeck::update(uint32_t nowMs) {
  float dt = (nowMs - lastUpdateMs_) / 1000.0f;
  lastUpdateMs_ = nowMs;
  if (dt <= 0 || dt > 0.5f) dt = 0.033f;

  if (idleGlances_ && nowMs >= nextGlanceMs_) {
    int range = static_cast<int>(glanceRange_);
    float glance = static_cast<float>(random(-range, range + 1));
    if (random(100) < 50) glance = 0;
    setTarget(glance);
    nextGlanceMs_ = nowMs + random(6000, 16000);
  }

  float diff = targetDeg_ - currentDeg_;
  if (fabsf(diff) > 0.25f) {
    float step = diff * 0.15f;
    float maxStep = maxSpeed_ * dt;
    step = constrain(step, -maxStep, maxStep);
    if (fabsf(step) < 0.15f) step = (diff > 0 ? 0.15f : -0.15f);
    currentDeg_ += step;
    attachIfNeeded();
    writeAngle(currentDeg_);
    settledSinceMs_ = 0;
  } else if (attached_) {
#if HAVE_SERVOS
    if (settledSinceMs_ == 0) {
      settledSinceMs_ = nowMs;
    } else if (nowMs - settledSinceMs_ >= relaxAfterMs_) {
      servo_.detach();
      attached_ = false;
    }
#endif
  }
}
