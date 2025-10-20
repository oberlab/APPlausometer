#pragma once

#include <stdint.h>
#include <string>


#define APPLAUSE_NAME_SIZE  50


struct web_events_t {
  bool update;                                // True, if a new event is detected
  bool button_reset;
  int level;                                  // 0..100
  int peak;                                   // 0..100, holds until reset
  char participant_name[APPLAUSE_NAME_SIZE];  // Name from browser
  bool name_updated;                          // Flag for new name
};


struct web_settings_t {
  int frq;                                    // Hz
  int gain_db;                                // 40, 50 or 60 dB
  unsigned long duration;                     // measurement duration in seconds
};

void setup_config(web_events_t *events, web_settings_t *settings);
