#pragma once
#include <stdint.h>
#include <string>

struct status_t {
  // Measurement
  int level;        // 0..100
  int peak;         // 0..100, holds until reset

  // Config
  int f1;           // Hz
  int f2;           // Hz
  int gain_db;      // 40 or 60
  unsigned long duration_ms; // measurement duration
};

extern struct status_t system_status;

// only set in main.cpp - this way, the init code and the values can be right here
#ifdef STATUS_INIT
void init_status()
{
  system_status.level = 0;
  system_status.peak = 0;
  // sensible defaults for demo
  system_status.f1 = 1000;
  system_status.f2 = 2000;
  system_status.gain_db = 40;
  system_status.duration_ms = 5000;
}

#endif
