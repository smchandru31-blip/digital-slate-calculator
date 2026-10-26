#pragma once

#include "CYD_Display_Config.hpp"
#include "StrokeStore.hpp"

class CanvasEngine {
public:
    static constexpr int16_t HEADER_HEIGHT = 28;
    static constexpr uint16_t COLOR_BG     = TFT_BLACK;
    static constexpr uint16_t COLOR_HEADER = 0x18E3; // Dark slate
    static constexpr uint16_t COLOR_INK    = TFT_WHITE;
    static constexpr uint16_t COLOR_BUTTON = 0x2965; // Slate gray
    static constexpr uint16_t COLOR_BTN_TXT= TFT_WHITE;

    CanvasEngine(LGFX_CYD& display, StrokeStore& store);

    void init();
    void update();
    void redraw();
    void clear();
    void undo();

    void dumpStrokesJSON();

private:
    LGFX_CYD& _tft;
    StrokeStore& _store;

    bool _pen_down;
    int16_t _last_x;
    int16_t _last_y;
    uint32_t _last_touch_ms;

    // UI Buttons
    bool isInsideUndoBtn(int16_t x, int16_t y) const;
    bool isInsideClearBtn(int16_t x, int16_t y) const;

    void drawHeader();
    void drawInkSegment(int16_t x0, int16_t y0, int16_t x1, int16_t y1);
};
