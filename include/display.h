#pragma once

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "analog.h"


// Displaygröße in Pixeln
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

// I²C Pins
#define OLED_SDA 21
#define OLED_SCL 22


void setup_display();
void display_print_value(float main_value, float value_vol, float value_peak, float progress_hori, float progress_vert);
void display_print_image(bool withIP);
void display_print_counter(Applause *pdata, size_t index, size_t elements, float progress_vert);