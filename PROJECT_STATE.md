Phase completed: P1 — Hardware identification and diagnostic firmware

Status of acceptance criteria (pass/fail each):
- Display initializes: PASS (ILI9341 on HSPI verified)
- Touch coordinates track stylus correctly across all four corners and center: PASS (TL, TR, BL, BR, C target markers verified by user)
- Hardware config recorded in docs/HARDWARE.md: PASS (Full specifications and verified pinouts documented)
- Library and framework chosen and justified: PASS (Arduino framework + LovyanGFX v1.1.16)
- Touch calibration documented: PASS (ADC range [300..3900], [3700..200] inverted Y mapping to Landscape X)

Verified on hardware? (yes/no, what exactly):
- YES.
- Display initialization, primary color fill, and corner target crosshairs rendered on 2.8" LCD.
- Stylus tracking and corner validation executed on physical hardware over COM21.
- ESP32 chip probe confirmed: ESP32-D0WD-V3 (rev 3.1), 240 MHz dual-core, 4MB Flash (DIO @ 40MHz), 0MB PSRAM, 321 KB free heap.

Key decisions + why:
- Framework: Arduino core on ESP32 (`espressif32`).
- Graphics & Touch Library: **LovyanGFX** (`lovyan03/LovyanGFX@^1.1.16`). Handles dual SPI buses seamlessly with hardware HSPI for display and software SPI for touch to eliminate DMA conflicts.
- Touch mapping: Configured `x_min=300`, `x_max=3900`, `y_min=3700`, `y_max=200` to account for the physical touch panel's inverted Y-axis relative to landscape orientation.

Measured numbers (only real ones):
- Free heap at boot: 321,116 bytes internal SRAM.
- Minimum free heap ever: 288,936 bytes.
- Flash size: 4096 KB.
- Flash write speed: Uploaded at 921,600 baud in 4.5 seconds (910 kbit/s).
- Program size: Flash 496,247 bytes (37.9% of partition); RAM 25,224 bytes (7.7% of internal SRAM).

Known issues / risks:
- Absence of external PSRAM is now verified. All TinyML models and stroke buffers in later phases must fit within the ~288 KB available internal SRAM budget.

Files added or changed:
- docs/HARDWARE.md (Updated with confirmed hardware metrics and calibration)
- docs/TEST_REPORT.md (Logged P1 hardware validation pass)
- firmware/platformio.ini (PlatformIO build configuration)
- firmware/src/CYD_Display_Config.hpp (Calibrated LovyanGFX driver definition)
- firmware/src/main.cpp (Interactive diagnostic firmware)
- PROJECT_STATE.md (Updated state)

Next phase and what it needs from the user:
- Phase P2 — Low-level display + touch driver & raw stroke capture utility.
- What it needs: User provides the prompt for Phase P2.
