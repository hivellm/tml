// Builtin IO functions: print, println, panic
#include "types/env.hpp"

namespace tml::types {

void TypeEnv::init_builtin_io() {
    SourceSpan builtin_span{};

    // print(message: Str) -> Unit
    functions_["print"].push_back(FuncSig{.name = "print",
                                          .params = {make_primitive(PrimitiveKind::Str)},
                                          .return_type = make_unit(),
                                          .type_params = {},
                                          .is_async = false,
                                          .span = builtin_span,
                                          .stability = StabilityLevel::Stable,
                                          .deprecated_message = "",
                                          .since_version = "1.0"});

    // println(message: Str) -> Unit
    functions_["println"].push_back(FuncSig{.name = "println",
                                            .params = {make_primitive(PrimitiveKind::Str)},
                                            .return_type = make_unit(),
                                            .type_params = {},
                                            .is_async = false,
                                            .span = builtin_span,
                                            .stability = StabilityLevel::Stable,
                                            .deprecated_message = "",
                                            .since_version = "1.0"});

    // panic(message: Str) -> Never
    functions_["panic"].push_back(FuncSig{.name = "panic",
                                          .params = {make_primitive(PrimitiveKind::Str)},
                                          .return_type = make_never(),
                                          .type_params = {},
                                          .is_async = false,
                                          .span = builtin_span,
                                          .stability = StabilityLevel::Stable,
                                          .deprecated_message = "",
                                          .since_version = "1.0"});

    // assert(condition: Bool, message: Str) -> Unit
    functions_["assert"].push_back(
        FuncSig{.name = "assert",
                .params = {make_primitive(PrimitiveKind::Bool), make_primitive(PrimitiveKind::Str)},
                .return_type = make_unit(),
                .type_params = {},
                .is_async = false,
                .span = builtin_span,
                .stability = StabilityLevel::Stable,
                .deprecated_message = "",
                .since_version = "1.0"});

    // assert_eq[T](left: T, right: T) -> Unit
    // Generic assert_eq - compares two values of same type
    // Note: This is a simplified version. The type checker handles the generics.
    // For now we register common type overloads.

    // assert_eq(left: I32, right: I32) -> Unit
    functions_["assert_eq"].push_back(
        FuncSig{.name = "assert_eq",
                .params = {make_primitive(PrimitiveKind::I32), make_primitive(PrimitiveKind::I32)},
                .return_type = make_unit(),
                .type_params = {},
                .is_async = false,
                .span = builtin_span,
                .stability = StabilityLevel::Stable,
                .deprecated_message = "",
                .since_version = "1.0"});

    // assert_eq(left: I64, right: I64) -> Unit
    functions_["assert_eq"].push_back(
        FuncSig{.name = "assert_eq",
                .params = {make_primitive(PrimitiveKind::I64), make_primitive(PrimitiveKind::I64)},
                .return_type = make_unit(),
                .type_params = {},
                .is_async = false,
                .span = builtin_span,
                .stability = StabilityLevel::Stable,
                .deprecated_message = "",
                .since_version = "1.0"});

    // assert_eq(left: Bool, right: Bool) -> Unit
    functions_["assert_eq"].push_back(FuncSig{
        .name = "assert_eq",
        .params = {make_primitive(PrimitiveKind::Bool), make_primitive(PrimitiveKind::Bool)},
        .return_type = make_unit(),
        .type_params = {},
        .is_async = false,
        .span = builtin_span,
        .stability = StabilityLevel::Stable,
        .deprecated_message = "",
        .since_version = "1.0"});

    // assert_eq(left: Str, right: Str) -> Unit
    functions_["assert_eq"].push_back(
        FuncSig{.name = "assert_eq",
                .params = {make_primitive(PrimitiveKind::Str), make_primitive(PrimitiveKind::Str)},
                .return_type = make_unit(),
                .type_params = {},
                .is_async = false,
                .span = builtin_span,
                .stability = StabilityLevel::Stable,
                .deprecated_message = "",
                .since_version = "1.0"});

    // assert_eq(left: F64, right: F64) -> Unit
    functions_["assert_eq"].push_back(
        FuncSig{.name = "assert_eq",
                .params = {make_primitive(PrimitiveKind::F64), make_primitive(PrimitiveKind::F64)},
                .return_type = make_unit(),
                .type_params = {},
                .is_async = false,
                .span = builtin_span,
                .stability = StabilityLevel::Stable,
                .deprecated_message = "",
                .since_version = "1.0"});
}

} // namespace tml::types
