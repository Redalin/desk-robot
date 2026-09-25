#include "touch.h"

static const uint32_t DEBOUNCE_DELAY_MS = 40;

void Touch::begin(uint8_t headPin, uint8_t cheekPin) {
  headPin_ = headPin;
  cheekPin_ = cheekPin;
  if (headPin_ != 255) {
    pinMode(headPin_, INPUT);
  }
  if (cheekPin_ != 255) {
    pinMode(cheekPin_, INPUT);
  }
}

void Touch::update(uint32_t nowMs) {
  // Head Touch (Touch 1: Pet / Talk)
  if (headPin_ != 255) {
    bool raw = digitalRead(headPin_) == HIGH;
    if (raw != lastHeadRaw_) {
      headDebounceMs_ = nowMs;
      lastHeadRaw_ = raw;
    }
    if ((nowMs - headDebounceMs_) > DEBOUNCE_DELAY_MS) {
      if (raw != headState_) {
        headState_ = raw;
        if (headState_ && onHead_) {
          onHead_();
        }
      }
    }
  }

  // Cheek Touch (Touch 2: Mute / Sleep)
  if (cheekPin_ != 255) {
    bool raw = digitalRead(cheekPin_) == HIGH;
    if (raw != lastCheekRaw_) {
      cheekDebounceMs_ = nowMs;
      lastCheekRaw_ = raw;
    }
    if ((nowMs - cheekDebounceMs_) > DEBOUNCE_DELAY_MS) {
      if (raw != cheekState_) {
        cheekState_ = raw;
        if (cheekState_ && onCheek_) {
          onCheek_();
        }
      }
    }
  }
}
