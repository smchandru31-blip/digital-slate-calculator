#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <cmath>
#include <cassert>
#include <iomanip>
#include <filesystem>

#include "StrokeStore.hpp"
#include "ClassifierEngine.hpp"

namespace fs = std::filesystem;
using namespace DigitalSlate;

// Simple JSON parser for stroke test fixtures
bool loadSampleJSON(const std::string& path, StrokeStore& store, std::string& expected_label) {
    std::ifstream f(path);
    if (!f.is_open()) return false;

    std::string content((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());

    // Extract label
    size_t lbl_pos = content.find("\"label\":");
    if (lbl_pos == std::string::npos) return false;
    size_t q1 = content.find("\"", lbl_pos + 8);
    size_t q2 = content.find("\"", q1 + 1);
    expected_label = content.substr(q1 + 1, q2 - q1 - 1);

    // Extract strokes
    store.clear();
    size_t stroke_pos = content.find("\"points\":");
    while (stroke_pos != std::string::npos) {
        store.startStroke();
        size_t arr_end = content.find("]", stroke_pos);
        size_t cur = stroke_pos;

        while (cur < arr_end) {
            size_t x_pos = content.find("\"x\":", cur);
            if (x_pos == std::string::npos || x_pos > arr_end) break;
            size_t comma1 = content.find_first_of(",}", x_pos);
            int x = std::stoi(content.substr(x_pos + 4, comma1 - (x_pos + 4)));

            size_t y_pos = content.find("\"y\":", comma1);
            if (y_pos == std::string::npos || y_pos > arr_end) break;
            size_t comma2 = content.find_first_of(",}", y_pos);
            int y = std::stoi(content.substr(y_pos + 4, comma2 - (y_pos + 4)));

            size_t t_pos = content.find("\"t\":", comma2);
            uint32_t t = 0;
            if (t_pos != std::string::npos && t_pos < arr_end) {
                size_t comma3 = content.find_first_of(",}", t_pos);
                t = (uint32_t)(std::stoull(content.substr(t_pos + 4, comma3 - (t_pos + 4))) & 0xFFFFFFFFULL);
                cur = comma3;
            } else {
                cur = comma2;
            }

            store.addPoint((int16_t)x, (int16_t)y, t);
        }
        store.endStroke();
        stroke_pos = content.find("\"points\":", arr_end + 1);
    }

    return store.getTotalPointCount() > 0;
}

int main(int argc, char* argv[]) {
    std::cout << "=========================================================================\n";
    std::cout << "  DIGITAL SLATE - CLASSIFIER ENGINE & PREPROCESSING HOST UNIT TESTS      \n";
    std::cout << "=========================================================================\n\n";

    ClassifierEngine classifier;
    std::cout << "Model Architecture    : SlateNet-CNN (INT8 Quantized)\n";
    std::cout << "Input Dimensions      : 28 x 28 grayscale\n";
    std::cout << "Confidence Threshold  : " << classifier.getConfidenceThreshold() << "\n";
    std::cout << "Margin Threshold      : " << classifier.getMarginThreshold() << "\n\n";

    // Candidate root paths
    std::vector<std::string> test_dirs;
    if (argc > 1) {
        test_dirs.push_back(std::string(argv[1]));
    } else {
        std::vector<std::string> candidates = {
            "data/dataset/test", "data/dataset/val", "data/dataset/raw/W01",
            "../data/dataset/test", "../data/dataset/val", "../data/dataset/raw/W01"
        };
        for (const auto& c : candidates) {
            if (fs::exists(c)) {
                test_dirs.push_back(c);
            }
        }
    }

    size_t total_tested = 0;
    size_t total_passed = 0;
    size_t total_confident = 0;
    double total_inference_us = 0.0;
    double total_rasterize_us = 0.0;

    std::cout << std::left << std::setw(28) << "Sample File"
              << std::setw(8) << "Target"
              << std::setw(8) << "Top-1"
              << std::setw(10) << "Conf-1"
              << std::setw(8) << "Top-2"
              << std::setw(10) << "Conf-2"
              << std::setw(14) << "Status"
              << std::setw(12) << "Latency"
              << "\n";
    std::cout << std::string(98, '-') << "\n";

    for (const auto& dir_str : test_dirs) {
        if (!fs::exists(dir_str)) continue;

        for (const auto& entry : fs::recursive_directory_iterator(dir_str)) {
            if (entry.path().extension() == ".json") {
                StrokeStore store;
                std::string target_label;
                if (!loadSampleJSON(entry.path().string(), store, target_label)) continue;

                ClassificationResult res = classifier.classifyStrokes(store);

                total_tested++;
                bool match = (std::string(1, res.top1_label) == target_label);
                if (match) total_passed++;
                if (res.is_confident) total_confident++;
                total_inference_us += res.inference_us;
                total_rasterize_us += res.rasterize_us;

                std::string status_str = res.is_confident ? "CONFIDENT" : 
                    (res.status == RecognitionStatus::REJECT_AMBIGUOUS_PAIR ? "AMBIGUOUS" : "LOW_CONF");

                // Print first 20 samples to keep output readable
                if (total_tested <= 20) {
                    std::cout << std::left << std::setw(28) << entry.path().filename().string().substr(0, 26)
                              << std::setw(8) << target_label
                              << std::setw(8) << res.top1_label
                              << std::setw(10) << (std::to_string((int)(res.top1_confidence * 100)) + "%")
                              << std::setw(8) << res.top2_label
                              << std::setw(10) << (std::to_string((int)(res.top2_confidence * 100)) + "%")
                              << std::setw(14) << status_str
                              << (std::to_string(res.total_us) + " us")
                              << "\n";
                }
            }
        }
    }

    std::cout << std::string(98, '-') << "\n";
    std::cout << "\n================ SUMMARY OF INFERENCE UNIT TESTS ================\n";
    std::cout << "Total Samples Evaluated : " << total_tested << "\n";
    std::cout << "Top-1 Matches           : " << total_passed << " ("
              << std::fixed << std::setprecision(1) << (total_passed * 100.0 / total_tested) << "%)\n";
    std::cout << "Confident Classifications: " << total_confident << " ("
              << (total_confident * 100.0 / total_tested) << "%)\n";
    if (total_tested > 0) {
        std::cout << "Average Rasterize Time  : " << (total_rasterize_us / total_tested) << " us\n";
        std::cout << "Average Inference Time  : " << (total_inference_us / total_tested) << " us\n";
        std::cout << "Average Total Latency   : " << ((total_rasterize_us + total_inference_us) / total_tested) << " us\n";
    }
    std::cout << "=================================================================\n";

    assert(total_tested > 0);
    assert(total_passed * 100.0 / total_tested >= 70.0);
    std::cout << "\n[PASS] All ClassifierEngine host tests completed successfully!\n";
    return 0;
}
