#pragma once


#define LED_TEST 13


enum DisplayModes {
  DISPLAY_LOGO,
  DISPLAY_NORMAL,
  DISPLAY_EXTENDED_INFO
};

enum DisplayPages {
  DISPLAY_COUNTER,
  DISPLAY_LAST_COUNTERS1,
  DISPLAY_LAST_COUNTERS2, 
  DISPLAY_LAST_COUNTERS3,
  DISPLAY_LAST_COUNTERS4,
  DISPLAY_VERY_LAST_COUNTER,
  DISPLAY_SYSTEM
};



void setup_applausometer();
void loop_applausometer();

