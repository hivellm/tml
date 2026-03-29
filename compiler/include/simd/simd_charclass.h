#pragma once

//! # SIMD Character Classification Tables
//!
//! 256-byte lookup tables for fast single-byte character classification.
//! Each table maps ASCII byte -> bool (0 or 1). Used by SIMD lexer
//! acceleration (Phase 5) and string operations (Phase 3).
//!
//! The tables are constexpr arrays -- they live in .rodata and are
//! typically pulled into L1 cache on first access.

#include "simd_utils.h"

#include <array>
#include <cstdint>

namespace tml::simd {

// ============================================================================
// Character predicate helpers (constexpr, no lambdas)
// ============================================================================

namespace detail {

constexpr auto is_ws(uint8_t c) -> bool {
    return c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\v' || c == '\f';
}

constexpr auto is_alpha(uint8_t c) -> bool {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

constexpr auto is_digit(uint8_t c) -> bool {
    return c >= '0' && c <= '9';
}

constexpr auto is_hex(uint8_t c) -> bool {
    return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

constexpr auto is_ident(uint8_t c) -> bool {
    return is_alpha(c) || is_digit(c) || c == '_';
}

constexpr auto is_ident_start(uint8_t c) -> bool {
    return is_alpha(c) || c == '_';
}

// Table generators -- explicit functions, no template + lambda
constexpr auto make_whitespace_table() -> std::array<uint8_t, 256> {
    std::array<uint8_t, 256> t{};
    for (int i = 0; i < 256; ++i)
        t[i] = is_ws(static_cast<uint8_t>(i)) ? 1 : 0;
    return t;
}

constexpr auto make_alpha_table() -> std::array<uint8_t, 256> {
    std::array<uint8_t, 256> t{};
    for (int i = 0; i < 256; ++i)
        t[i] = is_alpha(static_cast<uint8_t>(i)) ? 1 : 0;
    return t;
}

constexpr auto make_digit_table() -> std::array<uint8_t, 256> {
    std::array<uint8_t, 256> t{};
    for (int i = 0; i < 256; ++i)
        t[i] = is_digit(static_cast<uint8_t>(i)) ? 1 : 0;
    return t;
}

constexpr auto make_hex_table() -> std::array<uint8_t, 256> {
    std::array<uint8_t, 256> t{};
    for (int i = 0; i < 256; ++i)
        t[i] = is_hex(static_cast<uint8_t>(i)) ? 1 : 0;
    return t;
}

constexpr auto make_ident_table() -> std::array<uint8_t, 256> {
    std::array<uint8_t, 256> t{};
    for (int i = 0; i < 256; ++i)
        t[i] = is_ident(static_cast<uint8_t>(i)) ? 1 : 0;
    return t;
}

constexpr auto make_ident_start_table() -> std::array<uint8_t, 256> {
    std::array<uint8_t, 256> t{};
    for (int i = 0; i < 256; ++i)
        t[i] = is_ident_start(static_cast<uint8_t>(i)) ? 1 : 0;
    return t;
}

} // namespace detail

// ============================================================================
// Lookup tables -- constexpr, initialized at compile time
// ============================================================================

inline constexpr auto WHITESPACE_TABLE = detail::make_whitespace_table();
inline constexpr auto ALPHA_TABLE = detail::make_alpha_table();
inline constexpr auto DIGIT_TABLE = detail::make_digit_table();
inline constexpr auto HEX_TABLE = detail::make_hex_table();
inline constexpr auto IDENT_TABLE = detail::make_ident_table();
inline constexpr auto IDENT_START_TABLE = detail::make_ident_start_table();

// ============================================================================
// Query functions
// ============================================================================

SIMD_INLINE auto is_whitespace_simd(uint8_t c) -> bool {
    return WHITESPACE_TABLE[c] != 0;
}
SIMD_INLINE auto is_alpha_simd(uint8_t c) -> bool {
    return ALPHA_TABLE[c] != 0;
}
SIMD_INLINE auto is_digit_simd(uint8_t c) -> bool {
    return DIGIT_TABLE[c] != 0;
}
SIMD_INLINE auto is_hex_simd(uint8_t c) -> bool {
    return HEX_TABLE[c] != 0;
}
SIMD_INLINE auto is_ident_simd(uint8_t c) -> bool {
    return IDENT_TABLE[c] != 0;
}
SIMD_INLINE auto is_ident_start_simd(uint8_t c) -> bool {
    return IDENT_START_TABLE[c] != 0;
}

// ============================================================================
// Bitmask table -- packs 8 character classes into one byte per input byte.
// Useful for SIMD PSHUFB-based classification (one table covers all classes).
// ============================================================================

enum CharClass : uint8_t {
    CHAR_WHITESPACE = 1u << 0,  // space, tab, CR, LF, VT, FF
    CHAR_ALPHA = 1u << 1,       // a-z, A-Z
    CHAR_DIGIT = 1u << 2,       // 0-9
    CHAR_HEX = 1u << 3,         // 0-9, a-f, A-F
    CHAR_IDENT = 1u << 4,       // a-z, A-Z, 0-9, _
    CHAR_IDENT_START = 1u << 5, // a-z, A-Z, _
    CHAR_UPPER = 1u << 6,       // A-Z
    CHAR_LOWER = 1u << 7,       // a-z
};

namespace detail {

constexpr auto make_bitmask_table() -> std::array<uint8_t, 256> {
    std::array<uint8_t, 256> t{};
    for (int i = 0; i < 256; ++i) {
        auto c = static_cast<uint8_t>(i);
        uint8_t mask = 0;
        if (is_ws(c))
            mask |= CHAR_WHITESPACE;
        if (is_alpha(c))
            mask |= CHAR_ALPHA;
        if (is_digit(c))
            mask |= CHAR_DIGIT;
        if (is_hex(c))
            mask |= CHAR_HEX;
        if (is_ident(c))
            mask |= CHAR_IDENT;
        if (is_ident_start(c))
            mask |= CHAR_IDENT_START;
        if (c >= 'A' && c <= 'Z')
            mask |= CHAR_UPPER;
        if (c >= 'a' && c <= 'z')
            mask |= CHAR_LOWER;
        t[i] = mask;
    }
    return t;
}

} // namespace detail

inline constexpr auto BITMASK_TABLE = detail::make_bitmask_table();

// Query the bitmask table for any combination of character classes
SIMD_INLINE auto has_charclass(uint8_t c, uint8_t class_mask) -> bool {
    return (BITMASK_TABLE[c] & class_mask) != 0;
}

} // namespace tml::simd
