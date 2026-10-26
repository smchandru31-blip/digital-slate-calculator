# Hardware Specification — ESP32-2432S028 ("Cheap Yellow Display")

> Status: UNVERIFIED ON HARDWARE (Pending Phase P1 Diagnostic)

## 1. Board Identity
- **Model / Silkscreen**: TBD (to be verified in P1)
- **Module Can Markings**: TBD (to be verified in P1)
- **ESP32 Chip Revision**: TBD
- **Flash Size**: TBD (Expected 4MB)
- **PSRAM Present**: TBD (Expected None)

## 2. Display Interface
- **Panel**: 2.8-inch TFT LCD, 320x240 pixels
- **Controller IC**: TBD (ILI9341 vs ST7789)
- **SPI Bus / Frequency**: TBD
- **Pinout**:
  - `TFT_MOSI`: TBD (commonly IO13)
  - `TFT_MISO`: TBD (commonly IO12)
  - `TFT_SCLK`: TBD (commonly IO14)
  - `TFT_CS`: TBD (commonly IO15)
  - `TFT_DC`: TBD (commonly IO2)
  - `TFT_RST`: TBD (commonly IO-1 or EN)
  - `TFT_BL` (Backlight): TBD (commonly IO21 or IO27)

## 3. Touch Interface
- **Controller IC**: XPT2046 (Resistive 4-wire)
- **SPI Bus**: TBD (Dedicated vs Shared VSPI/HSPI)
- **Pinout**:
  - `TOUCH_MOSI`: TBD (commonly IO32)
  - `TOUCH_MISO`: TBD (commonly IO39)
  - `TOUCH_CLK`: TBD (commonly IO25)
  - `TOUCH_CS`: TBD (commonly IO33)
  - `TOUCH_IRQ`: TBD (commonly IO36)

## 4. Peripherals & Aux Pins
- **Micro-SD CS**: TBD (commonly IO5)
- **RGB LED**: Red (IO4/IO22), Green (IO16/IO17), Blue (IO17/IO16) - TBD
- **LDR (ADC)**: TBD (commonly IO34)
- **Speaker / DAC**: TBD (commonly IO26)
