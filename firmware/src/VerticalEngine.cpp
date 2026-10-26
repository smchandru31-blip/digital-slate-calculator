#include "VerticalEngine.hpp"

#include <chrono>
#include <cmath>
#include <algorithm>
#include <sstream>
#include <iostream>

namespace DigitalSlate {

VerticalEngine::VerticalEngine()
    : _confidence_threshold(DEFAULT_CONFIDENCE_THRESH),
      _margin_threshold(DEFAULT_MARGIN_THRESH) {}

VerticalResult VerticalEngine::processStrokes(const StrokeStore& store, ClassifierEngine& classifier) {
    std::vector<std::vector<Point2D>> strokes;
    size_t stroke_count = store.getStrokeCount();
    for (size_t s = 0; s < stroke_count; ++s) {
        const StrokeSegment* seg = store.getStroke(s);
        if (!seg || seg->point_count == 0) continue;
        std::vector<Point2D> pts;
        pts.reserve(seg->point_count);
        for (size_t p = 0; p < seg->point_count; ++p) {
            const StrokePoint* sp = store.getPoint(seg->start_index + p);
            if (sp) {
                pts.emplace_back(sp->x, sp->y, sp->t);
            }
        }
        strokes.push_back(pts);
    }
    return processStrokes(strokes, classifier);
}

VerticalResult VerticalEngine::processStrokes(const std::vector<std::vector<Point2D>>& strokes,
                                             ClassifierEngine& classifier) {
    auto t_start = std::chrono::high_resolution_clock::now();

    VerticalResult result;
    result.status = VerticalStatus::INCONSISTENT_ROW_COUNT;
    result.resolved_operator = '\0';
    result.is_operator_inferred = false;
    result.trigger_stroke_index = -1;
    result.diagnostic_reason = "";

    auto calc_latency = [&]() {
        auto t_end = std::chrono::high_resolution_clock::now();
        result.total_latency_us = (uint32_t)std::chrono::duration_cast<std::chrono::microseconds>(t_end - t_start).count();
    };

    if (strokes.empty()) {
        result.status = VerticalStatus::INCONSISTENT_ROW_COUNT;
        result.diagnostic_reason = "Empty input: zero strokes provided";
        calc_latency();
        return result;
    }

    // 1. Detect trigger line beneath written mathematical content (P4 integration)
    TriggerResult trig = GeometryEngine::detectTriggerLine(strokes);

    if (trig.verdict == TriggerVerdict::AMBIGUOUS) {
        result.status = VerticalStatus::AMBIGUOUS_TRIGGER;
        result.trigger_stroke_index = trig.triggerStrokeIndex;
        result.diagnostic_reason = "Ambiguous horizontal trigger line gesture";
        if (trig.triggerStrokeIndex >= 0 && (size_t)trig.triggerStrokeIndex < strokes.size()) {
            result.trigger_bbox = GeometryEngine::computeBoundingBox(
                strokes[trig.triggerStrokeIndex].data(), strokes[trig.triggerStrokeIndex].size()
            );
        }
        calc_latency();
        return result;
    }

    if (trig.verdict != TriggerVerdict::TRIGGER_DETECTED) {
        result.status = VerticalStatus::WAITING_FOR_TRIGGER;
        result.diagnostic_reason = "Awaiting horizontal trigger underline beneath content";
        calc_latency();
        return result;
    }

    result.trigger_stroke_index = trig.triggerStrokeIndex;
    result.trigger_bbox = GeometryEngine::computeBoundingBox(
        strokes[trig.triggerStrokeIndex].data(), strokes[trig.triggerStrokeIndex].size()
    );

    // 2. Separate content strokes from trigger line
    std::vector<std::vector<Point2D>> content_strokes;
    std::vector<size_t> original_indices;

    for (size_t s = 0; s < strokes.size(); ++s) {
        if ((int)s == trig.triggerStrokeIndex) continue;
        content_strokes.push_back(strokes[s]);
        original_indices.push_back(s);
    }

    if (content_strokes.empty()) {
        result.status = VerticalStatus::INCONSISTENT_ROW_COUNT;
        result.diagnostic_reason = "No content strokes found above trigger line";
        calc_latency();
        return result;
    }

    // 3. Segment content strokes into recognized symbols
    ExpressionEngine exprEngine;
    std::vector<RecognizedSymbol> symbols = exprEngine.segmentAndRecognizeSymbols(
        content_strokes, original_indices, classifier
    );

    for (const auto& s : symbols) {
        std::cerr << "[SYM FIXTURE] label='" << s.label << "' is_digit=" << s.is_digit 
                  << " is_op=" << s.is_operator << " bbox=[" << s.bbox.xMin << "," << s.bbox.yMin 
                  << ".." << s.bbox.xMax << "," << s.bbox.yMax << "] strokes=[";
        for (size_t k : s.stroke_indices) std::cerr << k << ",";
        std::cerr << "]\n";
    }

    if (symbols.empty()) {
        result.status = VerticalStatus::INCONSISTENT_ROW_COUNT;
        result.diagnostic_reason = "No valid content symbols recognized";
        calc_latency();
        return result;
    }

    // 4. Verify whether layout forms a 2D vertical stack
    if (!isVerticalLayout(symbols, result.trigger_bbox)) {
        result.status = VerticalStatus::NOT_VERTICAL_LAYOUT;
        result.diagnostic_reason = "Content symbols form a single horizontal row, not a vertical stack";
        calc_latency();
        return result;
    }

    // 5. Group symbols into horizontal rows
    std::vector<VerticalRow> rows;
    VerticalStatus group_stat;
    std::string group_reason;
    if (!groupRows(symbols, rows, group_stat, group_reason)) {
        result.status = group_stat;
        result.diagnostic_reason = group_reason;
        calc_latency();
        return result;
    }
    result.rows = rows;

    // 6. Validate place-value alignment (right-aligned units column)
    VerticalStatus align_stat;
    std::string align_reason;
    if (!validatePlaceValueAlignment(rows[0], rows[1], align_stat, align_reason)) {
        result.status = align_stat;
        result.diagnostic_reason = align_reason;
        calc_latency();
        return result;
    }

    // 7. Resolve explicit operator or conservatively infer addition
    char resolved_op = '\0';
    bool is_inferred = false;
    VerticalStatus op_stat;
    std::string op_reason;
    if (!resolveOperator(rows[0], rows[1], result.trigger_bbox, resolved_op, is_inferred, op_stat, op_reason)) {
        result.status = op_stat;
        result.diagnostic_reason = op_reason;
        calc_latency();
        return result;
    }

    result.resolved_operator = resolved_op;
    result.is_operator_inferred = is_inferred;

    // 8. Check if any digits are uncertain
    bool has_uncertain = false;
    for (const auto& r : rows) {
        for (const auto& d : r.digits) {
            if (!d.is_confident) {
                has_uncertain = true;
                break;
            }
        }
    }

    // 9. Construct canonical mathematical expression: "operand0 + op + operand1"
    std::string expr = rows[0].operand_str + resolved_op + rows[1].operand_str;
    result.reconstructed_expr = expr;

    // 10. Evaluate exact result via CalcEngine recursive descent parser
    result.calc_result = CalcEngine::evaluate(expr.c_str());

    if (has_uncertain) {
        result.status = VerticalStatus::UNCERTAIN_SYMBOLS;
        result.diagnostic_reason = "One or more digits have low confidence or ambiguous margin";
    } else {
        result.status = VerticalStatus::VERTICAL_OK;
        result.diagnostic_reason = is_inferred ? "Vertical addition inferred from strict right-alignment"
                                               : "Vertical expression evaluated with explicit operator";
    }

    calc_latency();
    return result;
}

bool VerticalEngine::isVerticalLayout(const std::vector<RecognizedSymbol>& symbols,
                                     const Rect2D& trigger_bbox) const {
    if (symbols.size() < 2) return false;

    int16_t min_y = 9999, max_y = -9999;
    int32_t total_h = 0;
    size_t count = 0;

    for (const auto& s : symbols) {
        min_y = std::min(min_y, s.bbox.yMin);
        max_y = std::max(max_y, s.bbox.yMax);
        total_h += (s.bbox.yMax - s.bbox.yMin + 1);
        count++;
    }

    // 1. Primary check: Are any digits horizontally co-located (stacked in X)?
    // In a horizontal expression, digits never overlap in X. Overlapping X means vertical stacking!
    for (size_t i = 0; i < symbols.size(); ++i) {
        if (!symbols[i].is_digit) continue;
        for (size_t j = i + 1; j < symbols.size(); ++j) {
            if (!symbols[j].is_digit) continue;
            int16_t ov_x = std::min(symbols[i].bbox.xMax, symbols[j].bbox.xMax) -
                           std::max(symbols[i].bbox.xMin, symbols[j].bbox.xMin);
            int16_t min_w = std::min(symbols[i].bbox.width(), symbols[j].bbox.width());
            if (ov_x > (int16_t)(0.35f * min_w)) {
                return true; // Confirmed vertically stacked digits
            }
        }
    }

    // 2. Secondary check: Vertical span vs average glyph height
    float avg_h = (float)total_h / (float)count;
    float vertical_span = (float)(max_y - min_y + 1);

    if (vertical_span < 1.45f * avg_h) {
        return false;
    }

    // Check if there are at least two digit symbols whose vertical centers differ significantly
    std::vector<int16_t> mid_ys;
    for (const auto& s : symbols) {
        if (s.is_digit) {
            mid_ys.push_back((s.bbox.yMin + s.bbox.yMax) / 2);
        }
    }
    if (mid_ys.size() < 2) return false;

    std::sort(mid_ys.begin(), mid_ys.end());
    int16_t max_gap = 0;
    for (size_t i = 1; i < mid_ys.size(); ++i) {
        max_gap = std::max(max_gap, (int16_t)(mid_ys[i] - mid_ys[i-1]));
    }

    return (max_gap >= (int16_t)(0.40f * avg_h));
}

bool VerticalEngine::groupRows(const std::vector<RecognizedSymbol>& symbols,
                              std::vector<VerticalRow>& out_rows,
                              VerticalStatus& out_status,
                              std::string& out_reason) const {
    out_rows.clear();

    // 1. Separate digits and explicit operator
    std::vector<RecognizedSymbol> digits;
    std::vector<RecognizedSymbol> operators;

    for (const auto& s : symbols) {
        if (s.is_digit) {
            digits.push_back(s);
        } else if (s.is_operator) {
            operators.push_back(s);
        }
    }

    if (digits.empty()) {
        out_status = VerticalStatus::INCONSISTENT_ROW_COUNT;
        out_reason = "No digits found in vertical expression";
        return false;
    }

    // 2. Compute average digit height
    int32_t total_h = 0;
    for (const auto& d : digits) {
        total_h += (d.bbox.yMax - d.bbox.yMin + 1);
    }
    float avg_h = (float)total_h / (float)digits.size();

    // 3. Sort digits vertically by yMid
    std::vector<RecognizedSymbol> sorted_digits = digits;
    std::sort(sorted_digits.begin(), sorted_digits.end(), [](const RecognizedSymbol& a, const RecognizedSymbol& b) {
        return ((a.bbox.yMin + a.bbox.yMax) / 2) < ((b.bbox.yMin + b.bbox.yMax) / 2);
    });

    // 4. Cluster digits into rows
    std::vector<std::vector<RecognizedSymbol>> row_clusters;
    for (const auto& d : sorted_digits) {
        int16_t d_mid_y = (d.bbox.yMin + d.bbox.yMax) / 2;
        bool placed = false;

        for (auto& cluster : row_clusters) {
            int16_t cluster_mid_y = 0;
            for (const auto& cd : cluster) {
                cluster_mid_y += (cd.bbox.yMin + cd.bbox.yMax) / 2;
            }
            cluster_mid_y /= (int16_t)cluster.size();

            if (std::abs(d_mid_y - cluster_mid_y) <= (int16_t)(0.40f * avg_h)) {
                cluster.push_back(d);
                placed = true;
                break;
            }
        }

        if (!placed) {
            row_clusters.push_back({d});
        }
    }

    if (row_clusters.size() != 2) {
        out_status = VerticalStatus::INCONSISTENT_ROW_COUNT;
        out_reason = "Expected exactly 2 stacked operand rows; found " + std::to_string(row_clusters.size());
        return false;
    }

    // Sort rows top-to-bottom
    std::sort(row_clusters.begin(), row_clusters.end(), [](const std::vector<RecognizedSymbol>& a, const std::vector<RecognizedSymbol>& b) {
        return a[0].bbox.yMin < b[0].bbox.yMin;
    });

    // 5. Check vertical overlap between rows (crampedness rejection)
    int16_t r0_yMax = -9999;
    for (const auto& d : row_clusters[0]) r0_yMax = std::max(r0_yMax, d.bbox.yMax);

    int16_t r1_yMin = 9999;
    for (const auto& d : row_clusters[1]) r1_yMin = std::min(r1_yMin, d.bbox.yMin);

    int16_t overlap_px = r0_yMax - r1_yMin;
    if (overlap_px > (int16_t)(MAX_ROW_VERTICAL_OVERLAP * avg_h)) {
        out_status = VerticalStatus::AMBIGUOUS_CRAMPED_ROWS;
        out_reason = "Stacked rows vertically overlap by " + std::to_string(overlap_px) + " px (cramped/ambiguous layout)";
        return false;
    }

    // 6. Assemble VerticalRow structures
    for (size_t r = 0; r < row_clusters.size(); ++r) {
        VerticalRow row;
        row.row_index = (int)r;
        row.digits = row_clusters[r];
        row.has_operator = false;
        row.operator_char = '\0';

        // Sort digits left-to-right
        std::sort(row.digits.begin(), row.digits.end(), [](const RecognizedSymbol& a, const RecognizedSymbol& b) {
            return a.bbox.xMin < b.bbox.xMin;
        });

        // Compute row bounding box & operand string
        row.bbox = row.digits[0].bbox;
        std::string num_str = "";
        for (const auto& d : row.digits) {
            row.bbox.expandToInclude(d.bbox);
            num_str += d.label;
        }
        row.operand_str = num_str;
        try {
            row.operand_value = std::stoll(num_str);
        } catch (...) {
            row.operand_value = 0;
        }

        out_rows.push_back(row);
    }

    // 7. Associate explicit operator with Row 1 (or between rows on the left)
    if (!operators.empty()) {
        for (const auto& op : operators) {
            // Operator should be on the left of digits (centerX is to the left of row 1 digits)
            if (!out_rows[1].digits.empty() && op.bbox.centerX() < out_rows[1].digits[0].bbox.centerX()) {
                out_rows[1].has_operator = true;
                out_rows[1].operator_char = op.label;
                out_rows[1].operator_symbol = op;
                break;
            }
        }
    }

    return true;
}

bool VerticalEngine::validatePlaceValueAlignment(const VerticalRow& r0,
                                               const VerticalRow& r1,
                                               VerticalStatus& out_status,
                                               std::string& out_reason) const {
    if (r0.digits.empty() || r1.digits.empty()) {
        out_status = VerticalStatus::INCONSISTENT_ROW_COUNT;
        out_reason = "Empty row encountered";
        return false;
    }

    const auto& units0 = r0.digits.back();
    const auto& units1 = r1.digits.back();

    int16_t units_delta = std::abs(units0.bbox.xMax - units1.bbox.xMax);

    // If units delta exceeds standard tolerance, check if it's completely misaligned
    if (units_delta > MAX_UNITS_ALIGN_DIFF_PX) {
        // Check if Row 1's units digit is left-aligned or centered under higher place-values
        if (r0.digits.size() > 1 && units1.bbox.xMax < r0.digits[r0.digits.size() - 2].bbox.xMax + 6) {
            out_status = VerticalStatus::AMBIGUOUS_PLACE_VALUE;
            out_reason = "Units column severely misaligned: units digit of Row 1 aligns with tens/hundreds place of Row 0";
            return false;
        }

        out_status = VerticalStatus::AMBIGUOUS_PLACE_VALUE;
        out_reason = "Units column right-alignment error: delta = " + std::to_string(units_delta) + " px (exceeds max tolerance)";
        return false;
    }

    return true;
}

bool VerticalEngine::resolveOperator(VerticalRow& r0,
                                    VerticalRow& r1,
                                    const Rect2D& trigger_bbox,
                                    char& out_op,
                                    bool& out_inferred,
                                    VerticalStatus& out_status,
                                    std::string& out_reason) const {
    // 1. Explicit operator present
    if (r1.has_operator) {
        char op = r1.operator_char;
        if (op == '+' || op == '-') {
            out_op = op;
            out_inferred = false;
            return true;
        }
    }

    // 2. Conservative operator inference (Project Constitution rule)
    // Never infer subtraction; infer addition ONLY when layout evidence is strict and unambiguous
    const auto& units0 = r0.digits.back();
    const auto& units1 = r1.digits.back();
    int16_t units_delta = std::abs(units0.bbox.xMax - units1.bbox.xMax);

    // Criteria:
    // a. Strict units alignment (<= 16 px)
    if (units_delta > 16) {
        out_status = VerticalStatus::AMBIGUOUS_OPERATOR;
        out_reason = "No explicit operator; units alignment delta (" + std::to_string(units_delta) + " px) too wide to safely infer addition. Ask user to confirm operator [+] or [-].";
        return false;
    }

    // b. Consistent vertical row gap
    int16_t row_gap = r1.bbox.yMin - r0.bbox.yMax;
    if (row_gap < MIN_ROW_VERTICAL_GAP_PX || row_gap > 32) {
        out_status = VerticalStatus::AMBIGUOUS_OPERATOR;
        out_reason = "No explicit operator; row gap (" + std::to_string(row_gap) + " px) is irregular. Ask user to confirm operator [+] or [-].";
        return false;
    }

    // c. Consistent digit scale
    int16_t h0 = r0.bbox.yMax - r0.bbox.yMin + 1;
    int16_t h1 = r1.bbox.yMax - r1.bbox.yMin + 1;
    float h_ratio = (float)h0 / (float)h1;
    if (h_ratio < 0.65f || h_ratio > 1.45f) {
        out_status = VerticalStatus::AMBIGUOUS_OPERATOR;
        out_reason = "No explicit operator; digit heights between rows are inconsistent. Ask user to confirm operator [+] or [-].";
        return false;
    }

    // d. Trigger line covers operand width
    int16_t max_operand_w = std::max(r0.bbox.xMax - r0.bbox.xMin + 1, r1.bbox.xMax - r1.bbox.xMin + 1);
    int16_t trigger_w = trigger_bbox.xMax - trigger_bbox.xMin + 1;
    if (trigger_w < (int16_t)(0.80f * max_operand_w)) {
        out_status = VerticalStatus::AMBIGUOUS_OPERATOR;
        out_reason = "No explicit operator; trigger underline is shorter than operands. Ask user to confirm operator [+] or [-].";
        return false;
    }

    // All strict conservative criteria satisfied: infer addition
    out_op = '+';
    out_inferred = true;
    return true;
}

} // namespace DigitalSlate
