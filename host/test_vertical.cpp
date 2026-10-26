#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <cmath>
#include <cassert>
#include <iomanip>
#include <algorithm>
#include <filesystem>

#include "VerticalEngine.hpp"
#include "ClassifierEngine.hpp"
#include "GeometryEngine.hpp"

namespace fs = std::filesystem;
using namespace DigitalSlate;

struct VerticalFixture {
    std::string filename;
    std::string description;
    std::string expected_status;
    std::string expected_expression;
    double expected_value;
    bool is_inferred;
    std::vector<std::vector<Point2D>> strokes;
};

static std::string statusToString(VerticalStatus status) {
    switch (status) {
        case VerticalStatus::VERTICAL_OK: return "VERTICAL_OK";
        case VerticalStatus::NOT_VERTICAL_LAYOUT: return "NOT_VERTICAL_LAYOUT";
        case VerticalStatus::AMBIGUOUS_OPERATOR: return "AMBIGUOUS_OPERATOR";
        case VerticalStatus::AMBIGUOUS_PLACE_VALUE: return "AMBIGUOUS_PLACE_VALUE";
        case VerticalStatus::AMBIGUOUS_CRAMPED_ROWS: return "AMBIGUOUS_CRAMPED_ROWS";
        case VerticalStatus::INCONSISTENT_ROW_COUNT: return "INCONSISTENT_ROW_COUNT";
        case VerticalStatus::WAITING_FOR_TRIGGER: return "WAITING_FOR_TRIGGER";
        case VerticalStatus::AMBIGUOUS_TRIGGER: return "AMBIGUOUS_TRIGGER";
        case VerticalStatus::UNCERTAIN_SYMBOLS: return "UNCERTAIN_SYMBOLS";
        default: return "UNKNOWN";
    }
}

bool loadVerticalFixture(const std::string& path, VerticalFixture& fix) {
    std::ifstream f(path);
    if (!f.is_open()) return false;

    std::string content((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    fix.filename = fs::path(path).filename().string();

    // Extract description
    size_t desc_pos = content.find("\"description\":");
    if (desc_pos != std::string::npos) {
        size_t q1 = content.find("\"", desc_pos + 14);
        size_t q2 = content.find("\"", q1 + 1);
        fix.description = content.substr(q1 + 1, q2 - q1 - 1);
    }

    // Extract expected_status
    size_t stat_pos = content.find("\"expected_status\":");
    if (stat_pos != std::string::npos) {
        size_t q1 = content.find("\"", stat_pos + 18);
        size_t q2 = content.find("\"", q1 + 1);
        fix.expected_status = content.substr(q1 + 1, q2 - q1 - 1);
    }

    // Extract expected_expression
    size_t expr_pos = content.find("\"expected_expression\":");
    if (expr_pos != std::string::npos) {
        size_t q1 = content.find("\"", expr_pos + 22);
        size_t q2 = content.find("\"", q1 + 1);
        fix.expected_expression = content.substr(q1 + 1, q2 - q1 - 1);
    }

    // Extract expected_value
    size_t val_pos = content.find("\"expected_value\":");
    if (val_pos != std::string::npos) {
        size_t colon = content.find(":", val_pos);
        size_t comma = content.find_first_of(",\n}", colon);
        fix.expected_value = std::stod(content.substr(colon + 1, comma - (colon + 1)));
    }

    // Extract strokes
    fix.strokes.clear();
    size_t stroke_pos = content.find("\"points\":");
    while (stroke_pos != std::string::npos) {
        std::vector<Point2D> pts;
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

            pts.emplace_back((int16_t)x, (int16_t)y, t);
        }

        if (!pts.empty()) {
            fix.strokes.push_back(pts);
        }

        stroke_pos = content.find("\"points\":", arr_end);
    }

    return !fix.strokes.empty();
}

int main(int argc, char* argv[]) {
    ClassifierEngine classifier;
    VerticalEngine vertEngine;

    // CLI mode for simulator integration
    if (argc >= 3 && std::string(argv[1]) == "--eval") {
        std::string json_path = argv[2];
        VerticalFixture fix;
        if (!loadVerticalFixture(json_path, fix)) {
            std::cout << "{\"status\":\"ERROR\",\"error\":\"Failed to load file\"}\n";
            return 1;
        }
        VerticalResult res = vertEngine.processStrokes(fix.strokes, classifier);

        std::cout << "{\n";
        std::cout << "  \"status\": \"" << statusToString(res.status) << "\",\n";
        std::cout << "  \"expression\": \"" << res.reconstructed_expr << "\",\n";
        std::cout << "  \"calc_is_ok\": " << (res.calc_result.isOk() ? "true" : "false") << ",\n";
        std::cout << "  \"calc_value\": " << res.calc_result.value << ",\n";
        std::cout << "  \"formatted_answer\": \"" << res.calc_result.formatted << "\",\n";
        std::cout << "  \"is_operator_inferred\": " << (res.is_operator_inferred ? "true" : "false") << ",\n";
        std::cout << "  \"trigger_index\": " << res.trigger_stroke_index << ",\n";
        std::cout << "  \"latency_us\": " << res.total_latency_us << ",\n";
        std::cout << "  \"rows\": [\n";
        for (size_t r = 0; r < res.rows.size(); ++r) {
            const auto& row = res.rows[r];
            std::cout << "    {\"operand\":\"" << row.operand_str << "\",\"xMin\":" << row.bbox.xMin 
                      << ",\"xMax\":" << row.bbox.xMax << ",\"yMin\":" << row.bbox.yMin << ",\"yMax\":" << row.bbox.yMax 
                      << ",\"digits\":[";
            for (size_t d = 0; d < row.digits.size(); ++d) {
                if (d > 0) std::cout << ",";
                std::cout << "{\"label\":\"" << row.digits[d].label << "\",\"xMin\":" << row.digits[d].bbox.xMin << ",\"xMax\":" << row.digits[d].bbox.xMax << "}";
            }
            std::cout << "]}";
            if (r + 1 < res.rows.size()) std::cout << ",";
            std::cout << "\n";
        }
        std::cout << "  ],\n";
        std::cout << "  \"diagnostic_reason\": \"" << res.diagnostic_reason << "\"\n";
        std::cout << "}\n";
        return 0;
    }

    std::cout << "=========================================================================\n";
    std::cout << "    DIGITAL SLATE - VERTICAL EXPRESSION RECONSTRUCTION HOST TESTS       \n";
    std::cout << "=========================================================================\n\n";

    std::vector<std::string> search_dirs;
    if (argc > 1) {
        search_dirs.push_back(std::string(argv[1]));
    } else {
        std::vector<std::string> candidates = {
            "data/p9_vertical",
            "../data/p9_vertical"
        };
        for (const auto& c : candidates) {
            if (fs::exists(c)) search_dirs.push_back(c);
        }
    }

    std::vector<VerticalFixture> fixtures;
    for (const auto& d : search_dirs) {
        if (!fs::exists(d)) continue;
        for (const auto& entry : fs::directory_iterator(d)) {
            if (entry.path().extension() == ".json") {
                VerticalFixture fix;
                if (loadVerticalFixture(entry.path().string(), fix)) {
                    fixtures.push_back(fix);
                }
            }
        }
    }

    // Sort fixtures alphabetically by filename for clean reporting
    std::sort(fixtures.begin(), fixtures.end(), [](const VerticalFixture& a, const VerticalFixture& b) {
        return a.filename < b.filename;
    });

    std::cout << "Loaded " << fixtures.size() << " vertical expression fixtures.\n\n";

    size_t total_fixtures = fixtures.size();
    size_t status_matches = 0;
    size_t valid_eval_matches = 0;
    size_t valid_cases = 0;
    size_t rejected_cases = 0;
    size_t correct_rejections = 0;
    size_t silent_guesses = 0;
    uint64_t total_latency_us = 0;

    std::cout << std::left << std::setw(30) << "Fixture Description"
              << std::setw(24) << "Expected Status"
              << std::setw(24) << "Actual Status"
              << std::setw(12) << "Calc Value"
              << std::setw(12) << "Target Val"
              << std::setw(10) << "Result"
              << "Latency"
              << "\n";
    std::cout << std::string(120, '-') << "\n";

    for (const auto& fix : fixtures) {
        VerticalResult res = vertEngine.processStrokes(fix.strokes, classifier);
        total_latency_us += res.total_latency_us;

        std::string actual_status_str = statusToString(res.status);
        bool status_ok = (actual_status_str == fix.expected_status);
        if (status_ok) status_matches++;

        bool is_valid_target = (fix.expected_status == "VERTICAL_OK");
        bool eval_ok = false;

        if (is_valid_target) {
            valid_cases++;
            eval_ok = (res.status == VerticalStatus::VERTICAL_OK &&
                       res.calc_result.isOk() &&
                       std::abs(res.calc_result.value - fix.expected_value) < 1e-4 &&
                       res.reconstructed_expr == fix.expected_expression);
            if (eval_ok) valid_eval_matches++;
        } else {
            rejected_cases++;
            // A non-valid case MUST be rejected (never silently calculated!)
            if (res.status != VerticalStatus::VERTICAL_OK) {
                correct_rejections++;
            } else {
                silent_guesses++;
            }
        }

        bool overall_pass = is_valid_target ? eval_ok : status_ok;

        std::string calc_str = res.calc_result.isOk() ? res.calc_result.formatted : "--";
        std::string target_str = is_valid_target ? std::to_string((int)fix.expected_value) : "--";

        std::cout << std::left << std::setw(30) << fix.description.substr(0, 28)
                  << std::setw(24) << fix.expected_status
                  << std::setw(24) << actual_status_str
                  << std::setw(12) << calc_str
                  << std::setw(12) << target_str
                  << std::setw(10) << (overall_pass ? "[PASS]" : "[FAIL]")
                  << (std::to_string(res.total_latency_us) + " us")
                  << "\n";
    }

    std::cout << std::string(120, '-') << "\n\n";

    double status_acc = (total_fixtures > 0) ? (status_matches * 100.0 / total_fixtures) : 0.0;
    double valid_acc  = (valid_cases > 0) ? (valid_eval_matches * 100.0 / valid_cases) : 0.0;
    double reject_recall = (rejected_cases > 0) ? (correct_rejections * 100.0 / rejected_cases) : 0.0;
    double avg_lat = (total_fixtures > 0) ? ((double)total_latency_us / total_fixtures) : 0.0;

    std::cout << "===================== P9 VERTICAL EXPRESSION METRICS =====================\n";
    std::cout << "Total Layout Fixtures Evaluated : " << total_fixtures << "\n";
    std::cout << "Layout Status Accuracy          : " << status_matches << " / " << total_fixtures
              << " (" << std::fixed << std::setprecision(2) << status_acc << "%)\n";
    std::cout << "Valid Layout Evaluation Accuracy: " << valid_eval_matches << " / " << valid_cases
              << " (" << valid_acc << "%)\n";
    std::cout << "Ambiguity Rejection Recall      : " << correct_rejections << " / " << rejected_cases
              << " (" << reject_recall << "%)\n";
    std::cout << "Silent Guess Violations         : " << silent_guesses << " (Requirement: exactly 0)\n";
    std::cout << "Average Host Latency            : " << avg_lat << " us (" << (avg_lat / 1000.0) << " ms)\n";
    std::cout << "==========================================================================\n\n";

    if (silent_guesses > 0) {
        std::cerr << "[FAIL] Constitutional Violation: Ambiguous layout was silently guessed!\n";
        return 1;
    }
    if (status_acc < 100.0 || valid_acc < 100.0) {
        std::cerr << "[FAIL] One or more vertical layout tests failed!\n";
        return 1;
    }

    std::cout << "[PASS] All Phase P9 vertical expression acceptance criteria PASSED!\n";
    return 0;
}
