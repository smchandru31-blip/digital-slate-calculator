#include "GeometryEngine.hpp"
#include <cmath>
#include <algorithm>

namespace DigitalSlate {

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// ============================================================================
// Rect2D Implementation
// ============================================================================

Rect2D::Rect2D()
    : xMin(32767), yMin(32767), xMax(-32768), yMax(-32768) {}

Rect2D::Rect2D(int16_t x0, int16_t y0, int16_t x1, int16_t y1)
    : xMin(std::min(x0, x1)), yMin(std::min(y0, y1)),
      xMax(std::max(x0, x1)), yMax(std::max(y0, y1)) {}

bool Rect2D::contains(int16_t x, int16_t y) const {
    return (x >= xMin && x <= xMax && y >= yMin && y <= yMax);
}

bool Rect2D::intersects(const Rect2D& other) const {
    if (xMax < other.xMin || xMin > other.xMax) return false;
    if (yMax < other.yMin || yMin > other.yMax) return false;
    return true;
}

void Rect2D::expandToInclude(int16_t x, int16_t y) {
    if (x < xMin) xMin = x;
    if (x > xMax) xMax = x;
    if (y < yMin) yMin = y;
    if (y > yMax) yMax = y;
}

void Rect2D::expandToInclude(const Rect2D& other) {
    if (other.xMin < xMin) xMin = other.xMin;
    if (other.xMax > xMax) xMax = other.xMax;
    if (other.yMin < yMin) yMin = other.yMin;
    if (other.yMax > yMax) yMax = other.yMax;
}

// ============================================================================
// Geometry Engine Helpers
// ============================================================================

Rect2D GeometryEngine::computeBoundingBox(const Point2D* points, size_t count) {
    Rect2D box;
    for (size_t i = 0; i < count; ++i) {
        box.expandToInclude(points[i].x, points[i].y);
    }
    return box;
}

StrokeMetrics GeometryEngine::computeStrokeMetrics(const Point2D* points, size_t count) {
    StrokeMetrics m;
    m.pointCount = count;
    m.pathLength = 0.0f;
    m.displacement = 0.0f;
    m.straightness = 0.0f;
    m.aspectRatio = 1.0f;
    m.angleDeg = 0.0f;
    m.startX = 0;
    m.startY = 0;
    m.endX = 0;
    m.endY = 0;

    if (count == 0) return m;

    m.bbox = computeBoundingBox(points, count);
    m.startX = points[0].x;
    m.startY = points[0].y;
    m.endX = points[count - 1].x;
    m.endY = points[count - 1].y;

    if (count == 1) return m;

    float len = 0.0f;
    for (size_t i = 1; i < count; ++i) {
        float dx = static_cast<float>(points[i].x - points[i - 1].x);
        float dy = static_cast<float>(points[i].y - points[i - 1].y);
        len += std::sqrt(dx * dx + dy * dy);
    }
    m.pathLength = len;

    float dispX = static_cast<float>(m.endX - m.startX);
    float dispY = static_cast<float>(m.endY - m.startY);
    m.displacement = std::sqrt(dispX * dispX + dispY * dispY);

    if (m.pathLength > 0.001f) {
        m.straightness = m.displacement / m.pathLength;
        if (m.straightness > 1.0f) m.straightness = 1.0f;
    }

    float w = static_cast<float>(m.bbox.width());
    float h = static_cast<float>(m.bbox.height());
    m.aspectRatio = (h > 0.5f) ? (w / h) : w;

    m.angleDeg = static_cast<float>(std::atan2(dispY, dispX) * 180.0 / M_PI);
    return m;
}

static int orientation(Point2D p, Point2D q, Point2D r) {
    int32_t val = (int32_t)(q.y - p.y) * (r.x - q.x) - (int32_t)(q.x - p.x) * (r.y - q.y);
    if (val == 0) return 0; // Collinear
    return (val > 0) ? 1 : 2; // 1 = Clockwise, 2 = Counterclockwise
}

static bool onSegment(Point2D p, Point2D q, Point2D r) {
    return (q.x <= std::max(p.x, r.x) && q.x >= std::min(p.x, r.x) &&
            q.y <= std::max(p.y, r.y) && q.y >= std::min(p.y, r.y));
}

bool GeometryEngine::segmentsIntersect(Point2D p1, Point2D p2, Point2D p3, Point2D p4) {
    int o1 = orientation(p1, p2, p3);
    int o2 = orientation(p1, p2, p4);
    int o3 = orientation(p3, p4, p1);
    int o4 = orientation(p3, p4, p2);

    if (o1 != o2 && o3 != o4) return true;

    if (o1 == 0 && onSegment(p1, p3, p2)) return true;
    if (o2 == 0 && onSegment(p1, p4, p2)) return true;
    if (o3 == 0 && onSegment(p3, p1, p4)) return true;
    if (o4 == 0 && onSegment(p3, p2, p4)) return true;

    return false;
}

bool GeometryEngine::strokesIntersect(const Point2D* stroke1, size_t count1,
                                     const Point2D* stroke2, size_t count2) {
    if (count1 < 2 || count2 < 2) return false;

    Rect2D b1 = computeBoundingBox(stroke1, count1);
    Rect2D b2 = computeBoundingBox(stroke2, count2);
    if (!b1.intersects(b2)) return false;

    for (size_t i = 1; i < count1; ++i) {
        for (size_t j = 1; j < count2; ++j) {
            if (segmentsIntersect(stroke1[i - 1], stroke1[i], stroke2[j - 1], stroke2[j])) {
                return true;
            }
        }
    }
    return false;
}

// ============================================================================
// Trigger-Line Detector
// ============================================================================

TriggerResult GeometryEngine::detectTriggerLine(const std::vector<std::vector<Point2D>>& strokes) {
    TriggerResult res;
    res.verdict = TriggerVerdict::NO_TRIGGER;
    res.triggerStrokeIndex = -1;
    res.confidence = 0.0f;
    res.reason = "No trigger line found";

    if (strokes.size() < 2) {
        res.reason = "Need at least 2 strokes (content + trigger)";
        return res;
    }

    // Evaluate the last stroke as the primary candidate for trigger line
    int candidateIdx = static_cast<int>(strokes.size()) - 1;
    const auto& candidatePts = strokes[candidateIdx];
    if (candidatePts.size() < 2) {
        res.reason = "Candidate stroke has too few points";
        return res;
    }

    StrokeMetrics candMetrics = computeStrokeMetrics(candidatePts.data(), candidatePts.size());

    // Calculate content bounding box (all strokes except candidate)
    Rect2D contentBBox;
    size_t totalContentPoints = 0;
    for (int i = 0; i < candidateIdx; ++i) {
        for (const auto& pt : strokes[i]) {
            contentBBox.expandToInclude(pt.x, pt.y);
            totalContentPoints++;
        }
    }
    res.contentBBox = contentBBox;

    if (totalContentPoints < 2) {
        res.reason = "Insufficient written content";
        return res;
    }

    // Measure angle relative to horizontal: 0 deg = horizontal, 90 deg = vertical
    float absAngle = std::abs(candMetrics.angleDeg);
    float angleFromHorizontal = std::min(absAngle, 180.0f - absAngle);

    float candWidth = static_cast<float>(candMetrics.bbox.width());
    float contentHeight = static_cast<float>(contentBBox.height());

    // Check intersection with any content stroke
    int intersectionCount = 0;
    for (int i = 0; i < candidateIdx; ++i) {
        if (strokesIntersect(candidatePts.data(), candidatePts.size(),
                             strokes[i].data(), strokes[i].size())) {
            intersectionCount++;
        }
    }

    // Calculate maximum perpendicular deviation from the chord (straightness test)
    float maxChordDeviation = 0.0f;
    if (candMetrics.displacement > 5.0f) {
        float x1 = static_cast<float>(candMetrics.startX);
        float y1 = static_cast<float>(candMetrics.startY);
        float x2 = static_cast<float>(candMetrics.endX);
        float y2 = static_cast<float>(candMetrics.endY);
        float lineLen = candMetrics.displacement;

        for (const auto& pt : candidatePts) {
            float px = static_cast<float>(pt.x);
            float py = static_cast<float>(pt.y);
            float dist = std::abs((y2 - y1) * px - (x2 - x1) * py + x2 * y1 - y2 * x1) / lineLen;
            if (dist > maxChordDeviation) maxChordDeviation = dist;
        }
    }

    // 1. Accidental strokes (too short or curved scribble)
    if (candMetrics.pathLength < 25.0f || candWidth < 20.0f) {
        res.verdict = TriggerVerdict::NO_TRIGGER;
        res.reason = "Too short (accidental dot or stray stroke)";
        return res;
    }

    if (candMetrics.straightness < 0.65f) {
        res.verdict = TriggerVerdict::NO_TRIGGER;
        res.reason = "Curved stroke (not a line)";
        return res;
    }

    // 2. Steeply angled strokes (vertical/diagonal)
    if (angleFromHorizontal > 32.0f) {
        res.verdict = TriggerVerdict::NO_TRIGGER;
        res.reason = "Steeply angled stroke (vertical/diagonal)";
        return res;
    }

    // 3. Fraction Bar Check: significant content both above and below
    int contentAbove = 0;
    int contentBelow = 0;
    float candMidY = static_cast<float>(candMetrics.bbox.centerY());
    for (int i = 0; i < candidateIdx; ++i) {
        Rect2D b = computeBoundingBox(strokes[i].data(), strokes[i].size());
        if (b.yMax < candMidY) contentAbove++;
        if (b.yMin > candMidY) contentBelow++;
    }
    if (contentAbove > 0 && contentBelow > 0) {
        res.verdict = TriggerVerdict::NO_TRIGGER;
        res.reason = "Fraction bar (content exists both above and below line)";
        return res;
    }

    // 4. Vertical Position relative to content baseline
    float candTop = static_cast<float>(candMetrics.bbox.yMin);
    float candBottom = static_cast<float>(candMetrics.bbox.yMax);
    float contentBottom = static_cast<float>(contentBBox.yMax);
    float contentTop = static_cast<float>(contentBBox.yMin);

    // Is the candidate located inside the expression row (like a minus sign)?
    bool verticallyInsideContent = (candBottom <= contentBottom && candTop >= contentTop);
    if (verticallyInsideContent && candWidth < 45.0f) {
        res.verdict = TriggerVerdict::NO_TRIGGER;
        res.reason = "Minus sign inside expression";
        return res;
    }

    // Strike-through line cutting across middle of expression
    if (intersectionCount > 2 || (intersectionCount > 0 && candTop < contentTop + 0.4f * contentHeight)) {
        res.verdict = TriggerVerdict::NO_TRIGGER;
        res.reason = "Crosses through multiple content strokes";
        return res;
    }

    // =========================================================================
    // 5. Ambiguity Checks (Must never guess silently on unclear cases)
    // =========================================================================

    // Case A: Borderline tilt angle (18 deg .. 32 deg)
    if (angleFromHorizontal > 18.0f && angleFromHorizontal <= 32.0f) {
        res.verdict = TriggerVerdict::AMBIGUOUS;
        res.triggerStrokeIndex = candidateIdx;
        res.reason = "Ambiguous: Tilted horizontal stroke (18-32 degrees)";
        return res;
    }

    // Case B: Borderline vertical positioning (partially overlaps bottom of content)
    float verticalOverlap = contentBottom - candTop;
    if (verticalOverlap > 4.0f && verticalOverlap <= contentHeight * 0.55f) {
        res.verdict = TriggerVerdict::AMBIGUOUS;
        res.triggerStrokeIndex = candidateIdx;
        res.reason = "Ambiguous: Partially overlaps bottom of written content";
        return res;
    }

    // Case C: Touching bottom descender (1 or 2 intersections near the baseline)
    if (intersectionCount > 0 && candTop >= contentBottom - 12.0f) {
        res.verdict = TriggerVerdict::AMBIGUOUS;
        res.triggerStrokeIndex = candidateIdx;
        res.reason = "Ambiguous: Slightly touches content descender";
        return res;
    }

    // Case D: Wavy or undulating stroke (chord deviation > 5.0px or straightness 0.65-0.80)
    if (maxChordDeviation > 5.0f || (candMetrics.straightness >= 0.65f && candMetrics.straightness < 0.80f)) {
        res.verdict = TriggerVerdict::AMBIGUOUS;
        res.triggerStrokeIndex = candidateIdx;
        res.reason = "Ambiguous: Stroke is wavy/curled";
        return res;
    }

    // Case E: Borderline short line below content (< 50px)
    if (candTop >= contentBottom - 5.0f && candWidth < 50.0f) {
        res.verdict = TriggerVerdict::AMBIGUOUS;
        res.triggerStrokeIndex = candidateIdx;
        res.reason = "Ambiguous: Short line below content (< 50px)";
        return res;
    }

    // =========================================================================
    // 6. Confident Trigger Detection Criteria
    // =========================================================================
    if (candTop >= contentBottom - 5.0f &&
        candWidth >= 50.0f &&
        candMetrics.straightness >= 0.80f &&
        maxChordDeviation <= 5.0f &&
        angleFromHorizontal <= 18.0f &&
        intersectionCount == 0) {
        
        res.verdict = TriggerVerdict::TRIGGER_DETECTED;
        res.triggerStrokeIndex = candidateIdx;
        res.confidence = std::min(1.0f, candMetrics.straightness * (1.0f - angleFromHorizontal / 30.0f));
        res.reason = "Clear horizontal trigger line below content";
        return res;
    }

    // Default to NO_TRIGGER
    res.verdict = TriggerVerdict::NO_TRIGGER;
    res.reason = "Does not meet trigger geometry criteria";
    return res;
}

} // namespace DigitalSlate
