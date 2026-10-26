#pragma once

#include <stdint.h>
#include <stddef.h>
#ifdef ARDUINO
#include <Arduino.h>
#endif

/**
 * High-performance, zero-dynamic-allocation stroke storage engine.
 * Stores raw handwriting points, timestamps, and stroke segmentation boundaries.
 */
struct StrokePoint {
    int16_t x;
    int16_t y;
    uint32_t t; // Timestamp in milliseconds
};

struct StrokeSegment {
    uint16_t start_index;
    uint16_t point_count;
};

class StrokeStore {
public:
    static constexpr size_t MAX_POINTS  = 1200;
    static constexpr size_t MAX_STROKES = 32;

    StrokeStore();

    void clear();
    bool startStroke();
    bool addPoint(int16_t x, int16_t y, uint32_t t);
    void endStroke();
    bool undoLastStroke();

    size_t getStrokeCount() const;
    size_t getTotalPointCount() const;
    bool isStrokeActive() const;

    const StrokeSegment* getStroke(size_t index) const;
    const StrokePoint* getPoint(size_t index) const;

    // Bounding box calculation for stroke or entire store
    void getBounds(int16_t& min_x, int16_t& min_y, int16_t& max_x, int16_t& max_y) const;
    void getStrokeBounds(size_t stroke_index, int16_t& min_x, int16_t& min_y, int16_t& max_x, int16_t& max_y) const;

    // Export strokes as standard JSON stream
    template <typename StreamType>
    void serializeJSON(StreamType& out) const {
        out.print("{\"format\":\"digital_slate_v1\",\"stroke_count\":");
        out.print((unsigned)_stroke_count);
        out.print(",\"total_points\":");
        out.print((unsigned)_total_points);
        out.print(",\"strokes\":[");

        for (size_t s = 0; s < _stroke_count; ++s) {
            if (s > 0) out.print(",");
            out.print("{\"id\":");
            out.print((unsigned)s);
            out.print(",\"points\":[");

            const StrokeSegment& seg = _strokes[s];
            for (uint16_t p = 0; p < seg.point_count; ++p) {
                if (p > 0) out.print(",");
                const StrokePoint& pt = _points[seg.start_index + p];
                out.print("[");
                out.print(pt.x);
                out.print(",");
                out.print(pt.y);
                out.print(",");
                out.print(pt.t);
                out.print("]");
            }
            out.print("]}");
        }
        out.println("]}");
    }

private:
    StrokePoint _points[MAX_POINTS];
    StrokeSegment _strokes[MAX_STROKES];
    size_t _total_points;
    size_t _stroke_count;
    bool _active_stroke;
};
