#include <WiFi.h>
#include <WiFiClient.h>
#include <ESPmDNS.h>
#include "fs_compat.h"

#include "httpd.h"
#include "websocketd.h"
#include "ota.h"
#define STATUS_INIT
#include "status.h"
#undef STATUS_INIT

// ----------------------- WIFI credentials -----------------------
// #ifndef wird gebraucht um SSID und passwort auch über Umgebungsvariablen zu setzen
#ifndef WIFI_SSID
#define WIFI_SSID "FallbackSSID"
#endif

#ifndef WIFI_PASSWORD
#define WIFI_PASSWORD "FallbackPassword"
#endif

// ----------------------- WIFI credentials -----------------------

struct status_t system_status;

void setup()
{
  init_status();

  Serial.begin(115200);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }
  Serial.println("Connected to WiFi!");

  // Print the IP address
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  // Set hostname
  WiFi.setHostname("websocketdemo");

  // Set mDNS hostname
  if (!MDNS.begin("websocketdemo"))
  {
    Serial.println("Error setting mDNS hostname!");
  }
  else
  {
    Serial.println("mDNS hostname set to 'websocketdemo'");
  }

  // dateisystem starten
  if (!FILESYSTEM.begin(true))
  {
    Serial.println("An Error has occurred while mounting filesystem");
    // create filesystem
    if (!FILESYSTEM.format())
    {
      Serial.println("Failed to format filesystem");
    }
    else
    {
      Serial.println("Filesystem formatted successfully");
    }
    if (!FILESYSTEM.begin(true)) {
      Serial.println("Failed to mount filesystem after formatting");
    }
  }

  setup_httpd();
  setup_websocketd();
  setup_ota();
}

void loop()
{
  loop_ota();
  loop_httpd();
  loop_websocketd();
}
