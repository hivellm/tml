//! # Fast JSON Parser
//!
//! High-performance JSON parser using V8-inspired optimizations:
//! - O(1) lookup tables for character classification
//! - SIMD whitespace skipping (SSE2/AVX2)
//! - SWAR (SIMD Within A Register) for unicode escapes
//! - Single-pass parsing (no separate lexer)
//! - Pre-allocated buffers

#pragma once

#include "common.hpp"
#include "json/json_error.hpp"
#include "json/json_value.hpp"

#include <cstdint>
#include <string_view>

#if defined(__SSE2__) || defined(_M_X64) || defined(_M_AMD64)
#define JSON_FAST_SSE2 1
#include <emmintrin.h>
#endif

#if defined(__AVX2__)
#define JSON_FAST_AVX2 1
#include <immintrin.h>
#endif

namespace tml::json::fast {

// ============================================================================
// Character Classification Lookup Tables (V8 optimization)
// ============================================================================

/// Flags for fast character classification
enum CharFlags : uint8_t {
    CHAR_NONE = 0,
    CHAR_WHITESPACE = 1 << 0,    // ' ', '\t', '\n', '\r'
    CHAR_DIGIT = 1 << 1,         // '0'-'9'
    CHAR_HEX = 1 << 2,           // '0'-'9', 'a'-'f', 'A'-'F'
    CHAR_STRUCTURAL = 1 << 3,    // '{', '}', '[', ']', ':', ','
    CHAR_STRING_ESCAPE = 1 << 4, // Characters requiring escape in strings
    CHAR_NUMBER_START = 1 << 5,  // '-', '0'-'9'
    CHAR_KEYWORD = 1 << 6,       // 't', 'f', 'n' (true, false, null)
};

/// Global lookup table for character classification (256 entries)
extern const uint8_t kCharFlags[256];

/// Lookup table for hex digit values (256 entries, 255 = invalid)
extern const uint8_t kHexValues[256];

/// Fast character classification
inline bool is_whitespace(char c) {
    return (kCharFlags[static_cast<uint8_t>(c)] & CHAR_WHITESPACE) != 0;
}

inline bool is_digit(char c) {
    return (kCharFlags[static_cast<uint8_t>(c)] & CHAR_DIGIT) != 0;
}

inline bool is_hex(char c) {
    return (kCharFlags[static_cast<uint8_t>(c)] & CHAR_HEX) != 0;
}

inline bool is_structural(char c) {
    return (kCharFlags[static_cast<uint8_t>(c)] & CHAR_STRUCTURAL) != 0;
}

inline bool is_number_start(char c) {
    return (kCharFlags[static_cast<uint8_t>(c)] & CHAR_NUMBER_START) != 0;
}

inline uint8_t hex_value(char c) {
    return kHexValues[static_cast<uint8_t>(c)];
}

// ============================================================================
// SWAR (SIMD Within A Register) Utilities
// ============================================================================

/// Parse 4 hex digits using SWAR (V8 technique)
/// Returns 0xFFFFFFFF on error
inline uint32_t parse_hex4_swar(const char* p) {
    // Load 4 bytes into a 32-bit value
    uint32_t block;
    std::memcpy(&block, p, 4);

    // Check all characters are valid hex
    uint8_t v0 = kHexValues[p[0]];
    uint8_t v1 = kHexValues[p[1]];
    uint8_t v2 = kHexValues[p[2]];
    uint8_t v3 = kHexValues[p[3]];

    if ((v0 | v1 | v2 | v3) == 0xFF) {
        return 0xFFFFFFFF; // Invalid hex
    }

    // Combine: (v0 << 12) | (v1 << 8) | (v2 << 4) | v3
    return (static_cast<uint32_t>(v0) << 12) | (static_cast<uint32_t>(v1) << 8) |
           (static_cast<uint32_t>(v2) << 4) | static_cast<uint32_t>(v3);
}

// ============================================================================
// SIMD Whitespace Skipping
// ============================================================================

#ifdef JSON_FAST_SSE2
/// Skip whitespace using SSE2 (16 bytes at a time)
inline const char* skip_whitespace_simd(const char* p, const char* end) {
    // Handle small inputs without SIMD
    while (p < end && (end - p < 16)) {
        if (!is_whitespace(*p)) return p;
        ++p;
    }

    if (p >= end) return p;

    // Create masks for whitespace characters
    __m128i space = _mm_set1_epi8(' ');
    __m128i tab = _mm_set1_epi8('\t');
    __m128i newline = _mm_set1_epi8('\n');
    __m128i carriage = _mm_set1_epi8('\r');

    while (p + 16 <= end) {
        __m128i chunk = _mm_loadu_si128(reinterpret_cast<const __m128i*>(p));

        // Check for each whitespace character
        __m128i is_space = _mm_cmpeq_epi8(chunk, space);
        __m128i is_tab = _mm_cmpeq_epi8(chunk, tab);
        __m128i is_newline = _mm_cmpeq_epi8(chunk, newline);
        __m128i is_carriage = _mm_cmpeq_epi8(chunk, carriage);

        // Combine: is any of them?
        __m128i is_ws = _mm_or_si128(_mm_or_si128(is_space, is_tab),
                                      _mm_or_si128(is_newline, is_carriage));

        // Get mask of non-whitespace positions
        int mask = _mm_movemask_epi8(is_ws);

        if (mask != 0xFFFF) {
            // Found non-whitespace - find first non-ws byte
            // ~mask gives us positions that are NOT whitespace
            int first_non_ws = 0;
            while (first_non_ws < 16 && (mask & (1 << first_non_ws))) {
                ++first_non_ws;
            }
            return p + first_non_ws;
        }

        p += 16;
    }

    // Handle remaining bytes
    while (p < end && is_whitespace(*p)) {
        ++p;
    }

    return p;
}
#else
inline const char* skip_whitespace_simd(const char* p, const char* end) {
    while (p < end && is_whitespace(*p)) {
        ++p;
    }
    return p;
}
#endif

// ============================================================================
// SIMD String Scanning
// ============================================================================

#ifdef JSON_FAST_SSE2
/// Find quote or backslash using SSE2
inline const char* find_string_special_simd(const char* p, const char* end) {
    // Handle small inputs
    while (p < end && (end - p < 16)) {
        char c = *p;
        if (c == '"' || c == '\\' || static_cast<uint8_t>(c) < 0x20) {
            return p;
        }
        ++p;
    }

    if (p >= end) return end;

    __m128i quote = _mm_set1_epi8('"');
    __m128i backslash = _mm_set1_epi8('\\');
    __m128i control_max = _mm_set1_epi8(0x1F);

    while (p + 16 <= end) {
        __m128i chunk = _mm_loadu_si128(reinterpret_cast<const __m128i*>(p));

        // Check for quote
        __m128i is_quote = _mm_cmpeq_epi8(chunk, quote);
        // Check for backslash
        __m128i is_backslash = _mm_cmpeq_epi8(chunk, backslash);
        // Check for control characters (< 0x20)
        // Use saturating subtraction: if chunk[i] <= 0x1F, chunk[i] - 0x1F saturates to 0
        __m128i is_control = _mm_cmpeq_epi8(_mm_min_epu8(chunk, control_max), chunk);

        // Combine
        __m128i is_special = _mm_or_si128(_mm_or_si128(is_quote, is_backslash), is_control);
        int mask = _mm_movemask_epi8(is_special);

        if (mask != 0) {
            // Find first special character
            int pos = 0;
            while (!(mask & (1 << pos))) ++pos;
            return p + pos;
        }

        p += 16;
    }

    // Handle remaining bytes
    while (p < end) {
        char c = *p;
        if (c == '"' || c == '\\' || static_cast<uint8_t>(c) < 0x20) {
            return p;
        }
        ++p;
    }

    return end;
}
#else
inline const char* find_string_special_simd(const char* p, const char* end) {
    while (p < end) {
        char c = *p;
        if (c == '"' || c == '\\' || static_cast<uint8_t>(c) < 0x20) {
            return p;
        }
        ++p;
    }
    return end;
}
#endif

// ============================================================================
// Fast JSON Parser
// ============================================================================

/// High-performance JSON parser
class FastJsonParser {
public:
    explicit FastJsonParser(std::string_view input);

    /// Parse JSON and return result
    [[nodiscard]] auto parse() -> Result<JsonValue, JsonError>;

private:
    const char* input_;
    const char* pos_;
    const char* end_;
    size_t line_ = 1;
    size_t column_ = 1;
    static constexpr size_t MAX_DEPTH = 1000;
    size_t depth_ = 0;

    // Pre-allocated string buffer for reuse
    std::string string_buffer_;

    /// Skip whitespace (uses SIMD when available)
    void skip_ws();

    /// Peek current character
    [[nodiscard]] char peek() const {
        return pos_ < end_ ? *pos_ : '\0';
    }

    /// Advance position
    void advance() {
        if (pos_ < end_) {
            if (*pos_ == '\n') {
                ++line_;
                column_ = 1;
            } else {
                ++column_;
            }
            ++pos_;
        }
    }

    /// Advance by n bytes (for SIMD fast paths)
    void advance_by(size_t n) {
        while (n > 0 && pos_ < end_) {
            if (*pos_ == '\n') {
                ++line_;
                column_ = 1;
            } else {
                ++column_;
            }
            ++pos_;
            --n;
        }
    }

    /// Create error at current position
    [[nodiscard]] auto make_error(const char* msg) const -> JsonError;

    /// Parse any value
    auto parse_value() -> Result<JsonValue, JsonError>;

    /// Parse string (fast path)
    auto parse_string() -> Result<std::string, JsonError>;

    /// Parse number (with SMI fast path)
    auto parse_number() -> Result<JsonValue, JsonError>;

    /// Parse object
    auto parse_object() -> Result<JsonValue, JsonError>;

    /// Parse array
    auto parse_array() -> Result<JsonValue, JsonError>;

    /// Parse keyword (true/false/null)
    auto parse_keyword() -> Result<JsonValue, JsonError>;
};

/// Fast JSON parsing entry point
[[nodiscard]] auto parse_json_fast(std::string_view input) -> Result<JsonValue, JsonError>;

} // namespace tml::json::fast
