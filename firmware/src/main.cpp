#include <Arduino.h>
#include "esp_chip_info.h"
#include "esp_heap_caps.h"
#include "CYD_Display_Config.hpp"
#include "StrokeStore.hpp"
#include "CanvasEngine.hpp"
#include "CollectorEngine.hpp"
#include "ClassifierEngine.hpp"

// Hardware Pin Definitions for Auxiliary Peripherals
#define RGB_LED_RED   4
#define RGB_LED_GREEN 16
#define RGB_LED_BLUE  17

// Global hardware and engine instances
static LGFX_CYD tft;
static StrokeStore strokeStore;
static CanvasEngine canvas(tft, strokeStore);
static DigitalSlate::CollectorEngine collector(tft, strokeStore);
static DigitalSlate::ClassifierEngine classifier;

// Non-blocking FreeRTOS inference architecture
// Core 1 runs touch polling and immediate ink rendering at ~120 Hz.
// Core 0 runs TinyML neural inference asynchronously without blocking touch.
static TaskHandle_t s_inferenceTaskHandle = NULL;
static volatile bool s_inferenceRequested = false;
static volatile bool s_inferenceBusy = false;
static volatile bool s_hasNewResult = false;
static DigitalSlate::ClassificationResult s_lastResult;

// Background task running on Core 0 (PRO_CPU_NUM)
static void inferenceTask(void* pvParameters) {
    (void)pvParameters;
    while (true) {
        // Wait for notification or request
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        s_inferenceBusy = true;
        // Run neural inference on Core 0
        s_lastResult = classifier.classifyStrokes(strokeStore);
        s_inferenceBusy = false;
        s_hasNewResult = true;

        // Stream real-time telemetry over Serial
        Serial.printf("[INFERENCE] Top-1: '%c' (%.1f%%) | Top-2: '%c' (%.1f%%) | Margin: %.1f%% | Status: %s | Latency: %lu us (%lu us inference)\n",
                      s_lastResult.top1_label, s_lastResult.top1_confidence * 100.0f,
                      s_lastResult.top2_label, s_lastResult.top2_confidence * 100.0f,
                      s_lastResult.confidence_margin * 100.0f,
                      s_lastResult.is_confident ? "CONFIDENT" : 
                        (s_lastResult.status == DigitalSlate::RecognitionStatus::REJECT_AMBIGUOUS_PAIR ? "AMBIGUOUS_PAIR" : "LOW_CONFIDENCE"),
                      (unsigned long)s_lastResult.total_us, (unsigned long)s_lastResult.inference_us);
    }
}

void triggerAsyncInference() {
    if (strokeStore.getTotalPointCount() == 0 || s_inferenceBusy) return;
    if (s_inferenceTaskHandle != NULL) {
        xTaskNotifyGive(s_inferenceTaskHandle);
    }
}

void displayInferenceResult(const DigitalSlate::ClassificationResult& res) {
    // Render inference banner on top bar (y: 2..22)
    uint16_t bannerColor = res.is_confident ? 0x03E0 /* Green */ : 0xF800 /* Red */;
    tft.fillRect(60, 2, 200, 20, bannerColor);
    tft.setTextColor(TFT_WHITE, bannerColor);
    tft.setTextSize(1);

    if (res.is_confident) {
        tft.setCursor(66, 6);
        tft.printf("'%c' (%.0f%%)  Top-2: '%c' (%.0f%%)  %lums",
                   res.top1_label, res.top1_confidence * 100.0f,
                   res.top2_label, res.top2_confidence * 100.0f,
                   (unsigned long)(res.total_us / 1000));
    } else {
        tft.setCursor(66, 6);
        tft.printf("REJECT: '%c' (%.0f%%) vs '%c' (%.0f%%)",
                   res.top1_label, res.top1_confidence * 100.0f,
                   res.top2_label, res.top2_confidence * 100.0f);
    }
}

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
    Serial.println(F("  DIGITAL SLATE - P7 ON-DEVICE INFERENCE FIRMWARE "));
    Serial.println(F("=================================================="));

    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);
    Serial.printf("SoC Cores: %d @ %lu MHz\n", chip_info.cores, (unsigned long)getCpuFrequencyMhz());
    Serial.printf("Flash: %lu KB\n", (unsigned long)(ESP.getFlashChipSize() / 1024));
    Serial.printf("Free Internal Heap: %lu bytes\n", (unsigned long)heap_caps_get_free_size(MALLOC_CAP_INTERNAL));

    // Initialize display and touch
    tft.init();
    tft.setRotation(1);     // Landscape (320 x 240, USB on left)
    tft.setBrightness(255); // Full backlight

    // Initialize handwriting canvas UI
    canvas.init();
    collector.init();

    // Spawn Non-blocking Inference Task on Core 0 (PRO_CPU_NUM)
    // 4096 words stack (~16 KB), Priority 1
    xTaskCreatePinnedToCore(
        inferenceTask,
        "inferenceTask",
        4096,
        NULL,
        1,
        &s_inferenceTaskHandle,
        0 // Core 0
    );

    Serial.println(F("[CANVAS] Ready for handwriting and real-time TinyML inference."));
    Serial.println(F("[COMMANDS] 'i' = Run Inference on current canvas"));
    Serial.println(F("[COMMANDS] 'm' = Toggle Collection Mode"));
    Serial.println(F("[COMMANDS] 'u' = Undo, 'c' = Clear, 'd' = Dump JSON"));
}

void loop() {
    static uint32_t last_led_blink = 0;
    static bool led_state = false;
    static size_t last_stroke_count = 0;

    // Heartbeat LED on Blue LED (GPIO 17) every 500ms
    if (millis() - last_led_blink > 500) {
        last_led_blink = millis();
        led_state = !led_state;
        digitalWrite(RGB_LED_BLUE, led_state ? LOW : HIGH);
    }

    // Touch sampling and ink rendering (Core 1 - runs unimpeded at 100+ Hz)
    if (collector.isCollecting()) {
        int32_t tx = 0, ty = 0;
        if (tft.getTouch(&tx, &ty)) {
            if (!collector.handleTouch((int16_t)tx, (int16_t)ty)) {
                canvas.update();
            }
        } else {
            canvas.update();
        }
    } else {
        canvas.update();

        // Check if user completed a stroke (trigger background inference on Core 0)
        size_t current_strokes = strokeStore.getStrokeCount();
        if (current_strokes > 0 && current_strokes != last_stroke_count && !strokeStore.isStrokeActive()) {
            last_stroke_count = current_strokes;
            triggerAsyncInference();
        }
    }

    // Check if Core 0 produced a new inference result
    if (s_hasNewResult) {
        s_hasNewResult = false;
        displayInferenceResult(s_lastResult);
    }

    // Process Serial commands
    if (Serial.available() > 0) {
        String cmdStr = Serial.readStringUntil('\n');
        cmdStr.trim();
        if (cmdStr.length() == 0) return;

        char cmd = cmdStr.charAt(0);
        switch (cmd) {
            case 'i':
            case 'I':
                Serial.println(F("[INFERENCE] Triggered manual inference."));
                triggerAsyncInference();
                break;
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
                    last_stroke_count = 0;
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
                last_stroke_count = strokeStore.getStrokeCount();
                if (last_stroke_count > 0) triggerAsyncInference();
                break;
            case 'c':
            case 'C':
                canvas.clear();
                last_stroke_count = 0;
                break;
            default:
                break;
        }
    }
}
