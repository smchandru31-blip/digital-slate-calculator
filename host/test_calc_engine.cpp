#include "../firmware/src/CalcEngine.hpp"
#include <iostream>
#include <string>
#include <vector>
#include <cmath>
#include <iomanip>

using namespace DigitalSlate;

struct TestCase {
    std::string name;
    std::string expr;
    bool shouldPass;
    double expectedValue;
    CalcStatus expectedStatus;
};

int main() {
    std::cout << "=========================================================\n";
    std::cout << "  DIGITAL SLATE CALCULATOR - P3 CALCULATION ENGINE TESTS  \n";
    std::cout << "=========================================================\n";

    std::vector<TestCase> testCases = {
        // --- 1. Core Arithmetic Precedence (Mandatory Tests) ---
        {"Precedence Mul over Add",         "2+3*4",            true,  14.0,  CalcStatus::OK},
        {"Parentheses Override",            "(2+3)*4",          true,  20.0,  CalcStatus::OK},
        {"Left-Associative Div and Mul",    "24/6*2",           true,  8.0,   CalcStatus::OK},
        {"Left-Associative Subtraction",    "20-5-3",           true,  12.0,  CalcStatus::OK},
        {"Nested Parentheses Subtraction",  "100-(25+15)",      true,  60.0,  CalcStatus::OK},
        {"Multi-digit Add and Sub",         "23+12-13",         true,  22.0,  CalcStatus::OK},
        {"Decimal Arithmetic",              "3.5+2.75",         true,  6.25,  CalcStatus::OK},

        // --- 2. Error Rejection (Mandatory Tests) ---
        {"Divide by Zero Rejection",        "10/0",             false, 0.0,   CalcStatus::ERR_DIVIDE_BY_ZERO},
        {"Divide by Zero via Expr",         "24/(10-10)",       false, 0.0,   CalcStatus::ERR_DIVIDE_BY_ZERO},
        {"Unmatched Open Paren",            "(2+3",             false, 0.0,   CalcStatus::ERR_UNMATCHED_PAREN},
        {"Unmatched Close Paren",           "2+3)",             false, 0.0,   CalcStatus::ERR_UNMATCHED_PAREN},
        {"Mismatched Bracket Types",        "(2+3]",            false, 0.0,   CalcStatus::ERR_UNMATCHED_PAREN},
        {"Missing Right Operand",           "2+",               false, 0.0,   CalcStatus::ERR_MISSING_OPERAND},
        {"Missing Left Operand",            "*4",               false, 0.0,   CalcStatus::ERR_MISSING_OPERAND},
        {"Double Operator (Missing)",       "2+*3",             false, 0.0,   CalcStatus::ERR_MISSING_OPERAND},
        {"Empty Parentheses",               "()",               false, 0.0,   CalcStatus::ERR_MISSING_OPERAND},
        {"Empty String Rejection",          "",                 false, 0.0,   CalcStatus::ERR_EMPTY},
        {"Overflow Handled",                "1e308*10",         false, 0.0,   CalcStatus::ERR_OVERFLOW},

        // --- 3. Extended Features & Unicode Symbols ---
        {"Unicode Minus (U+2212)",          "20\xE2\x88\x92" "5\xE2\x88\x92" "3", true, 12.0, CalcStatus::OK},
        {"Unicode Multiply (U+00D7)",       "6\xC3\x97" "7",    true,  42.0,  CalcStatus::OK},
        {"Unicode Divide (U+00F7)",         "24\xC3\xB7" "6",   true,  4.0,   CalcStatus::OK},
        {"Letter 'x' as Multiply",          "5x6",              true,  30.0,  CalcStatus::OK},
        {"Nested Brackets [] and {}",       "[10+{5*(3-1)}]",   true,  20.0,  CalcStatus::OK},
        {"Unary Minus Leading",             "-5+12",            true,  7.0,   CalcStatus::OK},
        {"Unary Minus in Factor",           "10*-3",            true,  -30.0, CalcStatus::OK},
        {"Double Unary Minus",              "-(-8)",            true,  8.0,   CalcStatus::OK},
        {"Decimals without Leading Zero",   ".5+.25",           true,  0.75,  CalcStatus::OK},

        // --- 4. Experimental Operators (Powers, Sqrt, Percent) ---
        {"Power Operator",                  "2^3",              true,  8.0,   CalcStatus::OK},
        {"Power Right-Associative",         "2^3^2",            true,  512.0, CalcStatus::OK},
        {"Power with Negation",             "-2^2",             true,  -4.0,  CalcStatus::OK},
        {"Square Root (ASCII sqrt)",        "sqrt 16",          false, 0.0,   CalcStatus::ERR_INVALID_CHARACTER},
        {"Unicode Square Root (U+221A)",    "\xE2\x88\x9A" "25", true, 5.0,   CalcStatus::OK},
        {"Square Root of Negative",         "\xE2\x88\x9A" "(-9)", false, 0.0, CalcStatus::ERR_INVALID_ROOT},
        {"Percent Postfix",                 "50%",              true,  0.5,   CalcStatus::OK},
        {"Percent in Multiplication",       "200*15%",          true,  30.0,  CalcStatus::OK}
    };

    int passed = 0;
    int failed = 0;

    for (size_t i = 0; i < testCases.size(); ++i) {
        const auto& tc = testCases[i];
        CalcResult res = CalcEngine::evaluate(tc.expr.c_str());

        bool testOk = false;
        if (tc.shouldPass) {
            if (res.isOk() && std::abs(res.value - tc.expectedValue) < 1e-9) {
                testOk = true;
            }
        } else {
            if (res.status == tc.expectedStatus) {
                testOk = true;
            }
        }

        std::cout << "[" << (i + 1 < 10 ? " " : "") << (i + 1) << "] "
                  << std::left << std::setw(34) << tc.name
                  << " Expr: " << std::setw(18) << tc.expr
                  << " -> Result: " << std::setw(12) << res.formatted;

        if (testOk) {
            std::cout << " [PASS]\n";
            passed++;
        } else {
            std::cout << " [FAIL] (Expected "
                      << (tc.shouldPass ? std::to_string(tc.expectedValue) : res.getErrorMessage())
                      << ")\n";
            failed++;
        }
    }

    std::cout << "---------------------------------------------------------\n";
    std::cout << "Summary: " << passed << " Passed, " << failed << " Failed, Total: " << testCases.size() << "\n";
    std::cout << "=========================================================\n";

    return (failed == 0) ? 0 : 1;
}
