#include "ClassifierEngine.hpp"
#include <math.h>
#include <string.h>
#include <algorithm>

#ifdef ARDUINO
#include <Arduino.h>
#else
#include <chrono>
static uint32_t hostMicros() {
    auto now = std::chrono::high_resolution_clock::now();
    return (uint32_t)std::chrono::duration_cast<std::chrono::microseconds>(now.time_since_epoch()).count();
}
#define micros() hostMicros()
#endif

namespace DigitalSlate {

ClassifierEngine::ClassifierEngine() 
    : _threshold(DEFAULT_CONFIDENCE_THRESHOLD),
      _margin_threshold(DEFAULT_MARGIN_THRESHOLD) {
    memset(_conv1_out, 0, sizeof(_conv1_out));
    memset(_conv2_out, 0, sizeof(_conv2_out));
    memset(_gap_out, 0, sizeof(_gap_out));
    memset(_logits, 0, sizeof(_logits));
    memset(_probs, 0, sizeof(_probs));
}

void ClassifierEngine::setConfidenceThreshold(float threshold) {
    _threshold = threshold;
}

float ClassifierEngine::getConfidenceThreshold() const {
    return _threshold;
}

void ClassifierEngine::setMarginThreshold(float margin) {
    _margin_threshold = margin;
}

float ClassifierEngine::getMarginThreshold() const {
    return _margin_threshold;
}

bool ClassifierEngine::isKnownConfusionPair(char c1, char c2) {
    if (c1 > c2) std::swap(c1, c2);
    // Standard targeted confusion pairs
    if (c1 == '3' && c2 == '8') return true;
    if (c1 == '1' && c2 == '7') return true;
    if (c1 == '0' && c2 == '6') return true;
    if (c1 == '5' && c2 == '6') return true;
    if (c1 == '2' && c2 == '7') return true;
    if (c1 == '4' && c2 == '9') return true;
    return false;
}

// --------------------------------------------------------------------------
// Preprocessing / Rasterization (identical logic to Python training pipeline)
// --------------------------------------------------------------------------

static void drawThickLine(float bitmap[28][28], int x0, int y0, int x1, int y1) {
    int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = dx + dy, e2;

    while (true) {
        // Draw 2x2 stamp for thickness
        for (int dy_offset = 0; dy_offset <= 1; ++dy_offset) {
            for (int dx_offset = 0; dx_offset <= 1; ++dx_offset) {
                int px = x0 + dx_offset;
                int py = y0 + dy_offset;
                if (px >= 0 && px < 28 && py >= 0 && py < 28) {
                    bitmap[py][px] = 1.0f;
                }
            }
        }

        if (x0 == x1 && y0 == y1) break;
        e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

void ClassifierEngine::rasterize(const StrokeStore& store, float out_bitmap[SLATENET_INPUT_DIM][SLATENET_INPUT_DIM]) const {
    memset(out_bitmap, 0, sizeof(float) * 28 * 28);
    if (store.getTotalPointCount() == 0) return;

    int16_t min_x, min_y, max_x, max_y;
    store.getBounds(min_x, min_y, max_x, max_y);

    float w = (float)std::max((int)(max_x - min_x), 1);
    float h = (float)std::max((int)(max_y - min_y), 1);
    float max_dim = std::max(w, h);

    // Active box is 20x20 with 4px border margin
    float scale = 20.0f / max_dim;
    float center_x = (min_x + max_x) / 2.0f;
    float center_y = (min_y + max_y) / 2.0f;
    float target_center = 14.0f;

    auto transformX = [&](int16_t x) -> int {
        return (int)roundf((x - center_x) * scale + target_center);
    };
    auto transformY = [&](int16_t y) -> int {
        return (int)roundf((y - center_y) * scale + target_center);
    };

    for (size_t s = 0; s < store.getStrokeCount(); ++s) {
        const StrokeSegment* seg = store.getStroke(s);
        if (!seg || seg->point_count == 0) continue;

        if (seg->point_count == 1) {
            const StrokePoint* pt = store.getPoint(seg->start_index);
            if (pt) {
                int px = transformX(pt->x);
                int py = transformY(pt->y);
                if (px >= 0 && px < 28 && py >= 0 && py < 28) {
                    out_bitmap[py][px] = 1.0f;
                }
            }
        } else {
            for (size_t p = 0; p < seg->point_count - 1; ++p) {
                const StrokePoint* p0 = store.getPoint(seg->start_index + p);
                const StrokePoint* p1 = store.getPoint(seg->start_index + p + 1);
                if (p0 && p1) {
                    drawThickLine(out_bitmap, transformX(p0->x), transformY(p0->y),
                                  transformX(p1->x), transformY(p1->y));
                }
            }
        }
    }
}

// --------------------------------------------------------------------------
// SlateNet-CNN Forward Pass (Conv1 -> ReLU -> MaxPool -> Conv2 -> ReLU -> MaxPool -> GAP -> FC)
// --------------------------------------------------------------------------

void ClassifierEngine::forwardConv1(const float input[28][28]) {
    for (int f = 0; f < 8; ++f) {
        float bias = CONV1_BIAS[f];
        for (int out_y = 0; out_y < 14; ++out_y) {
            for (int out_x = 0; out_x < 14; ++out_x) {
                float max_val = -1e9f;
                for (int py = 0; py < 2; ++py) {
                    int in_y = out_y * 2 + py;
                    for (int px = 0; px < 2; ++px) {
                        int in_x = out_x * 2 + px;

                        float sum = bias;
                        for (int ky = 0; ky < 3; ++ky) {
                            int iy = in_y + ky - 1;
                            if (iy < 0 || iy >= 28) continue;
                            for (int kx = 0; kx < 3; ++kx) {
                                int ix = in_x + kx - 1;
                                if (ix >= 0 && ix < 28) {
                                    float w = CONV1_WEIGHTS[f][0][ky][kx] * CONV1_WEIGHT_SCALE;
                                    sum += input[iy][ix] * w;
                                }
                            }
                        }
                        if (sum < 0.0f) sum = 0.0f;
                        if (sum > max_val) max_val = sum;
                    }
                }
                _conv1_out[f][out_y][out_x] = max_val;
            }
        }
    }
}

void ClassifierEngine::forwardConv2() {
    for (int f = 0; f < 16; ++f) {
        float bias = CONV2_BIAS[f];
        for (int out_y = 0; out_y < 7; ++out_y) {
            for (int out_x = 0; out_x < 7; ++out_x) {
                float max_val = -1e9f;
                for (int py = 0; py < 2; ++py) {
                    int in_y = out_y * 2 + py;
                    for (int px = 0; px < 2; ++px) {
                        int in_x = out_x * 2 + px;

                        float sum = bias;
                        for (int c = 0; c < 8; ++c) {
                            for (int ky = 0; ky < 3; ++ky) {
                                int iy = in_y + ky - 1;
                                if (iy < 0 || iy >= 14) continue;
                                for (int kx = 0; kx < 3; ++kx) {
                                    int ix = in_x + kx - 1;
                                    if (ix >= 0 && ix < 14) {
                                        float w = CONV2_WEIGHTS[f][c][ky][kx] * CONV2_WEIGHT_SCALE;
                                        sum += _conv1_out[c][iy][ix] * w;
                                    }
                                }
                            }
                        }
                        if (sum < 0.0f) sum = 0.0f;
                        if (sum > max_val) max_val = sum;
                    }
                }
                _conv2_out[f][out_y][out_x] = max_val;
            }
        }
    }
}

void ClassifierEngine::forwardGAP() {
    static const int bin_start[3] = {0, 2, 4};
    static const int bin_end[3]   = {3, 5, 7};

    int out_idx = 0;
    for (int f = 0; f < 16; ++f) {
        for (int by = 0; by < 3; ++by) {
            for (int bx = 0; bx < 3; ++bx) {
                float sum = 0.0f;
                int count = 0;
                for (int y = bin_start[by]; y < bin_end[by]; ++y) {
                    for (int x = bin_start[bx]; x < bin_end[bx]; ++x) {
                        sum += _conv2_out[f][y][x];
                        count++;
                    }
                }
                _gap_out[out_idx++] = sum / (float)count;
            }
        }
    }
}

void ClassifierEngine::forwardFC() {
    for (size_t cl = 0; cl < SLATENET_NUM_CLASSES; ++cl) {
        float sum = FC_BIAS[cl];
        for (int i = 0; i < 144; ++i) {
            float w = FC_WEIGHTS[cl][i] * FC_WEIGHT_SCALE;
            sum += _gap_out[i] * w;
        }
        _logits[cl] = sum;
    }
}

void ClassifierEngine::computeSoftmax() {
    float max_logit = -1e9f;
    for (size_t c = 0; c < SLATENET_NUM_CLASSES; ++c) {
        if (_logits[c] > max_logit) max_logit = _logits[c];
    }

    float sum_exp = 0.0f;
    for (size_t c = 0; c < SLATENET_NUM_CLASSES; ++c) {
        _probs[c] = expf(_logits[c] - max_logit);
        sum_exp += _probs[c];
    }

    for (size_t c = 0; c < SLATENET_NUM_CLASSES; ++c) {
        _probs[c] /= sum_exp;
    }
}

ClassificationResult ClassifierEngine::classify(const float input_bitmap[SLATENET_INPUT_DIM][SLATENET_INPUT_DIM]) {
    uint32_t t_start = micros();

    forwardConv1(input_bitmap);
    forwardConv2();
    forwardGAP();
    forwardFC();
    computeSoftmax();

    uint32_t t_inference = micros() - t_start;

    // Identify Top-1 and Top-2 Candidates
    size_t top1_idx = 0;
    size_t top2_idx = 0;
    float max1 = -1.0f;
    float max2 = -1.0f;

    for (size_t c = 0; c < SLATENET_NUM_CLASSES; ++c) {
        if (_probs[c] > max1) {
            max2 = max1;
            top2_idx = top1_idx;
            max1 = _probs[c];
            top1_idx = c;
        } else if (_probs[c] > max2) {
            max2 = _probs[c];
            top2_idx = c;
        }
    }

    ClassificationResult res;
    res.top1_label       = SLATENET_CLASSES[top1_idx][0];
    res.top1_str         = SLATENET_CLASSES[top1_idx];
    res.top1_index       = (uint8_t)top1_idx;
    res.top1_confidence  = max1;

    res.top2_label       = SLATENET_CLASSES[top2_idx][0];
    res.top2_str         = SLATENET_CLASSES[top2_idx];
    res.top2_index       = (uint8_t)top2_idx;
    res.top2_confidence  = max2;

    res.confidence_margin = max1 - max2;
    res.is_ambiguous_pair = isKnownConfusionPair(res.top1_label, res.top2_label) && 
                            (res.confidence_margin < 0.20f);

    // Decision Logic
    if (max1 < _threshold) {
        res.status = RecognitionStatus::REJECT_LOW_CONFIDENCE;
        res.is_confident = false;
    } else if (res.confidence_margin < _margin_threshold || res.is_ambiguous_pair) {
        res.status = RecognitionStatus::REJECT_AMBIGUOUS_PAIR;
        res.is_confident = false;
    } else {
        res.status = RecognitionStatus::CONFIDENT;
        res.is_confident = true;
    }

    res.rasterize_us = 0;
    res.inference_us = t_inference;
    res.total_us     = t_inference;
    return res;
}

ClassificationResult ClassifierEngine::classifyStrokes(const StrokeStore& store) {
    if (store.getTotalPointCount() == 0) {
        ClassificationResult res;
        memset(&res, 0, sizeof(res));
        res.top1_label = '?';
        res.top1_str = "?";
        res.status = RecognitionStatus::REJECT_EMPTY_INPUT;
        return res;
    }

    uint32_t t0 = micros();
    float bitmap[SLATENET_INPUT_DIM][SLATENET_INPUT_DIM];
    rasterize(store, bitmap);
    uint32_t t_rasterize = micros() - t0;

    ClassificationResult res = classify(bitmap);
    res.rasterize_us = t_rasterize;
    res.total_us     = t_rasterize + res.inference_us;
    return res;
}

} // namespace DigitalSlate
