#include <Arduino.h>
#include "esp_chip_info.h"
#include "esp_heap_caps.h"
#include "CYD_Display_Config.hpp"
#include "StrokeStore.hpp"
#include "CanvasEngine.hpp"
#include "CollectorEngine.hpp"
#include "ClassifierEngine.hpp"
#include "ExpressionEngine.hpp"

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
static DigitalSlate::ExpressionEngine expressionEngine;

// Non-blocking FreeRTOS inference architecture
// Core 1 runs touch polling and immediate ink rendering at ~120 Hz.
// Core 0 runs ExpressionEngine TinyML reconstruction asynchronously without blocking touch.
static TaskHandle_t s_inferenceTaskHandle = NULL;
static volatile bool s_inferenceRequested = false;
static volatile bool s_inferenceBusy = false;
static volatile bool s_hasNewResult = false;
static DigitalSlate::ExpressionResult s_lastExprResult;

// Background task running on Core 0 (PRO_CPU_NUM)
static void inferenceTask(void* pvParameters) {
    (void)pvParameters;
    while (true) {
        // Wait for notification or request from Core 1
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        s_inferenceBusy = true;
        // Run full expression reconstruction on Core 0
        s_lastExprResult = expressionEngine.processStrokes(strokeStore, classifier);
        s_inferenceBusy = false;
        s_hasNewResult = true;

        // Stream real-time telemetry over Serial
        if (s_lastExprResult.status == DigitalSlate::ExpressionStatus::EXPRESSION_RECOGNIZED) {
            Serial.printf("[CALCULATOR] Expression: \"%s\" = %s (Latency: %lu us)\n",
                          s_lastExprResult.reconstructed_expr.c_str(),
                          s_lastExprResult.calc_result.formatted,
                          (unsigned long)s_lastExprResult.total_latency_us);
        } else if (s_lastExprResult.status == DigitalSlate::ExpressionStatus::UNCERTAIN_SYMBOLS) {
            Serial.printf("[CALCULATOR] UNCERTAIN (%d symbols): \"%s\" = %s (Latency: %lu us)\n",
                          (int)s_lastExprResult.uncertain_symbol_count,
                          s_lastExprResult.reconstructed_expr.c_str(),
                          s_lastExprResult.calc_result.formatted,
                          (unsigned long)s_lastExprResult.total_latency_us);
        } else if (s_lastExprResult.status == DigitalSlate::ExpressionStatus::WAITING_FOR_TRIGGER) {
            Serial.printf("[CALCULATOR] In progress: \"%s\" (waiting for trigger line)\n",
                          s_lastExprResult.reconstructed_expr.c_str());
        } else if (s_lastExprResult.status == DigitalSlate::ExpressionStatus::AMBIGUOUS_TRIGGER) {
            Serial.println(F("[CALCULATOR] Ambiguous trigger line gesture detected."));
        } else if (s_lastExprResult.status == DigitalSlate::ExpressionStatus::SYNTAX_ERROR) {
            Serial.printf("[CALCULATOR] Syntax error in expression \"%s\": %s\n",
                          s_lastExprResult.reconstructed_expr.c_str(),
                          s_lastExprResult.calc_result.getErrorMessage());
        }
    }
}

void triggerAsyncInference() {
    if (strokeStore.getTotalPointCount() == 0 || s_inferenceBusy) return;
    if (s_inferenceTaskHandle != NULL) {
        xTaskNotifyGive(s_inferenceTaskHandle);
    }
}

void displayExpressionResult(const DigitalSlate::ExpressionResult& res) {
    // Top banner styling (y: 2..22)
    uint16_t bannerColor = 0x2104; // Dark slate
    if (res.status == DigitalSlate::ExpressionStatus::EXPRESSION_RECOGNIZED) {
        bannerColor = 0x03E0; // Green
    } else if (res.status == DigitalSlate::ExpressionStatus::UNCERTAIN_SYMBOLS) {
        bannerColor = 0xFD20; // Amber
    } else if (res.status == DigitalSlate::ExpressionStatus::AMBIGUOUS_TRIGGER ||
               res.status == DigitalSlate::ExpressionStatus::SYNTAX_ERROR) {
        bannerColor = 0xF800; // Red
    }

    tft.fillRect(60, 2, 200, 20, bannerColor);
    tft.setTextColor(TFT_WHITE, bannerColor);
    tft.setTextSize(1);
    tft.setCursor(66, 6);

    if (res.status == DigitalSlate::ExpressionStatus::EXPRESSION_RECOGNIZED) {
        tft.printf("%s = %s  (%lums)",
                   res.reconstructed_expr.c_str(),
                   res.calc_result.formatted,
                   (unsigned long)(res.total_latency_us / 1000));

        // Draw bold result beneath the trigger line if coordinates exist
        if (res.trigger_stroke_index >= 0) {
            int16_t ans_x = res.trigger_bbox.xMin + 10;
            int16_t ans_y = res.trigger_bbox.yMax + 8;
            if (ans_y < 210) {
                tft.setTextSize(2);
                tft.setTextColor(0x03E0, TFT_WHITE);
                tft.setCursor(ans_x, ans_y);
                tft.printf("= %s", res.calc_result.formatted);
                tft.setTextSize(1);
            }
        }
    } else if (res.status == DigitalSlate::ExpressionStatus::UNCERTAIN_SYMBOLS) {
        if (res.first_uncertain_index >= 0 && (size_t)res.first_uncertain_index < res.symbols.size()) {
            const auto& sym = res.symbols[res.first_uncertain_index];
            tft.printf("? '%c' vs '%c' (Tap to fix) = %s",
                       sym.label, sym.top2_label,
                       res.calc_result.isOk() ? res.calc_result.formatted : "?");

            // Highlight uncertain symbol with an amber outline box
            tft.drawRect(sym.bbox.xMin - 2, sym.bbox.yMin - 2,
                         sym.bbox.width() + 4, sym.bbox.height() + 4, 0xFD20);
        }
    } else if (res.status == DigitalSlate::ExpressionStatus::WAITING_FOR_TRIGGER) {
        tft.printf("Expr: %s (draw _ below)", res.reconstructed_expr.c_str());
    } else if (res.status == DigitalSlate::ExpressionStatus::AMBIGUOUS_TRIGGER) {
        tft.print("Ambiguous line: Redraw line");
    } else if (res.status == DigitalSlate::ExpressionStatus::SYNTAX_ERROR) {
        tft.printf("Error: %s", res.calc_result.getErrorMessage());
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
        displayExpressionResult(s_lastExprResult);
    }

    // Check if user tapped an uncertain symbol to toggle between candidates
    if (s_lastExprResult.uncertain_symbol_count > 0 && !collector.isCollecting()) {
        int32_t tx = 0, ty = 0;
        if (tft.getTouch(&tx, &ty)) {
            for (size_t i = 0; i < s_lastExprResult.symbols.size(); ++i) {
                if (!s_lastExprResult.symbols[i].is_confident &&
                    s_lastExprResult.symbols[i].bbox.contains((int16_t)tx, (int16_t)ty)) {
                    expressionEngine.toggleSymbolCandidate(s_lastExprResult, i);
                    displayExpressionResult(s_lastExprResult);
                    delay(200); // Debounce toggle tap
                    break;
                }
            }
        }
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
