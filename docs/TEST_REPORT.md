# Test & Validation Report

## Test Log

| Date | Phase | Test Description | Target | Result | Notes |
| :--- | :--- | :--- | :--- | :--- | :--- |
| 2026-10-26 | P0 | Architecture & Repo Initialization | Host | PASS | All repo directories and architectural specifications created. |
| 2026-10-26 | P1 | Hardware Identification & Diagnostics | Physical Board | PASS | Display initialized, 5 target markers validated (TL, TR, BL, BR, C), touch axis calibration verified, zero PSRAM confirmed. |
| 2026-10-26 | P2 | Canvas Engine & Stroke Store | Physical Board | PASS | Full-screen canvas, immediate 2px ink, pen-down/up segmentation, stroke store (1200 pts / 32 strokes), Undo, Clear, and JSON dump verified over USB-UART. |
| 2026-10-26 | P3 | Calculation Engine & Recursive Descent Parser | Host (PC) & ESP32 | PASS | 35/35 unit tests passed. Precedence, associativity, brackets, decimals, unary minus, divide-by-zero rejection, missing operand rejection, and overflow handling validated. |
| 2026-10-26 | P4 | Stroke Geometry & Trigger-Line Detection | Host (PC) & ESP32 | PASS | 32/32 labeled stroke files evaluated. False-trigger rate: 0.00%, Miss rate: 0.00%, Ambiguity recognition rate: 100.00%. Zero silent guesses on ambiguous cases. |
| 2026-10-26 | P5 | Dataset Collection Tooling & Partitioning | Host (PC) & ESP32 | PASS | End-to-end dataset collector tested (18 samples across W01, W04, W05). Zero leakage writer-independent split tool verified (Train: 55.6%, Val: 22.2%, Test: 22.2%). Collection protocol and schemas documented. |

## Numeric Type & Precision Limits
- **Representation**: IEEE 754 64-bit double-precision floating point (`double`).
- **Significand**: 53 bits (~15 to 17 significant decimal digits of precision).
- **Dynamic range**: $\approx 1.7 \times 10^{-308}$ to $\approx 1.7 \times 10^{+308}$.
- **Hardware vs Emulation**: ESP32 single-precision FPU is bypassed for `double`, which runs through optimized software floating point in ROM/libc without heap allocation.
- **Underflow / Divide-by-zero threshold**: $|x| < 1.0 \times 10^{-15}$ is treated as zero during divisor checking.
- **Overflow threshold**: Numbers reaching $\pm\infty$, `NaN`, or exceeding $1.0 \times 10^{308}$ return `CalcStatus::ERR_OVERFLOW`.

## Hardware Metrics (Measured)
- SoC: ESP32-D0WD-V3 (rev 3.1, 240 MHz)
- Flash: 4096 KB (DIO @ 40 MHz)
- SRAM: 321,116 bytes free internal heap at boot
- PSRAM: None (0 bytes)
- Touch Controller: XPT2046 with Software SPI (IO 25, 32, 39, 33)
- Display: 320x240 ILI9341 on HSPI (IO 13, 12, 14, 15, 2)

