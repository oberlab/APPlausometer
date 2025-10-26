
#include "pixel.h"

// ---------- Peak-Logic ----------
int peakIndex = 0;
int peakLastIndex = 0;


unsigned long lastPeakUpdate = 0;


Adafruit_NeoPixel strip(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);


/*********************************************************************************************
 * @brief Helper: Volume (0.0–1.0) --> LED-Index
 *  
 *********************************************************************************************/
int volumeToLedIndex(float volume) {
  // volume: 0.0 ... 1.0
  if (volume < 0.0) volume = 0.0;
  if (volume > 1.0) volume = 1.0;

  return (int)(volume * LED_COUNT);
}

/*********************************************************************************************
 * @brief showStripLED supports the LED strips
 *        To save power, just 4 LED will be displayed (with changing the color if it moves higher)
 *        Additionally, the peak will be displayed. The peak will remain for a whiel but then it will 
 *        slowly go down
 *        
 * @param volume    Value to show as main effect (valid: 0...1)
 * @param resetPeak Reset peak immediately to the bottom, if true
 *  
 *********************************************************************************************/
int showStripLED(float volume, bool resetPeak) {
  strip.clear();

  // --- Current volume ---
  int baseIndex = volumeToLedIndex(volume);
  uint8_t r = (uint8_t)(volume * 255);
  uint8_t g = (uint8_t)((1.0 - volume) * 255);
  uint32_t color = strip.Color(r, g, 0);

  // --- Show LEDs for volume ---
  for (int i = LED_SHOW-1; i >= 0; i--) {
    int led = baseIndex - i;
    if (led < LED_COUNT && led >= 0) {
      strip.setPixelColor(led, color);
    }   
  }

  peakLastIndex = (peakLastIndex + baseIndex)/2;
  peakLastIndex = baseIndex;

  if (peakLastIndex > peakIndex) {
    // Set new peak
    peakIndex = peakLastIndex;
    lastPeakUpdate = millis();
  }
  #ifdef PEAK_FALL_SPEED
   else {
    // Decrease peak after a delay
    if (millis() - lastPeakUpdate > PEAK_HOLD_TIME) {
      if (peakIndex > 0) peakIndex -= PEAK_FALL_SPEED;
      lastPeakUpdate = millis();
    }
  }
  #endif

  if (resetPeak) {
    peakIndex = 0;
  }
  
  // --- Peak-Display (e.g. 2 LEDs, blue) ---
  for (int i = LED_PEAK_SHOW-1; i >= 0; i--) {
    int led = peakIndex + i;
    if (led < LED_COUNT && led > 1) {
      strip.setPixelColor(led, strip.Color(0, 0, 255)); //Do not show peak at 0...1
    }
  }

  strip.show();
  return baseIndex;
}