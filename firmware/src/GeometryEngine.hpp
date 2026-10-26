#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace DigitalSlate {

struct Point2D {
    int16_t x;
    int16_t y;
    uint32_t timestampMs;

    Point2D() : x(0), y(0), timestampMs(0) {}
    Point2D(int16_t x_, int16_t y_, uint32_t t_ = 0) : x(x_), y(y_), timestampMs(t_) {}
};

struct Rect2D {
    int16_t xMin;
    int16_t yMin;
    int16_t xMax;
    int16_t yMax;

    Rect2D();
    Rect2D(int16_t x0, int16_t y0, int16_t x1, int16_t y1);

    int16_t width() const { return (xMax >= xMin) ? (xMax - xMin + 1) : 0; }
    int16_t height() const { return (yMax >= yMin) ? (yMax - yMin + 1) : 0; }
    int16_t centerX() const { return (xMin + xMax) / 2; }
    int16_t centerY() const { return (yMin + yMax) / 2; }

    bool contains(int16_t x, int16_t y) const;
    bool intersects(const Rect2D& other) const;
    void expandToInclude(int16_t x, int16_t y);
    void expandToInclude(const Rect2D& other);
};

struct StrokeMetrics {
    Rect2D bbox;
    float pathLength;       // Sum of segment lengths
    float displacement;     // Straight-line distance between start and end
    float straightness;     // displacement / pathLength [0.0 .. 1.0]
    float aspectRatio;      // width / height
    float angleDeg;         // Angle of displacement vector relative to positive X-axis (-180 .. +180)
    int16_t startX, startY;
    int16_t endX, endY;
    size_t pointCount;
};

enum class TriggerVerdict {
    NO_TRIGGER = 0,         // Clearly not a trigger line (e.g. normal stroke, minus, accidental dot)
    TRIGGER_DETECTED,       // Confident trigger line detected
    AMBIGUOUS               // Ambiguous/borderline gesture requiring user confirmation/retry
};

struct TriggerResult {
    TriggerVerdict verdict;
    int triggerStrokeIndex; // Index of the trigger stroke (-1 if none)
    Rect2D contentBBox;     // Bounding box of content strokes (excluding trigger)
    float confidence;       // 0.0 to 1.0 confidence score
    const char* reason;     // Diagnostic explanation
};

class GeometryEngine {
public:
    /**
     * @brief Compute geometric metrics for a single stroke.
     */
    static StrokeMetrics computeStrokeMetrics(const Point2D* points, size_t count);

    /**
     * @brief Compute the collective bounding box for an array of points.
     */
    static Rect2D computeBoundingBox(const Point2D* points, size_t count);

    /**
     * @brief Check if two line segments (p1-p2) and (p3-p4) intersect.
     */
    static bool segmentsIntersect(Point2D p1, Point2D p2, Point2D p3, Point2D p4);

    /**
     * @brief Check if two strokes intersect anywhere along their polyline paths.
     */
    static bool strokesIntersect(const Point2D* stroke1, size_t count1,
                                 const Point2D* stroke2, size_t count2);

    /**
     * @brief Detect deliberate trigger line beneath written mathematical content.
     * @param strokes Vector of strokes (each stroke is a vector of Point2D)
     * @return TriggerResult with verdict (NO_TRIGGER, TRIGGER_DETECTED, AMBIGUOUS)
     */
    static TriggerResult detectTriggerLine(const std::vector<std::vector<Point2D>>& strokes);
};

} // namespace DigitalSlate
