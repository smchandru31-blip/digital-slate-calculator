#include <Arduino.h>
#include "esp_chip_info.h"
#include "esp_heap_caps.h"
#include "CYD_Display_Config.hpp"
#include "StrokeStore.hpp"
#include "CanvasEngine.hpp"

#include "CollectorEngine.hpp"

// Hardware Pin Definitions for Auxiliary Peripherals
#define RGB_LED_RED   4
#define RGB_LED_GREEN 16
#define RGB_LED_BLUE  17

// Global hardware and engine instances
static LGFX_CYD tft;
static StrokeStore strokeStore;
static CanvasEngine canvas(tft, strokeStore);
static DigitalSlate::CollectorEngine collector(tft, strokeStore);

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
    Serial.println(F("  DIGITAL SLATE - P5 DATASET COLLECTION FIRMWARE  "));
    Serial.println(F("=================================================="));
    Serial.printf("Free Internal Heap: %lu bytes\n", (unsigned long)heap_caps_get_free_size(MALLOC_CAP_INTERNAL));

    // Initialize display and touch
    tft.init();
    tft.setRotation(1);     // Landscape (320 x 240, USB on left)
    tft.setBrightness(255); // Full backlight

    // Initialize handwriting canvas UI
    canvas.init();
    collector.init();

    Serial.println(F("[CANVAS] Ready for handwriting."));
    Serial.println(F("[COMMANDS] 'm' = Toggle Collection Mode"));
    Serial.println(F("[COMMANDS] 's'/'n' = Save & Next, 'r' = Redo, 'k' = Skip"));
    Serial.println(F("[COMMANDS] 'd' = Dump JSON, 'u' = Undo, 'c' = Clear"));
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
    if (collector.isCollecting()) {
        int32_t tx = 0, ty = 0;
        if (tft.getTouch(&tx, &ty)) {
            if (!collector.handleTouch((int16_t)tx, (int16_t)ty)) {
                // Free canvas writing area
                canvas.update();
            }
        } else {
            canvas.update();
        }
    } else {
        canvas.update();
    }

    // Process Serial commands
    if (Serial.available() > 0) {
        String cmdStr = Serial.readStringUntil('\n');
        cmdStr.trim();
        if (cmdStr.length() == 0) return;

        char cmd = cmdStr.charAt(0);
        switch (cmd) {
            case 'm':
            case 'M':
                if (collector.isCollecting()) {
                    collector.stopCollection();
                    canvas.init();
                } else {
                    collector.startCollection();
                }
                break;
            case 's':
            case 'S':
            case 'n':
            case 'N':
                if (collector.isCollecting()) {
                    collector.saveAndNext();
                }
                break;
            case 'r':
            case 'R':
                if (collector.isCollecting()) {
                    collector.redoCurrent();
                } else {
                    canvas.clear();
                }
                break;
            case 'k':
            case 'K':
                if (collector.isCollecting()) {
                    collector.skipPrompt();
                }
                break;
            case 'w':
            case 'W':
                if (cmdStr.length() > 2) {
                    collector.setWriterId(cmdStr.substring(2).c_str());
                    Serial.printf("[COLLECTOR] Writer set to: %s\n", collector.getMetadata().writerId);
                }
                break;
            case 'd':
            case 'D':
                if (cmdStr.length() > 2 && (cmdStr.charAt(1) == ' ' || cmdStr.charAt(1) == '=')) {
                    collector.setInputDevice(cmdStr.substring(2).c_str());
                    Serial.printf("[COLLECTOR] Device set to: %s\n", collector.getMetadata().inputDevice);
                } else {
                    canvas.dumpStrokesJSON();
                }
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
