# ESP32-2432S028 PlatformIO Example (LovyanGFX)

This is a minimal PlatformIO project inspired by
`alannishioka/esp32-2432s028-clock`, showing how to bring up the
ESP32-2432S028 2.8" TFT using LovyanGFX and draw a simple seconds clock.

## Hardware
- Board: ESP32-2432S028 (Sunton), 240x320 TFT (ILI9341) + XPT2046 touch
- Pins used (adjust if your unit differs):
  - SPI (HSPI): `SCLK=14`, `MOSI=13`, `MISO=12`
  - TFT: `CS=15`, `DC=2`, `RST=4`, `BL=27`
  - Touch: `CS=33`, `IRQ=36` (optional)

If touch is inverted or off, tweak the min/max in `src/lgfx_2432s028.hpp`.

## Build & Upload
- Build: `pio run`
- Upload: `pio run -t upload`
- Monitor: `pio device monitor -b 115200`

PlatformIO will automatically fetch `LovyanGFX`.

## Notes
- The example uses a simple software clock (millis) to avoid WiFi setup.
  You can add NTP later via `configTime()` if needed.
- If your display shows nothing, verify pin mapping and rotation in
  `src/lgfx_2432s028.hpp`.
- For deeper reference and a full clock implementation, see the original repo:
  https://github.com/alannishioka/esp32-2432s028-clock

