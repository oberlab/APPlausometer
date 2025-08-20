// Minimal LovyanGFX config for ESP32-2432S028 (2.8" 240x320 TFT + XPT2046 touch)
// Pin mapping here matches common Sunton ESP32-2432S028 boards.
// If your unit differs, adjust pins below.

#pragma once

#include <LovyanGFX.hpp>

class LGFX : public lgfx::LGFX_Device {
  lgfx::Panel_ILI9341 _panel;
  lgfx::Bus_SPI _bus;
  lgfx::Light_PWM _light;
  lgfx::Touch_XPT2046 _touch;

public:
  LGFX() {
    // SPI bus config (HSPI)
    auto bus_cfg = _bus.config();
    bus_cfg.spi_host     = HSPI_HOST;   // HSPI
    bus_cfg.spi_mode     = 0;
    bus_cfg.freq_write   = 40000000;    // 40 MHz write
    bus_cfg.freq_read    = 16000000;    // 16 MHz read
    bus_cfg.spi_3wire    = false;
    bus_cfg.use_lock     = true;
    bus_cfg.dma_channel  = 1;
    bus_cfg.pin_sclk     = 14;
    bus_cfg.pin_mosi     = 13;
    bus_cfg.pin_miso     = 12;
    bus_cfg.pin_dc       = 2;           // D/C
    _bus.config(bus_cfg);
    _panel.setBus(&_bus);

    // Panel config (ILI9341 240x320)
    auto panel_cfg = _panel.config();
    panel_cfg.pin_cs           = 15;
    panel_cfg.pin_rst          = -1;
    panel_cfg.pin_busy         = -1;
    panel_cfg.panel_width      = 320;
    panel_cfg.panel_height     = 240;
    panel_cfg.memory_width     = 320;
    panel_cfg.memory_height    = 240;
    panel_cfg.offset_x         = 0;
    panel_cfg.offset_y         = 0;
    panel_cfg.offset_rotation  = 4;
    panel_cfg.readable         = false;  // safer default; some boards don't wire MISO from TFT
    panel_cfg.invert           = false;
    panel_cfg.rgb_order        = true;   // fix R/B swap seen on some ILI9341 units
    panel_cfg.dlen_16bit       = false;
    panel_cfg.bus_shared       = false;   // shared with touch
    _panel.config(panel_cfg);

    // Backlight via PWM
    auto light_cfg = _light.config();
    light_cfg.pin_bl      = 21;   
    light_cfg.invert      = false;
    light_cfg.freq        = 44100;
    light_cfg.pwm_channel = 7;
    _light.config(light_cfg);
    _panel.setLight(&_light);

    // Touch (XPT2046) on same SPI
    auto touch_cfg = _touch.config();
    touch_cfg.spi_host   = VSPI_HOST;    // HSPI
    touch_cfg.freq       = 1000000;      // 1 MHz
    touch_cfg.pin_sclk   = 25;
    touch_cfg.pin_mosi   = 32;
    touch_cfg.pin_miso   = 39;
    touch_cfg.pin_cs     = 33;
    touch_cfg.bus_shared = false;
    // Raw calibration values (adjust if touch is inverted/off)
    touch_cfg.offset_rotation = 3;
    touch_cfg.x_min = 300;  touch_cfg.x_max = 3900;
    touch_cfg.y_min = 200;  touch_cfg.y_max = 3800;
    _touch.config(touch_cfg);
    _panel.setTouch(&_touch);

    setPanel(&_panel);
  }
};
