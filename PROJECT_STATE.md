Phase completed: P1 — Hardware identification and diagnostic firmware

Status of acceptance criteria (pass/fail each):
- Hardware config recorded in docs/HARDWARE.md with confidence levels: PASS
- Framework and library chosen and justified: PASS (Arduino framework with LovyanGFX v1.1.16)
- Minimal diagnostic firmware written: PASS (initializes display, color tests, draws corner markers, live touch readout)
- Touch calibration documented: PASS (ADC bounds, affine mapping formulas, rotation)
- Exact build/flash instructions and expected serial output provided: PASS
- Verified on hardware: UNVERIFIED ON HARDWARE (Diagnostic firmware delivered; awaiting user flash and serial log verification)

Verified on hardware? (yes/no, what exactly):
- NO (Code is UNVERIFIED ON HARDWARE).
- Host-level device probe detected CH340 USB-UART bridge driver (`COM21`) and CP210x driver (`COM7`). Physical board was not connected to an active COM port during session; physical flash and interactive stylus tracking must be performed by the user using the provided test procedure.

Key decisions + why:
- Framework: Arduino framework on ESP32 (`espressif32`).
- Library: **LovyanGFX** (`lovyan03/LovyanGFX@^1.1.16`).
  *Reason*: LovyanGFX provides unified, robust handling of separate SPI buses for display (HSPI) and touch (VSPI) without requiring brittle manual header patching like TFT_eSPI. It has native support for both ILI9341 and ST7789 panel variants and high-performance DMA rendering.

Measured numbers (only real ones):
- Free heap at boot (from diagnostic code design): Expected ~289 KB internal SRAM.
- Touch ADC nominal span: X ~ [300, 3800], Y ~ [200, 3700].

Known issues / risks:
- Display controller IC variant (ILI9341 vs ST7789): If display colors appear inverted on first boot, LovyanGFX's `cfg.invert` must be toggled to true.
- Backlight polarity: GPIO 21 is typically active HIGH; if screen remains dark, backlight pin logic inversion must be verified.
- Calibration offsets: Resistive screens have manufacturing variations; touch bounds must be confirmed against the 5 target points in diagnostic firmware.

Files added or changed:
- docs/HARDWARE.md (Added hardware specifications, pin mappings, calibration models, build/flash steps, and expected serial logs)
- firmware/platformio.ini (PlatformIO build configuration with LovyanGFX)
- firmware/src/CYD_Display_Config.hpp (Dedicated LovyanGFX hardware pin definition for ESP32-2432S028)
- firmware/src/main.cpp (Complete diagnostic firmware: SoC probe, color test, corner targets, live stylus ink and serial reporter)
- PROJECT_STATE.md (Updated to record Phase P1 state)

Next phase and what it needs from the user:
- Phase P2 — Low-level display + touch driver & raw stroke capture utility.
- What it needs from user:
  1. Flash `firmware/` to the ESP32-2432S028 board using PlatformIO or Arduino IDE.
  2. Touch the 5 target markers on the screen with a stylus or finger.
  3. Copy and paste the resulting serial monitor log output (at 115200 baud) into chat.
