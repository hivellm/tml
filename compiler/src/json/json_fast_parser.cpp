//! # Fast JSON Parser Implementation
//!
//! High-performance JSON parser using V8-inspired optimizations.

#include "json/json_fast_parser.hpp"

#include <cstring>
#include <charconv>

namespace tml::json::fast {

// ============================================================================
// Lookup Tables
// ============================================================================

/// Character classification lookup table
const uint8_t kCharFlags[256] = {
    // 0x00-0x0F (control characters)
    0, 0, 0, 0, 0, 0, 0, 0, 0,
    CHAR_WHITESPACE, // 0x09 = '\t'
    CHAR_WHITESPACE, // 0x0A = '\n'
    0, 0,
    CHAR_WHITESPACE, // 0x0D = '\r'
    0, 0,
    // 0x10-0x1F (control characters)
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    // 0x20-0x2F
    CHAR_WHITESPACE, // 0x20 = ' '
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    CHAR_STRUCTURAL, // 0x2C = ','
    CHAR_NUMBER_START, // 0x2D = '-'
    0, 0,
    // 0x30-0x3F (digits and more)
    CHAR_DIGIT | CHAR_HEX | CHAR_NUMBER_START, // '0'
    CHAR_DIGIT | CHAR_HEX | CHAR_NUMBER_START, // '1'
    CHAR_DIGIT | CHAR_HEX | CHAR_NUMBER_START, // '2'
    CHAR_DIGIT | CHAR_HEX | CHAR_NUMBER_START, // '3'
    CHAR_DIGIT | CHAR_HEX | CHAR_NUMBER_START, // '4'
    CHAR_DIGIT | CHAR_HEX | CHAR_NUMBER_START, // '5'
    CHAR_DIGIT | CHAR_HEX | CHAR_NUMBER_START, // '6'
    CHAR_DIGIT | CHAR_HEX | CHAR_NUMBER_START, // '7'
    CHAR_DIGIT | CHAR_HEX | CHAR_NUMBER_START, // '8'
    CHAR_DIGIT | CHAR_HEX | CHAR_NUMBER_START, // '9'
    CHAR_STRUCTURAL, // 0x3A = ':'
    0, 0, 0, 0, 0,
    // 0x40-0x4F
    0,
    CHAR_HEX, // 'A'
    CHAR_HEX, // 'B'
    CHAR_HEX, // 'C'
    CHAR_HEX, // 'D'
    CHAR_HEX, // 'E'
    CHAR_HEX, // 'F'
    0, 0, 0, 0, 0, 0, 0, 0, 0,
    // 0x50-0x5F
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    CHAR_STRUCTURAL, // 0x5B = '['
    0,
    CHAR_STRUCTURAL, // 0x5D = ']'
    0, 0,
    // 0x60-0x6F
    0,
    CHAR_HEX, // 'a'
    CHAR_HEX, // 'b'
    CHAR_HEX, // 'c'
    CHAR_HEX, // 'd'
    CHAR_HEX, // 'e'
    CHAR_KEYWORD | CHAR_HEX, // 'f' (false)
    0, 0, 0, 0, 0, 0, 0,
    CHAR_KEYWORD, // 'n' (null)
    0,
    // 0x70-0x7F
    0, 0, 0, 0,
    CHAR_KEYWORD, // 't' (true)
    0, 0, 0, 0, 0, 0,
    CHAR_STRUCTURAL, // 0x7B = '{'
    0,
    CHAR_STRUCTURAL, // 0x7D = '}'
    0, 0,
    // 0x80-0xFF (high bytes, used in UTF-8)
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
};

/// Hex digit value lookup (255 = invalid)
const uint8_t kHexValues[256] = {
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9, // '0'-'9'
    255, 255, 255, 255, 255, 255, 255,
    10, 11, 12, 13, 14, 15, // 'A'-'F'
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255,
    10, 11, 12, 13, 14, 15, // 'a'-'f'
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    // Rest is 255 (invalid)
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
};

// ============================================================================
// FastJsonParser Implementation
// ============================================================================

FastJsonParser::FastJsonParser(std::string_view input)
    : input_(input.data()), pos_(input.data()), end_(input.data() + input.size()) {
    string_buffer_.reserve(256); // Pre-allocate for typical strings
}

void FastJsonParser::skip_ws() {
    pos_ = skip_whitespace_simd(pos_, end_);
}

auto FastJsonParser::make_error(const char* msg) const -> JsonError {
    return JsonError::make(msg, line_, column_, static_cast<size_t>(pos_ - input_));
}

auto FastJsonParser::parse() -> Result<JsonValue, JsonError> {
    skip_ws();

    auto result = parse_value();
    if (is_err(result)) {
        return result;
    }

    skip_ws();

    if (pos_ < end_) {
        return make_error("Unexpected content after JSON value");
    }

    return result;
}

auto FastJsonParser::parse_value() -> Result<JsonValue, JsonError> {
    if (depth_ >= MAX_DEPTH) {
        return make_error("Maximum nesting depth exceeded");
    }

    skip_ws();

    if (pos_ >= end_) {
        return make_error("Unexpected end of input");
    }

    char c = *pos_;

    // Use lookup table for fast dispatch
    switch (c) {
    case '{':
        return parse_object();

    case '[':
        return parse_array();

    case '"': {
        auto str_result = parse_string();
        if (is_err(str_result)) {
            return unwrap_err(str_result);
        }
        return JsonValue(std::move(unwrap(str_result)));
    }

    case 't':
    case 'f':
    case 'n':
        return parse_keyword();

    case '-':
    case '0': case '1': case '2': case '3': case '4':
    case '5': case '6': case '7': case '8': case '9':
        return parse_number();

    default:
        return make_error("Unexpected character");
    }
}

auto FastJsonParser::parse_string() -> Result<std::string, JsonError> {
    ++pos_; // Skip opening quote
    ++column_;

    string_buffer_.clear();

    while (pos_ < end_) {
        // Use SIMD to find next special character
        const char* special = find_string_special_simd(pos_, end_);

        // Copy plain characters in bulk
        if (special > pos_) {
            string_buffer_.append(pos_, special - pos_);
            column_ += static_cast<size_t>(special - pos_);
            pos_ = special;
        }

        if (pos_ >= end_) {
            return make_error("Unterminated string");
        }

        char c = *pos_;

        if (c == '"') {
            ++pos_;
            ++column_;
            return std::move(string_buffer_);
        }

        if (c == '\\') {
            ++pos_;
            ++column_;

            if (pos_ >= end_) {
                return make_error("Unterminated escape sequence");
            }

            char escaped = *pos_;
            ++pos_;
            ++column_;

            switch (escaped) {
            case '"':  string_buffer_ += '"'; break;
            case '\\': string_buffer_ += '\\'; break;
            case '/':  string_buffer_ += '/'; break;
            case 'b':  string_buffer_ += '\b'; break;
            case 'f':  string_buffer_ += '\f'; break;
            case 'n':  string_buffer_ += '\n'; break;
            case 'r':  string_buffer_ += '\r'; break;
            case 't':  string_buffer_ += '\t'; break;
            case 'u': {
                // Use SWAR for fast hex parsing
                if (pos_ + 4 > end_) {
                    return make_error("Incomplete unicode escape");
                }

                uint32_t codepoint = parse_hex4_swar(pos_);
                if (codepoint == 0xFFFFFFFF) {
                    return make_error("Invalid unicode escape");
                }

                pos_ += 4;
                column_ += 4;

                // Convert to UTF-8
                if (codepoint < 0x80) {
                    string_buffer_ += static_cast<char>(codepoint);
                } else if (codepoint < 0x800) {
                    string_buffer_ += static_cast<char>(0xC0 | (codepoint >> 6));
                    string_buffer_ += static_cast<char>(0x80 | (codepoint & 0x3F));
                } else {
                    string_buffer_ += static_cast<char>(0xE0 | (codepoint >> 12));
                    string_buffer_ += static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F));
                    string_buffer_ += static_cast<char>(0x80 | (codepoint & 0x3F));
                }
                break;
            }
            default:
                return make_error("Invalid escape sequence");
            }
        } else if (static_cast<uint8_t>(c) < 0x20) {
            return make_error("Control character in string");
        } else {
            string_buffer_ += c;
            ++pos_;
            ++column_;
        }
    }

    return make_error("Unterminated string");
}

auto FastJsonParser::parse_number() -> Result<JsonValue, JsonError> {
    const char* start = pos_;
    bool is_float = false;
    bool negative = false;

    // Optional minus
    if (*pos_ == '-') {
        negative = true;
        ++pos_;
        ++column_;
    }

    // Integer part - SMI fast path for small integers
    if (pos_ >= end_ || !is_digit(*pos_)) {
        return make_error("Invalid number");
    }

    // Count digits for SMI fast path
    int64_t int_value = 0;
    bool use_smi = true;
    int digit_count = 0;

    if (*pos_ == '0') {
        ++pos_;
        ++column_;
        digit_count = 1;
    } else {
        // Parse integer digits with overflow detection
        while (pos_ < end_ && is_digit(*pos_)) {
            if (digit_count < 18) { // Safe for int64
                int_value = int_value * 10 + (*pos_ - '0');
            } else {
                use_smi = false;
            }
            ++pos_;
            ++column_;
            ++digit_count;
        }
    }

    // Fractional part
    if (pos_ < end_ && *pos_ == '.') {
        is_float = true;
        use_smi = false;
        ++pos_;
        ++column_;

        if (pos_ >= end_ || !is_digit(*pos_)) {
            return make_error("Expected digit after decimal point");
        }

        while (pos_ < end_ && is_digit(*pos_)) {
            ++pos_;
            ++column_;
        }
    }

    // Exponent part
    if (pos_ < end_ && (*pos_ == 'e' || *pos_ == 'E')) {
        is_float = true;
        use_smi = false;
        ++pos_;
        ++column_;

        if (pos_ < end_ && (*pos_ == '+' || *pos_ == '-')) {
            ++pos_;
            ++column_;
        }

        if (pos_ >= end_ || !is_digit(*pos_)) {
            return make_error("Expected digit in exponent");
        }

        while (pos_ < end_ && is_digit(*pos_)) {
            ++pos_;
            ++column_;
        }
    }

    // SMI fast path - avoid string parsing
    if (use_smi && !is_float && digit_count <= 18) {
        if (negative) {
            int_value = -int_value;
        }
        return JsonValue(JsonNumber(int_value));
    }

    // Slow path - parse as double
    std::string_view num_str(start, static_cast<size_t>(pos_ - start));

    if (is_float) {
        double value;
        auto [ptr, ec] = std::from_chars(num_str.data(), num_str.data() + num_str.size(), value);
        if (ec != std::errc{}) {
            // Fallback to strtod
            value = std::strtod(std::string(num_str).c_str(), nullptr);
        }
        return JsonValue(JsonNumber(value));
    } else {
        // Large integer - try int64 first, then uint64, then double
        if (negative) {
            int64_t value;
            auto [ptr, ec] = std::from_chars(num_str.data(), num_str.data() + num_str.size(), value);
            if (ec == std::errc{}) {
                return JsonValue(JsonNumber(value));
            }
        } else {
            uint64_t value;
            auto [ptr, ec] = std::from_chars(num_str.data(), num_str.data() + num_str.size(), value);
            if (ec == std::errc{}) {
                if (value <= static_cast<uint64_t>(INT64_MAX)) {
                    return JsonValue(JsonNumber(static_cast<int64_t>(value)));
                }
                return JsonValue(JsonNumber(value));
            }
        }

        // Fallback to double
        double value = std::strtod(std::string(num_str).c_str(), nullptr);
        return JsonValue(JsonNumber(value));
    }
}

auto FastJsonParser::parse_object() -> Result<JsonValue, JsonError> {
    ++depth_;
    ++pos_; // Skip '{'
    ++column_;

    JsonObject obj;
    // Note: std::map doesn't support reserve(), but unordered_map does

    skip_ws();

    if (pos_ < end_ && *pos_ == '}') {
        ++pos_;
        ++column_;
        --depth_;
        return JsonValue(std::move(obj));
    }

    while (true) {
        skip_ws();

        // Expect string key
        if (pos_ >= end_ || *pos_ != '"') {
            --depth_;
            return make_error("Expected string key in object");
        }

        auto key_result = parse_string();
        if (is_err(key_result)) {
            --depth_;
            return unwrap_err(key_result);
        }
        std::string key = std::move(unwrap(key_result));

        skip_ws();

        // Expect colon
        if (pos_ >= end_ || *pos_ != ':') {
            --depth_;
            return make_error("Expected ':' after object key");
        }
        ++pos_;
        ++column_;

        // Parse value
        auto value_result = parse_value();
        if (is_err(value_result)) {
            --depth_;
            return value_result;
        }

        obj.emplace(std::move(key), std::move(unwrap(value_result)));

        skip_ws();

        if (pos_ >= end_) {
            --depth_;
            return make_error("Unexpected end of object");
        }

        if (*pos_ == ',') {
            ++pos_;
            ++column_;
            skip_ws();
            if (pos_ < end_ && *pos_ == '}') {
                --depth_;
                return make_error("Trailing comma in object");
            }
        } else if (*pos_ == '}') {
            ++pos_;
            ++column_;
            --depth_;
            return JsonValue(std::move(obj));
        } else {
            --depth_;
            return make_error("Expected ',' or '}' in object");
        }
    }
}

auto FastJsonParser::parse_array() -> Result<JsonValue, JsonError> {
    ++depth_;
    ++pos_; // Skip '['
    ++column_;

    JsonArray arr;
    arr.reserve(8); // Pre-allocate for typical arrays

    skip_ws();

    if (pos_ < end_ && *pos_ == ']') {
        ++pos_;
        ++column_;
        --depth_;
        return JsonValue(std::move(arr));
    }

    while (true) {
        auto value_result = parse_value();
        if (is_err(value_result)) {
            --depth_;
            return value_result;
        }

        arr.push_back(std::move(unwrap(value_result)));

        skip_ws();

        if (pos_ >= end_) {
            --depth_;
            return make_error("Unexpected end of array");
        }

        if (*pos_ == ',') {
            ++pos_;
            ++column_;
            skip_ws();
            if (pos_ < end_ && *pos_ == ']') {
                --depth_;
                return make_error("Trailing comma in array");
            }
        } else if (*pos_ == ']') {
            ++pos_;
            ++column_;
            --depth_;
            return JsonValue(std::move(arr));
        } else {
            --depth_;
            return make_error("Expected ',' or ']' in array");
        }
    }
}

auto FastJsonParser::parse_keyword() -> Result<JsonValue, JsonError> {
    if (pos_ + 4 <= end_ && std::memcmp(pos_, "true", 4) == 0) {
        pos_ += 4;
        column_ += 4;
        return JsonValue(true);
    }

    if (pos_ + 5 <= end_ && std::memcmp(pos_, "false", 5) == 0) {
        pos_ += 5;
        column_ += 5;
        return JsonValue(false);
    }

    if (pos_ + 4 <= end_ && std::memcmp(pos_, "null", 4) == 0) {
        pos_ += 4;
        column_ += 4;
        return JsonValue();
    }

    return make_error("Unknown keyword");
}

// ============================================================================
// Entry Point
// ============================================================================

auto parse_json_fast(std::string_view input) -> Result<JsonValue, JsonError> {
    FastJsonParser parser(input);
    return parser.parse();
}

} // namespace tml::json::fast
