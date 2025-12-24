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

    // Float math functions (F64 -> F64)
    functions_["float_sqrt"] = FuncSig{
        "float_sqrt",
        {make_primitive(PrimitiveKind::F64)},
        make_primitive(PrimitiveKind::F64),
        {},
        false,
        builtin_span
    };

    functions_["sqrt"] = FuncSig{
        "sqrt",
        {make_primitive(PrimitiveKind::F64)},
        make_primitive(PrimitiveKind::F64),
        {},
        false,
        builtin_span
    };

    functions_["float_pow"] = FuncSig{
        "float_pow",
        {make_primitive(PrimitiveKind::F64), make_primitive(PrimitiveKind::I32)},
        make_primitive(PrimitiveKind::F64),
        {},
        false,
        builtin_span
    };

    functions_["pow"] = FuncSig{
        "pow",
        {make_primitive(PrimitiveKind::F64), make_primitive(PrimitiveKind::I32)},
        make_primitive(PrimitiveKind::F64),
        {},
        false,
        builtin_span
    };

    functions_["float_abs"] = FuncSig{
        "float_abs",
        {make_primitive(PrimitiveKind::F64)},
        make_primitive(PrimitiveKind::F64),
        {},
        false,
        builtin_span
    };

    // Float conversion functions
    functions_["int_to_float"] = FuncSig{
        "int_to_float",
        {make_primitive(PrimitiveKind::I32)},
        make_primitive(PrimitiveKind::F64),
        {},
        false,
        builtin_span
    };

    functions_["float_to_int"] = FuncSig{
        "float_to_int",
        {make_primitive(PrimitiveKind::F64)},
        make_primitive(PrimitiveKind::I32),
        {},
        false,
        builtin_span
    };

    functions_["float_round"] = FuncSig{
        "float_round",
        {make_primitive(PrimitiveKind::F64)},
        make_primitive(PrimitiveKind::I32),
        {},
        false,
        builtin_span
    };

    functions_["float_floor"] = FuncSig{
        "float_floor",
        {make_primitive(PrimitiveKind::F64)},
        make_primitive(PrimitiveKind::I32),
        {},
        false,
        builtin_span
    };

    functions_["float_ceil"] = FuncSig{
        "float_ceil",
        {make_primitive(PrimitiveKind::F64)},
        make_primitive(PrimitiveKind::I32),
        {},
        false,
        builtin_span
    };

    // ============ BIT MANIPULATION FUNCTIONS ============

    functions_["float32_bits"] = FuncSig{
        "float32_bits",
        {make_primitive(PrimitiveKind::F32)},
        make_primitive(PrimitiveKind::U32),
        {},
        false,
        builtin_span
    };

    functions_["float32_from_bits"] = FuncSig{
        "float32_from_bits",
        {make_primitive(PrimitiveKind::U32)},
        make_primitive(PrimitiveKind::F32),
        {},
        false,
        builtin_span
    };

    functions_["float64_bits"] = FuncSig{
        "float64_bits",
        {make_primitive(PrimitiveKind::F64)},
        make_primitive(PrimitiveKind::U64),
        {},
        false,
        builtin_span
    };

    functions_["float64_from_bits"] = FuncSig{
        "float64_from_bits",
        {make_primitive(PrimitiveKind::U64)},
        make_primitive(PrimitiveKind::F64),
        {},
        false,
        builtin_span
    };

    // ============ SPECIAL FLOAT VALUES ============

    functions_["infinity"] = FuncSig{
        "infinity",
        {make_primitive(PrimitiveKind::I32)},
        make_primitive(PrimitiveKind::F64),
        {},
        false,
        builtin_span
    };

    functions_["nan"] = FuncSig{
        "nan",
        {},
        make_primitive(PrimitiveKind::F64),
        {},
        false,
        builtin_span
    };

    functions_["is_inf"] = FuncSig{
        "is_inf",
        {make_primitive(PrimitiveKind::F64), make_primitive(PrimitiveKind::I32)},
        make_primitive(PrimitiveKind::Bool),
        {},
        false,
        builtin_span
    };

    functions_["is_nan"] = FuncSig{
        "is_nan",
        {make_primitive(PrimitiveKind::F64)},
        make_primitive(PrimitiveKind::Bool),
        {},
        false,
        builtin_span
    };

    // ============ NEXTAFTER FUNCTIONS ============

    functions_["nextafter"] = FuncSig{
        "nextafter",
        {make_primitive(PrimitiveKind::F64), make_primitive(PrimitiveKind::F64)},
        make_primitive(PrimitiveKind::F64),
        {},
        false,
        builtin_span
    };

    functions_["nextafter32"] = FuncSig{
        "nextafter32",
        {make_primitive(PrimitiveKind::F32), make_primitive(PrimitiveKind::F32)},
        make_primitive(PrimitiveKind::F32),
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

    // ============ Coverage Functions ============

    // cover_func(name: Str) -> Unit
    test_module.functions["cover_func"] = FuncSig{
        "cover_func",
        {make_primitive(PrimitiveKind::Str)},
        make_unit(),
        {},
        false,
        SourceSpan{},
        StabilityLevel::Stable,
        "",
        "1.0"
    };

    // cover_line(file: Str, line: I32) -> Unit
    test_module.functions["cover_line"] = FuncSig{
        "cover_line",
        {make_primitive(PrimitiveKind::Str), make_primitive(PrimitiveKind::I32)},
        make_unit(),
        {},
        false,
        SourceSpan{},
        StabilityLevel::Stable,
        "",
        "1.0"
    };

    // cover_branch(file: Str, line: I32, branch_id: I32) -> Unit
    test_module.functions["cover_branch"] = FuncSig{
        "cover_branch",
        {make_primitive(PrimitiveKind::Str), make_primitive(PrimitiveKind::I32),
         make_primitive(PrimitiveKind::I32)},
        make_unit(),
        {},
        false,
        SourceSpan{},
        StabilityLevel::Stable,
        "",
        "1.0"
    };

    // print_coverage_report() -> Unit
    test_module.functions["print_coverage_report"] = FuncSig{
        "print_coverage_report",
        {},
        make_unit(),
        {},
        false,
        SourceSpan{},
        StabilityLevel::Stable,
        "",
        "1.0"
    };

    // get_covered_func_count() -> I32
    test_module.functions["get_covered_func_count"] = FuncSig{
        "get_covered_func_count",
        {},
        make_primitive(PrimitiveKind::I32),
        {},
        false,
        SourceSpan{},
        StabilityLevel::Stable,
        "",
        "1.0"
    };

    // get_covered_line_count() -> I32
    test_module.functions["get_covered_line_count"] = FuncSig{
        "get_covered_line_count",
        {},
        make_primitive(PrimitiveKind::I32),
        {},
        false,
        SourceSpan{},
        StabilityLevel::Stable,
        "",
        "1.0"
    };

    // get_covered_branch_count() -> I32
    test_module.functions["get_covered_branch_count"] = FuncSig{
        "get_covered_branch_count",
        {},
        make_primitive(PrimitiveKind::I32),
        {},
        false,
        SourceSpan{},
        StabilityLevel::Stable,
        "",
        "1.0"
    };

    // reset_coverage() -> Unit
    test_module.functions["reset_coverage"] = FuncSig{
        "reset_coverage",
        {},
        make_unit(),
        {},
        false,
        SourceSpan{},
        StabilityLevel::Stable,
        "",
        "1.0"
    };

    // is_func_covered(name: Str) -> Bool
    test_module.functions["is_func_covered"] = FuncSig{
        "is_func_covered",
        {make_primitive(PrimitiveKind::Str)},
        make_primitive(PrimitiveKind::Bool),
        {},
        false,
        SourceSpan{},
        StabilityLevel::Stable,
        "",
        "1.0"
    };

    // get_coverage_percent() -> I32
    test_module.functions["get_coverage_percent"] = FuncSig{
        "get_coverage_percent",
        {},
        make_primitive(PrimitiveKind::I32),
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
