// Applausometer GUI for ESP32-2432S028
#include <Arduino.h>
#include "lgfx_2432s028.hpp"
#include <LittleFS.h>
#include <Preferences.h>
#include <WiFi.h>

LGFX lcd; // Global display instance

// ADC input for applause (adjust if needed)
#ifndef APPLAUSE_ADC_PIN
#define APPLAUSE_ADC_PIN 34
#endif

// Display transform state
static uint8_t rotation = 3;     // default landscape
static bool mirrorX = false;     // toggle horizontal mirror

// Calibration and measurement state
static bool running = false;
static bool haveQuiet = false;
static bool haveLoud = false;
static uint16_t calQuiet = 100;   // raw baseline
static uint16_t calLoud  = 2000;  // raw loud ref
static float ema = 0.0f;          // exponential moving average of ADC
static float alpha = 0.2f;        // smoothing factor
static float currentNorm = 0.0f;  // 0..1 normalized
static float maxHold = 0.0f;      // 0..1 max until reset

// UI layout constants
static int titleH = 60;          // fixed title height for logo on black
static const int MARGIN = 8;

struct Button { int x, y, w, h; const char* label; uint16_t bg; uint16_t fg; };
static Button btnLeise, btnLaut, btnStart, btnReset;
static Button btnHomeCal, btnHomeMeasure, btnHomeWifi;

struct Rect { int x, y, w, h; };
static Rect barRect; // right-side vertical bar
static Rect backRect; // back arrow area (on CAL/MEASURE)
static int lastFillPx = -1;
static int lastMaxPx  = -1;
static int lastPct    = -1;
static Preferences prefs;

enum Page { PAGE_HOME, PAGE_CAL, PAGE_MEASURE, PAGE_WIFI };
static Page currentPage = PAGE_HOME;

// WiFi settings
static bool wifiApMode = true; // true=AP, false=Client
static String wifiSsid = "";
static String wifiPass = "";

// WiFi page UI
static Button btnWifiAP, btnWifiSTA, btnWifiSEL, btnWifiPASS;

// Simple input overlay state
enum InputTarget { INPUT_NONE, INPUT_SSID, INPUT_PASS };
static InputTarget inputTarget = INPUT_NONE;
static bool inputActive = false;
static String inputBuffer = "";
// WiFi runtime state
enum WifiConnState { WIFI_STATE_IDLE, WIFI_STATE_AP_RUNNING, WIFI_STATE_STA_CONNECTING, WIFI_STATE_STA_CONNECTED, WIFI_STATE_STA_FAILED };
static WifiConnState wifiState = WIFI_STATE_IDLE;
static uint32_t wifiLastChange = 0;
static IPAddress wifiIP;

static void wifiStopAll()
{
  WiFi.softAPdisconnect(true);
  WiFi.disconnect(true);
  wifiState = WIFI_STATE_IDLE;
}

static String defaultApSsid()
{
  return String("applausometer");
}

static void wifiStartAp()
{
  wifiStopAll();
  WiFi.mode(WIFI_AP);
  String ssid = defaultApSsid();
  bool ok = false;
  if (wifiPass.length() >= 8) ok = WiFi.softAP(ssid.c_str(), wifiPass.c_str());
  else ok = WiFi.softAP(ssid.c_str());
  wifiIP = WiFi.softAPIP();
  wifiState = ok ? WIFI_STATE_AP_RUNNING : WIFI_STATE_STA_FAILED;
  wifiLastChange = millis();
}

static void wifiStartSta()
{
  wifiStopAll();
  if (!wifiSsid.length()) { wifiState = WIFI_STATE_STA_FAILED; return; }
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(50);
  WiFi.begin(wifiSsid.c_str(), wifiPass.c_str());
  wifiState = WIFI_STATE_STA_CONNECTING;
  wifiLastChange = millis();
}

static void drawWifiInfo()
{
  // Draw status text on the right side area
  int rightX = barRect.x;
  int rightW = barRect.w;
  int top = titleH + MARGIN;
  int h = lcd.height() - top - MARGIN;
  lcd.fillRect(rightX, top, rightW, h, lgfx::color888(0,0,0));
  lcd.setTextDatum(lgfx::top_left);
  lcd.setFont(&fonts::Font2);
  lcd.setTextColor(lgfx::color888(200,200,200), lgfx::color888(0,0,0));
  String mode = wifiApMode ? "Modus: AP" : "Modus: Client";
  lcd.drawString(mode, rightX, top);
  if (wifiApMode) {
    lcd.drawString(String("SSID: ") + defaultApSsid(), rightX, top + 18);
    if (wifiState == WIFI_STATE_AP_RUNNING) {
      char ipbuf[32]; snprintf(ipbuf, sizeof(ipbuf), "IP: %s", wifiIP.toString().c_str());
      lcd.drawString(ipbuf, rightX, top + 36);
    } else {
      lcd.drawString("AP gestoppt", rightX, top + 36);
    }
  } else {
    lcd.drawString(String("SSID: ") + (wifiSsid.length()?wifiSsid:"<keine>"), rightX, top + 18);
    if (wifiState == WIFI_STATE_STA_CONNECTING) lcd.drawString("Verbinde...", rightX, top + 36);
    else if (wifiState == WIFI_STATE_STA_CONNECTED) {
      char ipbuf[32]; snprintf(ipbuf, sizeof(ipbuf), "Verbunden: %s", WiFi.localIP().toString().c_str());
      lcd.drawString(ipbuf, rightX, top + 36);
    } else if (wifiState == WIFI_STATE_STA_FAILED) lcd.drawString("Fehlgeschlagen", rightX, top + 36);
    else lcd.drawString("Bereit", rightX, top + 36);
  }
}

static void updateWifi()
{
  if (wifiApMode) return;
  if (wifiState == WIFI_STATE_STA_CONNECTING) {
    wl_status_t st = WiFi.status();
    if (st == WL_CONNECTED) {
      wifiState = WIFI_STATE_STA_CONNECTED;
    } else if (millis() - wifiLastChange > 15000) {
      wifiState = WIFI_STATE_STA_FAILED;
    }
  }
}
static int logoX = 6, logoY = 4; // position within title, on black
static void drawLogoIfAny()
{
  if (!LittleFS.exists("/logo.png")) return;
  File f = LittleFS.open("/logo.png", "r");
  if (!f) return;
  lgfx::v1::StreamWrapper stream;
  stream.set(&f, f.size());
  lcd.drawPng(&stream, logoX, logoY);
  f.close();
}

static void drawTitle(const char* status)
{
  // black background to suit the logo
  lcd.fillRect(0, 0, lcd.width(), titleH, lgfx::color888(0, 0, 0));
  // status on right
  lcd.setTextDatum(lgfx::top_right);
  uint32_t statusCol = running ? lgfx::color888(0, 190, 120) : lgfx::color888(180, 180, 180);
  lcd.setTextColor(statusCol, lgfx::color888(0,0,0));
  lcd.setFont(&fonts::Font2);
  lcd.drawString(status, lcd.width()-6, 6);
}

static void computeLayoutCommon()
{
  int W = lcd.width();
  int H = lcd.height();
  int contentTop = titleH + MARGIN;
  int contentBottom = H - MARGIN;

  // Left column width ~ 40% of screen, min 100px
  int leftW = W * 2 / 5;
  if (leftW < 100) leftW = 100;

  int availH = contentBottom - contentTop;

  // Right bar column uses remaining width
  int rightX = leftW + MARGIN;
  int rightW = W - rightX - MARGIN;
  // Slim bar ~ 36-48 px
  int barW = rightW > 48 ? 36 : rightW;
  int barH = availH - 40; // leave space for value text
  int barY = contentTop + 40;
  int barX = rightX + (rightW - barW)/2;
  barRect = {barX, barY, barW, barH};
}

static void layoutHome()
{
  int W = lcd.width(); int H = lcd.height();
  int contentTop = titleH + MARGIN; int contentBottom = H - MARGIN;
  int leftW = W * 2 / 5; if (leftW < 100) leftW = 100;
  int x = MARGIN; int w = leftW - 2*MARGIN; int availH = contentBottom - contentTop;
  // Similar size to Measure page, 3 buttons; push down by one button height
  int reserveBottom = MARGIN; // no back button on home
  int gaps = 5; // top + between*2 + extra push + bottom
  int btnH = (availH - reserveBottom - MARGIN * gaps) / 3; if (btnH > 48) btnH = 48; if (btnH < 36) btnH = 36;
  int y = contentTop + MARGIN + btnH + MARGIN; // push down by one button height
  btnHomeCal     = {x, y, w, btnH, "Kalibrieren", (uint16_t)lgfx::color888(40, 110, 210), (uint16_t)lgfx::color888(255,255,255)}; y += btnH + MARGIN;
  btnHomeMeasure = {x, y, w, btnH, "Messung",     (uint16_t)lgfx::color888(0, 140, 110),  (uint16_t)lgfx::color888(255,255,255)}; y += btnH + MARGIN;
  btnHomeWifi    = {x, y, w, btnH, "WiFi",        (uint16_t)lgfx::color888(120, 120, 120), (uint16_t)lgfx::color888(255,255,255)};
}

static void layoutCal()
{
  int W = lcd.width(); int H = lcd.height();
  int contentTop = titleH + MARGIN; int contentBottom = H - MARGIN;
  int leftW = W * 2 / 5; if (leftW < 100) leftW = 100;
  int x = MARGIN; int w = leftW - 2*MARGIN; int availH = contentBottom - contentTop;
  // Reserve space for back button area at bottom
  int reserveBottom = 28 + MARGIN * 2;
  int usableH = availH - reserveBottom;
  int gaps = 3; int btnH = (usableH - MARGIN * (gaps+1)) / 2; if (btnH > 48) btnH = 48; if (btnH < 36) btnH = 36;
  int y = contentTop + MARGIN;
  btnLeise = {x, y, w, btnH, "Leise", (uint16_t)lgfx::color888(60, 80, 120), (uint16_t)lgfx::color888(255,255,255)}; y += btnH + MARGIN;
  btnLaut  = {x, y, w, btnH, "Laut",  (uint16_t)lgfx::color888(40, 110, 210), (uint16_t)lgfx::color888(255,255,255)};
  backRect = { MARGIN, H - (MARGIN + 28), 28, 28 };
}

static void layoutMeasure()
{
  int W = lcd.width(); int H = lcd.height();
  int contentTop = titleH + MARGIN; int contentBottom = H - MARGIN;
  int leftW = W * 2 / 5; if (leftW < 100) leftW = 100;
  int x = MARGIN; int w = leftW - 2*MARGIN; int availH = contentBottom - contentTop;
  int reserveBottom = 28 + MARGIN * 2;
  int usableH = availH - reserveBottom;
  int gaps = 3; int btnH = (usableH - MARGIN * (gaps+1)) / 2; if (btnH > 48) btnH = 48; if (btnH < 36) btnH = 36;
  int y = contentTop + MARGIN;
  btnStart = {x, y, w, btnH, "Start", (uint16_t)lgfx::color888(0, 140, 110),  (uint16_t)lgfx::color888(255,255,255)}; y += btnH + MARGIN;
  btnReset = {x, y, w, btnH, "Reset Max", (uint16_t)lgfx::color888(180, 40, 40), (uint16_t)lgfx::color888(255,255,255)};
  backRect = { MARGIN, H - (MARGIN + 28), 28, 28 };
}

static void layoutWifi()
{
  int W = lcd.width(); int H = lcd.height();
  int contentTop = titleH + MARGIN; int contentBottom = H - MARGIN;
  int leftW = W * 2 / 5; if (leftW < 100) leftW = 100;
  int x = MARGIN; int w = leftW - 2*MARGIN; int availH = contentBottom - contentTop;
  int reserveBottom = 28 + MARGIN * 2;
  int usableH = availH - reserveBottom;
  // 4 buttons stacked: AP, Client, SSID, Passwort
  int gaps = 6;
  int btnH = (usableH - MARGIN * (gaps+1)) / 4; if (btnH > 44) btnH = 44; if (btnH < 34) btnH = 34;
  int y = contentTop + MARGIN + MARGIN; // small extra offset
  btnWifiAP   = {x, y, w, btnH, "AP-Modus",     (uint16_t)lgfx::color888(40, 110, 210), (uint16_t)lgfx::color888(255,255,255)}; y += btnH + MARGIN;
  btnWifiSTA  = {x, y, w, btnH, "Client-Modus",(uint16_t)lgfx::color888(0, 140, 110),  (uint16_t)lgfx::color888(255,255,255)}; y += btnH + MARGIN*2;
  btnWifiSEL  = {x, y, w, btnH, "Select WiFi",  (uint16_t)lgfx::color888(60, 80, 120),  (uint16_t)lgfx::color888(255,255,255)}; y += btnH + MARGIN;
  btnWifiPASS = {x, y, w, btnH, "Passwort",     (uint16_t)lgfx::color888(120, 120, 120),(uint16_t)lgfx::color888(255,255,255)};
  backRect = { MARGIN, H - (MARGIN + 28), 28, 28 };
}

static void drawButton(const Button& b, bool active=false)
{
  uint16_t bg = active ? lgfx::color888(200,200,200) : b.bg;
  lcd.fillRoundRect(b.x, b.y, b.w, b.h, 6, bg);
  lcd.drawRoundRect(b.x, b.y, b.w, b.h, 6, lgfx::color888(80,80,80));
  lcd.setTextDatum(lgfx::middle_center);
  lcd.setTextColor(b.fg, bg);
  lcd.setFont(&fonts::Font2);
  lcd.drawString(b.label, b.x + b.w/2, b.y + b.h/2);
}

static void drawButtonsHome()
{
  drawButton(btnHomeCal);
  drawButton(btnHomeMeasure);
  drawButton(btnHomeWifi);
}

static void drawButtonsCal()
{
  drawButton(btnLeise);
  drawButton(btnLaut);
  // show current cal values under each button
  lcd.setTextDatum(lgfx::top_left);
  lcd.setFont(&fonts::Font2);
  lcd.setTextColor(lgfx::color888(200,200,200), lgfx::color888(0,0,0));
  char b1[24]; snprintf(b1, sizeof(b1), "= %u", (unsigned)calQuiet);
  char b2[24]; snprintf(b2, sizeof(b2), "= %u", (unsigned)calLoud);
  lcd.drawString(b1, btnLeise.x, btnLeise.y + btnLeise.h + 2);
  lcd.drawString(b2, btnLaut.x,  btnLaut.y  + btnLaut.h  + 2);
}

static void drawButtonsMeasure()
{
  btnStart.label = running ? "Stopp" : "Start";
  btnStart.bg = running ? (uint16_t)lgfx::color888(220, 140, 0) : (uint16_t)lgfx::color888(0, 140, 110);
  drawButton(btnStart);
  drawButton(btnReset);
}

static void drawButtonsWifi()
{
  // Indicate selected mode by slightly lighter bg
  drawButton(btnWifiAP,  wifiApMode);
  drawButton(btnWifiSTA, !wifiApMode);
  drawButton(btnWifiSEL);
  // Draw current SSID below Select button
  lcd.setTextDatum(lgfx::middle_left);
  lcd.setFont(&fonts::Font2);
  lcd.setTextColor(lgfx::color888(200,200,200), lgfx::color888(0,0,0));
  String ss = wifiSsid.length() ? wifiSsid : String("<keine SSID>");
  lcd.drawString(String("SSID: ") + ss, btnWifiSEL.x, btnWifiSEL.y + btnWifiSEL.h + 14);
  drawButton(btnWifiPASS);
  lcd.setTextColor(lgfx::color888(255,255,255), btnWifiPASS.bg);
  String pwMask = wifiPass.length() ? String("") : String("<Passwort>");
  if (wifiPass.length()) { for (size_t i=0; i<wifiPass.length() && i<24; ++i) pwMask += '*'; }
  lcd.drawString(pwMask, btnWifiPASS.x + 8, btnWifiPASS.y + btnWifiPASS.h/2);
}

static inline float clamp01(float v) { return v < 0 ? 0 : (v > 1 ? 1 : v); }

static void drawBarFrame()
{
  // frame and background drawn once; keep inner area for fills
  lcd.fillRoundRect(barRect.x, barRect.y, barRect.w, barRect.h, 8, lgfx::color888(20,22,26));
  lcd.drawRoundRect(barRect.x, barRect.y, barRect.w, barRect.h, 8, lgfx::color888(70,76,84));
  lastFillPx = -1;
  lastMaxPx  = -1;
}

static void drawPercent()
{
  int pct = (int)roundf(currentNorm * 100.0f);
  if (pct == lastPct) return;
  lastPct = pct;
  // Centered above bar
  lcd.setTextDatum(lgfx::bottom_center);
  lcd.setFont(&fonts::Font2); // same font as buttons
  lcd.setTextColor(lgfx::color888(230,230,230), lgfx::color888(0,0,0));
  char buf[16]; snprintf(buf, sizeof(buf), "%3d%%", pct);
  lcd.drawString(buf, barRect.x + barRect.w/2, barRect.y - 4);
}

static void updateBar()
{
  // Inner rect
  int ix = barRect.x + 2;
  int iy = barRect.y + 2;
  int iw = barRect.w - 4;
  int ih = barRect.h - 4;
  int fillPx = (int)(clamp01(currentNorm) * ih);
  int baseY = iy + ih - 1; // bottom

  if (lastFillPx < 0) {
    // initial draw
    if (fillPx > 0) {
      // gradient-ish color from cyan->orange
      uint8_t r = (uint8_t)(30 + 225 * currentNorm);
      uint8_t g = (uint8_t)(160 - 80 * currentNorm);
      uint8_t b = (uint8_t)(220 - 220 * currentNorm);
      lcd.fillRect(ix, baseY - fillPx + 1, iw, fillPx, lgfx::color888(r,g,b));
    }
  } else if (fillPx != lastFillPx) {
    if (fillPx > lastFillPx) {
      // grow: draw only new segment
      int add = fillPx - lastFillPx;
      uint8_t r = (uint8_t)(30 + 225 * currentNorm);
      uint8_t g = (uint8_t)(160 - 80 * currentNorm);
      uint8_t b = (uint8_t)(220 - 220 * currentNorm);
      lcd.fillRect(ix, baseY - fillPx + 1, iw, add, lgfx::color888(r,g,b));
    } else {
      // shrink: erase the removed segment
      int rem = lastFillPx - fillPx;
      lcd.fillRect(ix, baseY - lastFillPx + 1, iw, rem, lgfx::color888(20,22,26));
    }
  }
  lastFillPx = fillPx;

  // Max-hold marker
  int maxPx = (int)(clamp01(maxHold) * ih);
  if (maxPx != lastMaxPx) {
    // erase previous
    if (lastMaxPx >= 0) {
      int y = baseY - lastMaxPx + 1;
      lcd.drawFastHLine(ix, y, iw, lgfx::color888(20,22,26));
    }
    // draw new
    int y = baseY - maxPx + 1;
    lcd.drawFastHLine(ix, y, iw, lgfx::color888(255,200,60));
    lastMaxPx = maxPx;
  }
}

static void drawCalInfo()
{
  // Draw calibration values bottom-left
  int y = lcd.height() - 16 - MARGIN;
  lcd.fillRect(MARGIN, y, lcd.width()/2, 16, lgfx::color888(0,0,0));
  lcd.setTextDatum(lgfx::top_left);
  lcd.setFont(&fonts::Font2);
  lcd.setTextColor(lgfx::color888(180, 180, 180), lgfx::color888(0,0,0));
  char info[48];
  snprintf(info, sizeof(info), "Leise:%u  Laut:%u", (unsigned)calQuiet, (unsigned)calLoud);
  lcd.drawString(info, MARGIN, y);
}

static void drawBack()
{
  uint16_t bg = lgfx::color888(35, 38, 44);
  lcd.fillRoundRect(backRect.x, backRect.y, backRect.w, backRect.h, 4, bg);
  lcd.drawRoundRect(backRect.x, backRect.y, backRect.w, backRect.h, 4, lgfx::color888(90,96,106));
  int cx = backRect.x + backRect.w/2; int cy = backRect.y + backRect.h/2;
  lcd.drawLine(cx+4, cy, cx-2, cy, lgfx::color888(230,230,230));
  lcd.drawLine(cx-2, cy, cx+1, cy-3, lgfx::color888(230,230,230));
  lcd.drawLine(cx-2, cy, cx+1, cy+3, lgfx::color888(230,230,230));
}

static void applyDisplayTransform()
{
  lcd.setRotation((rotation & 3) | (mirrorX ? 4 : 0));
  lcd.fillScreen(lgfx::color888(0, 0, 0));
  const char* st = (currentPage == PAGE_MEASURE ? (running ? "RUN" : "STOP") : (currentPage == PAGE_CAL ? "CAL" : (currentPage == PAGE_WIFI ? "WIFI" : "MENU")));
  drawTitle(st);
  computeLayoutCommon();
  if (currentPage == PAGE_HOME) {
    layoutHome();
    drawButtonsHome();
    // Show logo on menu only
    drawLogoIfAny();
  } else if (currentPage == PAGE_CAL) {
    layoutCal();
    drawButtonsCal();
    drawBack();
    drawBarFrame();
    lastPct = -1; drawPercent();
  } else if (currentPage == PAGE_MEASURE) { // measure
    layoutMeasure();
    drawButtonsMeasure();
    drawBack();
    drawBarFrame();
    lastPct = -1; drawPercent();
  } else if (currentPage == PAGE_WIFI) {
    layoutWifi();
    drawButtonsWifi();
    drawBack();
    drawWifiInfo();
  }
}

// Boot screen removed per request; logo shown on HOME menu only.

static void updateAudienceLEDs(float norm)
{
  // Stub: hook up FastLED/NeoPixel later
  (void)norm;
}

static void updateMeasurement()
{
  int raw = analogRead(APPLAUSE_ADC_PIN);
  if (ema == 0.0f) ema = (float)raw;
  ema += alpha * ((float)raw - ema);

  int q = haveQuiet ? (int)calQuiet : (int)ema;
  int l = haveLoud  ? (int)calLoud  : (int)(q + 1000);
  if (l <= q) l = q + 1;
  currentNorm = clamp01(((float)ema - (float)q) / (float)(l - q));
  if (currentNorm > maxHold) maxHold = currentNorm;

  updateAudienceLEDs(currentNorm);
}

static bool pointIn(const Button& b, uint16_t x, uint16_t y)
{ return x >= b.x && x < (b.x + b.w) && y >= b.y && y < (b.y + b.h); }
static bool pointInRect(const Rect& r, uint16_t x, uint16_t y)
{ return x >= r.x && x < (r.x + r.w) && y >= r.y && y < (r.y + r.h); }

void setup()
{
  Serial.begin(115200);
  delay(50);
#if defined(ARDUINO_ARCH_ESP32)
  analogReadResolution(12);
  analogSetPinAttenuation(APPLAUSE_ADC_PIN, ADC_11db);
#endif
  // Mount FS for logo (no format)
  LittleFS.begin(false);

  // Load saved calibration
  prefs.begin("applauso", false);
  calQuiet = prefs.getUShort("calQ", calQuiet);
  calLoud  = prefs.getUShort("calL", calLoud);
  haveQuiet = prefs.getBool("haveQ", false);
  haveLoud  = prefs.getBool("haveL", false);
  // Load WiFi settings
  wifiApMode = prefs.getBool("wifiAP", true);
  wifiSsid = prefs.getString("wifiSSID", "");
  wifiPass = prefs.getString("wifiPASS", "");

  lcd.init();
  lcd.setBrightness(255);
  // Render GUI directly, show logo on HOME
  currentPage = PAGE_HOME;
  applyDisplayTransform();
}

void loop()
{
  static uint32_t lastUI = 0;
  uint32_t now = millis();

  // Sample ADC on CAL page always; on MEASURE only when running
  if (currentPage == PAGE_CAL || (currentPage == PAGE_MEASURE && running)) {
    updateMeasurement();
  }

  // Touch handling: highlight while pressed, act on release
  static uint32_t touchStart = 0; static uint16_t tx=0, ty=0; uint16_t x, y;
  if (lcd.getTouch(&x, &y)) {
    tx = x; ty = y;
    if (!touchStart) touchStart = now;
    // If input overlay active, consume touches there
    if (inputActive) {
      // keyboard handling is processed on release for simplicity
    } else
    // highlight pressed by page
    if (currentPage == PAGE_HOME) {
      if (pointIn(btnHomeCal, x, y)) drawButton(btnHomeCal, true);
      else if (pointIn(btnHomeMeasure, x, y)) drawButton(btnHomeMeasure, true);
      else if (pointIn(btnHomeWifi, x, y)) drawButton(btnHomeWifi, true);
    } else if (currentPage == PAGE_CAL) {
      if (pointIn(btnLeise, x, y)) drawButton(btnLeise, true);
      else if (pointIn(btnLaut, x, y)) drawButton(btnLaut, true);
    } else if (currentPage == PAGE_MEASURE) {
      if (pointIn(btnStart, x, y)) drawButton(btnStart, true);
      else if (pointIn(btnReset, x, y)) drawButton(btnReset, true);
    } else if (currentPage == PAGE_WIFI) {
      if (pointIn(btnWifiAP, x, y)) drawButton(btnWifiAP, true);
      else if (pointIn(btnWifiSTA, x, y)) drawButton(btnWifiSTA, true);
      else if (pointIn(btnWifiSEL, x, y)) drawButton(btnWifiSEL, true);
      else if (pointIn(btnWifiPASS, x, y)) drawButton(btnWifiPASS, true);
    }

    // Long-press gestures for orientation
    uint32_t held = now - touchStart;
    if (held > 2000) { touchStart = now; mirrorX = !mirrorX; applyDisplayTransform(); }
    else if (held > 1000) { touchStart = now; rotation = (rotation + 1) & 3; applyDisplayTransform(); }
  } else if (touchStart) {
    // release
    bool any=false;
    if (inputActive) {
      int W = lcd.width(); int H = lcd.height();
      int top = titleH + MARGIN;
      if (inputTarget == INPUT_SSID) {
        // Handle SSID selection from list overlay
        int listTop = top + 20 + MARGIN;
        int itemH = 28; int shown = (H - MARGIN - listTop) / itemH;
        int n = WiFi.scanComplete(); if (n < 0) n = 0; if (shown > n) shown = n;
        if (ty >= listTop && tx >= MARGIN && tx < (W - MARGIN)) {
          int idx = (ty - listTop) / itemH;
          if (idx >= 0 && idx < shown) {
            wifiSsid = WiFi.SSID(idx);
            prefs.putString("wifiSSID", wifiSsid);
            inputActive = false; inputTarget = INPUT_NONE; applyDisplayTransform(); any = true;
            if (!wifiApMode) { wifiStartSta(); }
          }
        }
      } else if (inputTarget == INPUT_PASS) {
        // On-screen keyboard handling for password
        int lh = 28; // buffer line height
        int kbTop = top + lh + MARGIN;
        int kbBottom = H - MARGIN;
        int kbH = kbBottom - kbTop;
        const char* row1 = "1234567890";
        const char* row2 = "QWERTYUIOP";
        const char* row3 = "ASDFGHJKL";
        const char* row4 = "ZXCVBNM.-_";
        int rows = 5; int rowH = kbH / rows;
        auto inRect = [&](int rx,int ry,int rw,int rh){return tx>=rx&&tx<rx+rw&&ty>=ry&&ty<ry+rh;};
        bool handled=false;
        auto processRow = [&](const char* chars, int count, int rowIndex){
          int y = kbTop + rowIndex*rowH;
          int pad = 4;
          int keyW = (W - MARGIN*2 - pad*(count-1)) / count;
          int x = MARGIN;
          for (int i=0;i<count;i++){
            if (inRect(x, y, keyW, rowH)) { inputBuffer += chars[i]; handled=true; break; }
            x += keyW + pad;
          }
        };
        if (!handled) processRow(row1, 10, 0);
        if (!handled) processRow(row2, 10, 1);
        if (!handled) processRow(row3, 9, 2);
        if (!handled) processRow(row4, 10, 3);
        if (!handled) {
          int y = kbTop + 4*rowH; int pad = 4; int keyW = (W - MARGIN*2 - pad*2) / 3; int x = MARGIN;
          if (inRect(x, y, keyW, rowH)) { inputBuffer += ' '; handled=true; }
          x += keyW + pad;
          if (inRect(x, y, keyW, rowH)) { if (inputBuffer.length()) inputBuffer.remove(inputBuffer.length()-1); handled=true; }
          x += keyW + pad;
          if (inRect(x, y, keyW, rowH)) {
            wifiPass = inputBuffer; prefs.putString("wifiPASS", wifiPass);
            inputActive = false; inputTarget = INPUT_NONE; applyDisplayTransform(); handled=true; any=true;
            if (!wifiApMode && wifiSsid.length()) { wifiStartSta(); }
          }
        }
        if (inputActive) {
          lcd.fillRect(MARGIN, top, W - 2*MARGIN, lh, lgfx::color888(0,0,0));
          lcd.setTextDatum(lgfx::middle_left);
          lcd.setFont(&fonts::Font2);
          lcd.setTextColor(lgfx::color888(230,230,230), lgfx::color888(0,0,0));
          lcd.drawString(inputBuffer, MARGIN+4, top+lh/2);
        }
      }
    }
    else if (currentPage == PAGE_HOME) {
      if (pointIn(btnHomeCal, tx, ty)) { currentPage = PAGE_CAL; any=true; applyDisplayTransform(); }
      else if (pointIn(btnHomeMeasure, tx, ty)) { currentPage = PAGE_MEASURE; any=true; applyDisplayTransform(); }
      else if (pointIn(btnHomeWifi, tx, ty)) { currentPage = PAGE_WIFI; any=true; applyDisplayTransform(); }
    } else if (currentPage == PAGE_CAL) {
      if (pointInRect(backRect, tx, ty)) { currentPage = PAGE_HOME; any=true; applyDisplayTransform(); }
      else if (pointIn(btnLeise, tx, ty)) { calQuiet = (uint16_t)ema; haveQuiet = true; prefs.putUShort("calQ", calQuiet); prefs.putBool("haveQ", true); any=true; drawButtonsCal(); }
      else if (pointIn(btnLaut, tx, ty))  { calLoud  = (uint16_t)ema; haveLoud  = true; prefs.putUShort("calL", calLoud ); prefs.putBool("haveL", true); any=true; drawButtonsCal(); }
    } else if (currentPage == PAGE_MEASURE) {
      if (pointInRect(backRect, tx, ty)) { currentPage = PAGE_HOME; any=true; applyDisplayTransform(); }
      else if (pointIn(btnStart, tx, ty)) { running = !running; any=true; drawTitle(running?"RUN":"STOP"); drawButtonsMeasure(); }
      else if (pointIn(btnReset, tx, ty)) { maxHold = 0.0f; any=true; drawButtonsMeasure(); }
    } else if (currentPage == PAGE_WIFI) {
      if (pointInRect(backRect, tx, ty)) { currentPage = PAGE_HOME; any=true; applyDisplayTransform(); }
      else if (pointIn(btnWifiAP, tx, ty))  { wifiApMode = true; prefs.putBool("wifiAP", true); wifiStartAp(); any=true; drawButtonsWifi(); drawWifiInfo(); }
      else if (pointIn(btnWifiSTA, tx, ty)) { wifiApMode = false; prefs.putBool("wifiAP", false); if (wifiSsid.length()) wifiStartSta(); else { wifiStopAll(); } any=true; drawButtonsWifi(); drawWifiInfo(); }
      else if (pointIn(btnWifiSEL, tx, ty)) {
        // Scan overlay
        int W = lcd.width(); int H = lcd.height(); int top = titleH + MARGIN;
        lcd.fillRect(0, top-2, W, H - top + 2, lgfx::color888(0,0,0));
        lcd.setTextDatum(lgfx::top_left);
        lcd.setFont(&fonts::Font2);
        lcd.setTextColor(lgfx::color888(200,200,200), lgfx::color888(0,0,0));
        lcd.drawString("Scanne WLANs...", MARGIN, top);
        WiFi.mode(WIFI_STA);
        WiFi.disconnect();
        delay(100);
        int n = WiFi.scanNetworks();
        // Draw list
        int listTop = top + 20 + MARGIN;
        int itemH = 28; int shown = (H - MARGIN - listTop) / itemH;
        if (shown > n) shown = n;
        for (int i = 0; i < shown; ++i) {
          int y = listTop + i*itemH;
          lcd.fillRoundRect(MARGIN, y, W - 2*MARGIN, itemH-4, 4, lgfx::color888(35,38,44));
          lcd.drawRoundRect(MARGIN, y, W - 2*MARGIN, itemH-4, 4, lgfx::color888(90,96,106));
          lcd.setTextDatum(lgfx::middle_left);
          lcd.setTextColor(lgfx::color888(230,230,230), lgfx::color888(35,38,44));
          String line = WiFi.SSID(i);
          if (WiFi.encryptionType(i) != WIFI_AUTH_OPEN) line += "  (LOCK)";
          char buf[64]; line.toCharArray(buf, sizeof(buf));
          lcd.drawString(buf, MARGIN+6, y + (itemH-4)/2);
        }
        // Store temp range in globals via input overlay states
        inputActive = true; inputTarget = INPUT_SSID; inputBuffer = ""; // reuse inputActive to capture taps
        // Temporarily store scan count in lastPct (unused) ? Better add globals
        // We'll use global variables below:
        // scanActive flag and count
      }
      else if (pointIn(btnWifiPASS, tx, ty)) { inputTarget = INPUT_PASS; inputBuffer = wifiPass; inputActive = true; }
      if (inputActive) {
        // draw keyboard overlay
        int W = lcd.width(); int H = lcd.height();
        int top = titleH + MARGIN;
        int lh = 28;
        // If we are editing password, draw keyboard; for SSID selection we skip keyboard
        if (inputTarget == INPUT_PASS) {
          lcd.fillRect(0, top-2, W, H - top + 2, lgfx::color888(0,0,0));
        }
        // show field name
        lcd.setTextDatum(lgfx::top_left);
        lcd.setFont(&fonts::Font2);
        lcd.setTextColor(lgfx::color888(180,180,180), lgfx::color888(0,0,0));
        if (inputTarget == INPUT_PASS) lcd.drawString("Passwort:", MARGIN, top-2);
        // buffer line
        if (inputTarget == INPUT_PASS) {
          lcd.fillRoundRect(MARGIN, top, W-2*MARGIN, lh, 4, lgfx::color888(20,22,26));
          lcd.drawRoundRect(MARGIN, top, W-2*MARGIN, lh, 4, lgfx::color888(70,76,84));
          lcd.setTextDatum(lgfx::middle_left);
          lcd.setTextColor(lgfx::color888(230,230,230), lgfx::color888(20,22,26));
          lcd.drawString(inputBuffer, MARGIN+6, top+lh/2);
          // draw keys
          int kbTop = top + lh + MARGIN;
          int kbBottom = H - MARGIN;
          int kbH = kbBottom - kbTop;
          int rows = 5; int rowH = kbH/rows; int pad=4;
          auto drawRow = [&](const char* chars, int count, int rowIndex){
            int y = kbTop + rowIndex*rowH; int keyW = (W - MARGIN*2 - pad*(count-1)) / count; int x = MARGIN;
            for (int i=0;i<count;i++){ lcd.fillRoundRect(x,y,keyW,rowH,4, lgfx::color888(35,38,44)); lcd.drawRoundRect(x,y,keyW,rowH,4, lgfx::color888(90,96,106)); lcd.setTextDatum(lgfx::middle_center); lcd.setTextColor(lgfx::color888(230,230,230), lgfx::color888(35,38,44)); char s[2]={chars[i],0}; lcd.drawString(s, x+keyW/2, y+rowH/2); x += keyW + pad; }
          };
          drawRow("1234567890", 10, 0);
          drawRow("QWERTYUIOP", 10, 1);
          drawRow("ASDFGHJKL", 9, 2);
          drawRow("ZXCVBNM.-_", 10, 3);
          // special keys
          int y = kbTop + 4*rowH; int keyW = (W - MARGIN*2 - pad*2) / 3; int x = MARGIN;
          auto btn = [&](const char* label){ lcd.fillRoundRect(x,y,keyW,rowH,4, lgfx::color888(35,38,44)); lcd.drawRoundRect(x,y,keyW,rowH,4, lgfx::color888(90,96,106)); lcd.setTextDatum(lgfx::middle_center); lcd.setTextColor(lgfx::color888(230,230,230), lgfx::color888(35,38,44)); lcd.drawString(label, x+keyW/2, y+rowH/2); x += keyW + pad; };
          btn("SPACE"); btn("DEL"); btn("OK");
        }
      }
    }
    touchStart = 0;
  }

  // UI refresh at ~30 Hz, but only minimal areas
  if (now - lastUI >= 33) {
    lastUI = now;
    if (currentPage == PAGE_CAL || currentPage == PAGE_MEASURE) {
      drawPercent();
      updateBar();
    } else if (currentPage == PAGE_WIFI) {
      updateWifi();
      drawWifiInfo();
    }
  }
}
