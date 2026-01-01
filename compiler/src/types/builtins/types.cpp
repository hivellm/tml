// Builtin primitive types and behavior implementations
#include "types/env.hpp"

namespace tml::types {

void TypeEnv::init_builtin_types() {
    // Primitive types
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

    // Ordering enum (core::cmp)
    // Ordering { Less, Equal, Greater }
    define_enum(EnumDef{.name = "Ordering",
                        .type_params = {},
                        .const_params = {},
                        .variants = {{"Less", {}}, {"Equal", {}}, {"Greater", {}}},
                        .span = {}});

    // Maybe[T] enum (core::option)
    // Maybe[T] { Just(T), Nothing }
    {
        auto T = std::make_shared<Type>(GenericType{"T"});
        define_enum(EnumDef{.name = "Maybe",
                            .type_params = {"T"},
                            .const_params = {},
                            .variants = {{"Just", {T}}, {"Nothing", {}}},
                            .span = {}});
    }

    // Outcome[T, E] enum (core::result)
    // Outcome[T, E] { Ok(T), Err(E) }
    {
        auto T = std::make_shared<Type>(GenericType{"T"});
        auto E = std::make_shared<Type>(GenericType{"E"});
        define_enum(EnumDef{.name = "Outcome",
                            .type_params = {"T", "E"},
                            .const_params = {},
                            .variants = {{"Ok", {T}}, {"Err", {E}}},
                            .span = {}});
    }

    // Poll[T] enum (core::async)
    // Poll[T] { Ready(T), Pending }
    {
        auto T = std::make_shared<Type>(GenericType{"T"});
        define_enum(EnumDef{.name = "Poll",
                            .type_params = {"T"},
                            .const_params = {},
                            .variants = {{"Ready", {T}}, {"Pending", {}}},
                            .span = {}});
    }

    // Future[T] behavior (core::async)
    // behavior Future { type Output; func poll(mut this, cx: mut ref Context) -> Poll[This.Output]
    // }
    {
        auto Output = std::make_shared<Type>(GenericType{"Output"});
        auto poll_output = std::make_shared<Type>(NamedType{"Poll", "", {Output}});
        define_behavior(BehaviorDef{
            .name = "Future",
            .type_params = {},
            .const_params = {},
            .associated_types = {AssociatedTypeDef{
                .name = "Output", .type_params = {}, .bounds = {}, .default_type = std::nullopt}},
            .methods = {FuncSig{.name = "poll",
                                .params = {}, // self is implicit
                                .return_type = poll_output,
                                .type_params = {},
                                .is_async = false, // poll itself is not async
                                .span = {}}},
            .super_behaviors = {},
            .methods_with_defaults = {},
            .span = {}});
    }

    // Drop behavior (core::ops)
    // behavior Drop { func drop(mut this) }
    // Enables RAII - automatic cleanup when values go out of scope
    {
        define_behavior(
            BehaviorDef{.name = "Drop",
                        .type_params = {},
                        .const_params = {},
                        .associated_types = {},
                        .methods = {FuncSig{.name = "drop",
                                            .params = {}, // mut this is implicit
                                            .return_type = make_unit(),
                                            .type_params = {},
                                            .is_async = false,
                                            .span = {}}},
                        .super_behaviors = {},
                        .methods_with_defaults = {}, // No default - must be explicitly implemented
                        .span = {}});
    }

    // Register builtin behavior implementations for integer types
    std::vector<std::string> integer_types = {"I8", "I16", "I32", "I64", "I128",
                                              "U8", "U16", "U32", "U64", "U128"};
    std::vector<std::string> integer_behaviors = {"Eq",      "Ord",   "Numeric", "Hash",
                                                  "Display", "Debug", "Default", "Duplicate"};
    for (const auto& type : integer_types) {
        for (const auto& behavior : integer_behaviors) {
            register_impl(type, behavior);
        }
    }

    // Float types
    std::vector<std::string> float_types = {"F32", "F64"};
    std::vector<std::string> float_behaviors = {"Eq",    "Ord",     "Numeric",  "Display",
                                                "Debug", "Default", "Duplicate"};
    for (const auto& type : float_types) {
        for (const auto& behavior : float_behaviors) {
            register_impl(type, behavior);
        }
    }

    // Bool
    register_impl("Bool", "Eq");
    register_impl("Bool", "Ord");
    register_impl("Bool", "Hash");
    register_impl("Bool", "Display");
    register_impl("Bool", "Debug");
    register_impl("Bool", "Default");
    register_impl("Bool", "Duplicate");

    // Char
    register_impl("Char", "Eq");
    register_impl("Char", "Ord");
    register_impl("Char", "Hash");
    register_impl("Char", "Display");
    register_impl("Char", "Debug");
    register_impl("Char", "Duplicate");

    // Str
    register_impl("Str", "Eq");
    register_impl("Str", "Ord");
    register_impl("Str", "Hash");
    register_impl("Str", "Display");
    register_impl("Str", "Debug");
    register_impl("Str", "Duplicate");
}

} // namespace tml::types
