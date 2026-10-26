#pragma once

#include <Arduino.h>
#include "CYD_Display_Config.hpp"
#include "StrokeStore.hpp"

namespace DigitalSlate {

struct CollectionMetadata {
    char writerId[16];     // e.g. "W01"
    char inputDevice[16];  // "stylus" or "finger"
    int currentPromptIndex;
    int totalCollected;
};

class CollectorEngine {
public:
    static const char* PROMPT_LIST[];
    static const size_t PROMPT_COUNT;

    static constexpr int16_t BANNER_HEIGHT   = 30;
    static constexpr int16_t TOOLBAR_HEIGHT  = 34;
    static constexpr int16_t SCREEN_WIDTH    = 320;
    static constexpr int16_t SCREEN_HEIGHT   = 240;

    CollectorEngine(LGFX_CYD& display, StrokeStore& store);

    void init();
    void startCollection();
    void stopCollection();
    bool isCollecting() const { return _active; }

    void nextPrompt();
    void prevPrompt();
    void skipPrompt();
    void redoCurrent();
    void saveAndNext();

    void setWriterId(const char* id);
    void setInputDevice(const char* device);
    void setPromptIndex(int idx);

    const char* getCurrentPrompt() const;
    const CollectionMetadata& getMetadata() const { return _meta; }

    /**
     * @brief Check if a touch coordinate hits collection UI buttons.
     * @return true if button handled, false if touch is inside the writing canvas.
     */
    bool handleTouch(int16_t x, int16_t y);
    void drawUI();

    /**
     * @brief Stream current sample formatted as JSON over Serial.
     */
    void emitLabeledSampleJSON();

private:
    LGFX_CYD& _tft;
    StrokeStore& _store;
    CollectionMetadata _meta;
    bool _active;

    // Bottom toolbar button boundaries
    bool isInsideSaveBtn(int16_t x, int16_t y) const;
    bool isInsideRedoBtn(int16_t x, int16_t y) const;
    bool isInsideSkipBtn(int16_t x, int16_t y) const;
    bool isInsideExitBtn(int16_t x, int16_t y) const;

    void drawTopBanner();
    void drawBottomToolbar();
};

} // namespace DigitalSlate
