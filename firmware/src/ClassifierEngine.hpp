#pragma once

#include <stdint.h>
#include <stddef.h>
#include "StrokeStore.hpp"
#include "model_data.h"

namespace DigitalSlate {

struct ClassificationResult {
    char label;                // e.g. '0'..'9', '+', '-', '*', '/'
    const char* label_str;     // string representation
    uint8_t class_index;       // 0..13
    float confidence;          // Softmax probability 0.0 .. 1.0
    bool is_confident;         // true if confidence >= calibrated threshold
};

class ClassifierEngine {
public:
    static constexpr float DEFAULT_CONFIDENCE_THRESHOLD = 0.40f;

    ClassifierEngine();

    // Sets the calibrated confidence threshold (from validation tuning)
    void setConfidenceThreshold(float threshold);
    float getConfidenceThreshold() const;

    // Preprocessing: rasterizes strokes from StrokeStore into normalized 28x28 grayscale bitmap
    void rasterize(const StrokeStore& store, float out_bitmap[SLATENET_INPUT_DIM][SLATENET_INPUT_DIM]) const;
    void rasterizeStroke(const StrokeStore& store, size_t stroke_index, float out_bitmap[SLATENET_INPUT_DIM][SLATENET_INPUT_DIM]) const;

    // TinyML Inference: Runs SlateNet-CNN on 28x28 bitmap using quantized weights
    ClassificationResult classify(const float input_bitmap[SLATENET_INPUT_DIM][SLATENET_INPUT_DIM]);

    // Convenience: Preprocesses store and classifies in one call
    ClassificationResult classifyStrokes(const StrokeStore& store);

private:
    float _threshold;

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
