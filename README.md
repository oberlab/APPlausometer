![APPlausumeter](/public/clap.png)

** APPlausometer **

> [https://wiki.oberlab.de](https://wiki.oberlab.de/books/verschiedene-projekte/page/applausometer)

# APPlausometer

Das Gemeinschaftsprojekt wurde realisiert, um beispielsweise für einen Poetry Slam eine Applausmessung zu ermöglichen.

Zum Einsatz kamen verschiedenste Techniken, wie Softwareentwicklung, Designentwicklung, mechanische Herausforderungen, Usability... 


Das Softwarepaket diente auch als Lernprojekt zum Einsatz von **ESP32**, **WebSockets** und **PlatformIO**. In der Software war das Ziel, eine bidirektionale Kommunikation zwischen einem ESP32 und einem Webbrowser zu ermöglichen – ideal für Sensorik, Debugging, Steuerung oder einfache Dashboards.

## Begriffe
- **webserver** (httpd) Bietet Dateien über http an
- **WebSocket:** Protokoll für dauerhafte, bidirektionale Verbindungen
- **SPIFFS:** Dateisystem im Flash-Speicher des ESP32
- **OTA:** Over-the-Air Firmware Updates
- **PlatformIO:** Cross-Plattform Entwicklungsumgebung für Embedded-Projekte

## Toolchain
### vscode / platformIO

Wir verwenden [Visual Studio Code](https://code.visualstudio.com/) mit [PlatformIO](https://platformio.org) als Entwicklungsumgebung.

**Installation:**
1. Installiere [VSCode](https://code.visualstudio.com/)
2. Installiere über den Extension Manager das Plugin "PlatformIO IDE"  
   → Anleitung: [platformio.org/install/ide?install=vscode](https://platformio.org/install/ide?install=vscode)

**Vorteile von PlatformIO:**
- Toolchain, Framework und Libraries sind versionierbar
- Einfache Board-Auswahl und Flash-Vorgänge
- Guter serieller Monitor und Build-Tools

### git
Dieses Projekt ist mit git versioniert. Um es direkt auf den Rechner zu clonen kann folgender call gemacht werden: `git clone https://github.com/maxpautsch/esp_websocket_demo.git`. Alternativ kann das Repository auch als ZIP heruntergeladen und entpackt werden.

## Projekt-Verzeichnise und Dateien

- `data/` – Statische HTML-Dateien für das ESP-Dateisystem (SPIFFS) -> Alles in diesem Ordner wird 1:1 in das Dateisystem des ESP übernommen
- `include/` – Header-Dateien für den code unter src.
- `lib/` – Externe Bibliotheken (falls benötigt)
- `src/` – c / cpp -Code, z. B. `main.cpp`
- `platformio.ini` – Board-/Toolchain-Konfiguration
- `.gitignore` – Regeln um Dateien im Git-Kontext zu ignorieren
- `README.md` – Diese Projektbeschreibung



## WIFI credentials:
Wifi credentials können über die #defines `WIFI_SSDI`und `WIFI_PASSWORDWIFI_PASSWOR` in `main.cpp` vorgegeben werden. 
### Credentials über Umgebungsvariable
Bei öffentlichen Projekten will man seine WiFi Credentials natürlich nicht direkt im Source Code haben, sie aber auch nicht jedes mal nach einem `git pull` oder vor einem `git commit` ersetzen. Um das zu umgeben, können mit Compile-Flags in `platformio.ini` die defines auf die entsprechenden System-Umgebungsvariablen gesetzt werden: 
```
    -DWIFI_SSID=\"${sysenv.WIFI_SSID}\"
    -DWIFI_PASSWORD=\"${sysenv.WIFI_PASSWORD}\"
```
Diese Umgebungsvariablen können beispielsweise in der `.bashrc` so gesetzt werden:
```bash
export WIFI_SSID="DeinNetzwerkName"
export WIFI_PASSWORD="DeinPasswort"
``` 

## OTA
Die IP des ESP muss in `platformio.ini` gesetzt werden. Alternativ kann auch der domain-Name verwendet werden. Entweder `websocketdemo.local` oder was auch immer in eurem Router eingestellt ist.

## Arduino Nano ESP32

Dieses Projekt ist auch für das Board „Arduino Nano ESP32“ vorbereitet. Wähle dazu die Umgebung `arduino-nano-esp32` in PlatformIO aus.

- Firmware flashen (USB):
  - `pio run -t upload -e arduino-nano-esp32`
- Dateisystem (SPIFFS) hochladen aus `data/`:
  - `pio run -t uploadfs -e arduino-nano-esp32`
- OTA (optional): Setze `upload_port` in `env:arduino-nano-esp32-ota` und nutze
  - `pio run -t upload -e arduino-nano-esp32-ota`
  - `pio run -t uploadfs -e arduino-nano-esp32-ota`

Hinweis: Das Projekt verwendet SPIFFS. Die `data/`-Dateien werden 1:1 in das Dateisystem des ESP übernommen. 

Weitere Details werden gerne zur Verfügung gestellt: Kontakt 