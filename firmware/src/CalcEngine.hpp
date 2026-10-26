#pragma once

#include <cstddef>
#include <cstdint>

/**
 * @brief Calculation Engine and Expression Parser for Digital Slate Calculator.
 * Hardware-independent: builds cleanly on ESP32 (Arduino / ESP-IDF) and PC host.
 * 
 * Numeric representation: IEEE 754 64-bit double-precision floating point (`double`).
 * - Dynamic range: ~1.7e-308 to ~1.7e+308
 * - Precision: ~15 to 17 significant decimal digits (53-bit significand)
 */

namespace DigitalSlate {

enum class CalcStatus {
    OK = 0,
    ERR_EMPTY,
    ERR_DIVIDE_BY_ZERO,
    ERR_UNMATCHED_PAREN,
    ERR_MISSING_OPERAND,
    ERR_INVALID_CHARACTER,
    ERR_OVERFLOW,
    ERR_INVALID_ROOT,
    ERR_SYNTAX
};

struct CalcResult {
    CalcStatus status;
    double value;
    char formatted[48];

    bool isOk() const { return status == CalcStatus::OK; }
    const char* getErrorMessage() const;
};

class CalcEngine {
public:
    CalcEngine();

    /**
     * @brief Parse and evaluate an arithmetic expression string.
     * @param expr Null-terminated ASCII/UTF-8 expression string (e.g. "2+3*4", "100-(25+15)")
     * @return CalcResult containing evaluation status, numeric value, and formatted output.
     */
    static CalcResult evaluate(const char* expr);

    /**
     * @brief Format a double-precision value cleanly without trailing zeros or float artifacts.
     */
    static void formatValue(double val, char* out, size_t maxLen);

private:
    enum TokenType {
        TOK_END,
        TOK_NUMBER,
        TOK_PLUS,
        TOK_MINUS,
        TOK_MUL,
        TOK_DIV,
        TOK_LPAREN,
        TOK_RPAREN,
        TOK_LBRACKET,
        TOK_RBRACKET,
        TOK_LBRACE,
        TOK_RBRACE,
        // Experimental operators
        TOK_POWER,
        TOK_SQRT,
        TOK_PERCENT,
        TOK_ERROR
    };

    struct Token {
        TokenType type;
        double numValue;
        size_t position;
    };

    class Parser {
    public:
        explicit Parser(const char* src);
        CalcResult parse();

    private:
        const char* _src;
        size_t _pos;
        Token _current;
        CalcStatus _status;

        void advance();
        Token nextToken();
        bool match(TokenType t);
        bool expect(TokenType t, CalcStatus errIfMissing);

        double parseExpression();
        double parseAddSub();
        double parseMulDiv();
        double parsePower();
        double parseUnary();
        double parsePostfix();
        double parsePrimary();

        void setError(CalcStatus err);
    };
};

} // namespace DigitalSlate
