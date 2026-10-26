# Hardware Specification — ESP32-2432S028 ("Cheap Yellow Display")

> Verification Status: **VERIFIED ON PHYSICAL HARDWARE (Phase P1 Diagnostic Passed)**

---

## 1. Board Identity & System Resources (Verified via Hardware Probe)

| Feature | Measured Specification | Status |
| :--- | :--- | :--- |
| **Board Name** | ESP32-2432S028 (Guition / CYD 2.8" Resistive) | Confirmed |
| **SoC** | ESP32-D0WD-V3 (Silicon Revision 3.1, Dual Core @ 240 MHz) | Confirmed |
| **Flash Memory** | 4096 KB SPI Flash (DIO Mode @ 40 MHz) | Confirmed |
| **PSRAM** | **0 MB (Not fitted)** — Internal SRAM only | Confirmed |
| **Free Heap** | 321,116 bytes internal SRAM at boot (Min: 288,936 bytes) | Confirmed |
| **USB-to-UART IC** | WCH CH340C (Targeted on COM21 @ 921600 baud upload) | Confirmed |
| **MAC Address** | `ec:e3:34:20:fb:a4` | Confirmed |

---

## 2. Display Subsystem (Verified)

- **Panel**: 2.8-inch TFT LCD, Transmissive, 320 × 240 pixels (Landscape primary: 320W × 240H).
- **Controller IC**: **ILI9341** (HSPI / SPI2).
- **SPI Mode**: Mode 0, 40 MHz Write, 16 MHz Read, DMA Auto.
- **Pinout**:
  - `TFT_MOSI`: **GPIO 13**
  - `TFT_MISO`: **GPIO 12**
  - `TFT_SCLK`: **GPIO 14**
  - `TFT_CS`: **GPIO 15**
  - `TFT_DC`: **GPIO 2**
  - `TFT_RST`: **-1 (EN)**
  - `TFT_BL` (Backlight): **GPIO 21** (PWM Channel 7, active HIGH)

---

## 3. Touch Controller Subsystem (Verified & Calibrated)

- **Controller IC**: **XPT2046** (12-bit SAR ADC resistive touch controller).
- **Interface**: Dedicated Software SPI bus (prevents contention with high-speed display DMA).
- **Pinout**:
  - `TOUCH_CLK`: **GPIO 25**
  - `TOUCH_MOSI`: **GPIO 32**
  - `TOUCH_MISO`: **GPIO 39** (Input only)
  - `TOUCH_CS`: **GPIO 33**
  - `TOUCH_IRQ`: **-1** (Polled in software)

### 3.1 Empirical Calibration Bounds (Landscape Orientation 1)
```cpp
cfg.x_min = 300;
cfg.x_max = 3900;
cfg.y_min = 3700;  // High raw value on Left
cfg.y_max = 200;   // Low raw value on Right
```
- **Tracking accuracy**: Validated across all four corners (`TL`, `TR`, `BL`, `BR`) and center (`C`).

---

## 4. Auxiliary Peripherals

- **RGB LED (Red)**: GPIO 4 (Active LOW)
- **RGB LED (Green)**: GPIO 16 (Active LOW, flashes on target hit)
- **RGB LED (Blue)**: GPIO 17 (Active LOW, heartbeat blinker)
- **MicroSD CS**: GPIO 5 (Shares TFT SPI bus)
- **LDR (Light Sensor)**: GPIO 34 (ADC1)
