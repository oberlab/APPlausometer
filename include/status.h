#pragma once

#include <stdint.h>
#include <string>

struct web_events_t {
  bool update;        // True, if a new event is detected

  // Measurement
  int level;        // 0..100
  int peak;         // 0..100, holds until reset

  // Control
  bool button_reset;
};

struct web_settings_t {
  // Config
  int frq;                 // Hz
  int gain_db;            // 40 or 60
  unsigned long duration; // measurement duration
};

void setup_config(web_events_t *events, web_settings_t *settings);
