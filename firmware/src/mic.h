#pragma once

#include <Arduino.h>

// Mic: MAX9814 Electret Microphone with AGC -> ADC1 16 kHz mono s16le frames.
//
// A FreeRTOS capture task samples ADC1_CH0 (GPIO 1) at 16 kHz in 30 ms chunks
// (480 samples = 960 bytes) and drops each frame into a queue; loop() drains
// the queue and ships frames to the brain.

class Mic {
 public:
  static const size_t FRAME_SAMPLES = 480;  // 30 ms at 16 kHz
  static const size_t FRAME_BYTES = FRAME_SAMPLES * 2;

  // I2S Digital MEMS Mic (INMP441 / MS3625)
  void beginI2S(uint8_t sckPin, uint8_t wsPin, uint8_t sdPin, float gain = 1.0f);

  // Analog Electret Mic (MAX9814)
  void beginAnalog(uint8_t adcPin, float gain = 1.0f);

  void setStreaming(bool on) { streaming_ = on; }
  bool streaming() const { return streaming_; }

  // Pops one captured frame into `out` (FRAME_BYTES). Returns false if none.
  bool nextFrame(uint8_t* out);

  // RMS (0..1) of the most recent frame, for level checks over serial.
  float level() const { return level_; }

  void setGain(float g) { gain_ = constrain(g, 0.1f, 15.0f); }
  float gain() const { return gain_; }

 private:
  static void taskEntry(void* self);
  void task();

  bool isI2S_ = true;
  uint8_t pin_ = 1;
  QueueHandle_t queue_ = nullptr;
  volatile bool streaming_ = false;
  volatile float level_ = 0.0f;
  float gain_ = 1.0f;
};
