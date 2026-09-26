#pragma once

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_GC9A01A.h>
#include <Adafruit_SH110X.h>
#include "config.h"

// Expressive two-eye face:
// - 1.3" Monochrome I2C OLED (SH1106 128x64) when USE_SH1106_OLED = 1
// - 1.28" Round IPS TFT LCD (GC9A01 240x240) when USE_SH1106_OLED = 0
//
// Eyes are rounded rectangles whose shape morphs smoothly between emotion
// presets. Layered on top: blinks, gaze shifts, a glint in each eye, a mouth
// that moves with the speaker's audio, per-emotion flourishes ("!" pop,
// blush, tears, thinking dots, angry steam), and a sleep state with slow
// breathing and floating Z's.

enum class Emotion : uint8_t {
  Neutral,
  Happy,
  Sad,
  Angry,
  Surprised,
  Sleepy,
  Thinking,
  COUNT,
};

const char* emotionName(Emotion e);
bool emotionFromName(const char* name, Emotion& out);

class Face {
 public:
  explicit Face(Adafruit_GC9A01A& display) : tft_(&display), oled_(nullptr) {}
  explicit Face(Adafruit_SH1106G& display) : tft_(nullptr), oled_(&display) {}
  ~Face();

  void begin();
  void setEmotion(Emotion e);
  Emotion emotion() const { return emotion_; }
  void blink();

  // Asleep: eyes shut, breathing, Z's. Overrides the emotion while on.
  void setAsleep(bool on);
  bool asleep() const { return asleep_; }

  // Talking: draws a mouth that opens with `level` (0..1, the speaker's
  // current loudness). Call every frame while audio plays.
  void setTalking(bool on, float level = 0.0f);

  // When idle behavior is on, the face blinks and glances around on its own.
  void setIdle(bool on) { idle_ = on; }

  // Neck gaze offset: allows virtual head turns to move eyes on the round screen.
  void setNeckGaze(float gx, float gy) { neckGazeX_ = gx; neckGazeY_ = gy; }

  // Call every frame: advances animation, then renders.
  void update(uint32_t nowMs);

 private:
  // The morphable parameters of one emotion. All units are pixels.
  struct Params {
    float eyeW;      // eye width
    float eyeH;      // eye height
    float radius;    // corner rounding
    float browSlant; // top edge tilt: >0 inner corners drop (angry), <0 outer (sad)
    float lowerLid;  // pushes up from below -> happy crescent
    float upperLid;  // droops from above -> sleepy
  };

  struct Zed {
    bool alive = false;
    float x = 0, y = 0;
    float age = 0;  // 0..1 over its life
  };

  static Params paramsFor(Emotion e);
  void stepAnimation(uint32_t nowMs);
  void render();
  void drawEye(int cx, int cy, bool isLeft, float hScale);
  void drawMouth(int cx, int cy);
  void drawFlourishes(int leftCx, int rightCx, int cy, int eyeTop, int eyeBottom);
  void drawZeds();
  Adafruit_GFX* getGFX();

  Adafruit_GC9A01A* tft_ = nullptr;
  Adafruit_SH1106G* oled_ = nullptr;
  GFXcanvas16* canvas_ = nullptr;
  GFXcanvas1* canvas1_ = nullptr;

  Emotion emotion_ = Emotion::Neutral;
  Params cur_ = paramsFor(Emotion::Neutral);
  Params target_ = paramsFor(Emotion::Neutral);
  uint32_t emotionSinceMs_ = 0;

  // Blink state: 0 = open, 1 = fully closed.
  float blinkAmount_ = 0.0f;
  bool blinkClosing_ = false;
  uint32_t nextBlinkMs_ = 0;

  // Gaze offset from center, eased toward gazeTarget.
  float gazeX_ = 0, gazeY_ = 0;
  float gazeTargetX_ = 0, gazeTargetY_ = 0;
  float neckGazeX_ = 0, neckGazeY_ = 0;
  uint32_t nextSaccadeMs_ = 0;

  // Flourish state.
  float pop_ = 0.0f;            // overshoot on emotion change, decays to 0
  float squint_ = 0.0f;         // 1 while squinting, decays
  uint32_t nextSquintMs_ = 0;
  float tearY_ = -1.0f;         // -1 = no tear; else its y
  uint32_t nextTearMs_ = 0;
  uint8_t thinkDots_ = 0;
  uint32_t nextDotMs_ = 0;
  int jitterX_ = 0;

  // Sleep.
  bool asleep_ = false;
  float breath_ = 0.0f;         // phase
  Zed zeds_[3];
  uint32_t nextZedMs_ = 0;
  uint32_t nextTwitchMs_ = 0;
  float twitch_ = 0.0f;

  // Mouth.
  bool talking_ = false;
  float mouthLevel_ = 0.0f;
  float mouth_ = 0.0f;          // eased openness 0..1

  bool idle_ = true;
  uint32_t lastFrameMs_ = 0;
  uint32_t frame_ = 0;
};
