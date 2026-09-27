#pragma once

#include <Arduino.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <ArduinoJson.h>

#include "config.h"
#include "face.h"
#include "speaker.h"
#include "mic.h"
#include "camera.h"
#include "touch.h"
#include "link.h"

#ifndef FW_VERSION
#define FW_VERSION "1.1"
#endif

#ifndef BRAIN_HOST
#define BRAIN_HOST "0.0.0.0"
#endif

#ifndef BRAIN_PORT
#define BRAIN_PORT 8765
#endif

// Forward references to globals in main.cpp
extern Face face;
extern Speaker speaker;
extern Mic mic;
extern Camera camera;
extern Touch touch;
extern Link brainLink;
extern bool brainConnected;
extern void handleCommand(String line);

class RockyWebServer {
 public:
  RockyWebServer() : server_(80) {}

  void begin() {
    if (started_) return;
    started_ = true;

    if (MDNS.begin("rocky")) {
      Serial.println(F("[web] mDNS responder started: http://rocky.local/"));
    }

    server_.on("/", [this]() { handleRoot(); });
    server_.on("/api/status", [this]() { handleStatus(); });
    server_.on("/api/volume", HTTP_POST, [this]() { handleVolume(); });
    server_.on("/api/mic_gain", HTTP_POST, [this]() { handleMicGain(); });
    server_.on("/api/emotion", HTTP_POST, [this]() { handleEmotion(); });
    server_.on("/api/touch", HTTP_POST, [this]() { handleTouch(); });

    server_.begin();
    Serial.printf("[web] Onboard Web Server active: http://%s/\n", WiFi.localIP().toString().c_str());
    logActivity("Web server started on port 80");
  }

  void update() {
    if (!started_) return;
    server_.handleClient();
  }

  void logActivity(const String& msg) {
    uint32_t sec = millis() / 1000;
    char timeBuf[16];
    snprintf(timeBuf, sizeof(timeBuf), "%02u:%02u:%02u", (sec / 3600) % 24, (sec / 60) % 60, sec % 60);
    
    activities_[activityHead_].time = String(timeBuf);
    activities_[activityHead_].msg = msg;
    activityHead_ = (activityHead_ + 1) % MAX_ACTIVITIES;
    if (activityCount_ < MAX_ACTIVITIES) activityCount_++;
  }

 private:
  struct ActivityItem {
    String time;
    String msg;
  };

  static const size_t MAX_ACTIVITIES = 20;
  ActivityItem activities_[MAX_ACTIVITIES];
  size_t activityHead_ = 0;
  size_t activityCount_ = 0;

  WebServer server_;
  bool started_ = false;

  void handleRoot() {
    String html = F("<!doctype html><html><head><meta charset='utf-8'>"
      "<meta name='viewport' content='width=device-width,initial-scale=1'>"
      "<title>Rocky · Onboard Status</title><style>"
      ":root{--bg:#06080d;--panel:#0d121b;--line:#1d2533;--ink:#ece6d9;--muted:#8a94a6;--amber:#f2b45c;--ok:#34d399;--warn:#ff4a3d;--cyan:#38bdf8}"
      "body{margin:0;padding:18px;background:var(--bg);color:var(--ink);font-family:-apple-system,sans-serif;font-size:14px}"
      ".wrap{max-width:760px;margin:0 auto}"
      ".card{background:var(--panel);border:1px solid var(--line);border-radius:6px;padding:14px 16px;margin-bottom:14px}"
      "h1{margin:0 0 4px;font-size:20px;letter-spacing:1px;text-transform:uppercase}"
      "h2{margin:0 0 10px;font-size:11px;letter-spacing:2px;text-transform:uppercase;color:var(--muted);display:flex;justify-content:space-between}"
      ".badge{font-size:10px;padding:2px 6px;border-radius:3px;font-family:monospace;font-weight:bold}"
      ".badge.ok{background:rgba(52,211,153,.15);color:var(--ok)}"
      ".badge.warn{background:rgba(255,74,61,.15);color:var(--warn)}"
      ".grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(160px,1fr));gap:8px}"
      ".pitem{background:#090d14;border:1px solid var(--line);border-radius:4px;padding:8px 10px;font-size:12px}"
      ".pitem b{display:block;color:var(--ink);margin-bottom:2px}"
      ".pitem span{font-size:10px;color:var(--muted);font-family:monospace}"
      ".field{display:grid;grid-template-columns:90px 1fr 50px;gap:10px;align-items:center;margin:8px 0}"
      "input[type=range]{width:100%}"
      "button{background:transparent;border:1px solid var(--line);color:var(--ink);border-radius:4px;padding:6px 12px;cursor:pointer;margin-right:6px;margin-bottom:6px}"
      "button:hover{border-color:var(--amber)}"
      ".log{background:#080b11;border:1px solid var(--line);border-radius:4px;height:140px;overflow-y:auto;padding:8px 10px;font-family:monospace;font-size:11px}"
      ".log-line{display:flex;gap:8px;margin-bottom:4px}.log-time{color:var(--muted)}"
      "</style></head><body><div class='wrap'>"
      "<div class='card'>"
      "<h1>🤖 ROCKY <span style='font-size:12px;color:var(--muted);font-weight:normal'>ESP32-S3</span></h1>"
      "<div style='font-family:monospace;font-size:12px;color:var(--muted)'>Brain Link: <b id='brain-status'>--</b> · WiFi: <span id='rssi'>--</span> dBm</div>"
      "</div>"
      "<div class='card'>"
      "<h2><span>Peripherals Status</span><span class='badge ok' id='all-periph'>ALL LOADED</span></h2>"
      "<div class='grid' id='periph-list'></div>"
      "</div>"
      "<div class='card'>"
      "<h2><span>Audio Controls</span></h2>"
      "<div class='field'><span>Mic Gain</span><input type='range' id='gain' min='0.5' max='12' step='0.5'><span id='gainv' style='font-family:monospace'>--</span></div>"
      "<div class='field'><span>Speaker Vol</span><input type='range' id='vol' min='0' max='1' step='0.05'><span id='volv' style='font-family:monospace'>--</span></div>"
      "</div>"
      "<div class='card'>"
      "<h2><span>Actions</span></h2>"
      "<div><button onclick='act(\"touch\",{sensor:\"head\"})'>Pet Head</button>"
      "<button onclick='act(\"touch\",{sensor:\"cheek\"})'>Cheek Touch</button>"
      "<button onclick='act(\"emotion\",{name:\"happy\"})'>Happy</button>"
      "<button onclick='act(\"emotion\",{name:\"thinking\"})'>Thinking</button>"
      "<button onclick='act(\"emotion\",{name:\"sleepy\"})'>Sleep</button></div>"
      "</div>"
      "<div class='card'>"
      "<h2><span>Onboard Activity Log</span></h2>"
      "<div class='log' id='log'></div>"
      "</div>"
      "</div>"
      "<script>"
      "const $=id=>document.getElementById(id);"
      "async function poll(){"
      " try{"
      "  const s=await (await fetch('/api/status')).json();"
      "  $('brain-status').textContent=s.brain_connected?'CONNECTED ('+s.brain_host+')':'WAITING / STANDALONE';"
      "  $('brain-status').style.color=s.brain_connected?'var(--ok)':'var(--warn)';"
      "  $('rssi').textContent=s.rssi;"
      "  $('gain').value=s.mic_gain; $('gainv').textContent=s.mic_gain.toFixed(1)+'x';"
      "  $('vol').value=s.volume; $('volv').textContent=Math.round(s.volume*100)+'%';"
      "  let ph='';"
      "  for(const [k,v] of Object.entries(s.peripherals)){"
      "    ph+=`<div class='pitem'><b>${v.type||k}</b><span style='color:${v.loaded?\"var(--ok)\":\"var(--warn)\"}'>${v.loaded?\"LOADED\":\"OFF\"}</span> · <span>${v.name}</span></div>`;"
      "  }"
      "  $('periph-list').innerHTML=ph;"
      "  let lh='';"
      "  for(const l of (s.activities||[])) lh+=`<div class='log-line'><span class='log-time'>${l.time}</span><span>${l.msg}</span></div>`;"
      "  $('log').innerHTML=lh;"
      " }catch(e){}"
      " setTimeout(poll, 1000);"
      "}"
      "poll();"
      "async function act(ep,body){"
      " await fetch('/api/'+ep,{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(body)});"
      "}"
      "$('gain').onchange=()=>act('mic_gain',{gain:+$('gain').value});"
      "$('vol').onchange=()=>act('volume',{volume:+$('vol').value});"
      "</script></body></html>");
    server_.send(200, "text/html", html);
  }

  void handleStatus() {
    JsonDocument doc;
    doc["robot"] = "Rocky";
    doc["fw"] = FW_VERSION;
    doc["uptime_s"] = millis() / 1000;
    doc["brain_connected"] = brainConnected;
    doc["brain_host"] = String(BRAIN_HOST) + ":" + String(BRAIN_PORT);
    doc["rssi"] = WiFi.RSSI();
    doc["heap_kb"] = ESP.getFreeHeap() / 1024;
    doc["temp_c"] = temperatureRead();
    doc["volume"] = speaker.volume();
    doc["mic_gain"] = mic.gain();
    doc["mic_level"] = mic.level();
    doc["emotion"] = emotionName(face.emotion());
    doc["asleep"] = face.asleep();

    JsonObject p = doc["peripherals"].to<JsonObject>();
    JsonObject pD = p["display"].to<JsonObject>();
    pD["loaded"] = true;
    pD["type"] = "Display";
    pD["name"] = USE_SH1106_OLED ? "SH1106 OLED (128x64 I2C)" : "GC9A01 Round TFT (240x240 SPI)";

    JsonObject pM = p["mic"].to<JsonObject>();
    pM["loaded"] = true;
    pM["type"] = "Microphone";
    pM["name"] = MIC_TYPE_I2S ? "INMP441 MEMS (I2S)" : "MAX9814 (ADC Analog)";

    JsonObject pS = p["speaker"].to<JsonObject>();
    pS["loaded"] = true;
    pS["type"] = "Speaker";
    pS["name"] = "MAX98357A Class-D (I2S)";

    JsonObject pT = p["touch"].to<JsonObject>();
    pT["loaded"] = (HAVE_TOUCH != 0);
    pT["type"] = "Touch Sensors";
    pT["name"] = "TTP223 Dual (Head/Cheek)";

    JsonObject pC = p["camera"].to<JsonObject>();
    pC["loaded"] = (HAVE_CAMERA && camera.ok());
    pC["type"] = "Camera";
    pC["name"] = HAVE_CAMERA ? "OV2640" : "Not Installed";

    JsonObject pN = p["servos"].to<JsonObject>();
    pN["loaded"] = true;
    pN["type"] = "Motion / Neck";
    pN["name"] = HAVE_SERVOS ? "Dual PWM Servos" : "Virtual Easing Neck";

    JsonArray actArr = doc["activities"].to<JsonArray>();
    for (size_t i = 0; i < activityCount_; ++i) {
      size_t idx = (activityHead_ + MAX_ACTIVITIES - 1 - i) % MAX_ACTIVITIES;
      JsonObject a = actArr.add<JsonObject>();
      a["time"] = activities_[idx].time;
      a["msg"] = activities_[idx].msg;
    }

    String out;
    serializeJson(doc, out);
    server_.send(200, "application/json", out);
  }

  void handleVolume() {
    if (!server_.hasArg("plain")) { server_.send(400, "text/plain", "missing body"); return; }
    JsonDocument doc;
    if (deserializeJson(doc, server_.arg("plain"))) { server_.send(400, "text/plain", "bad json"); return; }
    float v = doc["volume"] | -1.0f;
    if (v >= 0.0f && v <= 1.0f) {
      speaker.setVolume(v);
      logActivity("Speaker volume set to " + String(static_cast<int>(v * 100)) + "%");
      server_.send(200, "application/json", "{\"ok\":true}");
    } else {
      server_.send(400, "text/plain", "invalid volume 0..1");
    }
  }

  void handleMicGain() {
    if (!server_.hasArg("plain")) { server_.send(400, "text/plain", "missing body"); return; }
    JsonDocument doc;
    if (deserializeJson(doc, server_.arg("plain"))) { server_.send(400, "text/plain", "bad json"); return; }
    float g = doc["gain"] | 0.0f;
    if (g >= 0.1f && g <= 15.0f) {
      mic.setGain(g);
      logActivity("Mic sensitivity set to " + String(g, 1) + "x");
      server_.send(200, "application/json", "{\"ok\":true}");
    } else {
      server_.send(400, "text/plain", "invalid gain 0.1..15");
    }
  }

  void handleEmotion() {
    if (!server_.hasArg("plain")) { server_.send(400, "text/plain", "missing body"); return; }
    JsonDocument doc;
    if (deserializeJson(doc, server_.arg("plain"))) { server_.send(400, "text/plain", "bad json"); return; }
    const char* name = doc["name"] | "";
    Emotion e;
    if (emotionFromName(name, e)) {
      face.setEmotion(e);
      logActivity("Emotion set to " + String(name));
      server_.send(200, "application/json", "{\"ok\":true}");
    } else {
      server_.send(400, "text/plain", "unknown emotion");
    }
  }

  void handleTouch() {
    if (!server_.hasArg("plain")) { server_.send(400, "text/plain", "missing body"); return; }
    JsonDocument doc;
    if (deserializeJson(doc, server_.arg("plain"))) { server_.send(400, "text/plain", "bad json"); return; }
    String sensor = doc["sensor"] | "head";
    if (sensor == "head") {
      face.setEmotion(Emotion::Happy);
      face.blink();
      logActivity("Simulated Head touch (pet)");
    } else {
      face.setAsleep(!face.asleep());
      logActivity("Simulated Cheek touch (sleep toggle)");
    }
    server_.send(200, "application/json", "{\"ok\":true}");
  }
};
