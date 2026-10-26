#include "CanvasEngine.hpp"
#include <Arduino.h>

CanvasEngine::CanvasEngine(LGFX_CYD& display, StrokeStore& store)
    : _tft(display)
    , _store(store)
    , _pen_down(false)
    , _last_x(0)
    , _last_y(0)
    , _last_touch_ms(0) {
}

void CanvasEngine::init() {
    _tft.fillScreen(COLOR_BG);
    drawHeader();
}

bool CanvasEngine::isInsideUndoBtn(int16_t x, int16_t y) const {
    return (x >= 180 && x <= 245 && y >= 3 && y <= 25);
}

bool CanvasEngine::isInsideClearBtn(int16_t x, int16_t y) const {
    return (x >= 250 && x <= 316 && y >= 3 && y <= 25);
}

void CanvasEngine::drawHeader() {
    // Header background
    _tft.fillRect(0, 0, 320, HEADER_HEIGHT, COLOR_HEADER);
    _tft.drawLine(0, HEADER_HEIGHT, 320, HEADER_HEIGHT, TFT_DARKGRAY);

    // Title & Stroke count
    _tft.setTextSize(1);
    _tft.setTextColor(TFT_CYAN, COLOR_HEADER);
    _tft.drawString("SLATE", 8, 10);

    _tft.setTextColor(TFT_LIGHTGRAY, COLOR_HEADER);
    char buf[32];
    snprintf(buf, sizeof(buf), "Strokes: %u/%u", 
             (unsigned)_store.getStrokeCount(), (unsigned)StrokeStore::MAX_STROKES);
    _tft.drawString(buf, 55, 10);

    // Undo button [x:180, y:3, w:65, h:22]
    _tft.fillRoundRect(180, 3, 65, 22, 3, COLOR_BUTTON);
    _tft.drawRoundRect(180, 3, 65, 22, 3, TFT_LIGHTGRAY);
    _tft.setTextColor(COLOR_BTN_TXT, COLOR_BUTTON);
    _tft.setTextDatum(MC_DATUM);
    _tft.drawString("UNDO", 212, 14);

    // Clear button [x:250, y:3, w:66, h:22]
    _tft.fillRoundRect(250, 3, 66, 22, 3, COLOR_BUTTON);
    _tft.drawRoundRect(250, 3, 66, 22, 3, TFT_LIGHTGRAY);
    _tft.drawString("CLEAR", 283, 14);

    _tft.setTextDatum(TL_DATUM); // restore default
}

void CanvasEngine::drawInkSegment(int16_t x0, int16_t y0, int16_t x1, int16_t y1) {
    // 2-pixel thick smooth ink line
    _tft.drawLine(x0, y0, x1, y1, COLOR_INK);
    _tft.drawLine(x0 + 1, y0, x1 + 1, y1, COLOR_INK);
    _tft.drawLine(x0, y0 + 1, x1, y1 + 1, COLOR_INK);
}

void CanvasEngine::redraw() {
    // Clear only writing canvas area
    _tft.fillRect(0, HEADER_HEIGHT + 1, 320, 240 - (HEADER_HEIGHT + 1), COLOR_BG);
    drawHeader();

    // Replay all committed strokes
    size_t stroke_count = _store.getStrokeCount();
    for (size_t s = 0; s < stroke_count; ++s) {
        const StrokeSegment* seg = _store.getStroke(s);
        if (!seg || seg->point_count == 0) continue;

        const StrokePoint* p0 = _store.getPoint(seg->start_index);
        if (seg->point_count == 1) {
            _tft.fillCircle(p0->x, p0->y, 1, COLOR_INK);
        } else {
            for (uint16_t i = 1; i < seg->point_count; ++i) {
                const StrokePoint* p1 = _store.getPoint(seg->start_index + i);
                drawInkSegment(p0->x, p0->y, p1->x, p1->y);
                p0 = p1;
            }
        }
    }
}

void CanvasEngine::clear() {
    _store.clear();
    redraw();
    Serial.println(F("[CANVAS] Cleared all strokes."));
}

void CanvasEngine::undo() {
    if (_store.undoLastStroke()) {
        redraw();
        Serial.printf("[CANVAS] Undo last stroke. Remaining: %u strokes.\n", 
                      (unsigned)_store.getStrokeCount());
    } else {
        Serial.println(F("[CANVAS] Undo ignored: no strokes to undo."));
    }
}

void CanvasEngine::dumpStrokesJSON() {
    Serial.println(F("--- BEGIN STROKE DUMP ---"));
    _store.serializeJSON(Serial);
    Serial.println(F("--- END STROKE DUMP ---"));
}

void CanvasEngine::update() {
    uint16_t tx, ty;
    uint32_t now = millis();
    bool touched = _tft.getTouch(&tx, &ty);

    if (touched) {
        _last_touch_ms = now;

        // Check button clicks in header bar
        if (ty <= HEADER_HEIGHT) {
            // Button interaction
            if (isInsideUndoBtn((int16_t)tx, (int16_t)ty)) {
                _tft.fillRoundRect(180, 3, 65, 22, 3, TFT_MAROON);
                undo();
                delay(120); // Debounce button press
                drawHeader();
                return;
            } else if (isInsideClearBtn((int16_t)tx, (int16_t)ty)) {
                _tft.fillRoundRect(250, 3, 66, 22, 3, TFT_MAROON);
                clear();
                delay(120); // Debounce button press
                drawHeader();
                return;
            }
        }

        // Active writing in canvas area (below header)
        if (ty > HEADER_HEIGHT) {
            int16_t cur_x = (int16_t)tx;
            int16_t cur_y = (int16_t)ty;

            if (!_pen_down) {
                // Pen Down: Begin new stroke
                if (_store.startStroke()) {
                    _pen_down = true;
                    _last_x = cur_x;
                    _last_y = cur_y;
                    _store.addPoint(cur_x, cur_y, now);
                    _tft.fillCircle(cur_x, cur_y, 1, COLOR_INK);
                }
            } else {
                // Pen Drag: Movement filter
                int32_t dx = (int32_t)cur_x - _last_x;
                int32_t dy = (int32_t)cur_y - _last_y;
                int32_t dist_sq = dx * dx + dy * dy;

                // Threshold of 3 px^2: suppresses stationary touch jitter, captures handwriting
                if (dist_sq >= 3) {
                    drawInkSegment(_last_x, _last_y, cur_x, cur_y);
                    _store.addPoint(cur_x, cur_y, now);
                    _last_x = cur_x;
                    _last_y = cur_y;
                }
            }
        }
    } else {
        // Touch released
        if (_pen_down) {
            // Pen Up Debounce (35ms debounce threshold prevents phantom lifts from splitting strokes)
            if (now - _last_touch_ms > 35) {
                _store.endStroke();
                _pen_down = false;
                drawHeader(); // update stroke count

                size_t s_idx = _store.getStrokeCount() > 0 ? _store.getStrokeCount() - 1 : 0;
                const StrokeSegment* seg = _store.getStroke(s_idx);
                if (seg) {
                    Serial.printf("[STROKE #%u] Finished: %u points (Total points: %u)\n",
                                  (unsigned)s_idx + 1,
                                  (unsigned)seg->point_count,
                                  (unsigned)_store.getTotalPointCount());
                }
            }
        }
    }
}
