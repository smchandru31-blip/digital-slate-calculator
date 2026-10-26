#pragma once

#include <stdint.h>
#include <stddef.h>
#include <string>
#include <vector>

#include "GeometryEngine.hpp"
#include "StrokeStore.hpp"
#include "ClassifierEngine.hpp"
#include "CalcEngine.hpp"

namespace DigitalSlate {

struct RecognizedSymbol {
    char label;                          // Primary recognized character (e.g. '2', '+', '(', etc.)
    char top2_label;                     // Alternative second candidate (e.g. '8')
    float confidence;                    // Top-1 confidence [0.0 .. 1.0]
    float top2_confidence;               // Top-2 confidence [0.0 .. 1.0]
    float margin;                        // confidence - top2_confidence
    bool is_confident;                   // True if meets confidence and margin thresholds
    Rect2D bbox;                         // Screen bounding box
    std::vector<size_t> stroke_indices;  // Indices of strokes comprising this symbol
    bool is_operator;                    // True if +, -, *, /, =, (, ), .
    bool is_digit;                       // True if 0..9
    bool is_geometrically_resolved;      // True if resolved by unambiguous geometry
};

enum class ExpressionStatus : uint8_t {
    WAITING_FOR_TRIGGER = 0,             // No trigger line drawn yet (content still being authored)
    AMBIGUOUS_TRIGGER = 1,               // Borderline trigger line detected (user retry/confirmation)
    EXPRESSION_RECOGNIZED = 2,           // Clean trigger line, expression parsed and evaluated
    SYNTAX_ERROR = 3,                    // Parse or arithmetic error (divide by zero, unmatched paren)
    UNCERTAIN_SYMBOLS = 4,               // Expression evaluated but one or more symbols flagged uncertain
    EMPTY_INPUT = 5                      // No content strokes found
};

struct ExpressionResult {
    ExpressionStatus status;
    std::string reconstructed_expr;      // Clean mathematical expression string (e.g. "23+12-13")
    CalcResult calc_result;              // Evaluation result from CalcEngine
    std::vector<RecognizedSymbol> symbols; // Left-to-right sorted symbols
    int trigger_stroke_index;            // Index of trigger stroke in stroke store (-1 if none)
    Rect2D trigger_bbox;                 // Trigger line bounding box
    size_t uncertain_symbol_count;       // Count of symbols where is_confident == false
    int first_uncertain_index;           // Index into symbols vector for first uncertain symbol (-1 if none)
    uint32_t total_latency_us;           // End-to-end processing latency in microseconds
};

class ExpressionEngine {
public:
    static constexpr float DEFAULT_CONFIDENCE_THRESHOLD = 0.40f;
    static constexpr float DEFAULT_MARGIN_THRESHOLD     = 0.15f;
    static constexpr int16_t MAX_DIGIT_GAP_PX           = 32;

    ExpressionEngine();

    void setConfidenceThreshold(float threshold);
    float getConfidenceThreshold() const;

    void setMarginThreshold(float margin);
    float getMarginThreshold() const;

    /**
     * @brief End-to-end horizontal expression reconstruction from a StrokeStore.
     * 1. Detects trigger line beneath content.
     * 2. Segments content strokes into symbols (multi-stroke +, x, =, 4, 5, etc.).
     * 3. Recognizes operators via geometry first, classifier where needed.
     * 4. Classifies digit clusters using quantized TinyML SlateNet-CNN.
     * 5. Groups digits into multi-digit numbers by horizontal spatial spacing.
     * 6. Validates token stream and evaluates with CalcEngine.
     */
    ExpressionResult processStrokes(const StrokeStore& store, ClassifierEngine& classifier);

    /**
     * @brief Process strokes provided as std::vector<std::vector<Point2D>> (host replay files)
     */
    ExpressionResult processStrokes(const std::vector<std::vector<Point2D>>& strokes, ClassifierEngine& classifier);

    /**
     * @brief Interactive correction: toggle an uncertain symbol between Top-1 and Top-2.
     * Re-assembles token stream and re-evaluates.
     */
    bool toggleSymbolCandidate(ExpressionResult& result, size_t symbol_index);

private:
    float _confidence_threshold;
    float _margin_threshold;

    // Helper: Segments content strokes into symbol clusters
    std::vector<RecognizedSymbol> segmentAndRecognizeSymbols(
        const std::vector<std::vector<Point2D>>& content_strokes,
        const std::vector<size_t>& original_indices,
        ClassifierEngine& classifier
    );

    // Helper: Geometry-based operator detection for 1, 2, and 3 stroke patterns
    bool detectGeometricOperator(
        const std::vector<Point2D>& stroke,
        const StrokeMetrics& metrics,
        RecognizedSymbol& out_sym
    );

    bool detectGeometricOperatorPair(
        const std::vector<Point2D>& s1, const StrokeMetrics& m1, size_t idx1,
        const std::vector<Point2D>& s2, const StrokeMetrics& m2, size_t idx2,
        RecognizedSymbol& out_sym
    );

    // Helper: Curvature test for parentheses
    bool isParenthesis(const std::vector<Point2D>& stroke, const StrokeMetrics& m, char& out_paren);

    // Helper: Multi-digit string assembly from ordered symbols
    std::string assembleExpressionString(const std::vector<RecognizedSymbol>& sorted_symbols);
};

} // namespace DigitalSlate
