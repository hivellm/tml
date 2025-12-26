// Builtin string functions
#include "tml/types/env.hpp"

namespace tml::types {

void TypeEnv::init_builtin_string() {
    SourceSpan builtin_span{};

    // str_len(s: Str) -> I32
    functions_["str_len"].push_back(FuncSig{
        "str_len",
        {make_primitive(PrimitiveKind::Str)},
        make_primitive(PrimitiveKind::I32),
        {},
        false,
        builtin_span
    });

    // str_eq(a: Str, b: Str) -> Bool
    functions_["str_eq"].push_back(FuncSig{
        "str_eq",
        {make_primitive(PrimitiveKind::Str), make_primitive(PrimitiveKind::Str)},
        make_primitive(PrimitiveKind::Bool),
        {},
        false,
        builtin_span
    });

    // str_hash(s: Str) -> I32
    functions_["str_hash"].push_back(FuncSig{
        "str_hash",
        {make_primitive(PrimitiveKind::Str)},
        make_primitive(PrimitiveKind::I32),
        {},
        false,
        builtin_span
    });

    // str_concat(a: Str, b: Str) -> Str
    functions_["str_concat"].push_back(FuncSig{
        "str_concat",
        {make_primitive(PrimitiveKind::Str), make_primitive(PrimitiveKind::Str)},
        make_primitive(PrimitiveKind::Str),
        {},
        false,
        builtin_span
    });

    // str_substring(s: Str, start: I32, len: I32) -> Str
    functions_["str_substring"].push_back(FuncSig{
        "str_substring",
        {make_primitive(PrimitiveKind::Str),
         make_primitive(PrimitiveKind::I32),
         make_primitive(PrimitiveKind::I32)},
        make_primitive(PrimitiveKind::Str),
        {},
        false,
        builtin_span
    });

    // str_contains(haystack: Str, needle: Str) -> Bool
    functions_["str_contains"].push_back(FuncSig{
        "str_contains",
        {make_primitive(PrimitiveKind::Str), make_primitive(PrimitiveKind::Str)},
        make_primitive(PrimitiveKind::Bool),
        {},
        false,
        builtin_span
    });

    // str_starts_with(s: Str, prefix: Str) -> Bool
    functions_["str_starts_with"].push_back(FuncSig{
        "str_starts_with",
        {make_primitive(PrimitiveKind::Str), make_primitive(PrimitiveKind::Str)},
        make_primitive(PrimitiveKind::Bool),
        {},
        false,
        builtin_span
    });

    // str_ends_with(s: Str, suffix: Str) -> Bool
    functions_["str_ends_with"].push_back(FuncSig{
        "str_ends_with",
        {make_primitive(PrimitiveKind::Str), make_primitive(PrimitiveKind::Str)},
        make_primitive(PrimitiveKind::Bool),
        {},
        false,
        builtin_span
    });

    // str_to_upper(s: Str) -> Str
    functions_["str_to_upper"].push_back(FuncSig{
        "str_to_upper",
        {make_primitive(PrimitiveKind::Str)},
        make_primitive(PrimitiveKind::Str),
        {},
        false,
        builtin_span
    });

    // str_to_lower(s: Str) -> Str
    functions_["str_to_lower"].push_back(FuncSig{
        "str_to_lower",
        {make_primitive(PrimitiveKind::Str)},
        make_primitive(PrimitiveKind::Str),
        {},
        false,
        builtin_span
    });

    // str_trim(s: Str) -> Str
    functions_["str_trim"].push_back(FuncSig{
        "str_trim",
        {make_primitive(PrimitiveKind::Str)},
        make_primitive(PrimitiveKind::Str),
        {},
        false,
        builtin_span
    });

    // str_char_at(s: Str, index: I32) -> Char
    functions_["str_char_at"].push_back(FuncSig{
        "str_char_at",
        {make_primitive(PrimitiveKind::Str), make_primitive(PrimitiveKind::I32)},
        make_primitive(PrimitiveKind::Char),
        {},
        false,
        builtin_span
    });
}

} // namespace tml::types
