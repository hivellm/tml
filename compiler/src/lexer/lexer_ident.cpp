TML_MODULE("compiler")

//! # Lexer - Identifiers
//!
//! This file implements identifier and keyword lexing.
//!
//! ## Identifier Rules
//!
//! - Start with letter (a-z, A-Z) or underscore
//! - Continue with letters, digits, or underscores
//! - Unicode letters supported for internationalization
//!
//! ## Keyword Lookup
//!
//! After lexing an identifier, it's checked against the keyword table.
//! If found, the token kind is set to the keyword. Boolean literals
//! (`true`, `false`) are special-cased to set their value.
//!
//! ## Compile-Time Constants
//!
//! Special identifiers are expanded at lex time:
//! - `__FILE__`    → StringLiteral with the source file path
//! - `__DIRNAME__` → StringLiteral with the source file's directory
//! - `__LINE__`    → IntLiteral with the current line number

#include "lexer/lexer.hpp"
#include "simd/simd_charclass.h"

#include <algorithm>
#include <string>
#include <unordered_map>

#if TML_SSE2
#include <emmintrin.h>
#ifdef _MSC_VER
#include <intrin.h>
#else
#include <x86intrin.h>
#endif
#endif

namespace tml::lexer {

// Get keywords from core module
extern const std::unordered_map<std::string_view, TokenKind>& get_keywords();

auto Lexer::lex_identifier() -> Token {
    const char* src = source_.content().data();
    size_t len = source_.length();

#if TML_SSE2
    // SSE2 fast path: scan 16 bytes at a time for non-identifier characters.
    // Only works for ASCII identifiers (high bit clear). If we hit a byte >= 0x80,
    // fall back to scalar for Unicode identifier support.
    //
    // Identifier continue chars: [a-z], [A-Z], [0-9], '_'
    // We use the precomputed IDENT_TABLE for classification.
    while (pos_ + 16 <= len) {
        __m128i chunk = _mm_loadu_si128(reinterpret_cast<const __m128i*>(src + pos_));

        // Check for non-ASCII bytes (high bit set) — need scalar for UTF-8
        int high_mask = _mm_movemask_epi8(chunk);
        if (high_mask != 0) {
            break; // Fall through to scalar loop for Unicode handling
        }

        // For each byte, check if it's an identifier character using range checks.
        // Identifier chars: [a-z] || [A-Z] || [0-9] || '_'
        //
        // Range check using SSE2: (x - lo) <=u (hi - lo) via saturating subtract
        // For unsigned comparison: if (x - lo) <=u (hi - lo), then x is in [lo, hi]
        // SSE2 has signed comparison, so we bias: subtract 0x80 from both sides.

        // Check [a-z]: bytes in range [0x61, 0x7A]
        __m128i bias = _mm_set1_epi8(static_cast<char>(0x80));
        __m128i biased = _mm_xor_si128(chunk, bias); // convert to signed comparison
        __m128i lo_az = _mm_set1_epi8(static_cast<char>(0x61 ^ 0x80));
        __m128i hi_az = _mm_set1_epi8(static_cast<char>(0x7A ^ 0x80));
        __m128i ge_a = _mm_cmpgt_epi8(biased, _mm_sub_epi8(lo_az, _mm_set1_epi8(1)));
        __m128i le_z = _mm_cmpgt_epi8(_mm_add_epi8(hi_az, _mm_set1_epi8(1)), biased);
        __m128i is_lower = _mm_and_si128(ge_a, le_z);

        // Check [A-Z]: bytes in range [0x41, 0x5A]
        __m128i lo_AZ = _mm_set1_epi8(static_cast<char>(0x41 ^ 0x80));
        __m128i hi_AZ = _mm_set1_epi8(static_cast<char>(0x5A ^ 0x80));
        __m128i ge_A = _mm_cmpgt_epi8(biased, _mm_sub_epi8(lo_AZ, _mm_set1_epi8(1)));
        __m128i le_Z = _mm_cmpgt_epi8(_mm_add_epi8(hi_AZ, _mm_set1_epi8(1)), biased);
        __m128i is_upper = _mm_and_si128(ge_A, le_Z);

        // Check [0-9]: bytes in range [0x30, 0x39]
        __m128i lo_09 = _mm_set1_epi8(static_cast<char>(0x30 ^ 0x80));
        __m128i hi_09 = _mm_set1_epi8(static_cast<char>(0x39 ^ 0x80));
        __m128i ge_0 = _mm_cmpgt_epi8(biased, _mm_sub_epi8(lo_09, _mm_set1_epi8(1)));
        __m128i le_9 = _mm_cmpgt_epi8(_mm_add_epi8(hi_09, _mm_set1_epi8(1)), biased);
        __m128i is_digit = _mm_and_si128(ge_0, le_9);

        // Check '_' (0x5F)
        __m128i is_underscore = _mm_cmpeq_epi8(chunk, _mm_set1_epi8('_'));

        // Combine: ident = lower | upper | digit | underscore
        __m128i is_ident =
            _mm_or_si128(_mm_or_si128(is_lower, is_upper), _mm_or_si128(is_digit, is_underscore));

        int ident_mask = _mm_movemask_epi8(is_ident);

        if (ident_mask == 0xFFFF) {
            // All 16 bytes are identifier characters
            pos_ += 16;
            continue;
        }

        // Find first non-identifier byte
#ifdef _MSC_VER
        unsigned long idx;
        _BitScanForward(&idx, static_cast<unsigned long>(~ident_mask & 0xFFFF));
        pos_ += idx;
#else
        pos_ += __builtin_ctz(~ident_mask & 0xFFFF);
#endif
        goto ident_done;
    }
#endif // TML_SSE2

    // Scalar path: handles remaining bytes and Unicode identifiers
    while (pos_ < len && is_identifier_continue(static_cast<char32_t>(src[pos_]))) {
        ++pos_;
    }

#if TML_SSE2
ident_done:
#endif

    auto lexeme = source_.slice(token_start_, pos_);

    // Check if it's a keyword
    const auto& keywords = get_keywords();
    auto it = keywords.find(lexeme);
    if (it != keywords.end()) {
        auto token = make_token(it->second);
        // Handle boolean literals
        if (it->second == TokenKind::BoolLiteral) {
            token.value = (lexeme == "true");
        }
        return token;
    }

    // Compile-time constants: __FILE__, __DIRNAME__, __LINE__
    if (lexeme == "__FILE__") {
        auto token = make_token(TokenKind::StringLiteral);
        std::string filepath(source_.filename());
        std::replace(filepath.begin(), filepath.end(), '\\', '/');
        token.value = StringValue{.value = std::move(filepath), .is_raw = false};
        return token;
    }

    if (lexeme == "__DIRNAME__") {
        auto token = make_token(TokenKind::StringLiteral);
        std::string filepath(source_.filename());
        std::replace(filepath.begin(), filepath.end(), '\\', '/');
        auto slash_pos = filepath.rfind('/');
        std::string dir = (slash_pos != std::string::npos) ? filepath.substr(0, slash_pos) : ".";
        token.value = StringValue{.value = std::move(dir), .is_raw = false};
        return token;
    }

    if (lexeme == "__LINE__") {
        auto token = make_token(TokenKind::IntLiteral);
        auto loc = source_.location(token_start_);
        token.value = IntValue{.value = static_cast<uint64_t>(loc.line), .base = 10, .suffix = ""};
        return token;
    }

    return make_token(TokenKind::Identifier);
}

} // namespace tml::lexer
