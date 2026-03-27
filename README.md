# Pictureframe

Dedicated to an awesome boss – thank you for your leadership and support.
==============================
Files:
Photo_frame.cpp - Main programm
PictrureFrame.pdf - Schematic
==============================
ESP32 RGBW PICTURE FRAME
==============================

PIN MAPPING
-----------
IO4   -> RGBW LED data (30 addressable LEDs)
IO5   -> Backlight PWM (MOSFET control)
IO0   -> LDR (light sensor)
IO6   -> Charlieplex 1
IO7   -> Charlieplex 2
IO10  -> Charlieplex 3
IO20  -> RGBW button
IO21  -> Backlight button


------------------------------
BUTTON FUNCTIONS
------------------------------

RGBW BUTTON (IO20)
------------------
1x click:
- Switch RGB LED modes

2x click:
- Switch Charlieplex animation modes

3x click:
- Change Charlieplex animation speed
  (slow / medium / fast)


BACKLIGHT BUTTON (IO21)
-----------------------
1x click:
- Cycle backlight modes:
  OFF -> MEDIUM -> HIGH

- Also disables LDR mode (manual override)

2x click:
- Change RGB brightness preset (5 levels)

3x click:
- Toggle LDR mode ON / OFF


------------------------------
LDR MODE (AUTO BRIGHTNESS)
------------------------------

When LDR mode is ON:

RGB LEDs:
- Bright environment  -> brighter
- Dark environment    -> dimmer

Backlight:
- Bright environment  -> brighter
- Dark environment    -> dimmer



------------------------------
CHARLIEPLEX MODES
------------------------------
- RUN        (sequential)
- BOUNCE     (back and forth)
- PAIR       (paired LEDs)
- SKIP       (skipping pattern)
- MODE       (linked to RGB mode)


------------------------------
BACKLIGHT MODES
------------------------------
- OFF
- MEDIUM
- HIGH


------------------------------
NOTES
------------------------------
- LDR overrides both RGB and backlight brightness
- Manual backlight change disables LDR automatically
- All settings persist during runtime (if preferences used)
- RGB strip uses GRBW format
- PWM frequency: 5kHz
