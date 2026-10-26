#pragma once

#include <stdint.h>
#include <stddef.h>
#include <string>
#include <vector>

#include "GeometryEngine.hpp"
#include "StrokeStore.hpp"
#include "ClassifierEngine.hpp"
#include "CalcEngine.hpp"
#include "ExpressionEngine.hpp"

namespace DigitalSlate {

enum class VerticalStatus : uint8_t {
    VERTICAL_OK = 0,                   // Clean vertical column expression parsed and evaluated
    NOT_VERTICAL_LAYOUT = 1,           // Single horizontal baseline layout (pass to ExpressionEngine)
    AMBIGUOUS_OPERATOR = 2,            // Stacked numbers without explicit operator and ambiguous evidence
    AMBIGUOUS_PLACE_VALUE = 3,         // Misaligned units column or ambiguous place values
    AMBIGUOUS_CRAMPED_ROWS = 4,        // Vertically overlapping / cramped rows (< min vertical gap)
    INCONSISTENT_ROW_COUNT = 5,        // Coherent rows could not be formed (< 2 or > 2 rows)
    WAITING_FOR_TRIGGER = 6,           // Writing in progress; no bottom trigger underline yet
    AMBIGUOUS_TRIGGER = 7,             // Borderline trigger line
    UNCERTAIN_SYMBOLS = 8              // Low confidence or rejected digit classifier predictions
};

struct VerticalRow {
    int row_index;
    std::string operand_str;
    int64_t operand_value;
    Rect2D bbox;
    std::vector<RecognizedSymbol> digits;
    bool has_operator;
    char operator_char;                // '+', '-', or '\0'
    RecognizedSymbol operator_symbol;
};

struct VerticalResult {
    VerticalStatus status;
    std::string reconstructed_expr;    // Canonical horizontal expression string, e.g. "23+10"
    CalcResult calc_result;            // Exact parsed computation from CalcEngine
    std::vector<VerticalRow> rows;     // Identified rows (top to bottom)
    char resolved_operator;            // '+', '-', or '\0'
    bool is_operator_inferred;         // True if operator was inferred without explicit written glyph
    int trigger_stroke_index;          // Index of trigger stroke in stroke collection
    Rect2D trigger_bbox;               // Trigger line bounding box
    uint32_t total_latency_us;         // Execution time in microseconds
    std::string diagnostic_reason;     // Diagnostic message explaining layout decision or rejection
};

class VerticalEngine {
public:
    static constexpr int16_t MAX_UNITS_ALIGN_DIFF_PX   = 18;  // Maximum difference in units column xMax
    static constexpr int16_t MIN_ROW_VERTICAL_GAP_PX    = 2;   // Minimum vertical gap between row boxes
    static constexpr int16_t MAX_DIGIT_HORIZ_GAP_PX     = 28;  // Gap between digits inside same row
    static constexpr float   MAX_ROW_VERTICAL_OVERLAP   = 0.25f; // Max vertical overlap ratio before reject
    static constexpr float   DEFAULT_CONFIDENCE_THRESH  = 0.40f;
    static constexpr float   DEFAULT_MARGIN_THRESH      = 0.15f;

    VerticalEngine();

    /**
     * @brief End-to-end processing of vertical column expressions from stroke collection.
     */
    VerticalResult processStrokes(const std::vector<std::vector<Point2D>>& strokes, ClassifierEngine& classifier);
    VerticalResult processStrokes(const StrokeStore& store, ClassifierEngine& classifier);

    /**
     * @brief Determines whether a segmented set of symbols forms a 2D vertical column layout
     * rather than a 1D horizontal expression.
     */
    bool isVerticalLayout(const std::vector<RecognizedSymbol>& symbols, const Rect2D& trigger_bbox) const;

private:
    float _confidence_threshold;
    float _margin_threshold;

    // Helper: Segments content strokes into symbols
    std::vector<RecognizedSymbol> segmentSymbols(const std::vector<std::vector<Point2D>>& strokes,
                                                int trigger_idx,
                                                ClassifierEngine& classifier);

    // Helper: Groups symbols into horizontal rows
    bool groupRows(const std::vector<RecognizedSymbol>& symbols,
                   std::vector<VerticalRow>& out_rows,
                   VerticalStatus& out_status,
                   std::string& out_reason) const;

    // Helper: Validates right-aligned units place value
    bool validatePlaceValueAlignment(const VerticalRow& r0,
                                     const VerticalRow& r1,
                                     VerticalStatus& out_status,
                                     std::string& out_reason) const;

    // Helper: Resolves explicit operator or conservatively infers addition
    bool resolveOperator(VerticalRow& r0,
                         VerticalRow& r1,
                         const Rect2D& trigger_bbox,
                         char& out_op,
                         bool& out_inferred,
                         VerticalStatus& out_status,
                         std::string& out_reason) const;
};

} // namespace DigitalSlate
