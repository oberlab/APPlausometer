#pragma once

#include <Arduino.h>


// --- Used Pin ---
#define BUTTON_PIN 32


#define BTN_PRESSED   LOW
#define BTN_RELEASED  HIGH

#define DEBOUNCING_TIME        50  //ms
#define MEDIUM_TIME_PRESSED  2000  //ms
#define LONG_TIME_PRESSED    5000  //ms


// --- Stages ---
enum ButtonEvent {
  BUTTON_NONE,
  BUTTON_SHORT,
  BUTTON_LONG2S,
  BUTTON_LONG5S
};


// --- Prototypes---
void setup_button();
ButtonEvent checkButton(ButtonEvent *pressedState, bool pressedUpdate);
