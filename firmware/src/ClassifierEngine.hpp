#pragma once

#include <stdint.h>
#include <stddef.h>
#include "StrokeStore.hpp"
#include "model_data.h"

namespace DigitalSlate {

enum class RecognitionStatus : uint8_t {
    CONFIDENT = 0,               // High confidence, safe to evaluate
    REJECT_LOW_CONFIDENCE = 1,   // Top-1 probability below calibrated threshold
    REJECT_AMBIGUOUS_PAIR = 2,   // Top-2 candidates are close in probability (e.g. 3/8, 1/7)
    REJECT_EMPTY_INPUT = 3       // No strokes provided
};

struct ClassificationResult {
    char top1_label;             // e.g. '7'
    const char* top1_str;        // "7"
    uint8_t top1_index;          // 0..13
    float top1_confidence;       // Softmax probability 0.0 .. 1.0

    char top2_label;             // Second candidate e.g. '1'
    const char* top2_str;        // "1"
    uint8_t top2_index;          // 0..13
    float top2_confidence;       // Second candidate probability

    float confidence_margin;     // top1_confidence - top2_confidence
    bool is_confident;           // true if safe to accept without prompt
    bool is_ambiguous_pair;      // true if top-2 are a known confusion pair with small margin
    RecognitionStatus status;    // status enum

    uint32_t rasterize_us;       // Preprocessing time in microseconds
    uint32_t inference_us;       // Neural forward pass time in microseconds
    uint32_t total_us;           // Total latency in microseconds
};

class ClassifierEngine {
public:
    static constexpr float DEFAULT_CONFIDENCE_THRESHOLD = 0.40f;
    static constexpr float DEFAULT_MARGIN_THRESHOLD     = 0.15f;

    ClassifierEngine();

    // Calibration setters/getters
    void setConfidenceThreshold(float threshold);
    float getConfidenceThreshold() const;
    void setMarginThreshold(float margin);
    float getMarginThreshold() const;

    // Preprocessing: rasterizes strokes into normalized 28x28 grayscale bitmap
    void rasterize(const StrokeStore& store, float out_bitmap[SLATENET_INPUT_DIM][SLATENET_INPUT_DIM]) const;

    // TinyML Inference: Runs SlateNet-CNN on 28x28 bitmap using quantized weights
    ClassificationResult classify(const float input_bitmap[SLATENET_INPUT_DIM][SLATENET_INPUT_DIM]);

    // Convenience: Preprocesses store and classifies in one call with latency tracking
    ClassificationResult classifyStrokes(const StrokeStore& store);

    // Confusion pair checker
    static bool isKnownConfusionPair(char c1, char c2);

private:
    float _threshold;
    float _margin_threshold;

    // Fixed-size tensor buffers (Tensor Arena: ~3.2 KB in static SRAM, zero heap allocation)
    float _conv1_out[8][14][14];
    float _conv2_out[16][7][7];
    float _gap_out[144];
    float _logits[SLATENET_NUM_CLASSES];
    float _probs[SLATENET_NUM_CLASSES];

    void forwardConv1(const float input[28][28]);
    void forwardConv2();
    void forwardGAP();
    void forwardFC();
    void computeSoftmax();
};

} // namespace DigitalSlate
