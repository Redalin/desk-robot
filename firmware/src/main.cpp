// desk-robot firmware (ESP32-S3 SuperMini Edition)
//
// Hardware:
//   - MCU: ESP32-S3 SuperMini (Dual-core Xtensa LX7 @ 240MHz, 4MB Flash, USB-C)
//   - Display: 1.28" Round IPS TFT LCD (240x240 GC9A01 4-wire SPI)
//   - Audio Output: Dual MAX98357A I2S 3W Class-D Amplifiers (Dual Mono)
//   - Microphone: MAX9814 Electret Microphone with AGC (ADC1_CH0 / GPIO 1)
//   - Touch: Dual TTP223 Capacitive Sensors (Head = Pet/Talk, Cheek = Mute/Sleep)

#include <Arduino.h>
#include <SPI.h>
#include <WiFi.h>
#include <Adafruit_GFX.h>
#include <Adafruit_GC9A01A.h>

#include "camera.h"
#include "config.h"
#include "face.h"
#include "link.h"
#include "mic.h"
#include "servo_neck.h"
#include "speaker.h"
#include "touch.h"

#if __has_include("secrets.h")
#include "secrets.h"
#define HAVE_BRAIN 1
#elif __has_include(<secrets.h>)
#include <secrets.h>
#define HAVE_BRAIN 1
#elif __has_include("../include/secrets.h")
#include "../include/secrets.h"
#define HAVE_BRAIN 1
#else
#define HAVE_BRAIN 0
#endif

// Display driver on SPI
Adafruit_GC9A01A tft(PIN_LCD_CS, PIN_LCD_DC, PIN_LCD_RES);

Face face(tft);
ServoNeck panNeck;
ServoNeck tiltNeck;
Link brainLink;
Speaker speaker;
Mic mic;
Camera camera;
Touch touch;

uint32_t nextTempMs = 0;
volatile bool speakDonePending = false;  // set by the speaker task, sent from loop()

uint32_t lastFrameMs = 0;
uint32_t nextStateMs = 0;
bool demoMode = true;  // cycles emotions on its own; off while the brain is connected
bool brainConnected = false;
bool glanceWanted = true;  // the brain's `glance on|off` (off while it holds a pose or tracks a face)

void applyGlances() {
  bool on = brainConnected && !face.asleep() && glanceWanted;
  panNeck.setIdleGlances(on);
  tiltNeck.setIdleGlances(on);
}

uint32_t nextDemoEmotionMs = 0;
uint8_t demoEmotionIdx = 0;

void printHelp() {
  Serial.println(F("desk-robot (ESP32-S3 SuperMini) commands:"));
  Serial.println(F("  emo <name>   neutral|happy|sad|angry|surprised|sleepy|thinking"));
  Serial.println(F("  pan <deg>    turn head (virtual gaze), -60..60 (0 = center)"));
  Serial.println(F("  tilt <deg>   nod head (virtual gaze), -60 (down)..0 (level)"));
  Serial.println(F("  center       head to center on both axes"));
  Serial.println(F("  blink        blink now"));
  Serial.println(F("  sleep on|off eyes shut, breathing, Z's"));
  Serial.println(F("  demo on|off  idle life: auto blinks and emotion changes"));
  Serial.println(F("  glance on|off idle head wander"));
  Serial.println(F("  volume <0-1> speaker volume"));
  Serial.println(F("  beep         play a test tone through the MAX98357A speakers"));
  Serial.println(F("  mic on|off   stream the MAX9814 microphone to the brain"));
  Serial.println(F("  miclevel     print mic level for 3 s (talk to it)"));
  Serial.println(F("  touch <head|cheek> simulate touch sensor press"));
  Serial.println(F("  temp         chip temperature"));
  Serial.println(F("  help         this text"));
}

void handleCommand(String line) {
  line.trim();
  if (line.isEmpty()) return;

  int space = line.indexOf(' ');
  String cmd = (space < 0) ? line : line.substring(0, space);
  String arg = (space < 0) ? String() : line.substring(space + 1);
  cmd.toLowerCase();
  arg.trim();

  if (cmd == "help") {
    printHelp();
  } else if (cmd == "emo") {
    Emotion e;
    if (emotionFromName(arg.c_str(), e)) {
      face.setEmotion(e);
      Serial.printf("emotion: %s\n", emotionName(e));
    } else {
      Serial.println(F("unknown emotion — try: neutral happy sad angry surprised sleepy thinking"));
    }
  } else if (cmd == "pan") {
    panNeck.setTarget(arg.toFloat());
    Serial.printf("pan -> %.0f deg\n", panNeck.target());
  } else if (cmd == "tilt") {
    tiltNeck.setTarget(arg.toFloat());
    Serial.printf("tilt -> %.0f deg\n", tiltNeck.target());
  } else if (cmd == "raw") {
    int sp = arg.indexOf(' ');
    String axis = sp < 0 ? arg : arg.substring(0, sp);
    float deg = sp < 0 ? 0 : arg.substring(sp + 1).toFloat();
    if (axis == "tilt") tiltNeck.setRaw(deg);
    else if (axis == "pan") panNeck.setRaw(deg);
    else { Serial.println(F("usage: raw pan|tilt <deg>")); return; }
    Serial.printf("raw %s -> %.0f deg\n", axis.c_str(), deg);
  } else if (cmd == "center") {
    panNeck.setTarget(0);
    tiltNeck.setTarget(0);
    Serial.println(F("head -> center"));
  } else if (cmd == "blink") {
    face.blink();
  } else if (cmd == "sleep") {
    face.setAsleep(arg == "on");
    applyGlances();
    Serial.printf("sleep %s\n", face.asleep() ? "on" : "off");
  } else if (cmd == "speak_begin") {
    speaker.beginSpeech(static_cast<size_t>(arg.toInt()));
  } else if (cmd == "speak_end") {
    speaker.endSpeech();
  } else if (cmd == "volume") {
    speaker.setVolume(arg.toFloat());
    Serial.printf("volume -> %.2f\n", arg.toFloat());
  } else if (cmd == "beep") {
    // 0.4 s of 440 Hz test tone
    static int16_t tone[16000 * 4 / 10];
    for (size_t i = 0; i < sizeof(tone) / sizeof(tone[0]); ++i) {
      tone[i] = static_cast<int16_t>(8000 * sinf(2 * PI * 440 * i / 16000.0f));
    }
    speaker.beginSpeech();
    speaker.feed(reinterpret_cast<uint8_t*>(tone), sizeof(tone));
    speaker.endSpeech();
    Serial.println(F("beep"));
  } else if (cmd == "mic") {
    if (arg == "on") {
      mic.setStreaming(true);
    } else if (arg == "off") {
      mic.setStreaming(false);
    } else {
      mic.setStreaming(!mic.streaming());
    }
    Serial.printf("mic streaming: %s (level: %.3f)\n", mic.streaming() ? "ON" : "OFF", mic.level());
  } else if (cmd == "miclevel") {
    bool wasStreaming = mic.streaming();
    mic.setStreaming(true);
    for (int i = 0; i < 12; ++i) {
      delay(250);
      int bars = static_cast<int>(mic.level() * 200);
      Serial.printf("  level %.3f %.*s\n", mic.level(), min(bars, 40), "########################################");
    }
    mic.setStreaming(wasStreaming);
  } else if (cmd == "touch") {
    if (arg == "head") {
      if (face.asleep()) {
        face.setAsleep(false);
        applyGlances();
        Serial.println(F("[touch] Head simulated: woke up"));
      } else {
        face.setEmotion(Emotion::Happy);
        face.blink();
        Serial.println(F("[touch] Head simulated: happy"));
      }
    } else if (arg == "cheek") {
      if (speaker.speaking()) {
        speaker.endSpeech();
        Serial.println(F("[touch] Cheek simulated: muted speech"));
      } else {
        bool sleeping = !face.asleep();
        face.setAsleep(sleeping);
        applyGlances();
        Serial.printf("[touch] Cheek simulated: sleep %s\n", sleeping ? "on" : "off");
      }
    } else {
      Serial.println(F("usage: touch head|cheek"));
    }
  } else if (cmd == "stream") {
    Serial.println(F("camera not available on SuperMini build"));
  } else if (cmd == "snap") {
    Serial.println(F("camera not available on SuperMini build"));
  } else if (cmd == "temp") {
    Serial.printf("chip %.1f C\n", temperatureRead());
  } else if (cmd == "glance") {
    glanceWanted = (arg == "on");
    applyGlances();
    Serial.printf("glance %s\n", glanceWanted ? "on" : "off");
  } else if (cmd == "demo") {
    demoMode = (arg == "on");
    face.setIdle(demoMode);
    Serial.printf("demo %s\n", demoMode ? "on" : "off");
  } else {
    Serial.println(F("unknown command — type `help`"));
  }
}

void setup() {
  Serial.begin(115200);
  randomSeed(esp_random());

  // Backlight control for GC9A01 LCD (if connected to GPIO 13)
  if (PIN_LCD_BLK != 255) {
    pinMode(PIN_LCD_BLK, OUTPUT);
    digitalWrite(PIN_LCD_BLK, HIGH);
  }

  // SPI Hardware bus for GC9A01 display: SCL=12, SDA=11, CS=10
  SPI.begin(PIN_LCD_SCL, -1, PIN_LCD_SDA, PIN_LCD_CS);
  face.begin();

  // Virtual neck for easing & gaze (no servos wired)
#if HAVE_SERVOS
  ESP32PWM::allocateTimer(0);
  ESP32PWM::allocateTimer(1);
  ESP32PWM::allocateTimer(2);
  ESP32PWM::allocateTimer(3);
  panNeck.begin(PIN_SERVO_PAN, PAN_MIN_DEG, PAN_MAX_DEG, PAN_MAX_SPEED,
                SERVO_RELAX_MS, PAN_TRIM_DEG, 25.0f);
  tiltNeck.begin(PIN_SERVO_TILT, TILT_MIN_DEG, TILT_MAX_DEG, TILT_MAX_SPEED,
                 SERVO_RELAX_MS, TILT_TRIM_DEG, 10.0f, TILT_INVERT);
#else
  panNeck.begin(255, PAN_MIN_DEG, PAN_MAX_DEG, PAN_MAX_SPEED,
                SERVO_RELAX_MS, PAN_TRIM_DEG, 25.0f);
  tiltNeck.begin(255, TILT_MIN_DEG, TILT_MAX_DEG, TILT_MAX_SPEED,
                 SERVO_RELAX_MS, TILT_TRIM_DEG, 10.0f, TILT_INVERT);
#endif

  // Dual MAX98357A Amplifiers on I2S0 (BCLK=5, LRC=6, DIN=7)
  speaker.begin(PIN_I2S_BCLK, PIN_I2S_LRC, PIN_I2S_DIN, SPEAKER_VOLUME,
                []() { speakDonePending = true; });

#if MIC_TYPE_I2S
  // INMP441 / MS3625 Omnidirectional MEMS Microphone on I2S1
  mic.beginI2S(PIN_MIC_SCK, PIN_MIC_WS, PIN_MIC_SD, MIC_GAIN);
#else
  // MAX9814 Electret Microphone on ADC1 (GPIO 1)
  mic.beginAnalog(PIN_MIC_ADC, MIC_GAIN);
#endif

  // TTP223 Capacitive Touch Sensors (Head=GPIO 4, Cheek=GPIO 2)
#if HAVE_TOUCH
  touch.begin(PIN_TOUCH_HEAD, PIN_TOUCH_CHEEK);
  touch.onHeadTouch([]() {
    if (face.asleep()) {
      face.setAsleep(false);
      applyGlances();
      Serial.println(F("[touch] Head: Rocky woke up!"));
    } else {
      face.setEmotion(Emotion::Happy);
      face.blink();
      Serial.println(F("[touch] Head: Pet / Happy!"));
    }
#if HAVE_BRAIN
    if (brainLink.connected()) {
      brainLink.sendJson("{\"type\":\"touch\",\"sensor\":\"head\"}");
    }
#endif
  });

  touch.onCheekTouch([]() {
    if (speaker.speaking()) {
      speaker.endSpeech();
      Serial.println(F("[touch] Cheek: Muted speech"));
    } else {
      bool sleeping = !face.asleep();
      face.setAsleep(sleeping);
      applyGlances();
      Serial.printf("[touch] Cheek: Sleep %s\n", sleeping ? "on" : "off");
    }
#if HAVE_BRAIN
    if (brainLink.connected()) {
      brainLink.sendJson("{\"type\":\"touch\",\"sensor\":\"cheek\"}");
    }
#endif
  });
#endif

#if HAVE_CAMERA
  if (camera.begin()) Serial.println(F("camera: ready"));
#endif

  demoMode = true;
  face.setIdle(true);
  applyGlances();
  nextDemoEmotionMs = millis() + 8000;

  Serial.println(F("\ndesk-robot (ESP32-S3 SuperMini) — ready!"));
  printHelp();

#if HAVE_BRAIN
  Serial.printf("\n[link] secrets.h active! Connecting to WiFi \"%s\" (Brain: %s:%d)...\n", WIFI_SSID, BRAIN_HOST, BRAIN_PORT);
  brainLink.onAudio([](const uint8_t* pcm, size_t len) { speaker.feed(pcm, len); });
#ifndef ROBOT_TOKEN
#define ROBOT_TOKEN ""
#endif
  brainLink.begin(WIFI_SSID, WIFI_PASS, BRAIN_HOST, BRAIN_PORT, ROBOT_TOKEN,
             [](const String& cmd) { handleCommand(cmd); },
             [](bool connected) {
               brainConnected = connected;
               demoMode = !connected;
               if (connected) {
                 face.setEmotion(Emotion::Happy);
               }
               applyGlances();
             });
#else
  Serial.println(F("\n[link] WARNING: secrets.h NOT FOUND! Running in USB-only mode (WiFi disabled)."));
#endif
}

void loop() {
  // Serial console
  static String lineBuf;
  while (Serial.available()) {
    char c = static_cast<char>(Serial.read());
    if (c == '\n' || c == '\r') {
      if (!lineBuf.isEmpty()) handleCommand(lineBuf);
      lineBuf = "";
    } else if (lineBuf.length() < 80) {
      lineBuf += c;
    }
  }

  uint32_t now = millis();

#if HAVE_TOUCH
  touch.update(now);
#endif

#if HAVE_BRAIN
  brainLink.update(now);
  if (speakDonePending) {
    speakDonePending = false;
    brainLink.sendJson("{\"type\":\"speak_done\"}");
    Serial.printf("speech: %u underruns, prebuffered %u bytes in %u ms, wifi %d dBm\n",
                  speaker.lastUnderruns(), speaker.lastPrebuffered(),
                  speaker.lastPrebufferMs(), WiFi.RSSI());
  }
  static uint32_t micFramesSent = 0;
  {
    static uint8_t frame[Mic::FRAME_BYTES];
    while (brainLink.connected() && mic.nextFrame(frame)) {
      brainLink.sendBinary(0x01, frame, sizeof(frame));
      micFramesSent++;
    }
  }
  static uint32_t lastHeartbeatMs = 0;
  if (now - lastHeartbeatMs >= 4000) {
    lastHeartbeatMs = now;
    if (brainLink.connected()) {
      Serial.printf("[status] WiFi: %d dBm | Mic: %s (lvl: %.3f, sent: %u) | Spk: %s | Heap: %u KB\n",
                    WiFi.RSSI(),
                    mic.streaming() ? "STREAMING" : "OFF",
                    mic.level(),
                    static_cast<unsigned>(micFramesSent),
                    speaker.speaking() ? "PLAYING" : "IDLE",
                    static_cast<unsigned>(ESP.getFreeHeap() / 1024));
    }
  }
  if (brainLink.connected() && now >= nextTempMs) {
    nextTempMs = now + 10000;
    brainLink.sendJson(String("{\"type\":\"temp\",\"c\":") + String(temperatureRead(), 1) + "}");
  }
  if (brainLink.connected() && now >= nextStateMs) {
    nextStateMs = now + 5000;
    brainLink.sendState(panNeck.current(), emotionName(face.emotion()));
  }
#endif

  // Demo mode: wander through emotions
  if (demoMode && now >= nextDemoEmotionMs) {
    static const Emotion cycle[] = {
        Emotion::Neutral, Emotion::Happy,     Emotion::Thinking,
        Emotion::Neutral, Emotion::Surprised, Emotion::Sleepy,
    };
    demoEmotionIdx = (demoEmotionIdx + 1) % (sizeof(cycle) / sizeof(cycle[0]));
    face.setEmotion(cycle[demoEmotionIdx]);
    nextDemoEmotionMs = now + random(6000, 12000);
  }

  // Animation and rendering tick (~30 FPS)
  if (now - lastFrameMs >= FRAME_INTERVAL_MS) {
    lastFrameMs = now;
    face.setTalking(speaker.speaking(), speaker.level());

    // Map virtual neck pan/tilt to face gaze on the round screen
    float gazeX = (panNeck.current() / 60.0f) * 14.0f;
    float gazeY = (tiltNeck.current() / 60.0f) * 8.0f;
    face.setNeckGaze(gazeX, gazeY);

    face.update(now);
    panNeck.update(now);
    tiltNeck.update(now);
  }
}
