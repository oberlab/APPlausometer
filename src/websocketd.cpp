#include <WebSocketsServer.h>
#include <ArduinoJson.h>
#include <Arduino.h>

#include "websocketd.h"
#include "status.h"
#include "analog.h"
#include "filesystem.h"


WebSocketsServer webSocket(81);

extern SemaphoreHandle_t applause_mutex;

struct web_events_t system_status;
struct web_settings_t system_settings;


static unsigned long lastPush = 0;

Applause dataApplauseCopy;



// ToDo: Is this function in the correct module?
void setup_config(web_events_t *events, web_settings_t *settings)
{
  events->update = false;
  events->button_reset = false;
  
  events->level = 0;
  events->peak = 0;

  if (!loadSettings(settings)) {
    // sensible defaults for demo
    settings->frq = SOUND_BANDPASS_FRQ;
    settings->gain_db = 60;
    settings->duration = 90; //Seconds
  }
  Serial.printf("Used config: %d, %d, %lu", settings->frq, settings->gain_db, settings->duration);
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

      if (!strcmp(cmd, "reset_peaks")) 
      {
        MutexButtonEvent(true);

        websocket_update();
      } 
      else if (!strcmp(cmd, "set_config")) 
      {
        JsonVariant cfg = doc["config"];
        
        if (cfg.is<JsonObject>()) {
          if (cfg["frq"]) system_settings.frq = cfg["frq"].as<int>();
          if (cfg["gain"]) system_settings.gain_db = cfg["gain"].as<int>();
          if (cfg["duration"]) system_settings.duration = cfg["duration"].as<unsigned long>();
        }
        Serial.printf("Band pass settings http update: freq=%d Hz, gain=%d, duration=%lus\n", system_settings.frq, system_settings.gain_db, system_settings.duration);
        MutexUpdateSettings(&system_settings);

        saveSettings(cfg);

        websocket_update();
      } 
      else 
      {
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
  doc["level"] = dataApplauseCopy.finalVolume * 100;
  doc["peak"]  = dataApplauseCopy.finalPeak * 100;
  doc["uptime"] = uptimeString();                       // ToDo unnötig, aber andere Zeit für Messdauer einfügen

  JsonObject cfg = doc.createNestedObject("cfg");
  cfg["frq"] = system_settings.frq;
  cfg["gain"] = system_settings.gain_db;
  cfg["duration"] = system_settings.duration;

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

  unsigned long now = millis();
  if (now - lastPush >= websocket_update_interval) {
    lastPush = now;

    MutexCopySoundData(&dataApplauseCopy); // Lets use a copy to minimize mutex operations
   
    websocket_update();
  }
}
