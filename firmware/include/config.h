#pragma once

#include <Arduino.h>

// ─── Pin Map (ESP32-S3 SuperMini) ────────────────────────────────────────────
//
// Wiring Layout:
//                           +------------------------+
//                           |   ESP32-S3 SuperMini   |
//                           |       [ USB-C ]        |
//                           +---+----------------+---+
//               +5V (USB-C) | 1 | 5V           TX| 1 | ---> INMP441 WS (GPIO 43)
//                    Ground | 2 | GND          RX| 2 | ---> (Leave Free / Serial RX)
//         INMP441 VDD (+3V3)| 3 | 3V3           1| 3 | ---> INMP441 SD (Data Out -> GPIO 1)
//          (Optional BLK)   | 4 | 13            2| 4 | ---> Touch 2 (Cheek / Mute / Sleep)
//    GC9A01 SCL (SPI Clock) | 5 | 12            3| 5 | ---> INMP441 SCK (Clock -> GPIO 3)
//     GC9A01 SDA (SPI MOSI) | 6 | 11            4| 6 | ---> Touch 1 (Head / Pet / Talk)
//         GC9A01 CS (Chip)  | 7 | 10            5| 7 | ---> I2S BCLK  --+--> Amp 1 & 2 BCLK
//        GC9A01 DC (Data)   | 8 | 9             6| 8 | ---> I2S LRC   --+--> Amp 1 & 2 LRC
//       GC9A01 RES (Reset)  | 9 | 8             7| 9 | ---> I2S DIN   --+--> Amp 1 & 2 DIN
//                           +---+----------------+---+
//
// INMP441 / MS3625 I2S MEMS Microphone Module Wiring:
//   - VDD  ---> ESP32 3V3 (Left Pin 3)
//   - GND  ---> ESP32 GND (Left Pin 2)
//   - SD   ---> ESP32 GPIO 1 (Right Pin 3)
//   - SCK  ---> ESP32 GPIO 3 (Right Pin 5)
//   - WS   ---> ESP32 TX / GPIO 43 (Right Pin 1)
//   - L/R  ---> Connect to GND (selects Left audio channel)

// ─── Display (GC9A01 240x240 Round SPI TFT) ──────────────────────────────────
constexpr uint8_t PIN_LCD_SCL = 12; // SPI Clock
constexpr uint8_t PIN_LCD_SDA = 11; // SPI MOSI (Data)
constexpr uint8_t PIN_LCD_CS  = 10; // Chip Select
constexpr uint8_t PIN_LCD_DC  = 9;  // Data/Command
constexpr uint8_t PIN_LCD_RES = 8;  // Hardware Reset
constexpr uint8_t PIN_LCD_BLK = 13; // Backlight enable (optional)

constexpr int SCREEN_W = 240;
constexpr int SCREEN_H = 240;

// ─── Speaker (MAX98357A I2S Class-D Amps on I2S0) ────────────────────────────
constexpr uint8_t PIN_I2S_BCLK = 5; // Bit Clock (shared by Amp 1 & 2)
constexpr uint8_t PIN_I2S_LRC  = 6; // Word Select / Left-Right Clock
constexpr uint8_t PIN_I2S_DIN  = 7; // Serial Data In
constexpr float SPEAKER_VOLUME = 0.8f; // 0.0 - 1.0

// ─── Microphone Configuration (INMP441/MS3625 on I2S1 vs MAX9814 on ADC) ─────
#ifndef MIC_TYPE_I2S
#define MIC_TYPE_I2S 1   // 1 = Digital I2S (INMP441/MS3625), 0 = Analog (MAX9814)
#endif

#if MIC_TYPE_I2S
constexpr uint8_t PIN_MIC_SCK = 3;   // Bit Clock (SCK)       ---> GPIO 3
constexpr uint8_t PIN_MIC_WS  = 43;  // Word Select (WS / LRC) ---> Header TX pin (GPIO 43)
constexpr uint8_t PIN_MIC_SD  = 1;   // Serial Data In (SD)   ---> GPIO 1
constexpr float MIC_GAIN      = 1.5f; // Digital sensitivity gain
#else
constexpr uint8_t PIN_MIC_ADC = 1;   // GPIO 1 = ADC1_CH0
constexpr float MIC_GAIN      = 4.0f; // Analog gain
#endif

// ─── Touch Sensors (TTP223 Capacitive) ───────────────────────────────────────
constexpr uint8_t PIN_TOUCH_HEAD  = 4; // Touch 1: Head / Pet / Talk
constexpr uint8_t PIN_TOUCH_CHEEK = 2; // Touch 2: Cheek / Mute / Sleep

// ─── Hardware Feature Flags ──────────────────────────────────────────────────
#ifndef HAVE_SERVOS
#define HAVE_SERVOS 0  // No servos wired: uses virtual easing neck for angles & gaze
#endif

#ifndef HAVE_CAMERA
#define HAVE_CAMERA 0  // No camera module wired
#endif

#ifndef HAVE_TOUCH
#define HAVE_TOUCH 1   // TTP223 capacitive touch enabled
#endif

// ─── Motion Limits (Virtual Neck / Servo) ────────────────────────────────────
constexpr uint8_t PIN_SERVO_PAN  = 255;
constexpr uint8_t PIN_SERVO_TILT = 255;

constexpr float PAN_MIN_DEG   = -60.0f;
constexpr float PAN_MAX_DEG   =  60.0f;
constexpr float PAN_MAX_SPEED = 180.0f;
constexpr float PAN_TRIM_DEG  =   0.0f;

constexpr float TILT_MIN_DEG   = -60.0f;
constexpr float TILT_MAX_DEG   =   0.0f;
constexpr float TILT_MAX_SPEED = 120.0f;
constexpr float TILT_TRIM_DEG  =   0.0f;
constexpr bool  TILT_INVERT    =  true;

constexpr uint32_t SERVO_RELAX_MS = 1500;

// ─── Behavior ────────────────────────────────────────────────────────────────
constexpr uint32_t FRAME_INTERVAL_MS = 33; // ~30 FPS animation
