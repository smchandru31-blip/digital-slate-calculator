#include "CalcEngine.hpp"
#include <cmath>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <cstdio>

namespace DigitalSlate {

const char* CalcResult::getErrorMessage() const {
    switch (status) {
        case CalcStatus::OK:                    return "OK";
        case CalcStatus::ERR_EMPTY:             return "Error: Empty expression";
        case CalcStatus::ERR_DIVIDE_BY_ZERO:    return "Error: Divide by zero";
        case CalcStatus::ERR_UNMATCHED_PAREN:   return "Error: Unmatched bracket";
        case CalcStatus::ERR_MISSING_OPERAND:   return "Error: Missing operand";
        case CalcStatus::ERR_INVALID_CHARACTER: return "Error: Invalid character";
        case CalcStatus::ERR_OVERFLOW:          return "Error: Overflow";
        case CalcStatus::ERR_INVALID_ROOT:      return "Error: Negative square root";
        case CalcStatus::ERR_SYNTAX:            return "Error: Syntax error";
        default:                                return "Error: Calculation failed";
    }
}

CalcEngine::CalcEngine() {}

CalcResult CalcEngine::evaluate(const char* expr) {
    if (!expr) {
        CalcResult res;
        res.status = CalcStatus::ERR_EMPTY;
        res.value = 0.0;
        snprintf(res.formatted, sizeof(res.formatted), "%s", res.getErrorMessage());
        return res;
    }

    Parser parser(expr);
    return parser.parse();
}

void CalcEngine::formatValue(double val, char* out, size_t maxLen) {
    if (!out || maxLen == 0) return;

    if (std::isnan(val)) {
        snprintf(out, maxLen, "NaN");
        return;
    }
    if (std::isinf(val)) {
        snprintf(out, maxLen, val > 0 ? "Infinity" : "-Infinity");
        return;
    }

    // Check if within normal integer-like range
    double rounded = std::round(val);
    if (std::abs(val - rounded) < 1e-11 && std::abs(val) < 1e14) {
        snprintf(out, maxLen, "%.0f", rounded);
        return;
    }

    // Check for extreme magnitudes requiring scientific notation
    double absVal = std::abs(val);
    if (absVal > 0.0 && (absVal >= 1e12 || absVal < 1e-5)) {
        snprintf(out, maxLen, "%.6g", val);
        return;
    }

    // Standard decimal format, then trim trailing zeros
    char buf[64];
    snprintf(buf, sizeof(buf), "%.8f", val);

    char* dot = strchr(buf, '.');
    if (dot) {
        char* end = buf + strlen(buf) - 1;
        while (end > dot && *end == '0') {
            *end-- = '\0';
        }
        if (end == dot) {
            *dot = '\0';
        }
    }

    snprintf(out, maxLen, "%s", buf);
}

// ============================================================================
// Parser Implementation (Recursive Descent)
// ============================================================================

CalcEngine::Parser::Parser(const char* src)
    : _src(src), _pos(0), _status(CalcStatus::OK) {
    _current.type = TOK_END;
    _current.numValue = 0.0;
    _current.position = 0;
}

void CalcEngine::Parser::setError(CalcStatus err) {
    if (_status == CalcStatus::OK) {
        _status = err;
    }
}

void CalcEngine::Parser::advance() {
    _current = nextToken();
}

bool CalcEngine::Parser::match(TokenType t) {
    if (_current.type == t) {
        advance();
        return true;
    }
    return false;
}

bool CalcEngine::Parser::expect(TokenType t, CalcStatus errIfMissing) {
    if (_current.type == t) {
        advance();
        return true;
    }
    setError(errIfMissing);
    return false;
}

CalcEngine::Token CalcEngine::Parser::nextToken() {
    Token tok;
    tok.numValue = 0.0;

    // Skip whitespace
    while (_src[_pos] && (std::isspace(static_cast<unsigned char>(_src[_pos])) || _src[_pos] == '\r' || _src[_pos] == '\n')) {
        _pos++;
    }

    tok.position = _pos;

    if (_src[_pos] == '\0') {
        tok.type = TOK_END;
        return tok;
    }

    const unsigned char c = static_cast<unsigned char>(_src[_pos]);

    // UTF-8 multi-byte symbol checks
    if (c == 0xE2) {
        // Minus: 0xE2 0x88 0x92 (−)
        if (static_cast<unsigned char>(_src[_pos + 1]) == 0x88 && static_cast<unsigned char>(_src[_pos + 2]) == 0x92) {
            _pos += 3;
            tok.type = TOK_MINUS;
            return tok;
        }
        // Square root: 0xE2 0x88 0x9A (√)
        if (static_cast<unsigned char>(_src[_pos + 1]) == 0x88 && static_cast<unsigned char>(_src[_pos + 2]) == 0x9A) {
            _pos += 3;
            tok.type = TOK_SQRT;
            return tok;
        }
    } else if (c == 0xC3) {
        // Multiply: 0xC3 0x97 (×)
        if (static_cast<unsigned char>(_src[_pos + 1]) == 0x97) {
            _pos += 2;
            tok.type = TOK_MUL;
            return tok;
        }
        // Divide: 0xC3 0xB7 (÷)
        if (static_cast<unsigned char>(_src[_pos + 1]) == 0xB7) {
            _pos += 2;
            tok.type = TOK_DIV;
            return tok;
        }
    }

    // Number (digits or starting with '.')
    if (std::isdigit(c) || (c == '.' && std::isdigit(static_cast<unsigned char>(_src[_pos + 1])))) {
        char* endPtr = nullptr;
        tok.numValue = std::strtod(&_src[_pos], &endPtr);
        if (endPtr && endPtr > &_src[_pos]) {
            _pos = static_cast<size_t>(endPtr - _src);
            tok.type = TOK_NUMBER;
            return tok;
        }
    }

    // Single-byte ASCII operators & brackets
    _pos++;
    switch (c) {
        case '+': tok.type = TOK_PLUS; return tok;
        case '-': tok.type = TOK_MINUS; return tok;
        case '*': tok.type = TOK_MUL; return tok;
        case '/': tok.type = TOK_DIV; return tok;
        case '(': tok.type = TOK_LPAREN; return tok;
        case ')': tok.type = TOK_RPAREN; return tok;
        case '[': tok.type = TOK_LBRACKET; return tok;
        case ']': tok.type = TOK_RBRACKET; return tok;
        case '{': tok.type = TOK_LBRACE; return tok;
        case '}': tok.type = TOK_RBRACE; return tok;
        case '^': tok.type = TOK_POWER; return tok;
        case '%': tok.type = TOK_PERCENT; return tok;
        case 'x':
        case 'X':
            // 'x' as multiplication operator
            tok.type = TOK_MUL;
            return tok;
        default:
            tok.type = TOK_ERROR;
            setError(CalcStatus::ERR_INVALID_CHARACTER);
            return tok;
    }
}

CalcResult CalcEngine::Parser::parse() {
    CalcResult result;
    result.status = CalcStatus::OK;
    result.value = 0.0;
    result.formatted[0] = '\0';

    advance();

    if (_current.type == TOK_END) {
        result.status = CalcStatus::ERR_EMPTY;
        snprintf(result.formatted, sizeof(result.formatted), "%s", result.getErrorMessage());
        return result;
    }

    double val = parseExpression();

    if (_status != CalcStatus::OK) {
        result.status = _status;
        snprintf(result.formatted, sizeof(result.formatted), "%s", result.getErrorMessage());
        return result;
    }

    if (_current.type != TOK_END) {
        if (_current.type == TOK_RPAREN || _current.type == TOK_RBRACKET || _current.type == TOK_RBRACE) {
            result.status = CalcStatus::ERR_UNMATCHED_PAREN;
        } else {
            result.status = CalcStatus::ERR_SYNTAX;
        }
        snprintf(result.formatted, sizeof(result.formatted), "%s", result.getErrorMessage());
        return result;
    }

    // Check for overflow or special floating point states
    if (std::isinf(val) || std::isnan(val) || std::abs(val) > 1e308) {
        result.status = CalcStatus::ERR_OVERFLOW;
        snprintf(result.formatted, sizeof(result.formatted), "%s", result.getErrorMessage());
        return result;
    }

    result.status = CalcStatus::OK;
    result.value = val;
    formatValue(val, result.formatted, sizeof(result.formatted));
    return result;
}

double CalcEngine::Parser::parseExpression() {
    return parseAddSub();
}

double CalcEngine::Parser::parseAddSub() {
    double left = parseMulDiv();
    if (_status != CalcStatus::OK) return 0.0;

    while (_current.type == TOK_PLUS || _current.type == TOK_MINUS) {
        TokenType op = _current.type;
        advance();
        double right = parseMulDiv();
        if (_status != CalcStatus::OK) return 0.0;

        if (op == TOK_PLUS) {
            left += right;
        } else {
            left -= right;
        }

        if (std::isinf(left) || std::isnan(left)) {
            setError(CalcStatus::ERR_OVERFLOW);
            return 0.0;
        }
    }
    return left;
}

double CalcEngine::Parser::parseMulDiv() {
    double left = parseUnary();
    if (_status != CalcStatus::OK) return 0.0;

    while (_current.type == TOK_MUL || _current.type == TOK_DIV) {
        TokenType op = _current.type;
        advance();
        double right = parseUnary();
        if (_status != CalcStatus::OK) return 0.0;

        if (op == TOK_MUL) {
            left *= right;
        } else {
            if (std::abs(right) < 1e-15) {
                setError(CalcStatus::ERR_DIVIDE_BY_ZERO);
                return 0.0;
            }
            left /= right;
        }

        if (std::isinf(left) || std::isnan(left)) {
            setError(CalcStatus::ERR_OVERFLOW);
            return 0.0;
        }
    }
    return left;
}

double CalcEngine::Parser::parsePower() {
    double left = parsePostfix();
    if (_status != CalcStatus::OK) return 0.0;

    if (_current.type == TOK_POWER) {
        advance();
        // Exponent can have unary minus (e.g. 2^-2) or chained power (2^3^2)
        double right = parseUnary();
        if (_status != CalcStatus::OK) return 0.0;

        left = std::pow(left, right);
        if (std::isinf(left) || std::isnan(left)) {
            setError(CalcStatus::ERR_OVERFLOW);
            return 0.0;
        }
    }
    return left;
}

double CalcEngine::Parser::parseUnary() {
    if (_current.type == TOK_PLUS) {
        advance();
        return parseUnary();
    }
    if (_current.type == TOK_MINUS) {
        advance();
        double val = parseUnary();
        return -val;
    }
    if (_current.type == TOK_SQRT) {
        advance();
        double val = parseUnary();
        if (_status != CalcStatus::OK) return 0.0;
        if (val < 0.0) {
            setError(CalcStatus::ERR_INVALID_ROOT);
            return 0.0;
        }
        return std::sqrt(val);
    }
    return parsePower();
}

double CalcEngine::Parser::parsePostfix() {
    double val = parsePrimary();
    if (_status != CalcStatus::OK) return 0.0;

    while (_current.type == TOK_PERCENT) {
        advance();
        val = val / 100.0;
    }
    return val;
}

double CalcEngine::Parser::parsePrimary() {
    if (_status != CalcStatus::OK) return 0.0;

    if (_current.type == TOK_NUMBER) {
        double val = _current.numValue;
        advance();
        return val;
    }

    if (_current.type == TOK_LPAREN) {
        advance();
        double val = parseExpression();
        if (!expect(TOK_RPAREN, CalcStatus::ERR_UNMATCHED_PAREN)) {
            return 0.0;
        }
        return val;
    }

    if (_current.type == TOK_LBRACKET) {
        advance();
        double val = parseExpression();
        if (!expect(TOK_RBRACKET, CalcStatus::ERR_UNMATCHED_PAREN)) {
            return 0.0;
        }
        return val;
    }

    if (_current.type == TOK_LBRACE) {
        advance();
        double val = parseExpression();
        if (!expect(TOK_RBRACE, CalcStatus::ERR_UNMATCHED_PAREN)) {
            return 0.0;
        }
        return val;
    }

    if (_current.type == TOK_RPAREN || _current.type == TOK_RBRACKET || _current.type == TOK_RBRACE) {
        setError(CalcStatus::ERR_MISSING_OPERAND);
        return 0.0;
    }

    if (_current.type == TOK_END) {
        setError(CalcStatus::ERR_MISSING_OPERAND);
        return 0.0;
    }

    // Any operator encountered here means an operand is missing (e.g. "2 + * 3")
    setError(CalcStatus::ERR_MISSING_OPERAND);
    return 0.0;
}

} // namespace DigitalSlate
