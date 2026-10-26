#include <Arduino.h>
#include "esp_chip_info.h"
#include "esp_heap_caps.h"
#include "CYD_Display_Config.hpp"

// Hardware Pin Definitions for Auxiliary Peripherals
#define RGB_LED_RED   4
#define RGB_LED_GREEN 16
#define RGB_LED_BLUE  17
#define LDR_PIN       34

// Display instance
static LGFX_CYD tft;

// State tracking
static uint32_t touch_count = 0;
static bool target_hit[5] = {false, false, false, false, false};

// Corner target locations (x, y) in landscape 320x240
struct Target {
    int16_t x;
    int16_t y;
    const char* label;
};

static const Target targets[5] = {
    {15,  15,  "TL"},
    {305, 15,  "TR"},
    {15,  225, "BL"},
    {305, 225, "BR"},
    {160, 120, "C"}
};

void drawTarget(const Target& t, bool hit) {
    uint16_t color = hit ? TFT_GREEN : TFT_CYAN;
    tft.drawCircle(t.x, t.y, 8, color);
    tft.drawCircle(t.x, t.y, 4, color);
    tft.drawLine(t.x - 10, t.y, t.x + 10, t.y, color);
    tft.drawLine(t.x, t.y - 10, t.x, t.y + 10, color);
    tft.setTextSize(1);
    tft.setTextColor(hit ? TFT_GREEN : TFT_WHITE);
    tft.drawString(t.label, t.x + 10, t.y + 2);
}

void printSystemInfo() {
    Serial.println("==================================================");
    Serial.println("  DIGITAL SLATE CALCULATOR - P1 DIAGNOSTIC PROBE  ");
    Serial.println("==================================================");
    
    // Chip Model & Features
    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);
    Serial.printf("Chip Model         : %s (rev %d)\n", ESP.getChipModel(), chip_info.revision);
    Serial.printf("Cores              : %d @ %u MHz\n", chip_info.cores, (unsigned)getCpuFrequencyMhz());
    Serial.printf("Flash Size         : %lu KB (%s mode)\n", 
                  (unsigned long)(ESP.getFlashChipSize() / 1024),
                  (ESP.getFlashChipMode() == 0 ? "QIO" : "DIO"));
    Serial.printf("Flash Speed        : %lu MHz\n", (unsigned long)(ESP.getFlashChipSpeed() / 1000000));
    
    // PSRAM
    bool psram = psramFound();
    Serial.printf("PSRAM Detected     : %s\n", psram ? "YES" : "NO (Expected on CYD 2.8)");
    if (psram) {
        Serial.printf("PSRAM Size         : %lu KB\n", (unsigned long)(ESP.getPsramSize() / 1024));
    }
    
    // Heap Memory
    Serial.printf("Total Free Heap    : %lu bytes\n", (unsigned long)ESP.getFreeHeap());
    Serial.printf("Min Free Heap Ever : %lu bytes\n", (unsigned long)ESP.getMinFreeHeap());
    Serial.printf("Internal Free Heap : %lu bytes\n", (unsigned long)heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
    Serial.println("--------------------------------------------------");
}

void runColorTest() {
    Serial.println("[DISPLAY] Running primary color screen fill test...");
    
    const struct { uint16_t color; const char* name; } colors[] = {
        {TFT_RED,   "RED"},
        {TFT_GREEN, "GREEN"},
        {TFT_BLUE,  "BLUE"},
        {TFT_WHITE, "WHITE"},
        {TFT_BLACK, "BLACK"}
    };
    
    for (const auto& c : colors) {
        tft.fillScreen(c.color);
        tft.setTextColor(c.color == TFT_WHITE ? TFT_BLACK : TFT_WHITE);
        tft.setTextSize(2);
        tft.setTextDatum(MC_DATUM);
        tft.drawString(c.name, tft.width() / 2, tft.height() / 2);
        delay(400);
    }
}

void setupDashboard() {
    tft.fillScreen(TFT_BLACK);
    
    // Top banner
    tft.fillRect(0, 0, 320, 24, tft.color565(30, 30, 45));
    tft.drawLine(0, 24, 320, 24, TFT_DARKGRAY);
    tft.setTextSize(1);
    tft.setTextColor(TFT_WHITE);
    tft.drawString("SLATE P1 DIAGNOSTIC", 8, 8);
    
    // Status text in banner
    tft.setTextColor(TFT_YELLOW);
    tft.drawString("Touch targets with stylus", 160, 8);
    
    // Draw initial target markers
    for (int i = 0; i < 5; ++i) {
        drawTarget(targets[i], target_hit[i]);
    }
    
    // Bottom coordinate readout bar
    tft.fillRect(0, 222, 320, 18, tft.color565(20, 20, 25));
    tft.drawLine(0, 221, 320, 221, TFT_DARKGRAY);
    tft.setTextColor(TFT_LIGHTGRAY);
    tft.drawString("Raw: ----,----  Screen: (---,---)  Z:---", 8, 226);
}

void setup() {
    Serial.begin(115200);
    delay(500);

    // RGB LED init (Common anode: HIGH = OFF)
    pinMode(RGB_LED_RED, OUTPUT);
    pinMode(RGB_LED_GREEN, OUTPUT);
    pinMode(RGB_LED_BLUE, OUTPUT);
    digitalWrite(RGB_LED_RED, HIGH);
    digitalWrite(RGB_LED_GREEN, HIGH);
    digitalWrite(RGB_LED_BLUE, HIGH);

    printSystemInfo();

    // Initialize display and touch
    Serial.println("[DISPLAY] Initializing LovyanGFX controller...");
    tft.init();
    tft.setRotation(1); // Landscape: 320 x 240 (USB on left)
    tft.setBrightness(255); // Full backlight

    runColorTest();
    setupDashboard();

    Serial.println("[TOUCH] Ready. Awaiting stylus/touch input...");
    Serial.println("Format: [TOUCH] Screen:(x, y) Raw:(raw_x, raw_y) Z:pressure");
}

void loop() {
    static uint32_t last_led_toggle = 0;
    static bool led_state = false;

    // Heartbeat LED on Blue LED (GPIO 17)
    if (millis() - last_led_toggle > 500) {
        last_led_toggle = millis();
        led_state = !led_state;
        digitalWrite(RGB_LED_BLUE, led_state ? LOW : HIGH);
    }

    uint16_t x, y;
    if (tft.getTouch(&x, &y)) {
        touch_count++;
        
        // Read raw touch details
        lgfx::touch_point_t tp;
        tft.getTouchRaw(&tp, 1);

        // Print to serial
        Serial.printf("[TOUCH #%lu] Screen:(%3d, %3d) Raw:(%4d, %4d) Z:%3d\n",
                      (unsigned long)touch_count, x, y, tp.x, tp.y, tp.size);

        // Draw stylus ink point
        tft.fillCircle(x, y, 2, TFT_ORANGE);

        // Check if targets hit
        for (int i = 0; i < 5; ++i) {
            int16_t dx = (int16_t)x - targets[i].x;
            int16_t dy = (int16_t)y - targets[i].y;
            if ((dx * dx + dy * dy) <= 144) { // within 12 pixel radius
                if (!target_hit[i]) {
                    target_hit[i] = true;
                    drawTarget(targets[i], true);
                    Serial.printf("[TARGET HIT] Target %s (%d, %d) validated!\n", 
                                  targets[i].label, targets[i].x, targets[i].y);
                    
                    // Flash green LED briefly
                    digitalWrite(RGB_LED_GREEN, LOW);
                    delay(50);
                    digitalWrite(RGB_LED_GREEN, HIGH);
                }
            }
        }

        // Update coordinate readout bar
        tft.fillRect(0, 222, 320, 18, tft.color565(20, 20, 25));
        tft.setTextColor(TFT_WHITE);
        tft.setTextSize(1);
        char buf[64];
        snprintf(buf, sizeof(buf), "Raw:(%4d,%4d)  Screen:(%3d,%3d)  Hits:%d/5",
                 tp.x, tp.y, x, y,
                 (target_hit[0] + target_hit[1] + target_hit[2] + target_hit[3] + target_hit[4]));
        tft.drawString(buf, 8, 226);

        delay(15); // Debounce
    }
}
