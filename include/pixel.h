#pragma once

#include <Arduino.h>
#include <Adafruit_NeoPixel.h>


// ===== LED Band =====
#define LED_PIN         27     // Pin für WS2812b
#define LED_COUNT       78     // Anzahl LEDs
#define LED_SHOW        4      // Anzahl LEDs die angezeigt werden für Ausschlag
#define LED_PEAK_SHOW   2      // Anzahl LEDs die angezeigt werden für Peak



#define PEAK_HOLD_TIME      5000   // ms
#define PEAK_FALL_SPEED     1      // LEDs pro "Tick"



// Hilfsfunktion: Lautstärke (0.0–1.0) → LED-Index
int volumeToLedIndex(float volume);

// LEDs anzeigen
int showStripLED(float volume, bool resetPeak);
