#include "ExpressionEngine.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <chrono>

namespace DigitalSlate {

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

ExpressionEngine::ExpressionEngine()
    : _confidence_threshold(DEFAULT_CONFIDENCE_THRESHOLD),
      _margin_threshold(DEFAULT_MARGIN_THRESHOLD) {}

void ExpressionEngine::setConfidenceThreshold(float threshold) {
    _confidence_threshold = threshold;
}

float ExpressionEngine::getConfidenceThreshold() const {
    return _confidence_threshold;
}

void ExpressionEngine::setMarginThreshold(float margin) {
    _margin_threshold = margin;
}

float ExpressionEngine::getMarginThreshold() const {
    return _margin_threshold;
}

bool ExpressionEngine::isParenthesis(const std::vector<Point2D>& stroke, const StrokeMetrics& m, char& out_paren) {
    // Tall stroke requirement: height must be substantially greater than width
    if (m.bbox.height() < 20 || m.bbox.height() < (int16_t)(1.35f * m.bbox.width())) {
        return false;
    }

    // Bounding aspect ratio check
    if (m.aspectRatio > 0.70f) {
        return false;
    }

    if (stroke.size() < 4) return false;

    // Chord line from start point to end point
    Point2D p_start = stroke.front();
    Point2D p_end   = stroke.back();

    float dx = (float)(p_end.x - p_start.x);
    float dy = (float)(p_end.y - p_start.y);
    float chord_len = std::sqrt(dx * dx + dy * dy);
    if (chord_len < 10.0f) return false;

    // Compute signed distance (cross product) for all intermediate points
    // Cross product: (x_e - x_s)*(y_i - y_s) - (y_e - y_s)*(x_i - x_s)
    float max_deflection = 0.0f;
    float min_deflection = 0.0f;

    for (size_t i = 1; i < stroke.size() - 1; ++i) {
        float px = (float)(stroke[i].x - p_start.x);
        float py = (float)(stroke[i].y - p_start.y);
        float cross = dx * py - dy * px;

        if (cross > max_deflection) max_deflection = cross;
        if (cross < min_deflection) min_deflection = cross;
    }

    float norm_max = max_deflection / chord_len;
    float norm_min = min_deflection / chord_len;

    // Significant bowing is required (> 2.5 pixels normalized deflection)
    bool bows_left  = (norm_max > 2.5f && std::abs(norm_min) < norm_max * 0.6f);
    bool bows_right = (norm_min < -2.5f && std::abs(norm_max) < std::abs(norm_min) * 0.6f);

    // If drawn downwards (dy > 0):
    // Positive cross product means points are to the left of the chord: '('
    // Negative cross product means points are to the right of the chord: ')'
    if (dy > 0) {
        if (bows_left)  { out_paren = '('; return true; }
        if (bows_right) { out_paren = ')'; return true; }
    } else {
        // Drawn upwards (dy < 0): inverted sign
        if (bows_left)  { out_paren = ')'; return true; }
        if (bows_right) { out_paren = '('; return true; }
    }

    return false;
}

bool ExpressionEngine::detectGeometricOperator(
    const std::vector<Point2D>& stroke,
    const StrokeMetrics& m,
    RecognizedSymbol& out_sym
) {
    if (stroke.empty()) return false;

    // 1. Decimal point: very small dot
    if (m.bbox.width() <= 12 && m.bbox.height() <= 12 && m.pathLength <= 25.0f) {
        out_sym.label = '.';
        out_sym.top2_label = '0';
        out_sym.confidence = 1.0f;
        out_sym.top2_confidence = 0.0f;
        out_sym.margin = 1.0f;
        out_sym.is_confident = true;
        out_sym.bbox = m.bbox;
        out_sym.is_operator = true;
        out_sym.is_digit = false;
        out_sym.is_geometrically_resolved = true;
        return true;
    }

    // 2. Parentheses '(' and ')'
    char paren_char = 0;
    if (isParenthesis(stroke, m, paren_char)) {
        out_sym.label = paren_char;
        out_sym.top2_label = (paren_char == '(') ? ')' : '(';
        out_sym.confidence = 0.98f;
        out_sym.top2_confidence = 0.02f;
        out_sym.margin = 0.96f;
        out_sym.is_confident = true;
        out_sym.bbox = m.bbox;
        out_sym.is_operator = true;
        out_sym.is_digit = false;
        out_sym.is_geometrically_resolved = true;
        return true;
    }

    // 3. Minus sign '-': nearly horizontal line, good straightness, aspect ratio > 2.0
    float abs_angle = std::abs(m.angleDeg);
    bool is_horizontal_angle = (abs_angle < 22.0f || abs_angle > 158.0f);

    if (is_horizontal_angle && m.straightness > 0.85f && m.aspectRatio > 2.0f && m.bbox.width() >= 12) {
        out_sym.label = '-';
        out_sym.top2_label = '=';
        out_sym.confidence = 0.99f;
        out_sym.top2_confidence = 0.01f;
        out_sym.margin = 0.98f;
        out_sym.is_confident = true;
        out_sym.bbox = m.bbox;
        out_sym.is_operator = true;
        out_sym.is_digit = false;
        out_sym.is_geometrically_resolved = true;
        return true;
    }

    // 4. Slash operator '/': single diagonal stroke from bottom-left to top-right
    bool is_slash_angle = (m.angleDeg >= 30.0f && m.angleDeg <= 75.0f) ||
                          (m.angleDeg <= -105.0f && m.angleDeg >= -150.0f);
    if (is_slash_angle && m.straightness > 0.85f && m.bbox.height() > 16 && m.bbox.width() > 8) {
        out_sym.label = '/';
        out_sym.top2_label = '1';
        out_sym.confidence = 0.95f;
        out_sym.top2_confidence = 0.05f;
        out_sym.margin = 0.90f;
        out_sym.is_confident = true;
        out_sym.bbox = m.bbox;
        out_sym.is_operator = true;
        out_sym.is_digit = false;
        out_sym.is_geometrically_resolved = true;
        return true;
    }

    return false;
}

bool ExpressionEngine::detectGeometricOperatorPair(
    const std::vector<Point2D>& s1, const StrokeMetrics& m1, size_t idx1,
    const std::vector<Point2D>& s2, const StrokeMetrics& m2, size_t idx2,
    RecognizedSymbol& out_sym
) {
    Rect2D combined_bbox = m1.bbox;
    combined_bbox.expandToInclude(m2.bbox);

    // 1. Equals sign '=': two nearly horizontal, parallel lines with overlapping horizontal span
    float ang1 = std::abs(m1.angleDeg);
    float ang2 = std::abs(m2.angleDeg);
    bool h1 = (ang1 < 25.0f || ang1 > 155.0f);
    bool h2 = (ang2 < 25.0f || ang2 > 155.0f);

    if (h1 && h2 && m1.straightness > 0.80f && m2.straightness > 0.80f) {
        int16_t overlap_x = std::min(m1.bbox.xMax, m2.bbox.xMax) - std::max(m1.bbox.xMin, m2.bbox.xMin);
        int16_t min_w = std::min(m1.bbox.width(), m2.bbox.width());
        int16_t vert_dist = std::abs(m1.bbox.centerY() - m2.bbox.centerY());

        if (min_w > 8 && overlap_x >= (int16_t)(0.45f * min_w) && vert_dist >= 4 && vert_dist <= 30) {
            out_sym.label = '=';
            out_sym.top2_label = '-';
            out_sym.confidence = 1.0f;
            out_sym.top2_confidence = 0.0f;
            out_sym.margin = 1.0f;
            out_sym.is_confident = true;
            out_sym.bbox = combined_bbox;
            out_sym.stroke_indices = {idx1, idx2};
            out_sym.is_operator = true;
            out_sym.is_digit = false;
            out_sym.is_geometrically_resolved = true;
            return true;
        }
    }

    // Check stroke intersection for '+' and 'x'
    if (GeometryEngine::strokesIntersect(s1.data(), s1.size(), s2.data(), s2.size())) {
        float combined_ar = (float)combined_bbox.width() / (float)std::max((int16_t)1, combined_bbox.height());

        // A parenthesis is curved and must never be grouped as a stroke of '+', '=', or '*'
        char dummy_paren;
        if (isParenthesis(s1, m1, dummy_paren) || isParenthesis(s2, m2, dummy_paren)) {
            return false;
        }

        // 2. Plus sign '+': one horizontal stroke and one vertical stroke crossing centrally
        bool v1 = (ang1 >= 60.0f && ang1 <= 120.0f);
        bool v2 = (ang2 >= 60.0f && ang2 <= 120.0f);

        if (((h1 && v2) || (h2 && v1)) && combined_ar >= 0.45f && combined_ar <= 2.2f) {
            // Ensure intersection is central (horizontal stroke center is near vertical stroke X)
            int16_t h_center_x = h1 ? m1.bbox.centerX() : m2.bbox.centerX();
            int16_t v_min_x    = h1 ? m2.bbox.xMin : m1.bbox.xMin;
            int16_t v_max_x    = h1 ? m2.bbox.xMax : m1.bbox.xMax;

            int16_t v_center_y = h1 ? m2.bbox.centerY() : m1.bbox.centerY();
            int16_t h_min_y    = h1 ? m1.bbox.yMin : m2.bbox.yMin;
            int16_t h_max_y    = h1 ? m1.bbox.yMax : m2.bbox.yMax;

            // Horizontal stroke center must be within vertical stroke's horizontal bounds (+ margin)
            if (h_center_x >= v_min_x - 8 && h_center_x <= v_max_x + 8 &&
                v_center_y >= h_min_y - 8 && v_center_y <= h_max_y + 8) {
                out_sym.label = '+';
                out_sym.top2_label = 't';
                out_sym.confidence = 1.0f;
                out_sym.top2_confidence = 0.0f;
                out_sym.margin = 1.0f;
                out_sym.is_confident = true;
                out_sym.bbox = combined_bbox;
                out_sym.stroke_indices = {idx1, idx2};
                out_sym.is_operator = true;
                out_sym.is_digit = false;
                out_sym.is_geometrically_resolved = true;
                return true;
            }
        }

        // 3. Multiply sign 'x' or '*': both diagonal strokes
        bool d1 = (ang1 >= 25.0f && ang1 <= 68.0f) || (ang1 >= 112.0f && ang1 <= 155.0f);
        bool d2 = (ang2 >= 25.0f && ang2 <= 68.0f) || (ang2 >= 112.0f && ang2 <= 155.0f);

        if (d1 && d2 && combined_ar >= 0.45f && combined_ar <= 2.2f) {
            out_sym.label = '*';
            out_sym.top2_label = '+';
            out_sym.confidence = 1.0f;
            out_sym.top2_confidence = 0.0f;
            out_sym.margin = 1.0f;
            out_sym.is_confident = true;
            out_sym.bbox = combined_bbox;
            out_sym.stroke_indices = {idx1, idx2};
            out_sym.is_operator = true;
            out_sym.is_digit = false;
            out_sym.is_geometrically_resolved = true;
            return true;
        }
    }

    return false;
}

std::vector<RecognizedSymbol> ExpressionEngine::segmentAndRecognizeSymbols(
    const std::vector<std::vector<Point2D>>& content_strokes,
    const std::vector<size_t>& original_indices,
    ClassifierEngine& classifier
) {
    std::vector<RecognizedSymbol> symbols;
    size_t stroke_count = content_strokes.size();
    if (stroke_count == 0) return symbols;

    std::vector<bool> consumed(stroke_count, false);
    std::vector<StrokeMetrics> metrics(stroke_count);
    for (size_t i = 0; i < stroke_count; ++i) {
        metrics[i] = GeometryEngine::computeStrokeMetrics(content_strokes[i].data(), content_strokes[i].size());
    }

    // Step 1: Detect 3-stroke division sign '÷' (horizontal bar + top dot + bottom dot)
    for (size_t bar = 0; bar < stroke_count; ++bar) {
        if (consumed[bar]) continue;
        float ang = std::abs(metrics[bar].angleDeg);
        if ((ang < 25.0f || ang > 155.0f) && metrics[bar].aspectRatio > 2.0f && metrics[bar].bbox.width() >= 14) {
            int top_dot = -1;
            int bot_dot = -1;

            for (size_t d = 0; d < stroke_count; ++d) {
                if (d == bar || consumed[d]) continue;
                if (metrics[d].bbox.width() <= 12 && metrics[d].bbox.height() <= 12) {
                    // Check horizontal alignment with bar
                    int16_t dx = std::abs(metrics[d].bbox.centerX() - metrics[bar].bbox.centerX());
                    if (dx < metrics[bar].bbox.width() / 2 + 5) {
                        if (metrics[d].bbox.centerY() < metrics[bar].bbox.yMin && top_dot == -1) {
                            top_dot = (int)d;
                        } else if (metrics[d].bbox.centerY() > metrics[bar].bbox.yMax && bot_dot == -1) {
                            bot_dot = (int)d;
                        }
                    }
                }
            }

            if (top_dot != -1 && bot_dot != -1) {
                RecognizedSymbol sym;
                sym.label = '/'; // Div
                sym.top2_label = '%';
                sym.confidence = 1.0f;
                sym.top2_confidence = 0.0f;
                sym.margin = 1.0f;
                sym.is_confident = true;
                sym.bbox = metrics[bar].bbox;
                sym.bbox.expandToInclude(metrics[top_dot].bbox);
                sym.bbox.expandToInclude(metrics[bot_dot].bbox);
                sym.stroke_indices = {original_indices[bar], original_indices[top_dot], original_indices[bot_dot]};
                sym.is_operator = true;
                sym.is_digit = false;
                sym.is_geometrically_resolved = true;
                symbols.push_back(sym);

                consumed[bar] = true;
                consumed[top_dot] = true;
                consumed[bot_dot] = true;
            }
        }
    }

    // Step 2: Detect 2-stroke operators (=, +, x)
    for (size_t i = 0; i < stroke_count; ++i) {
        if (consumed[i]) continue;
        for (size_t j = i + 1; j < stroke_count; ++j) {
            if (consumed[j]) continue;

            RecognizedSymbol pair_sym;
            if (detectGeometricOperatorPair(content_strokes[i], metrics[i], original_indices[i],
                                           content_strokes[j], metrics[j], original_indices[j],
                                           pair_sym)) {
                symbols.push_back(pair_sym);
                consumed[i] = true;
                consumed[j] = true;
                break;
            }
        }
    }

    // Step 3: Detect 1-stroke geometric operators (-, /, (, ), .)
    for (size_t i = 0; i < stroke_count; ++i) {
        if (consumed[i]) continue;

        RecognizedSymbol single_sym;
        if (detectGeometricOperator(content_strokes[i], metrics[i], single_sym)) {
            single_sym.stroke_indices = {original_indices[i]};
            symbols.push_back(single_sym);
            consumed[i] = true;
        }
    }

    // Step 4: Multi-stroke digit merging (e.g. digit '4', '5', '7')
    for (size_t i = 0; i < stroke_count; ++i) {
        if (consumed[i]) continue;

        for (size_t j = i + 1; j < stroke_count; ++j) {
            if (consumed[j]) continue;

            // Check if stroke i and stroke j overlap strongly in X
            int16_t overlap_x = std::min(metrics[i].bbox.xMax, metrics[j].bbox.xMax) -
                                std::max(metrics[i].bbox.xMin, metrics[j].bbox.xMin);
            int16_t min_w = std::min(metrics[i].bbox.width(), metrics[j].bbox.width());

            bool intersects = GeometryEngine::strokesIntersect(
                content_strokes[i].data(), content_strokes[i].size(),
                content_strokes[j].data(), content_strokes[j].size()
            );

            int16_t center_dist_x = std::abs(metrics[i].bbox.centerX() - metrics[j].bbox.centerX());
            int16_t center_dist_y = std::abs(metrics[i].bbox.centerY() - metrics[j].bbox.centerY());

            if ((intersects || (overlap_x > (int16_t)(0.35f * min_w))) &&
                center_dist_x < 28 && center_dist_y < 45) {

                // Merge into multi-stroke cluster
                StrokeStore cluster_store;
                cluster_store.startStroke();
                for (const auto& pt : content_strokes[i]) cluster_store.addPoint(pt.x, pt.y, pt.timestampMs);
                cluster_store.endStroke();

                cluster_store.startStroke();
                for (const auto& pt : content_strokes[j]) cluster_store.addPoint(pt.x, pt.y, pt.timestampMs);
                cluster_store.endStroke();

                ClassificationResult res = classifier.classifyStrokes(cluster_store);

                RecognizedSymbol sym;
                sym.label = res.top1_label;
                sym.top2_label = res.top2_label;
                sym.confidence = res.top1_confidence;
                sym.top2_confidence = res.top2_confidence;
                sym.margin = res.confidence_margin;
                sym.is_confident = res.is_confident;
                sym.bbox = metrics[i].bbox;
                sym.bbox.expandToInclude(metrics[j].bbox);
                sym.stroke_indices = {original_indices[i], original_indices[j]};
                sym.is_operator = (sym.label == '+' || sym.label == '-' || sym.label == '*' || sym.label == '/');
                sym.is_digit = (sym.label >= '0' && sym.label <= '9');
                sym.is_geometrically_resolved = false;

                symbols.push_back(sym);
                consumed[i] = true;
                consumed[j] = true;
                break;
            }
        }
    }

    // Step 5: Classify all remaining unassigned single strokes via SlateNet-CNN
    for (size_t i = 0; i < stroke_count; ++i) {
        if (consumed[i]) continue;

        StrokeStore single_store;
        single_store.startStroke();
        for (const auto& pt : content_strokes[i]) {
            single_store.addPoint(pt.x, pt.y, pt.timestampMs);
        }
        single_store.endStroke();

        ClassificationResult res = classifier.classifyStrokes(single_store);

        RecognizedSymbol sym;
        sym.label = res.top1_label;
        sym.top2_label = res.top2_label;
        sym.confidence = res.top1_confidence;
        sym.top2_confidence = res.top2_confidence;
        sym.margin = res.confidence_margin;
        sym.is_confident = res.is_confident;
        sym.bbox = metrics[i].bbox;
        sym.stroke_indices = {original_indices[i]};
        sym.is_operator = (sym.label == '+' || sym.label == '-' || sym.label == '*' || sym.label == '/');
        sym.is_digit = (sym.label >= '0' && sym.label <= '9');
        sym.is_geometrically_resolved = false;

        symbols.push_back(sym);
        consumed[i] = true;
    }

    return symbols;
}

std::string ExpressionEngine::assembleExpressionString(const std::vector<RecognizedSymbol>& sorted_symbols) {
    std::string expr;
    if (sorted_symbols.empty()) return expr;

    for (size_t i = 0; i < sorted_symbols.size(); ++i) {
        char ch = sorted_symbols[i].label;

        // Skip equals sign '=' if present at the end of the input (e.g. "23+12-13=")
        if (ch == '=') {
            continue;
        }

        expr.push_back(ch);
    }

    return expr;
}

ExpressionResult ExpressionEngine::processStrokes(
    const std::vector<std::vector<Point2D>>& strokes,
    ClassifierEngine& classifier
) {
    auto t_start = std::chrono::high_resolution_clock::now();

    ExpressionResult result;
    result.status = ExpressionStatus::EMPTY_INPUT;
    result.trigger_stroke_index = -1;
    result.uncertain_symbol_count = 0;
    result.first_uncertain_index = -1;
    result.total_latency_us = 0;

    if (strokes.empty()) {
        return result;
    }

    // Step 1: Detect trigger line beneath written mathematical content (P4 integration)
    TriggerResult trig = GeometryEngine::detectTriggerLine(strokes);

    if (trig.verdict == TriggerVerdict::AMBIGUOUS) {
        result.status = ExpressionStatus::AMBIGUOUS_TRIGGER;
        result.trigger_stroke_index = trig.triggerStrokeIndex;
        result.trigger_bbox = (trig.triggerStrokeIndex >= 0) ?
            GeometryEngine::computeBoundingBox(strokes[trig.triggerStrokeIndex].data(), strokes[trig.triggerStrokeIndex].size()) :
            Rect2D();
        return result;
    }

    // Separate content strokes from trigger line
    std::vector<std::vector<Point2D>> content_strokes;
    std::vector<size_t> original_indices;

    for (size_t s = 0; s < strokes.size(); ++s) {
        if ((int)s == trig.triggerStrokeIndex && trig.verdict == TriggerVerdict::TRIGGER_DETECTED) {
            continue; // Skip trigger stroke from content
        }
        content_strokes.push_back(strokes[s]);
        original_indices.push_back(s);
    }

    if (content_strokes.empty()) {
        result.status = ExpressionStatus::EMPTY_INPUT;
        return result;
    }

    if (trig.verdict == TriggerVerdict::TRIGGER_DETECTED) {
        result.trigger_stroke_index = trig.triggerStrokeIndex;
        result.trigger_bbox = GeometryEngine::computeBoundingBox(
            strokes[trig.triggerStrokeIndex].data(), strokes[trig.triggerStrokeIndex].size()
        );
    }

    // Step 2: Segment content strokes into symbols and recognize each
    result.symbols = segmentAndRecognizeSymbols(content_strokes, original_indices, classifier);

    // Step 3: Sort symbols from left-to-right by horizontal coordinate
    std::sort(result.symbols.begin(), result.symbols.end(), [](const RecognizedSymbol& a, const RecognizedSymbol& b) {
        return a.bbox.xMin < b.bbox.xMin;
    });

    // Step 4: Count and track uncertain symbols
    for (size_t i = 0; i < result.symbols.size(); ++i) {
        if (!result.symbols[i].is_confident) {
            result.uncertain_symbol_count++;
            if (result.first_uncertain_index == -1) {
                result.first_uncertain_index = (int)i;
            }
        }
    }

    // Step 5: Assemble token stream
    result.reconstructed_expr = assembleExpressionString(result.symbols);

    // Step 6: Condition calculation on valid trigger line (Constitution rule!)
    if (trig.verdict == TriggerVerdict::TRIGGER_DETECTED) {
        // Evaluate with CalcEngine
        result.calc_result = CalcEngine::evaluate(result.reconstructed_expr.c_str());

        if (result.calc_result.isOk()) {
            if (result.uncertain_symbol_count > 0) {
                result.status = ExpressionStatus::UNCERTAIN_SYMBOLS;
            } else {
                result.status = ExpressionStatus::EXPRESSION_RECOGNIZED;
            }
        } else {
            result.status = ExpressionStatus::SYNTAX_ERROR;
        }
    } else {
        // Still writing; expression is reconstructed for live display, but answer withheld
        result.status = ExpressionStatus::WAITING_FOR_TRIGGER;
    }

    auto t_end = std::chrono::high_resolution_clock::now();
    result.total_latency_us = (uint32_t)std::chrono::duration_cast<std::chrono::microseconds>(t_end - t_start).count();

    return result;
}

ExpressionResult ExpressionEngine::processStrokes(
    const StrokeStore& store,
    ClassifierEngine& classifier
) {
    // Unpack StrokeStore into std::vector<std::vector<Point2D>>
    std::vector<std::vector<Point2D>> strokes;
    size_t count = store.getStrokeCount();

    for (size_t s = 0; s < count; ++s) {
        const StrokeSegment* seg = store.getStroke(s);
        if (!seg || seg->point_count == 0) continue;

        std::vector<Point2D> stroke;
        stroke.reserve(seg->point_count);

        for (uint16_t p = 0; p < seg->point_count; ++p) {
            const StrokePoint* pt = store.getPoint(seg->start_index + p);
            if (pt) {
                stroke.emplace_back(pt->x, pt->y, pt->t);
            }
        }
        strokes.push_back(stroke);
    }

    return processStrokes(strokes, classifier);
}

bool ExpressionEngine::toggleSymbolCandidate(ExpressionResult& result, size_t symbol_index) {
    if (symbol_index >= result.symbols.size()) return false;

    // Swap top-1 and top-2
    std::swap(result.symbols[symbol_index].label, result.symbols[symbol_index].top2_label);
    std::swap(result.symbols[symbol_index].confidence, result.symbols[symbol_index].top2_confidence);
    result.symbols[symbol_index].is_confident = true; // User manually confirmed

    // Recalculate uncertainty count
    result.uncertain_symbol_count = 0;
    result.first_uncertain_index = -1;
    for (size_t i = 0; i < result.symbols.size(); ++i) {
        if (!result.symbols[i].is_confident) {
            result.uncertain_symbol_count++;
            if (result.first_uncertain_index == -1) {
                result.first_uncertain_index = (int)i;
            }
        }
    }

    // Re-assemble expression string
    result.reconstructed_expr = assembleExpressionString(result.symbols);

    // Re-evaluate
    result.calc_result = CalcEngine::evaluate(result.reconstructed_expr.c_str());

    if (result.calc_result.isOk()) {
        result.status = (result.uncertain_symbol_count > 0) ?
            ExpressionStatus::UNCERTAIN_SYMBOLS : ExpressionStatus::EXPRESSION_RECOGNIZED;
    } else {
        result.status = ExpressionStatus::SYNTAX_ERROR;
    }

    return true;
}

} // namespace DigitalSlate
