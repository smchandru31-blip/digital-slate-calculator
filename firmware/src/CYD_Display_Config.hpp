#pragma once

#define LGFX_USE_V1
#include <LovyanGFX.hpp>

/**
 * Hardware configuration for Guition ESP32-2432S028 ("Cheap Yellow Display")
 * 2.8" TFT (ILI9341 / ST7789) on HSPI (SPI2)
 * XPT2046 Resistive Touch on dedicated pins (VSPI / Software SPI)
 */
class LGFX_CYD : public lgfx::LGFX_Device {
    lgfx::Panel_ILI9341 _panel_instance;
    lgfx::Bus_SPI       _bus_instance;
    lgfx::Light_PWM     _light_instance;
    lgfx::Touch_XPT2046 _touch_instance;

public:
    LGFX_CYD(void) {
        {
            // Configure SPI Bus for Display
            auto cfg = _bus_instance.config();
            cfg.spi_host   = HSPI_HOST;      // ESP32 HSPI (SPI2)
            cfg.spi_mode   = 0;
            cfg.freq_write = 40000000;       // 40 MHz write clock
            cfg.freq_read  = 16000000;
            cfg.spi_3wire  = false;
            cfg.dma_channel = SPI_DMA_CH_AUTO; // Auto DMA channel
            cfg.pin_sclk   = 14;             // TFT SCK
            cfg.pin_mosi   = 13;             // TFT MOSI
            cfg.pin_miso   = 12;             // TFT MISO
            cfg.pin_dc     = 2;              // TFT DC / RS
            _bus_instance.config(cfg);
            _panel_instance.setBus(&_bus_instance);
        }

        {
            // Configure Display Panel
            auto cfg = _panel_instance.config();
            cfg.pin_cs           = 15;       // TFT CS
            cfg.pin_rst          = -1;       // Tied to EN
            cfg.pin_busy         = -1;
            cfg.memory_width     = 240;
            cfg.memory_height    = 320;
            cfg.panel_width      = 240;
            cfg.panel_height     = 320;
            cfg.offset_x         = 0;
            cfg.offset_y         = 0;
            cfg.offset_rotation  = 0;
            cfg.dummy_read_pixel = 8;
            cfg.dummy_read_bits  = 1;
            cfg.readable         = false;
            cfg.invert           = false;
            cfg.rgb_order        = false;    // BGR mode for standard CYD ILI9341
            cfg.dlen_16bit       = false;
            cfg.bus_shared       = false;    // Touch uses separate bus pins
            _panel_instance.config(cfg);
        }

        {
            // Configure Backlight PWM
            auto cfg = _light_instance.config();
            cfg.pin_bl      = 21;            // Backlight pin
            cfg.invert      = false;         // Active HIGH
            cfg.freq        = 44100;         // PWM frequency
            cfg.pwm_channel = 7;
            _light_instance.config(cfg);
            _panel_instance.setLight(&_light_instance);
        }

        {
            // Configure XPT2046 Resistive Touch
            auto cfg = _touch_instance.config();
            cfg.x_min           = 300;
            cfg.x_max           = 3800;
            cfg.y_min           = 200;
            cfg.y_max           = 3700;
            cfg.pin_int         = 36;        // Touch IRQ
            cfg.bus_shared      = false;     // Separate SPI bus from display
            cfg.offset_rotation = 0;
            cfg.spi_host        = VSPI_HOST; // Use VSPI for touch controller
            cfg.freq            = 1000000;   // 1 MHz touch SPI
            cfg.pin_sclk        = 25;        // Touch CLK
            cfg.pin_mosi        = 32;        // Touch MOSI
            cfg.pin_miso        = 39;        // Touch MISO
            cfg.pin_cs          = 33;        // Touch CS
            _touch_instance.config(cfg);
            _panel_instance.setTouch(&_touch_instance);
        }

        setPanel(&_panel_instance);
    }
};
