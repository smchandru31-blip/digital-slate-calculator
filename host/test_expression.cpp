#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <cmath>
#include <cassert>
#include <iomanip>
#include <filesystem>

#include "ExpressionEngine.hpp"
#include "ClassifierEngine.hpp"
#include "GeometryEngine.hpp"

namespace fs = std::filesystem;
using namespace DigitalSlate;

struct ExpressionFixture {
    std::string filename;
    std::string description;
    std::string target_expression;
    double target_value;
    std::vector<std::vector<Point2D>> strokes;
};

bool loadExpressionFixture(const std::string& path, ExpressionFixture& fix) {
    std::ifstream f(path);
    if (!f.is_open()) return false;

    std::string content((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    fix.filename = fs::path(path).filename().string();

    // Extract target_expression
    size_t expr_pos = content.find("\"target_expression\":");
    if (expr_pos != std::string::npos) {
        size_t q1 = content.find("\"", expr_pos + 20);
        size_t q2 = content.find("\"", q1 + 1);
        fix.target_expression = content.substr(q1 + 1, q2 - q1 - 1);
    }

    // Extract target_value
    size_t val_pos = content.find("\"target_value\":");
    if (val_pos != std::string::npos) {
        size_t colon = content.find(":", val_pos);
        size_t comma = content.find_first_of(",\n}", colon);
        fix.target_value = std::stod(content.substr(colon + 1, comma - (colon + 1)));
    }

    // Extract description
    size_t desc_pos = content.find("\"description\":");
    if (desc_pos != std::string::npos) {
        size_t q1 = content.find("\"", desc_pos + 14);
        size_t q2 = content.find("\"", q1 + 1);
        fix.description = content.substr(q1 + 1, q2 - q1 - 1);
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
        stroke_pos = content.find("\"points\":", arr_end + 1);
    }

    return !fix.strokes.empty();
}

int main(int argc, char* argv[]) {
    ClassifierEngine classifier;
    ExpressionEngine exprEngine;

    if (argc >= 3 && std::string(argv[1]) == "--eval") {
        std::string json_path = argv[2];
        ExpressionFixture fix;
        if (!loadExpressionFixture(json_path, fix)) {
            std::cout << "{\"status\":\"ERROR\",\"error\":\"Failed to load file\"}\n";
            return 1;
        }
        ExpressionResult res = exprEngine.processStrokes(fix.strokes, classifier);

        std::cout << "{\n";
        std::cout << "  \"status\": \"" << (res.status == ExpressionStatus::EXPRESSION_RECOGNIZED ? "EXPRESSION_RECOGNIZED" :
                                          res.status == ExpressionStatus::WAITING_FOR_TRIGGER ? "WAITING_FOR_TRIGGER" :
                                          res.status == ExpressionStatus::AMBIGUOUS_TRIGGER ? "AMBIGUOUS_TRIGGER" :
                                          res.status == ExpressionStatus::SYNTAX_ERROR ? "SYNTAX_ERROR" :
                                          res.status == ExpressionStatus::UNCERTAIN_SYMBOLS ? "UNCERTAIN_SYMBOLS" : "EMPTY_INPUT") << "\",\n";
        std::cout << "  \"expression\": \"" << res.reconstructed_expr << "\",\n";
        std::cout << "  \"calc_is_ok\": " << (res.calc_result.isOk() ? "true" : "false") << ",\n";
        std::cout << "  \"calc_value\": " << res.calc_result.value << ",\n";
        std::cout << "  \"formatted_answer\": \"" << res.calc_result.formatted << "\",\n";
        std::cout << "  \"trigger_index\": " << res.trigger_stroke_index << ",\n";
        std::cout << "  \"uncertain_count\": " << res.uncertain_symbol_count << ",\n";
        std::cout << "  \"latency_us\": " << res.total_latency_us << ",\n";
        std::cout << "  \"symbols\": [\n";
        for (size_t i = 0; i < res.symbols.size(); ++i) {
            const auto& sym = res.symbols[i];
            std::cout << "    {\"label\":\"" << sym.label << "\",\"top2\":\"" << sym.top2_label 
                      << "\",\"conf\":" << sym.confidence << ",\"is_confident\":" << (sym.is_confident ? "true" : "false")
                      << ",\"xMin\":" << sym.bbox.xMin << ",\"yMin\":" << sym.bbox.yMin 
                      << ",\"xMax\":" << sym.bbox.xMax << ",\"yMax\":" << sym.bbox.yMax 
                      << ",\"is_operator\":" << (sym.is_operator ? "true" : "false")
                      << ",\"strokes\":[";
            for (size_t s = 0; s < sym.stroke_indices.size(); ++s) {
                if (s > 0) std::cout << ",";
                std::cout << sym.stroke_indices[s];
            }
            std::cout << "]}";
            if (i + 1 < res.symbols.size()) std::cout << ",";
            std::cout << "\n";
        }
        std::cout << "  ]\n";
        std::cout << "}\n";
        return 0;
    }

    std::cout << "=========================================================================\n";
    std::cout << "  DIGITAL SLATE - HORIZONTAL EXPRESSION RECONSTRUCTION HOST UNIT TESTS  \n";
    std::cout << "=========================================================================\n\n";

    std::vector<std::string> search_dirs;
    if (argc > 1) {
        search_dirs.push_back(std::string(argv[1]));
    } else {
        std::vector<std::string> candidates = {
            "data/p8_expressions",
            "../data/p8_expressions"
        };
        for (const auto& c : candidates) {
            if (fs::exists(c)) search_dirs.push_back(c);
        }
    }

    std::vector<ExpressionFixture> fixtures;
    for (const auto& d : search_dirs) {
        if (!fs::exists(d)) continue;
        for (const auto& entry : fs::directory_iterator(d)) {
            if (entry.path().extension() == ".json") {
                ExpressionFixture fix;
                if (loadExpressionFixture(entry.path().string(), fix)) {
                    fixtures.push_back(fix);
                }
            }
        }
    }

    std::cout << "Loaded " << fixtures.size() << " expression replay fixtures.\n\n";

    size_t total_expressions = fixtures.size();
    size_t correct_expressions = 0;
    size_t correct_answers = 0;
    size_t total_symbols = 0;
    size_t correct_symbols = 0;
    uint64_t total_latency_us = 0;

    std::cout << std::left << std::setw(32) << "Fixture Description"
              << std::setw(16) << "Target Expr"
              << std::setw(16) << "Recognized"
              << std::setw(12) << "Calc Value"
              << std::setw(12) << "Target Val"
              << std::setw(10) << "Expr Match"
              << std::setw(10) << "Ans Match"
              << std::setw(10) << "Latency"
              << "\n";
    std::cout << std::string(116, '-') << "\n";

    for (const auto& fix : fixtures) {
        ExpressionResult res = exprEngine.processStrokes(fix.strokes, classifier);

        bool expr_match = (res.reconstructed_expr == fix.target_expression);
        bool ans_match = (res.calc_result.isOk() && std::abs(res.calc_result.value - fix.target_value) < 1e-4);

        if (expr_match) correct_expressions++;
        if (ans_match) correct_answers++;
        total_latency_us += res.total_latency_us;

        // Count symbol-level accuracy against target string
        size_t min_len = std::min(res.reconstructed_expr.length(), fix.target_expression.length());
        for (size_t i = 0; i < min_len; ++i) {
            if (res.reconstructed_expr[i] == fix.target_expression[i]) {
                correct_symbols++;
            }
        }
        total_symbols += std::max(res.reconstructed_expr.length(), fix.target_expression.length());

        std::string calc_val_str = res.calc_result.isOk() ? res.calc_result.formatted : "ERROR";
        std::string target_val_str = std::to_string((int)fix.target_value);

        std::cout << std::left << std::setw(32) << fix.description.substr(0, 30)
                  << std::setw(16) << fix.target_expression
                  << std::setw(16) << res.reconstructed_expr
                  << std::setw(12) << calc_val_str
                  << std::setw(12) << target_val_str
                  << std::setw(10) << (expr_match ? "[OK]" : "[FAIL]")
                  << std::setw(10) << (ans_match ? "[OK]" : "[FAIL]")
                  << (std::to_string(res.total_latency_us) + " us")
                  << "\n" << std::flush;
    }

    std::cout << std::string(116, '-') << "\n\n" << std::flush;

    double symbol_acc = (total_symbols > 0) ? (correct_symbols * 100.0 / total_symbols) : 0.0;
    double expr_acc   = (total_expressions > 0) ? (correct_expressions * 100.0 / total_expressions) : 0.0;
    double ans_acc    = (total_expressions > 0) ? (correct_answers * 100.0 / total_expressions) : 0.0;
    double avg_lat    = (total_expressions > 0) ? ((double)total_latency_us / total_expressions) : 0.0;

    std::cout << "=================== P8 EXPRESSION RECONSTRUCTION METRICS ===================\n";
    std::cout << "Total Expressions Evaluated: " << total_expressions << "\n";
    std::cout << "Symbol Recognition Accuracy: " << correct_symbols << " / " << total_symbols
              << " (" << std::fixed << std::setprecision(2) << symbol_acc << "%)\n";
    std::cout << "Full Expression Accuracy   : " << correct_expressions << " / " << total_expressions
              << " (" << expr_acc << "%)\n";
    std::cout << "Exact Answer Accuracy      : " << correct_answers << " / " << total_expressions
              << " (" << ans_acc << "%)\n";
    std::cout << "Average Host Latency       : " << avg_lat << " us (" << (avg_lat / 1000.0) << " ms)\n";
    std::cout << "============================================================================\n\n" << std::flush;

    if (expr_acc < 100.0) {
        std::cerr << "[FAIL] One or more expressions failed recognition!\n" << std::flush;
        return 1;
    }

    std::cout << "[PASS] All P8 expression reconstruction acceptance tests passed successfully!\n" << std::flush;
    return 0;
}
