#include <WebSocketsServer.h>
#include <ArduinoJson.h>
#include <Arduino.h>

#include "websocketd.h"
#include "analog.h"
#include "applausometer.h"
#include "filesystem.h"


WebSocketsServer webSocket(81);

extern SemaphoreHandle_t applause_mutex;
extern Applause dataApplause;
extern Applause stored_counters[DISPLAY_VERY_LAST_COUNTER];

struct web_events_t system_status;
struct web_settings_t system_settings;


static unsigned long lastPush = 0;
static unsigned long lastListPush = 0;

Applause dataApplauseCopy;


/*********************************************************************************************
* @brief  setup_config
*         Initialize web socket
**********************************************************************************************/
void setup_webevents(web_events_t *events, web_settings_t *settings)
{
    events->update = false;
    events->button_reset = false;
    events->level = 0;
    events->peak = 0;
    events->name_updated = false;
    memset(events->participant_name, 0, APPLAUSE_NAME_SIZE);

    if (!loadSettings(settings)) {
        // sensible defaults for demo
        settings->frq = SOUND_BANDPASS_FRQ;
        settings->gain_db = 60;
        settings->duration = 90; //Seconds
    }
    mutexUpdateSettings(&system_settings);
    Serial.printf("Used config: band=%dHz, gain=%ddB, duration=%lus\n", settings->frq, settings->gain_db, settings->duration);
}

static String uptimeString() {
    unsigned long ms = millis();
    unsigned long s = ms / 1000;
    unsigned long m = s / 60; s %= 60;
    unsigned long h = m / 60; m %= 60;
    unsigned long d = h / 24; h %= 24;
    char buf[64];
    snprintf(buf, sizeof(buf), "%lu d, %lu h, %lu m, %lu s", d, h, m, s);
    return String(buf);
}

/*********************************************************************************************
* @brief  Show main data on a small display
*
* @param  main_value Large displayed value (typically final counter of the algorithm) (0.0 ... 1.0)
* @param  value_vol Small displayed value for current volume (0.0 ... 1.0)
* @param  value_peak Small displayed value for peak (0.0 ... 1.0)
* @param  progress_hori progress bar at the bottom line (e.g. for measurement duration)
* @param  progress_vert progress bar at the right side (e.g. to show button pressed time)  
*
**********************************************************************************************/
void webSocketEvent(uint8_t num, WStype_t type, uint8_t *payload, size_t length) {
    switch (type) {
        case WStype_DISCONNECTED:
            Serial.printf("[%u] Disconnected!\n", num);
            break;

        case WStype_CONNECTED: {
            IPAddress ip = webSocket.remoteIP(num);
            Serial.printf("[%u] Connected from %d.%d.%d.%d url: %s\n", num, ip[0], ip[1], ip[2], ip[3], payload);
            ws_update_livedata();
            ws_update_storedrecords();  // Get history during Connect
            ws_update_config();         // Get config during Connect
        } break;

        case WStype_TEXT: {
            StaticJsonDocument<512> doc;
            DeserializationError err = deserializeJson(doc, payload, length);
            if (err) { Serial.println("WS JSON parse error"); return; }

            const char* cmd = doc["command"] | "";
            if (!*cmd) return;

            if (!strcmp(cmd, "reset_peaks"))                //Start new applause measuring
            {
                mutexButtonEvent(true);
                ws_update_livedata();
                updateList(&dataApplause, true, false);
                Serial.printf("Reset by ws!\n");                
                ws_update_storedrecords();
            }
            else if (!strcmp(cmd, "set_config"))           //Configuration is set
            {
                JsonVariant cfg = doc["config"];
                if (cfg.is<JsonObject>()) {
                    if (cfg["frq"]) system_settings.frq = cfg["frq"].as<int>();
                    if (cfg["gain"]) system_settings.gain_db = cfg["gain"].as<int>();
                    if (cfg["duration"]) system_settings.duration = cfg["duration"].as<unsigned long>();
                }
                Serial.printf("Settings update: freq=%dHz, gain=%ddB, duration=%lus\n",
                              system_settings.frq, system_settings.gain_db, system_settings.duration);
                mutexUpdateSettings(&system_settings);
                ws_update_config();
            }
            else if (!strcmp(cmd, "set_name"))          // Receive participant name from browser
            {
                const char* name = doc["name"] | "";
                if (strlen(name) > 0) {
                    if (xSemaphoreTake(applause_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                        strncpy(system_status.participant_name, name, APPLAUSE_NAME_SIZE - 1);
                        system_status.participant_name[APPLAUSE_NAME_SIZE - 1] = '\0';
                        system_status.name_updated = true;
                        xSemaphoreGive(applause_mutex);
                        Serial.printf("Participant name received: %s\n", name);
                    }
                }
                ws_update_livedata();
                ws_update_storedrecords();
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

void ws_update_livedata() {
    StaticJsonDocument<512> doc;
    doc["level"] = dataApplauseCopy.finalVolume * 100;
    doc["peak"] = dataApplauseCopy.finalPeak * 100;
    doc["uptime"] = uptimeString();

    // Name logic
    if (strlen(system_status.participant_name) > 0) {
        doc["name"] = system_status.participant_name;
    } else if (strlen(dataApplauseCopy.name) > 0) {
        doc["name"] = dataApplauseCopy.name;
    } else {
        doc["name"] = "---";
    }

    doc["finalVolume"] = dataApplauseCopy.finalVolume * 100;
    doc["finalPeak"] = dataApplauseCopy.finalPeak * 100;
    doc["finalResult"] = dataApplauseCopy.finalResult;
    doc["timebased_measured"] = dataApplauseCopy.timebased_measured_max - dataApplauseCopy.timebased_measured;  // Countdown

    String json;
    serializeJson(doc, json);
    webSocket.broadcastTXT(json);
}

void ws_update_config() {
    StaticJsonDocument<256> doc;
    JsonObject cfg = doc.createNestedObject("cfg");
    cfg["frq"] = system_settings.frq;
    cfg["gain"] = system_settings.gain_db;
    cfg["duration"] = system_settings.duration;

    String json;
    serializeJson(doc, json);
    webSocket.broadcastTXT(json);
}

void ws_update_storedrecords() {
    StaticJsonDocument<2048> doc;
    JsonArray arr = doc.createNestedArray("records"); 

    for (size_t i = 0; i < DISPLAY_VERY_LAST_COUNTER; i++) {
        JsonObject o = arr.createNestedObject();
        o["name"] = stored_counters[i].name;
        o["finalPeak"] = stored_counters[i].finalPeak*100;
        o["finalResult"] = stored_counters[i].finalResult;
    }

    String json;
    serializeJson(doc, json);
    webSocket.broadcastTXT(json);
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
        mutexCopySoundData(&dataApplauseCopy); // Lets use a copy to minimize mutex operations
        ws_update_livedata();
    }
    if (now - lastListPush >= websocket_update_interval*10-7) {
        lastListPush = now;
        //MutexCopySoundData(&dataApplauseCopy); // Lets use a copy to minimize mutex operations
        ws_update_storedrecords();
    }}
