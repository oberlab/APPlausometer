#include "button.h"


// --- Setup ---
void setup_button() {
  pinMode(BUTTON_PIN, INPUT_PULLUP);
}


// --- Button-Check ---
ButtonEvent checkButton(ButtonEvent *pressedState, bool pressedUpdate) {
  static unsigned long lastDebounceTime = 0;
  static unsigned long pressStartTime = 0;
  static bool lastButtonState = BTN_RELEASED;
  static bool buttonState = BTN_RELEASED;
  static ButtonEvent lastState = BUTTON_NONE;

  ButtonEvent ret = BUTTON_NONE;


  // =================================
  // =     Get button state
  // =================================  
  bool reading = digitalRead(BUTTON_PIN);     // Our definition: true (BTN_RELEASED) if open, false (BTN_PRESSED) if switch is shorted to GND
  unsigned long now = millis();


  // =================================
  // =     Debouncing
  // =================================
  if (reading != lastButtonState) {
    lastDebounceTime = now;
  }

  unsigned long pressDuration = now - lastDebounceTime;


  // =================================
  // =     Check Button states
  // =================================
  if (pressDuration > DEBOUNCING_TIME) {
    
    // Stable state
    if (reading == BTN_PRESSED && buttonState == BTN_RELEASED) {
      // New stable pressed state
      pressStartTime = now;
      buttonState = reading;    
    }
    else if (reading == BTN_RELEASED && buttonState == BTN_PRESSED) {
      // Button released
      unsigned long pressReleased = now - pressStartTime;
      buttonState = reading; 

      if (pressReleased < MEDIUM_TIME_PRESSED) {
        ret = BUTTON_SHORT;   // Short time pressed 
      }
      else if (pressReleased >= MEDIUM_TIME_PRESSED && pressReleased < LONG_TIME_PRESSED && !pressedUpdate) {
        ret = BUTTON_LONG2S;  // Medium time pressed 
      }
      else if (pressReleased >= LONG_TIME_PRESSED && !pressedUpdate) {
        ret = BUTTON_LONG5S;  // Long time pressed
      }
    }

    if (reading == BTN_PRESSED) {
      // Button still pressed
      *pressedState = BUTTON_SHORT;
    } 
    else {
      *pressedState = BUTTON_NONE;
    }   
    if (reading == BTN_PRESSED && pressDuration >= MEDIUM_TIME_PRESSED) {
      *pressedState = BUTTON_LONG2S;
      if (pressedUpdate && lastState == BUTTON_SHORT) {
        ret = BUTTON_LONG2S;
      }
    }

    if (reading == BTN_PRESSED && pressDuration >= LONG_TIME_PRESSED) {
      *pressedState = BUTTON_LONG5S;
            if (pressedUpdate && lastState == BUTTON_LONG2S) {
        ret = BUTTON_LONG5S;
      }
    }
    lastState = *pressedState;
    return ret;
  }

  // =================================
  // =     Instable pulses
  // =================================  
  lastButtonState = reading;

  *pressedState = BUTTON_NONE;
  return BUTTON_NONE;
}
