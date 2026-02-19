//! # Builtin String Functions
//!
//! This file registers string manipulation functions.
//!
//! ## String Operations
//!
//! | Function        | Signature                       | Description          |
//! |-----------------|---------------------------------|----------------------|
//! | `str_len`       | `(Str) -> I32`                  | String length        |
//! | `str_eq`        | `(Str, Str) -> Bool`            | String equality      |
//! | `str_hash`      | `(Str) -> I32`                  | Hash value           |
//! | `str_concat`    | `(Str, Str) -> Str`             | Concatenation        |
//! | `str_substring` | `(Str, I32, I32) -> Str`        | Extract substring    |
//! | `str_char_at`   | `(Str, I32) -> Char`            | Character at index   |
//! | `str_contains`  | `(Str, Str) -> Bool`            | Substring check      |
//! | `str_from_char` | `(Char) -> Str`                 | Char to string       |
//! | `str_to_i32`    | `(Str) -> I32`                  | Parse integer        |
//! | `i32_to_str`    | `(I32) -> Str`                  | Format integer       |
//!
//! These are low-level intrinsics used by higher-level Str methods.

#include "types/env.hpp"

namespace tml::types {

void TypeEnv::init_builtin_string() {
    SourceSpan builtin_span{};

    // str_len(s: Str) -> I32
    functions_["str_len"].push_back(FuncSig{"str_len",
                                            {make_primitive(PrimitiveKind::Str)},
                                            make_primitive(PrimitiveKind::I32),
                                            {},
                                            false,
                                            builtin_span});

    // str_eq(a: Str, b: Str) -> Bool
    functions_["str_eq"].push_back(
        FuncSig{"str_eq",
                {make_primitive(PrimitiveKind::Str), make_primitive(PrimitiveKind::Str)},
                make_primitive(PrimitiveKind::Bool),
                {},
                false,
                builtin_span});

    // str_hash(s: Str) -> I32
    functions_["str_hash"].push_back(FuncSig{"str_hash",
                                             {make_primitive(PrimitiveKind::Str)},
                                             make_primitive(PrimitiveKind::I32),
                                             {},
                                             false,
                                             builtin_span});

    // str_concat(a: Str, b: Str) -> Str
    functions_["str_concat"].push_back(
        FuncSig{"str_concat",
                {make_primitive(PrimitiveKind::Str), make_primitive(PrimitiveKind::Str)},
                make_primitive(PrimitiveKind::Str),
                {},
                false,
                builtin_span});

    // str_substring(s: Str, start: I32, len: I32) -> Str
    functions_["str_substring"].push_back(
        FuncSig{"str_substring",
                {make_primitive(PrimitiveKind::Str), make_primitive(PrimitiveKind::I32),
                 make_primitive(PrimitiveKind::I32)},
                make_primitive(PrimitiveKind::Str),
                {},
                false,
                builtin_span});

    // str_contains(haystack: Str, needle: Str) -> Bool
    functions_["str_contains"].push_back(
        FuncSig{"str_contains",
                {make_primitive(PrimitiveKind::Str), make_primitive(PrimitiveKind::Str)},
                make_primitive(PrimitiveKind::Bool),
                {},
                false,
                builtin_span});

    // str_starts_with(s: Str, prefix: Str) -> Bool
    functions_["str_starts_with"].push_back(
        FuncSig{"str_starts_with",
                {make_primitive(PrimitiveKind::Str), make_primitive(PrimitiveKind::Str)},
                make_primitive(PrimitiveKind::Bool),
                {},
                false,
                builtin_span});

    // str_ends_with(s: Str, suffix: Str) -> Bool
    functions_["str_ends_with"].push_back(
        FuncSig{"str_ends_with",
                {make_primitive(PrimitiveKind::Str), make_primitive(PrimitiveKind::Str)},
                make_primitive(PrimitiveKind::Bool),
                {},
                false,
                builtin_span});

    // str_to_upper(s: Str) -> Str
    functions_["str_to_upper"].push_back(FuncSig{"str_to_upper",
                                                 {make_primitive(PrimitiveKind::Str)},
                                                 make_primitive(PrimitiveKind::Str),
                                                 {},
                                                 false,
                                                 builtin_span});

    // str_to_lower(s: Str) -> Str
    functions_["str_to_lower"].push_back(FuncSig{"str_to_lower",
                                                 {make_primitive(PrimitiveKind::Str)},
                                                 make_primitive(PrimitiveKind::Str),
                                                 {},
                                                 false,
                                                 builtin_span});

    // str_trim(s: Str) -> Str
    functions_["str_trim"].push_back(FuncSig{"str_trim",
                                             {make_primitive(PrimitiveKind::Str)},
                                             make_primitive(PrimitiveKind::Str),
                                             {},
                                             false,
                                             builtin_span});

    // str_char_at(s: Str, index: I32) -> Char
    functions_["str_char_at"].push_back(
        FuncSig{"str_char_at",
                {make_primitive(PrimitiveKind::Str), make_primitive(PrimitiveKind::I32)},
                make_primitive(PrimitiveKind::Char),
                {},
                false,
                builtin_span});

    // ========================================================================
    // Char Operations
    // ========================================================================

    // char_is_alphabetic(c: Char) -> Bool
    functions_["char_is_alphabetic"].push_back(FuncSig{"char_is_alphabetic",
                                                       {make_primitive(PrimitiveKind::Char)},
                                                       make_primitive(PrimitiveKind::Bool),
                                                       {},
                                                       false,
                                                       builtin_span});

    // char_is_numeric(c: Char) -> Bool
    functions_["char_is_numeric"].push_back(FuncSig{"char_is_numeric",
                                                    {make_primitive(PrimitiveKind::Char)},
                                                    make_primitive(PrimitiveKind::Bool),
                                                    {},
                                                    false,
                                                    builtin_span});

    // char_is_alphanumeric(c: Char) -> Bool
    functions_["char_is_alphanumeric"].push_back(FuncSig{"char_is_alphanumeric",
                                                         {make_primitive(PrimitiveKind::Char)},
                                                         make_primitive(PrimitiveKind::Bool),
                                                         {},
                                                         false,
                                                         builtin_span});

    // char_is_whitespace(c: Char) -> Bool
    functions_["char_is_whitespace"].push_back(FuncSig{"char_is_whitespace",
                                                       {make_primitive(PrimitiveKind::Char)},
                                                       make_primitive(PrimitiveKind::Bool),
                                                       {},
                                                       false,
                                                       builtin_span});

    // char_is_uppercase(c: Char) -> Bool
    functions_["char_is_uppercase"].push_back(FuncSig{"char_is_uppercase",
                                                      {make_primitive(PrimitiveKind::Char)},
                                                      make_primitive(PrimitiveKind::Bool),
                                                      {},
                                                      false,
                                                      builtin_span});

    // char_is_lowercase(c: Char) -> Bool
    functions_["char_is_lowercase"].push_back(FuncSig{"char_is_lowercase",
                                                      {make_primitive(PrimitiveKind::Char)},
                                                      make_primitive(PrimitiveKind::Bool),
                                                      {},
                                                      false,
                                                      builtin_span});

    // char_is_ascii(c: Char) -> Bool
    functions_["char_is_ascii"].push_back(FuncSig{"char_is_ascii",
                                                  {make_primitive(PrimitiveKind::Char)},
                                                  make_primitive(PrimitiveKind::Bool),
                                                  {},
                                                  false,
                                                  builtin_span});

    // char_is_control(c: Char) -> Bool
    functions_["char_is_control"].push_back(FuncSig{"char_is_control",
                                                    {make_primitive(PrimitiveKind::Char)},
                                                    make_primitive(PrimitiveKind::Bool),
                                                    {},
                                                    false,
                                                    builtin_span});

    // char_to_uppercase(c: Char) -> Char
    functions_["char_to_uppercase"].push_back(FuncSig{"char_to_uppercase",
                                                      {make_primitive(PrimitiveKind::Char)},
                                                      make_primitive(PrimitiveKind::Char),
                                                      {},
                                                      false,
                                                      builtin_span});

    // char_to_lowercase(c: Char) -> Char
    functions_["char_to_lowercase"].push_back(FuncSig{"char_to_lowercase",
                                                      {make_primitive(PrimitiveKind::Char)},
                                                      make_primitive(PrimitiveKind::Char),
                                                      {},
                                                      false,
                                                      builtin_span});

    // char_to_digit(c: Char, radix: I32) -> I32
    functions_["char_to_digit"].push_back(
        FuncSig{"char_to_digit",
                {make_primitive(PrimitiveKind::Char), make_primitive(PrimitiveKind::I32)},
                make_primitive(PrimitiveKind::I32),
                {},
                false,
                builtin_span});

    // char_from_digit(digit: I32, radix: I32) -> Char
    functions_["char_from_digit"].push_back(
        FuncSig{"char_from_digit",
                {make_primitive(PrimitiveKind::I32), make_primitive(PrimitiveKind::I32)},
                make_primitive(PrimitiveKind::Char),
                {},
                false,
                builtin_span});

    // char_code(c: Char) -> I32
    functions_["char_code"].push_back(FuncSig{"char_code",
                                              {make_primitive(PrimitiveKind::Char)},
                                              make_primitive(PrimitiveKind::I32),
                                              {},
                                              false,
                                              builtin_span});

    // char_from_code(code: I32) -> Char
    functions_["char_from_code"].push_back(FuncSig{"char_from_code",
                                                   {make_primitive(PrimitiveKind::I32)},
                                                   make_primitive(PrimitiveKind::Char),
                                                   {},
                                                   false,
                                                   builtin_span});

    // char_to_string, utf8_*byte_to_string — REMOVED (Phase 18.2)
    // Migrated to pure TML using mem_alloc + ptr_write in char/methods.tml
}

} // namespace tml::types
