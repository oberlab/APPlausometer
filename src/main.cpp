#include <WiFi.h>
//#include <WiFiClient.h>
#include <ESPmDNS.h>

#include "filesystem.h"
#include "applausometer.h"


#include "httpd.h"
#include "websocketd.h"
#include "ota.h"


// =================================
// WIFI credentials
// =================================
// #ifndef is used for SSID and password definition via environment variables
#ifndef WIFI_SSID
#define WIFI_SSID "FallbackSSID"
#endif

#ifndef WIFI_PASSWORD
#define WIFI_PASSWORD "FallbackPassword"
#endif


// Hotspot settings
#define AP_SSID "APPlausometer"
#ifndef WIFI_SSID
#define AP_PASSWORD "APPlaus!1234"
#endif

#define MAX_CLIENTS 6

// Eigene AP-Netzwerk-Config
IPAddress local_IP(192, 168, 4, 1);
IPAddress gateway(192, 168, 4, 1);
IPAddress subnet(255, 255, 255, 0);


// =================================
// WIFI credentials
// =================================
extern bool ota_running_flag;
extern web_events_t system_status;
extern web_settings_t system_settings;

SemaphoreHandle_t applause_mutex;


TaskHandle_t task_cpu0;

void cpu0_loop(void * parameter) {
  static unsigned long tMonitorStack;

  for(;;) 
  {
    if(!ota_running_flag){
      loop_applausometer();
    }
    // Check remaining stack space
    UBaseType_t stackHighWaterMark = uxTaskGetStackHighWaterMark(NULL);
    const unsigned long now = millis();
    if (now - tMonitorStack > 1000) {
      tMonitorStack = now;
      if (stackHighWaterMark < 1024) {
        Serial.printf("CPU0 task stack remaining: %d words\n", stackHighWaterMark);  
      }
    }   
    vTaskDelay(1);
  }
}


void setup()
{
  Serial.begin(115200);

  // =================================
  // Mutex for cpu access management
  // =================================
  applause_mutex = xSemaphoreCreateMutex();
  if (applause_mutex == NULL) {
      Serial.println("Failed to create mutex!");  //Should never be!
  }


  // =================================
  // =   Initialize file system
  // ================================= 
  fs_mount(true);


  // =================================
  // =   Initialize Applausometer
  // ================================= 
  
  setup_webevents(&system_status, &system_settings);
  
  setup_applausometer();


  // =================================
  // =   Activate CPU0 for 
  // =   Applausometer procedures
  // =================================  
  xTaskCreatePinnedToCore(
      cpu0_loop,   // Taskfunktion
      "cpu0",      // Name
      4096,       // Stackgröße (Bytes / 4 = Words)
      NULL,        // Parameter
      1,           // Priorität (klein halten!)
      &task_cpu0,  // Handle
      0);          // Core 0


  // =================================
  // =   Search Wifi and activate AP
  // =================================  
  WiFi.mode(WIFI_AP_STA);
  char ssid[100] = WIFI_SSID;
  char pw[100] = WIFI_PASSWORD;
  loadWifi(ssid, pw, sizeof(ssid), sizeof(pw));   // Insecure feature! Do you really want to read a plain file with a password?
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);           // Credentials for infrasstructure mode

  unsigned long startAttemptTime = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < 3000) {
    delay(200);
    Serial.print(".");
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWifi connected!");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\nNo Wifi found, continue with AP only.");
  }

  // Start Hotspot in any case
  if (!WiFi.softAPConfig(local_IP, gateway, subnet)) {
    Serial.println("AP Config Failed!");
  }
  if (WiFi.softAP(AP_SSID, AP_PASSWORD, 1, 0, MAX_CLIENTS)) {
    Serial.print("AP started. IP: ");
    Serial.println(WiFi.softAPIP());
  } else {
    Serial.println("AP start failed!");
  }

  
  // =================================
  // =   Initialize web services
  // =================================   
  setup_httpd();
  setup_websocketd();
  setup_ota();
}


void loop()
{
  static unsigned long tMonitorStack;

  loop_ota();
  loop_httpd();
  loop_websocketd();

  // Check remaining stack space
  UBaseType_t stackHighWaterMark = uxTaskGetStackHighWaterMark(NULL);
  const unsigned long now = millis();
  if (now - tMonitorStack > 1000) {
    tMonitorStack = now;
    if (stackHighWaterMark < 1024) {
      Serial.printf("CPU1 task stack remaining: %d words\n", stackHighWaterMark);  
    }
  }
}

