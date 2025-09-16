#include <WebSocketsServer.h>
#include <ArduinoJson.h>
#include <Arduino.h>

#include "websocketd.h"
#include "status.h"

WebSocketsServer webSocket(81);

// Simple demo signal generator (smooth random walk to target)
static unsigned long lastGen = 0;
static unsigned long lastPush = 0;
static int targetLevel = 20;

static void stepGenerator() {
  unsigned long now = millis();
  // choose new target every ~1.2s
  static unsigned long lastTarget = 0;
  if (now - lastTarget > 1200) {
    targetLevel = random(5, 100);
    lastTarget = now;
  }
  // move current level slowly towards target
  if (now - lastGen >= 40) {
    lastGen = now;
    int cur = system_status.level;
    int diff = targetLevel - cur;
    cur += constrain(diff / 5, -3, 3); // smooth approach
    cur = constrain(cur + (int)random(-1, 2), 0, 100); // small jitter
    system_status.level = cur;
    if (cur > system_status.peak) system_status.peak = cur;
  }
}

static String uptimeString() {
  unsigned long ms = millis();
  unsigned long s = ms / 1000;
  unsigned long m = s / 60;    s %= 60;
  unsigned long h = m / 60;    m %= 60;
  unsigned long d = h / 24;    h %= 24;
  char buf[64];
  snprintf(buf, sizeof(buf), "%lu d, %lu h, %lu m, %lu s", d, h, m, s);
  return String(buf);
}

void webSocketEvent(uint8_t num, WStype_t type, uint8_t *payload, size_t length) {
  switch (type) {
    case WStype_DISCONNECTED:
      Serial.printf("[%u] Disconnected!\n", num);
      break;
    case WStype_CONNECTED: {
      IPAddress ip = webSocket.remoteIP(num);
      Serial.printf("[%u] Connected from %d.%d.%d.%d url: %s\n", num, ip[0], ip[1], ip[2], ip[3], payload);
      websocket_update();
    } break;
    case WStype_TEXT: {
      StaticJsonDocument<512> doc;
      DeserializationError err = deserializeJson(doc, payload, length);
      if (err) { Serial.println("WS JSON parse error"); return; }
      const char* cmd = doc["command"] | "";
      if (!*cmd) return;

      if (!strcmp(cmd, "reset_peaks")) {
        system_status.peak = 0;
        websocket_update();
      } else if (!strcmp(cmd, "set_config")) {
        JsonVariant cfg = doc["config"];
        if (cfg.is<JsonObject>()) {
          if (cfg["f1"]) system_status.f1 = cfg["f1"].as<int>();
          if (cfg["f2"]) system_status.f2 = cfg["f2"].as<int>();
          if (cfg["gain"]) system_status.gain_db = cfg["gain"].as<int>();
          if (cfg["durationMs"]) system_status.duration_ms = cfg["durationMs"].as<unsigned long>();
        }
        websocket_update();
      } else {
        // unknown commands are ignored for the demo
      }
    } break;
    case WStype_BIN:
    default:
      break;
  }
}

void websocket_update() {
  StaticJsonDocument<512> doc;
  doc["level"] = system_status.level;
  doc["peak"] = system_status.peak;
  doc["uptime"] = uptimeString();

  JsonObject cfg = doc.createNestedObject("cfg");
  cfg["f1"] = system_status.f1;
  cfg["f2"] = system_status.f2;
  cfg["gain"] = system_status.gain_db;
  cfg["durationMs"] = system_status.duration_ms;

  String out;
  serializeJson(doc, out);
  webSocket.broadcastTXT(out);
}

void setup_websocketd() {
  webSocket.begin();
  webSocket.onEvent(webSocketEvent);
}

void loop_websocketd() {
  webSocket.loop();

  // generate demo signal frequently
  stepGenerator();

  unsigned long now = millis();
  if (now - lastPush >= websocket_update_interval) {
    lastPush = now;
    websocket_update();
  }
}
