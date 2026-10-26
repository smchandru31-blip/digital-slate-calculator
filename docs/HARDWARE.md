# Hardware Specification — ESP32-2432S028 ("Cheap Yellow Display")

> Verification Status: **DIAGNOSTIC FIRMWARE READY — UNVERIFIED ON HARDWARE**  
> Empirical validation requires flashing the P1 diagnostic firmware and recording serial output.

---

## 1. Board Identity & System Resources

| Feature | Specification | Confidence Level | Verification Method |
| :--- | :--- | :--- | :--- |
| **Board Name** | ESP32-2432S028 (Guition / CYD 2.8" Resistive) | Assumed (Standard) | Visual inspection of silkscreen markings. |
| **SoC** | ESP32-D0WD-V3 (Xtensa Dual-Core 32-bit @ 240MHz) | Assumed (Standard) | Bootloader / `ESP.getChipModel()` probe. |
| **Flash Memory** | 4 MB SPI Flash (Quad SPI @ 40/80MHz) | Assumed | `ESP.getFlashChipSize()` at boot. |
| **PSRAM** | 0 MB (Not fitted on standard CYD 2.8") | Assumed | `psramFound()` runtime probe. |
| **USB-to-UART IC** | WCH CH340C / CP2102 | Confirmed (Driver detected) | Windows PnP device enumeration. |

---

## 2. Display Subsystem

- **Panel**: 2.8-inch TFT LCD, Transmissive, 320 × 240 pixels (Landscape primary: 320W × 240H).
- **Controller IC**: **ILI9341** (predominant) or **ST7789V** (some newer batches).
  - *Confidence*: High (90% CYD 2.8" boards ship with ILI9341).
  - *Driver behavior*: LovyanGFX handles register quirks and color inversion (`setInvert`) dynamically.
- **SPI Bus**: Dedicated Hardware SPI (HSPI / SPI2).
- **Pin Mapping**:
  | Signal | GPIO Pin | Function | Confidence |
  | :--- | :--- | :--- | :--- |
  | `TFT_MOSI` | **GPIO 13** | SPI Master Out Slave In | Confirmed (CYD Schematic) |
  | `TFT_MISO` | **GPIO 12** | SPI Master In Slave Out | Confirmed (CYD Schematic) |
  | `TFT_SCLK` | **GPIO 14** | SPI Clock (up to 40 MHz) | Confirmed (CYD Schematic) |
  | `TFT_CS`   | **GPIO 15** | Chip Select (Active LOW) | Confirmed (CYD Schematic) |
  | `TFT_DC`   | **GPIO 2**  | Data / Command select | Confirmed (CYD Schematic) |
  | `TFT_RST`  | **-1 (EN)** | Hardware reset (tied to board EN) | Confirmed (CYD Schematic) |
  | `TFT_BL`   | **GPIO 21** | Backlight PWM control (Active HIGH) | Confirmed (CYD Schematic) |

---

## 3. Touch Controller Subsystem

- **Controller IC**: **XPT2046** (12-bit SAR ADC resistive touch controller).
- **Interface**: Separate SPI interface (VSPI / Software SPI pins) to prevent bus contention with high-speed TFT DMA.
- **Pin Mapping**:
  | Signal | GPIO Pin | Function | Confidence |
  | :--- | :--- | :--- | :--- |
  | `TOUCH_MOSI` | **GPIO 32** | Touch SPI MOSI | Confirmed (CYD Schematic) |
  | `TOUCH_MISO` | **GPIO 39** | Touch SPI MISO (Input only) | Confirmed (CYD Schematic) |
  | `TOUCH_CLK`  | **GPIO 25** | Touch SPI Clock (1–2 MHz) | Confirmed (CYD Schematic) |
  | `TOUCH_CS`   | **GPIO 33** | Touch Chip Select (Active LOW) | Confirmed (CYD Schematic) |
  | `TOUCH_IRQ`  | **GPIO 36** | Pen Interrupt / Touch Detect (Input only) | Confirmed (CYD Schematic) |

---

## 4. Peripherals & Auxiliary Pinout

| Peripheral | GPIO Pin | Electrical Characteristics |
| :--- | :--- | :--- |
| **RGB LED (Red)** | **GPIO 4** | Active LOW (Common Anode) |
| **RGB LED (Green)** | **GPIO 16** | Active LOW (Common Anode) |
| **RGB LED (Blue)** | **GPIO 17** | Active LOW (Common Anode) |
| **MicroSD CS** | **GPIO 5** | Shares HSPI with TFT (`MOSI=13`, `MISO=12`, `CLK=14`) |
| **LDR (Light Sensor)** | **GPIO 34** | ADC1 Channel 6 |
| **Audio / Speaker** | **GPIO 26** | DAC Channel 2 / PWM output |

---

## 5. Touchscreen Coordinate Calibration

Resistive touch screens output raw 12-bit ADC values ($0 \dots 4095$). Due to analog resistive tolerances, raw values do not correspond directly to pixel coordinates and require affine 2-point or 3-point calibration.

### 5.1 Nominal Raw ADC Bounds (Landscape Orientation 1: USB port on the left)
- $X_{\text{raw, min}} \approx 300$, $X_{\text{raw, max}} \approx 3800$
- $Y_{\text{raw, min}} \approx 200$, $Y_{\text{raw, max}} \approx 3700$

## 6. Build & Flash Procedures

### Option A: Using PlatformIO (Recommended)
1. Open terminal in the `d:/calculator/firmware` directory:
   ```bash
   cd d:/calculator/firmware
   ```
2. Build and upload firmware to the board (replace COM port with your board's active port, e.g. `COM21`):
   ```bash
   pio run -t upload --upload-port COM21
   ```
3. Open serial monitor at 115200 baud:
   ```bash
   pio device monitor -b 115200
   ```

### Option B: Using Arduino IDE
1. Open Arduino IDE.
2. Select Board: **ESP32 Dev Module**.
3. Partition Scheme: **Default 4MB with spiffs** (or Huge APP).
4. Install Library: **LovyanGFX** (by lovyan03) via Library Manager.
5. Open [firmware/src/main.cpp](file:///d:/calculator/firmware/src/main.cpp) (or save as `.ino`).
6. Select your COM port and click **Upload**.

---

## 7. Expected Serial Diagnostic Output

Upon booting, the serial terminal will print:
```text
==================================================
  DIGITAL SLATE CALCULATOR - P1 DIAGNOSTIC PROBE  
==================================================
Chip Model         : ESP32 (rev 1)
Cores              : 2 @ 240 MHz
Flash Size         : 4096 KB (DIO mode)
Flash Speed        : 40 MHz
PSRAM Detected     : NO (Expected on CYD 2.8)
Total Free Heap    : 289452 bytes
Min Free Heap Ever : 281200 bytes
Internal Free Heap : 289452 bytes
--------------------------------------------------
[DISPLAY] Initializing LovyanGFX controller...
[DISPLAY] Running primary color screen fill test...
[TOUCH] Ready. Awaiting stylus/touch input...
Format: [TOUCH] Screen:(x, y) Raw:(raw_x, raw_y) Z:pressure
```

When touching the screen targets (Top-Left, Top-Right, Bottom-Left, Bottom-Right, Center):
```text
[TOUCH #1] Screen:( 16,  14) Raw:( 392,  348) Z: 82
[TARGET HIT] Target TL (15, 15) validated!
[TOUCH #2] Screen:(304,  16) Raw:(3680,  352) Z: 91
[TARGET HIT] Target TR (305, 15) validated!
[TOUCH #3] Screen:( 15, 224) Raw:( 390, 3590) Z: 88
[TARGET HIT] Target BL (15, 225) validated!
[TOUCH #4] Screen:(306, 226) Raw:(3690, 3610) Z: 95
[TARGET HIT] Target BR (305, 225) validated!
[TOUCH #5] Screen:(160, 120) Raw:(2040, 1980) Z: 90
[TARGET HIT] Target C (160, 120) validated!
```

