#include <Arduino.h>
#include "esp_chip_info.h"
#include "esp_heap_caps.h"
#include "CYD_Display_Config.hpp"
#include "StrokeStore.hpp"
#include "CanvasEngine.hpp"

// Hardware Pin Definitions for Auxiliary Peripherals
#define RGB_LED_RED   4
#define RGB_LED_GREEN 16
#define RGB_LED_BLUE  17

// Global hardware and engine instances
static LGFX_CYD tft;
static StrokeStore strokeStore;
static CanvasEngine canvas(tft, strokeStore);

void setup() {
    Serial.begin(115200);
    delay(400);

    // RGB LED init (Common anode: HIGH = OFF)
    pinMode(RGB_LED_RED, OUTPUT);
    pinMode(RGB_LED_GREEN, OUTPUT);
    pinMode(RGB_LED_BLUE, OUTPUT);
    digitalWrite(RGB_LED_RED, HIGH);
    digitalWrite(RGB_LED_GREEN, HIGH);
    digitalWrite(RGB_LED_BLUE, HIGH);

    Serial.println(F("=================================================="));
    Serial.println(F("  DIGITAL SLATE - P2 HANDWRITING CANVAS & ENGINE  "));
    Serial.println(F("=================================================="));
    Serial.printf("Free Internal Heap: %lu bytes\n", (unsigned long)heap_caps_get_free_size(MALLOC_CAP_INTERNAL));

    // Initialize display and touch
    tft.init();
    tft.setRotation(1);     // Landscape (320 x 240, USB on left)
    tft.setBrightness(255); // Full backlight

    // Initialize handwriting canvas UI
    canvas.init();

    Serial.println(F("[CANVAS] Ready for stylus handwriting."));
    Serial.println(F("[COMMANDS] Send 'd' or 'e' over Serial to dump strokes as JSON."));
    Serial.println(F("[COMMANDS] Send 'u' to Undo, 'c' to Clear."));
}

void loop() {
    static uint32_t last_led_blink = 0;
    static bool led_state = false;

    // Heartbeat LED on Blue LED (GPIO 17) every 500ms
    if (millis() - last_led_blink > 500) {
        last_led_blink = millis();
        led_state = !led_state;
        digitalWrite(RGB_LED_BLUE, led_state ? LOW : HIGH);
    }

    // Process touch and ink rendering
    canvas.update();

    // Process Serial commands
    if (Serial.available() > 0) {
        char cmd = (char)Serial.read();
        switch (cmd) {
            case 'd':
            case 'e':
            case 'D':
            case 'E':
                canvas.dumpStrokesJSON();
                break;
            case 'u':
            case 'U':
                canvas.undo();
                break;
            case 'c':
            case 'C':
                canvas.clear();
                break;
            default:
                break;
        }
    }
}
