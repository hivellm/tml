#include "tml/types/env.hpp"
#include "tml/types/module.hpp"

namespace tml::types {

void TypeEnv::init_builtins() {
    // Builtin types
    builtins_["I8"] = make_primitive(PrimitiveKind::I8);
    builtins_["I16"] = make_primitive(PrimitiveKind::I16);
    builtins_["I32"] = make_primitive(PrimitiveKind::I32);
    builtins_["I64"] = make_primitive(PrimitiveKind::I64);
    builtins_["I128"] = make_primitive(PrimitiveKind::I128);
    builtins_["U8"] = make_primitive(PrimitiveKind::U8);
    builtins_["U16"] = make_primitive(PrimitiveKind::U16);
    builtins_["U32"] = make_primitive(PrimitiveKind::U32);
    builtins_["U64"] = make_primitive(PrimitiveKind::U64);
    builtins_["U128"] = make_primitive(PrimitiveKind::U128);
    builtins_["F32"] = make_primitive(PrimitiveKind::F32);
    builtins_["F64"] = make_primitive(PrimitiveKind::F64);
    builtins_["Bool"] = make_primitive(PrimitiveKind::Bool);
    builtins_["Char"] = make_primitive(PrimitiveKind::Char);
    builtins_["Str"] = make_primitive(PrimitiveKind::Str);
    builtins_["Unit"] = make_unit();

    // Collection types (defined here before function signatures)
    auto list_type = std::make_shared<Type>(Type{NamedType{"List", "", {}}});
    auto hashmap_type = std::make_shared<Type>(Type{NamedType{"HashMap", "", {}}});
    auto buffer_type = std::make_shared<Type>(Type{NamedType{"Buffer", "", {}}});

    builtins_["List"] = list_type;
    builtins_["HashMap"] = hashmap_type;
    builtins_["Buffer"] = buffer_type;

    // ONLY essential core functions remain global
    // All other functions are in modules and require 'use' import
    SourceSpan builtin_span{};

    // Core I/O (always available)
    functions_["print"] = FuncSig{
        "print",
        {make_primitive(PrimitiveKind::Str)},
        make_unit(),
        {},
        false,
        builtin_span,
        StabilityLevel::Stable,
        "",
        "1.0"
    };

    functions_["println"] = FuncSig{
        "println",
        {make_primitive(PrimitiveKind::Str)},
        make_unit(),
        {},
        false,
        builtin_span,
        StabilityLevel::Stable,
        "",
        "1.0"
    };

    functions_["panic"] = FuncSig{
        "panic",
        {make_primitive(PrimitiveKind::Str)},
        make_unit(),
        {},
        false,
        builtin_span,
        StabilityLevel::Stable,
        "",
        "1.0"
    };

    // String utilities (always available)
    functions_["str_len"] = FuncSig{
        "str_len",
        {make_primitive(PrimitiveKind::Str)},
        make_primitive(PrimitiveKind::I32),
        {},
        false,
        builtin_span
    };

    functions_["str_eq"] = FuncSig{
        "str_eq",
        {make_primitive(PrimitiveKind::Str), make_primitive(PrimitiveKind::Str)},
        make_primitive(PrimitiveKind::Bool),
        {},
        false,
        builtin_span
    };

    functions_["str_hash"] = FuncSig{
        "str_hash",
        {make_primitive(PrimitiveKind::Str)},
        make_primitive(PrimitiveKind::I32),
        {},
        false,
        builtin_span
    };

    // Time functions (always available)
    functions_["time_ms"] = FuncSig{
        "time_ms",
        {},
        make_primitive(PrimitiveKind::I32),
        {},
        false,
        builtin_span
    };

    functions_["time_us"] = FuncSig{
        "time_us",
        {},
        make_primitive(PrimitiveKind::I64),
        {},
        false,
        builtin_span
    };

    functions_["time_ns"] = FuncSig{
        "time_ns",
        {},
        make_primitive(PrimitiveKind::I64),
        {},
        false,
        builtin_span
    };
}

void TypeEnv::init_test_module() {
    if (!module_registry_) {
        return;  // No module registry available
    }

    Module test_module;
    test_module.name = "test";

    // Basic assert(condition: Bool, message: Str) -> Unit
    test_module.functions["assert"] = FuncSig{
        "assert",
        {make_primitive(PrimitiveKind::Bool), make_primitive(PrimitiveKind::Str)},
        make_unit(),
        {},
        false,
        SourceSpan{},
        StabilityLevel::Stable,
        "",
        "1.0"
    };

    // Specific assert_eq_i32(left: I32, right: I32, message: Str) -> Unit
    test_module.functions["assert_eq_i32"] = FuncSig{
        "assert_eq_i32",
        {make_primitive(PrimitiveKind::I32), make_primitive(PrimitiveKind::I32), 
         make_primitive(PrimitiveKind::Str)},
        make_unit(),
        {},
        false,
        SourceSpan{},
        StabilityLevel::Stable,
        "",
        "1.0"
    };

    // assert_ne_i32(left: I32, right: I32, message: Str) -> Unit
    test_module.functions["assert_ne_i32"] = FuncSig{
        "assert_ne_i32",
        {make_primitive(PrimitiveKind::I32), make_primitive(PrimitiveKind::I32), 
         make_primitive(PrimitiveKind::Str)},
        make_unit(),
        {},
        false,
        SourceSpan{},
        StabilityLevel::Stable,
        "",
        "1.0"
    };

    // assert_eq_str(left: Str, right: Str, message: Str) -> Unit
    test_module.functions["assert_eq_str"] = FuncSig{
        "assert_eq_str",
        {make_primitive(PrimitiveKind::Str), make_primitive(PrimitiveKind::Str), 
         make_primitive(PrimitiveKind::Str)},
        make_unit(),
        {},
        false,
        SourceSpan{},
        StabilityLevel::Stable,
        "",
        "1.0"
    };

    // assert_eq_bool(left: Bool, right: Bool, message: Str) -> Unit
    test_module.functions["assert_eq_bool"] = FuncSig{
        "assert_eq_bool",
        {make_primitive(PrimitiveKind::Bool), make_primitive(PrimitiveKind::Bool), 
         make_primitive(PrimitiveKind::Str)},
        make_unit(),
        {},
        false,
        SourceSpan{},
        StabilityLevel::Stable,
        "",
        "1.0"
    };

    module_registry_->register_module("test", std::move(test_module));
}

} // namespace tml::types
