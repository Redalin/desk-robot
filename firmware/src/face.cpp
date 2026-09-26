#include "face.h"

namespace {

#if USE_SH1106_OLED
constexpr int EYE_GAP = 14; // space between the two eyes on 128x64

constexpr uint16_t COLOR_BG     = 0; // Black / pixel off
constexpr uint16_t COLOR_WHITE  = 1; // Lit pixel (White / Blue)
constexpr uint16_t COLOR_CYAN   = 1;
constexpr uint16_t COLOR_YELLOW = 1;
constexpr uint16_t COLOR_PINK   = 1;
constexpr uint16_t COLOR_AQUA   = 1;
constexpr uint16_t COLOR_ORANGE = 1;
constexpr uint16_t COLOR_PURPLE = 1;

uint16_t eyeColorFor(Emotion /*e*/) {
  return 1;
}
#else
constexpr int EYE_GAP = 32; // space between the two eyes on 240x240

constexpr uint16_t COLOR_BG     = 0x0000; // Deep black
constexpr uint16_t COLOR_WHITE  = 0xFFFF; // Glint / white
constexpr uint16_t COLOR_CYAN   = 0x07FF; // Expressive Cyber Cyan
constexpr uint16_t COLOR_YELLOW = 0xFFE0; // Alert / Sleep Z's
constexpr uint16_t COLOR_PINK   = 0xFC18; // Blush pink
constexpr uint16_t COLOR_AQUA   = 0x051F; // Sad tears
constexpr uint16_t COLOR_ORANGE = 0xFD20; // Steam flame orange
constexpr uint16_t COLOR_PURPLE = 0xBDF7; // Thinking dots

uint16_t eyeColorFor(Emotion e) {
  switch (e) {
    case Emotion::Happy:     return 0x07FF; // Cyan
    case Emotion::Sad:       return 0x7DDF; // Pale blue
    case Emotion::Angry:     return 0xF980; // Fiery orange/red
    case Emotion::Surprised: return 0xFFE0; // Bright yellow
    case Emotion::Sleepy:    return 0x3CDF; // Soft dim cyan
    case Emotion::Thinking:  return 0xBDF7; // Gentle violet
    case Emotion::Neutral:
    default:                 return 0x07FF; // Electric cyan
  }
}
#endif

const char* kEmotionNames[] = {
    "neutral", "happy", "sad", "angry", "surprised", "sleepy", "thinking",
};

float approach(float cur, float target, float factor) {
  return cur + (target - cur) * factor;
}

} // namespace

Adafruit_GFX* Face::getGFX() {
#if USE_SH1106_OLED
  return oled_;
#else
  return canvas_ ? static_cast<Adafruit_GFX*>(canvas_) : static_cast<Adafruit_GFX*>(canvas1_);
#endif
}

Face::~Face() {
  if (canvas_) delete canvas_;
  if (canvas1_) delete canvas1_;
}

const char* emotionName(Emotion e) {
  return kEmotionNames[static_cast<uint8_t>(e)];
}

bool emotionFromName(const char* name, Emotion& out) {
  for (uint8_t i = 0; i < static_cast<uint8_t>(Emotion::COUNT); i++) {
    if (strcasecmp(name, kEmotionNames[i]) == 0) {
      out = static_cast<Emotion>(i);
      return true;
    }
  }
  return false;
}

Face::Params Face::paramsFor(Emotion e) {
#if USE_SH1106_OLED
  //                     eyeW  eyeH  radius browSlant lowerLid upperLid (scaled for 128x64 OLED)
  switch (e) {
    case Emotion::Happy:     return {34, 34, 10,   0, 14,  0};
    case Emotion::Sad:       return {32, 26,  8,  -8,  0,  4};
    case Emotion::Angry:     return {34, 24,  7,   9,  0,  0};
    case Emotion::Surprised: return {36, 42, 16,   0,  0,  0};
    case Emotion::Sleepy:    return {34, 26,  8,   0,  0, 13};
    case Emotion::Thinking:  return {30, 30,  9,   0,  0,  5};
    case Emotion::Neutral:
    default:                 return {34, 34, 10,   0,  0,  0};
  }
#else
  //                     eyeW  eyeH  radius browSlant lowerLid upperLid (scaled for 240x240 TFT)
  switch (e) {
    case Emotion::Happy:     return {66, 66, 22,   0, 30,  0};
    case Emotion::Sad:       return {60, 50, 18, -16,  0,  8};
    case Emotion::Angry:     return {64, 46, 16,  18,  0,  0};
    case Emotion::Surprised: return {70, 80, 34,   0,  0,  0};
    case Emotion::Sleepy:    return {64, 50, 18,   0,  0, 26};
    case Emotion::Thinking:  return {58, 58, 20,   0,  0, 10};
    case Emotion::Neutral:
    default:                 return {64, 64, 22,   0,  0,  0};
  }
#endif
}

void Face::begin() {
#if USE_SH1106_OLED
  if (oled_) {
    bool ok = oled_->begin(OLED_I2C_ADDR, true);
    if (!ok) {
      Serial.printf("[Face] WARNING: SH1106 OLED begin failed at 0x%02X! Trying 0x3D...\n", OLED_I2C_ADDR);
      ok = oled_->begin(0x3D, true);
    }
    if (ok) {
      Serial.println(F("[Face] SH1106 OLED initialized successfully!"));
      oled_->setRotation(OLED_ROTATION);
      oled_->setContrast(255);
      oled_->clearDisplay();
      oled_->display();
    } else {
      Serial.println(F("[Face] ERROR: SH1106 OLED failed to initialize on 0x3C and 0x3D!"));
    }
  }
#else
  if (tft_) {
    tft_->begin(40000000);
    tft_->setRotation(TFT_ROTATION);
    tft_->fillScreen(COLOR_BG);

    // Allocate double-buffered canvas in SRAM
    canvas_ = new GFXcanvas16(SCREEN_W, SCREEN_H);
    if (!canvas_) {
      Serial.println(F("[Face] 16-bit canvas failed, falling back to 1-bit"));
      canvas1_ = new GFXcanvas1(SCREEN_W, SCREEN_H);
    }
  }
#endif

  // Wake-up: start with eyes shut, then open.
  blinkAmount_ = 1.0f;
  blinkClosing_ = false;
  nextBlinkMs_ = millis() + 1500;
  nextSaccadeMs_ = millis() + 1500;
  nextSquintMs_ = millis() + 9000;
}

void Face::setEmotion(Emotion e) {
  if (e != emotion_) {
    pop_ = 1.0f;  // a little overshoot sells the change
    emotionSinceMs_ = millis();
    tearY_ = -1.0f;
    nextTearMs_ = millis() + 1500;
    thinkDots_ = 0;
  }
  emotion_ = e;
  target_ = paramsFor(e);
  // Thinking looks up and to the side; other emotions release the gaze.
  if (e == Emotion::Thinking) {
#if USE_SH1106_OLED
    gazeTargetX_ = 8;
    gazeTargetY_ = -5;
#else
    gazeTargetX_ = 14;
    gazeTargetY_ = -10;
#endif
  } else {
    gazeTargetX_ = 0;
    gazeTargetY_ = 0;
  }
}

void Face::blink() { blinkClosing_ = true; }

void Face::setAsleep(bool on) {
  if (on == asleep_) return;
  asleep_ = on;
  if (on) {
    for (Zed& z : zeds_) z.alive = false;
    nextZedMs_ = millis() + 800;
    nextTwitchMs_ = millis() + random(5000, 9000);
    gazeTargetX_ = gazeTargetY_ = 0;
  } else {
    pop_ = 1.0f;  // eyes spring open
    blinkAmount_ = 0.0f;
  }
}

void Face::setTalking(bool on, float level) {
  talking_ = on;
  mouthLevel_ = constrain(level, 0.0f, 1.0f);
}

void Face::stepAnimation(uint32_t nowMs) {
  frame_++;

  // Morph eye shape toward the current emotion preset (or shut, if asleep).
  Params goal = asleep_ ?
#if USE_SH1106_OLED
      Params{28, 4, 1, 0, 0, 0}
#else
      Params{56, 6, 2, 0, 0, 0}
#endif
      : target_;
  float f = asleep_ ? 0.12f : 0.25f;  // eyes close slowly, change fast
  cur_.eyeW = approach(cur_.eyeW, goal.eyeW, f);
  cur_.eyeH = approach(cur_.eyeH, goal.eyeH, f);
  cur_.radius = approach(cur_.radius, goal.radius, f);
  cur_.browSlant = approach(cur_.browSlant, goal.browSlant, f);
  cur_.lowerLid = approach(cur_.lowerLid, goal.lowerLid, f);
  cur_.upperLid = approach(cur_.upperLid, goal.upperLid, f);

  pop_ *= 0.82f;
  squint_ *= 0.85f;

  if (asleep_) {
    breath_ += 0.035f;
    blinkAmount_ = 0.0f;
    blinkClosing_ = false;
    twitch_ *= 0.8f;
    if (nowMs >= nextTwitchMs_) {
      twitch_ = 1.0f;
      nextTwitchMs_ = nowMs + random(6000, 12000);
    }
    if (nowMs >= nextZedMs_) {
      for (Zed& z : zeds_) {
        if (!z.alive) {
          z.alive = true;
          z.age = 0;
#if USE_SH1106_OLED
          z.x = SCREEN_W / 2 + EYE_GAP / 2 + cur_.eyeW + 2;
          z.y = SCREEN_H / 2 - 4;
#else
          z.x = SCREEN_W / 2 + EYE_GAP / 2 + cur_.eyeW + 4;
          z.y = SCREEN_H / 2 - 12;
#endif
          break;
        }
      }
      nextZedMs_ = nowMs + random(1100, 1700);
    }
    for (Zed& z : zeds_) {
      if (!z.alive) continue;
      z.age += 0.012f;
      z.y -= 0.6f;
      z.x += 0.4f;
#if USE_SH1106_OLED
      if (z.age >= 1.0f || z.y < 6 || z.x > SCREEN_W - 10) z.alive = false;
#else
      if (z.age >= 1.0f || z.y < 20 || z.x > SCREEN_W - 20) z.alive = false;
#endif
    }
    gazeX_ = approach(gazeX_, 0, 0.2f);
    gazeY_ = approach(gazeY_, 0, 0.2f);
    mouth_ = approach(mouth_, 0, 0.3f);
    return;
  }

  // Blink animation
  if (blinkClosing_) {
    blinkAmount_ += 0.45f;
    if (blinkAmount_ >= 1.0f) {
      blinkAmount_ = 1.0f;
      blinkClosing_ = false;
    }
  } else if (blinkAmount_ > 0.0f) {
    blinkAmount_ = max(0.0f, blinkAmount_ - 0.28f);
  }

  if (idle_) {
    if (nowMs >= nextBlinkMs_) {
      blinkClosing_ = true;
      nextBlinkMs_ = nowMs + random(2200, 6000);
      if (random(100) < 20) nextBlinkMs_ = nowMs + 400; // Double blink
    }
    if (nowMs >= nextSaccadeMs_ && emotion_ != Emotion::Thinking) {
#if USE_SH1106_OLED
      gazeTargetX_ = static_cast<float>(random(-8, 9));
      gazeTargetY_ = static_cast<float>(random(-4, 5));
#else
      gazeTargetX_ = static_cast<float>(random(-14, 15));
      gazeTargetY_ = static_cast<float>(random(-8, 9));
#endif
      if (random(100) < 40) gazeTargetX_ = gazeTargetY_ = 0;
      nextSaccadeMs_ = nowMs + random(1200, 4000);
    }
    if (nowMs >= nextSquintMs_ && emotion_ == Emotion::Neutral) {
      squint_ = 1.0f;
      nextSquintMs_ = nowMs + random(8000, 16000);
    }
  }

  // Per-emotion flourishes
  if (emotion_ == Emotion::Sad) {
    if (tearY_ < 0 && nowMs >= nextTearMs_) tearY_ = 0;
    if (tearY_ >= 0) {
      tearY_ += 1.4f;
      if (tearY_ > 60) {
        tearY_ = -1;
        nextTearMs_ = nowMs + random(2500, 5000);
      }
    }
  }
  if (emotion_ == Emotion::Thinking && nowMs >= nextDotMs_) {
    thinkDots_ = (thinkDots_ + 1) % 4;
    nextDotMs_ = nowMs + 420;
  }
  jitterX_ = (emotion_ == Emotion::Angry && (frame_ % 3 == 0)) ? random(-2, 3) : 0;

  // Mouth movement with speaker loudness
  float mouthGoal = talking_ ? constrain(0.15f + mouthLevel_ * 4.0f, 0.15f, 1.0f) : 0.0f;
  mouth_ = approach(mouth_, mouthGoal, talking_ ? 0.5f : 0.3f);

  gazeX_ = approach(gazeX_, gazeTargetX_, 0.35f);
  gazeY_ = approach(gazeY_, gazeTargetY_, 0.35f);
}

void Face::drawEye(int cx, int cy, bool isLeft, float hScale) {
  Adafruit_GFX* g = getGFX();
  if (!g) return;

#if USE_SH1106_OLED
  uint16_t eyeColor = 1;
#else
  uint16_t eyeColor = canvas_ ? eyeColorFor(emotion_) : 1;
#endif

  float h = cur_.eyeH * hScale * (1.0f - blinkAmount_);
  if (h < 3) h = 3;
  float w = cur_.eyeW;

  int x = cx - static_cast<int>(w / 2);
  int y = cy - static_cast<int>(h / 2);
  int r = min(static_cast<int>(cur_.radius), static_cast<int>(min(w, h) / 2 - 1));
  if (r < 0) r = 0;

  // Main eye body
  g->fillRoundRect(x, y, static_cast<int>(w), static_cast<int>(h), r, eyeColor);

  // Brow slant cut
  float slant = cur_.browSlant;
  if (fabsf(slant) > 0.5f) {
#if USE_SH1106_OLED
    int depth = static_cast<int>(fabsf(slant) * 1.2f);
#else
    int depth = static_cast<int>(fabsf(slant) * 1.8f);
#endif
    bool cutInner = (slant > 0);
    bool cutRightCorner = isLeft ? cutInner : !cutInner;
    int x0 = x - 2, x1 = x + static_cast<int>(w) + 2;
    if (cutRightCorner) {
      g->fillTriangle(x0, y - 2, x1, y - 2, x1, y + depth, COLOR_BG);
    } else {
      g->fillTriangle(x0, y - 2, x1, y - 2, x0, y + depth, COLOR_BG);
    }
  }

  // Lower lid cut (happy crescent)
  if (cur_.lowerLid > 0.5f) {
    int lidR = static_cast<int>(w * 1.05f);
#if USE_SH1106_OLED
    int lidY = y + static_cast<int>(h) + lidR - static_cast<int>(cur_.lowerLid * 1.4f);
#else
    int lidY = y + static_cast<int>(h) + lidR - static_cast<int>(cur_.lowerLid * 1.8f);
#endif
    g->fillCircle(cx, lidY, lidR, COLOR_BG);
  }

  // Upper lid droop (sleepy/thinking)
  if (cur_.upperLid > 0.5f) {
#if USE_SH1106_OLED
    g->fillRect(x - 2, y - 2, static_cast<int>(w) + 4,
                static_cast<int>(cur_.upperLid * 1.3f) + 2, COLOR_BG);
#else
    g->fillRect(x - 2, y - 2, static_cast<int>(w) + 4,
                static_cast<int>(cur_.upperLid * 1.8f) + 2, COLOR_BG);
#endif
  }

  // Glint: highlight offset with gaze
#if USE_SH1106_OLED
  if (h > 12 && !asleep_) {
    int gx = x + static_cast<int>(w * 0.22f) + static_cast<int>((gazeX_ + neckGazeX_) * 0.3f);
    int gy = y + static_cast<int>(h * 0.20f) + static_cast<int>(cur_.upperLid * 0.8f);
    int gs = (w > 30) ? 4 : 3;
    g->fillRect(gx, gy, gs, gs, COLOR_BG); // Dark pupil/notch in the lit eye
  }
#else
  if (h > 24 && !asleep_) {
    int gx = x + static_cast<int>(w * 0.22f) + static_cast<int>((gazeX_ + neckGazeX_) * 0.3f);
    int gy = y + static_cast<int>(h * 0.18f) + static_cast<int>(cur_.upperLid * 1.2f);
    int gs = (w > 60) ? 7 : 5;
    g->fillRect(gx, gy, gs, gs, canvas_ ? COLOR_WHITE : 0);
  }
#endif
}

void Face::drawMouth(int cx, int cy) {
  if (mouth_ < 0.05f) return;
  Adafruit_GFX* g = getGFX();
  if (!g) return;
  uint16_t mouthColor = eyeColorFor(emotion_);

#if USE_SH1106_OLED
  int mh = 2 + static_cast<int>(mouth_ * 10);
  int mw = 16 + static_cast<int>(mouth_ * 12);
  int my = cy + static_cast<int>(cur_.eyeH / 2) + 4;
  if (my + mh > SCREEN_H - 2) my = SCREEN_H - 2 - mh;

  g->fillRoundRect(cx - mw / 2, my, mw, mh, min(mh / 2, 4), mouthColor);
  if (mh > 5) {
    g->fillRoundRect(cx - mw / 2 + 2, my + 1, mw - 4, mh - 2, 2, COLOR_BG);
  }
#else
  int mh = 4 + static_cast<int>(mouth_ * 18);
  int mw = 36 + static_cast<int>(mouth_ * 20);
  int my = cy + static_cast<int>(cur_.eyeH / 2) + 12;
  if (my + mh > SCREEN_H - 15) my = SCREEN_H - 15 - mh;

  g->fillRoundRect(cx - mw / 2, my, mw, mh, min(mh / 2, 8), mouthColor);
  if (mh > 10) {
    g->fillRoundRect(cx - mw / 2 + 4, my + 3, mw - 8, mh - 6, 3, COLOR_BG);
  }
#endif
}

void Face::drawFlourishes(int leftCx, int rightCx, int cy, int eyeTop, int eyeBottom) {
  Adafruit_GFX* g = getGFX();
  if (!g) return;
  int half = static_cast<int>(cur_.eyeW / 2);
  uint32_t since = millis() - emotionSinceMs_;

  switch (emotion_) {
    case Emotion::Surprised:
      if (since < 900) {
#if USE_SH1106_OLED
        g->setTextSize(2);
        g->setTextColor(COLOR_WHITE);
        g->setCursor(SCREEN_W - 18, 8);
        g->print("!");
#else
        g->setTextSize(3);
        g->setTextColor(canvas_ ? COLOR_YELLOW : 1);
        g->setCursor(185, 45);
        g->print("!");
#endif
      }
      break;
    case Emotion::Happy:
      if (cur_.lowerLid > (USE_SH1106_OLED ? 4 : 8)) {
        // Blush: two short slashes below the outer corner of each eye
        uint16_t blushCol = (USE_SH1106_OLED ? COLOR_WHITE : (canvas_ ? COLOR_PINK : 1));
        for (int i = 0; i < 2; ++i) {
#if USE_SH1106_OLED
          int lx = leftCx - half - 6 + i * 3;
          int rx = rightCx + half + 2 + i * 3;
          g->drawLine(lx, eyeBottom + 2, lx + 3, eyeBottom - 2, blushCol);
          g->drawLine(rx, eyeBottom + 2, rx + 3, eyeBottom - 2, blushCol);
#else
          int lx = leftCx - half - 14 + i * 6;
          int rx = rightCx + half + 6 + i * 6;
          g->drawLine(lx, eyeBottom + 4, lx + 6, eyeBottom - 4, blushCol);
          g->drawLine(lx + 1, eyeBottom + 4, lx + 7, eyeBottom - 4, blushCol);
          g->drawLine(rx, eyeBottom + 4, rx + 6, eyeBottom - 4, blushCol);
          g->drawLine(rx + 1, eyeBottom + 4, rx + 7, eyeBottom - 4, blushCol);
#endif
        }
      }
      break;
    case Emotion::Sad:
      if (tearY_ >= 0) {
#if USE_SH1106_OLED
        int tx = rightCx + half - 2;
        int ty = eyeBottom + static_cast<int>(tearY_);
        if (ty < SCREEN_H - 3) {
          g->fillCircle(tx, ty, 2, COLOR_WHITE);
        }
#else
        int tx = rightCx + half - 4;
        int ty = eyeBottom + static_cast<int>(tearY_);
        uint16_t tearCol = canvas_ ? COLOR_AQUA : 1;
        g->fillCircle(tx, ty, 4, tearCol);
        g->fillTriangle(tx - 4, ty, tx + 4, ty, tx, ty - 8, tearCol);
#endif
      }
      break;
    case Emotion::Thinking:
      for (int i = 0; i < thinkDots_; ++i) {
#if USE_SH1106_OLED
        g->fillCircle(SCREEN_W - 24 + i * 7, 10, 2, COLOR_WHITE);
#else
        g->fillCircle(160 + i * 14, 45, 4, canvas_ ? COLOR_PURPLE : 1);
#endif
      }
      break;
    case Emotion::Angry:
      if ((frame_ / 6) % 2 == 0) {
        uint16_t steamCol = (USE_SH1106_OLED ? COLOR_WHITE : (canvas_ ? COLOR_ORANGE : 1));
#if USE_SH1106_OLED
        g->drawLine(leftCx - half - 4, eyeTop - 2, leftCx - half - 2, eyeTop - 8, steamCol);
        g->drawLine(rightCx + half + 4, eyeTop - 2, rightCx + half + 2, eyeTop - 8, steamCol);
#else
        g->drawLine(leftCx - half - 8, eyeTop - 6, leftCx - half - 4, eyeTop - 16, steamCol);
        g->drawLine(leftCx - half - 7, eyeTop - 6, leftCx - half - 3, eyeTop - 16, steamCol);
        g->drawLine(rightCx + half + 8, eyeTop - 6, rightCx + half + 4, eyeTop - 16, steamCol);
        g->drawLine(rightCx + half + 7, eyeTop - 6, rightCx + half + 3, eyeTop - 16, steamCol);
#endif
      }
      break;
    default:
      break;
  }
}

void Face::drawZeds() {
  Adafruit_GFX* g = getGFX();
  if (!g) return;
  g->setTextColor(USE_SH1106_OLED ? COLOR_WHITE : (canvas_ ? COLOR_YELLOW : 1));

  for (const Zed& z : zeds_) {
    if (!z.alive) continue;
    if (z.age > 0.8f && (frame_ % 2 == 0)) continue; // flicker out at end

#if USE_SH1106_OLED
    if (z.age < 0.5f) g->setTextSize(1);
    else g->setTextSize(2);

    int x = static_cast<int>(z.x);
    int y = static_cast<int>(z.y);
    if (x >= 4 && x < SCREEN_W - 12 && y > 8) {
      g->setCursor(x, y);
      g->print("Z");
    }
#else
    if (z.age < 0.33f) g->setTextSize(1);
    else if (z.age < 0.66f) g->setTextSize(2);
    else g->setTextSize(3);

    int x = static_cast<int>(z.x);
    int y = static_cast<int>(z.y);
    if (x >= 20 && x < SCREEN_W - 25 && y > 20) {
      g->setCursor(x, y);
      g->print("Z");
    }
#endif
  }
}

void Face::render() {
  Adafruit_GFX* g = getGFX();
  if (!g) return;

#if USE_SH1106_OLED
  oled_->clearDisplay();
#else
  g->fillScreen(COLOR_BG);
#endif

  // Vertical bob: breathing while asleep, bounce while talking
#if USE_SH1106_OLED
  float bob = asleep_ ? sinf(breath_) * 1.5f : -mouth_ * 1.5f;
  int cy = SCREEN_H / 2 + static_cast<int>(gazeY_ + neckGazeY_ + bob);
  if (talking_) cy -= 2;
#else
  float bob = asleep_ ? sinf(breath_) * 3.0f : -mouth_ * 3.0f;
  int cy = SCREEN_H / 2 - 6 + static_cast<int>(gazeY_ + neckGazeY_ + bob);
  if (talking_) cy -= 4;
#endif

  int half = static_cast<int>(cur_.eyeW / 2);
  int totalGazeX = static_cast<int>(gazeX_ + neckGazeX_) + jitterX_;
  int leftCx = SCREEN_W / 2 - EYE_GAP / 2 - half + totalGazeX;
  int rightCx = SCREEN_W / 2 + EYE_GAP / 2 + half + totalGazeX;

  // Height scale: emotion overshoot, idle squint, dream twitch
  float hScale = 1.0f + pop_ * (emotion_ == Emotion::Surprised ? 0.3f : 0.12f);
  hScale *= 1.0f - squint_ * 0.4f;
  if (asleep_) hScale += twitch_ * 2.5f;

  drawEye(leftCx, cy, true, hScale);
  drawEye(rightCx, cy, false, hScale);

  int eyeTop = cy - static_cast<int>(cur_.eyeH * hScale / 2);
  int eyeBottom = cy + static_cast<int>(cur_.eyeH * hScale / 2);

  if (asleep_) {
    drawZeds();
  } else {
    drawFlourishes(leftCx, rightCx, cy, eyeTop, eyeBottom);
    drawMouth(SCREEN_W / 2 + static_cast<int>((gazeX_ + neckGazeX_) * 0.5f), cy);
  }

#if USE_SH1106_OLED
  oled_->display();
#else
  // Push frame buffer to GC9A01 display
  if (canvas_ && tft_) {
    tft_->drawRGBBitmap(0, 0, canvas_->getBuffer(), SCREEN_W, SCREEN_H);
  } else if (canvas1_ && tft_) {
    tft_->drawBitmap(0, 0, canvas1_->getBuffer(), SCREEN_W, SCREEN_H, eyeColorFor(emotion_), COLOR_BG);
  }
#endif
}

void Face::update(uint32_t nowMs) {
  lastFrameMs_ = nowMs;
  stepAnimation(nowMs);
  render();
}
