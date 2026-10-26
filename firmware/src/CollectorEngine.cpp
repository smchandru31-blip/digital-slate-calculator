#include "CollectorEngine.hpp"
#include <cstring>
#include <cstdio>

namespace DigitalSlate {

const char* CollectorEngine::PROMPT_LIST[] = {
    // Standard Digits (0 - 9)
    "0", "1", "2", "3", "4", "5", "6", "7", "8", "9",
    // Standard Operators
    "+", "-", "*", "/",
    // Targeted Confusion Pairs (Extra Emphasis)
    "3", "8",
    "1", "7",
    "0", "6",
    "5", "6",
    "2", "7",
    "4", "9"
};

const size_t CollectorEngine::PROMPT_COUNT = sizeof(PROMPT_LIST) / sizeof(PROMPT_LIST[0]);

CollectorEngine::CollectorEngine(LGFX_CYD& display, StrokeStore& store)
    : _tft(display), _store(store), _active(false) {
    strncpy(_meta.writerId, "W01", sizeof(_meta.writerId));
    strncpy(_meta.inputDevice, "stylus", sizeof(_meta.inputDevice));
    _meta.currentPromptIndex = 0;
    _meta.totalCollected = 0;
}

void CollectorEngine::init() {
    // Default initialization
    _active = false;
}

void CollectorEngine::startCollection() {
    _active = true;
    _store.clear();
    drawUI();
    Serial.println(F("[COLLECTOR] Mode ACTIVE. Ready to collect labeled samples."));
    Serial.printf("[COLLECTOR] Writer: %s, Device: %s, Current Prompt: %s\n",
                  _meta.writerId, _meta.inputDevice, getCurrentPrompt());
}

void CollectorEngine::stopCollection() {
    _active = false;
    _store.clear();
    Serial.println(F("[COLLECTOR] Mode STOPPED. Returned to normal calculator canvas."));
}

const char* CollectorEngine::getCurrentPrompt() const {
    if (_meta.currentPromptIndex >= 0 && (size_t)_meta.currentPromptIndex < PROMPT_COUNT) {
        return PROMPT_LIST[_meta.currentPromptIndex];
    }
    return "?";
}

void CollectorEngine::setWriterId(const char* id) {
    if (!id || strlen(id) == 0) return;
    strncpy(_meta.writerId, id, sizeof(_meta.writerId) - 1);
    _meta.writerId[sizeof(_meta.writerId) - 1] = '\0';
    if (_active) drawTopBanner();
}

void CollectorEngine::setInputDevice(const char* device) {
    if (!device || strlen(device) == 0) return;
    strncpy(_meta.inputDevice, device, sizeof(_meta.inputDevice) - 1);
    _meta.inputDevice[sizeof(_meta.inputDevice) - 1] = '\0';
    if (_active) drawTopBanner();
}

void CollectorEngine::setPromptIndex(int idx) {
    if (idx >= 0 && (size_t)idx < PROMPT_COUNT) {
        _meta.currentPromptIndex = idx;
        if (_active) drawTopBanner();
    }
}

void CollectorEngine::nextPrompt() {
    _meta.currentPromptIndex = (_meta.currentPromptIndex + 1) % PROMPT_COUNT;
    if (_active) drawTopBanner();
}

void CollectorEngine::prevPrompt() {
    _meta.currentPromptIndex = (_meta.currentPromptIndex + PROMPT_COUNT - 1) % PROMPT_COUNT;
    if (_active) drawTopBanner();
}

void CollectorEngine::skipPrompt() {
    Serial.printf("[COLLECTOR] Skipped prompt: %s\n", getCurrentPrompt());
    _store.clear();
    nextPrompt();
    drawUI();
}

void CollectorEngine::redoCurrent() {
    Serial.printf("[COLLECTOR] Redo current prompt: %s\n", getCurrentPrompt());
    _store.clear();
    drawUI();
}

void CollectorEngine::saveAndNext() {
    if (_store.getStrokeCount() == 0) {
        // Flash warning
        _tft.fillRect(100, 4, 180, 22, TFT_RED);
        _tft.setTextColor(TFT_WHITE, TFT_RED);
        _tft.drawString("Draw symbol first!", 110, 8);
        delay(350);
        drawTopBanner();
        return;
    }

    // Emit labeled sample over Serial
    emitLabeledSampleJSON();

    _meta.totalCollected++;
    nextPrompt();
    _store.clear();
    drawUI();
}

void CollectorEngine::emitLabeledSampleJSON() {
    Serial.println(F("--- BEGIN LABELED SAMPLE ---"));
    Serial.print(F("{\"format\":\"digital_slate_v1\",\"type\":\"labeled_sample\",\"label\":\""));
    Serial.print(getCurrentPrompt());
    Serial.print(F("\",\"prompt\":\""));
    Serial.print(getCurrentPrompt());
    Serial.print(F("\",\"sample_index\":"));
    Serial.print(_meta.totalCollected + 1);
    Serial.print(F(",\"writer_id\":\""));
    Serial.print(_meta.writerId);
    Serial.print(F("\",\"input_device\":\""));
    Serial.print(_meta.inputDevice);
    Serial.print(F("\",\"timestamp_ms\":"));
    Serial.print(millis());
    Serial.print(F(",\"stroke_count\":"));
    Serial.print(_store.getStrokeCount());
    Serial.print(F(",\"total_points\":"));
    Serial.print(_store.getTotalPointCount());
    Serial.print(F(",\"strokes\":["));

    for (size_t s = 0; s < _store.getStrokeCount(); ++s) {
        if (s > 0) Serial.print(F(","));
        const StrokeSegment* seg = _store.getStroke(s);
        if (!seg) continue;
        Serial.print(F("{\"stroke_index\":"));
        Serial.print(s);
        Serial.print(F(",\"point_count\":"));
        Serial.print(seg->point_count);
        Serial.print(F(",\"points\":["));

        for (size_t p = 0; p < seg->point_count; ++p) {
            if (p > 0) Serial.print(F(","));
            const StrokePoint* pt = _store.getPoint(seg->start_index + p);
            if (pt) {
                Serial.print(F("{\"x\":"));
                Serial.print(pt->x);
                Serial.print(F(",\"y\":"));
                Serial.print(pt->y);
                Serial.print(F(",\"t\":"));
                Serial.print(pt->t);
                Serial.print(F("}"));
            }
        }
        Serial.print(F("]}"));
    }
    Serial.println(F("]}"));
    Serial.println(F("--- END LABELED SAMPLE ---"));
}

// Button Hit Tests
bool CollectorEngine::isInsideExitBtn(int16_t x, int16_t y) const {
    return (x >= 4 && x <= 56 && y >= 206 && y <= 236);
}

bool CollectorEngine::isInsideRedoBtn(int16_t x, int16_t y) const {
    return (x >= 62 && x <= 122 && y >= 206 && y <= 236);
}

bool CollectorEngine::isInsideSkipBtn(int16_t x, int16_t y) const {
    return (x >= 128 && x <= 188 && y >= 206 && y <= 236);
}

bool CollectorEngine::isInsideSaveBtn(int16_t x, int16_t y) const {
    return (x >= 194 && x <= 316 && y >= 206 && y <= 236);
}

bool CollectorEngine::handleTouch(int16_t x, int16_t y) {
    if (!_active) return false;

    // Bottom toolbar buttons
    if (y >= 200) {
        if (isInsideSaveBtn(x, y)) {
            saveAndNext();
            return true;
        }
        if (isInsideRedoBtn(x, y)) {
            redoCurrent();
            return true;
        }
        if (isInsideSkipBtn(x, y)) {
            skipPrompt();
            return true;
        }
        if (isInsideExitBtn(x, y)) {
            stopCollection();
            return true;
        }
        return true; // Consume any other touch in toolbar area
    }

    // Top banner area touch (e.g. tap prompt to cycle)
    if (y < BANNER_HEIGHT) {
        return true; // Consume touch in header
    }

    return false; // Free writing canvas
}

void CollectorEngine::drawTopBanner() {
    _tft.fillRect(0, 0, SCREEN_WIDTH, BANNER_HEIGHT, 0x18E3); // Dark slate
    _tft.drawFastHLine(0, BANNER_HEIGHT, SCREEN_WIDTH, 0x39E7);

    // Prompt Text
    _tft.setTextSize(1);
    _tft.setTextColor(TFT_WHITE, 0x18E3);
    char buf[48];
    snprintf(buf, sizeof(buf), "WRITE: '%s'", getCurrentPrompt());
    _tft.drawString(buf, 10, 8);

    // Big prompt highlight in center
    _tft.setTextSize(2);
    _tft.setTextColor(TFT_YELLOW, 0x18E3);
    _tft.drawString(getCurrentPrompt(), 145, 6);

    // Meta details (Right)
    _tft.setTextSize(1);
    _tft.setTextColor(0x7BEF, 0x18E3);
    snprintf(buf, sizeof(buf), "[%d/%d] %s", _meta.currentPromptIndex + 1, (int)PROMPT_COUNT, _meta.writerId);
    _tft.drawString(buf, 205, 8);
}

void CollectorEngine::drawBottomToolbar() {
    int16_t y0 = SCREEN_HEIGHT - TOOLBAR_HEIGHT;
    _tft.fillRect(0, y0, SCREEN_WIDTH, TOOLBAR_HEIGHT, 0x18E3);
    _tft.drawFastHLine(0, y0, SCREEN_WIDTH, 0x39E7);

    // [EXIT] button
    _tft.fillRoundRect(4, y0 + 3, 52, 28, 4, 0x4208); // Dark gray
    _tft.setTextColor(TFT_WHITE, 0x4208);
    _tft.drawString("EXIT", 16, y0 + 10);

    // [REDO] button
    _tft.fillRoundRect(62, y0 + 3, 60, 28, 4, 0x2124); // Dark blue-gray
    _tft.setTextColor(TFT_WHITE, 0x2124);
    _tft.drawString("REDO", 76, y0 + 10);

    // [SKIP] button
    _tft.fillRoundRect(128, y0 + 3, 60, 28, 4, 0x2124);
    _tft.setTextColor(TFT_WHITE, 0x2124);
    _tft.drawString("SKIP", 142, y0 + 10);

    // [SAVE & NEXT] button (Highlight Green)
    _tft.fillRoundRect(194, y0 + 3, 122, 28, 4, 0x04C0); // Dark Green
    _tft.setTextColor(TFT_WHITE, 0x04C0);
    _tft.drawString("SAVE & NEXT", 212, y0 + 10);
}

void CollectorEngine::drawUI() {
    if (!_active) return;
    _tft.fillScreen(TFT_BLACK);
    drawTopBanner();
    drawBottomToolbar();
}

} // namespace DigitalSlate
