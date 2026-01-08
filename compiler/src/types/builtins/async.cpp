//! # Builtin Async Functions
//!
//! This file registers async runtime intrinsics.
//!
//! ## block_on
//!
//! `block_on(future: Poll[T]) -> T`
//!
//! Executes an async function synchronously and extracts the result.
//! In the current synchronous execution model, this simply unwraps `Poll.Ready`.
//!
//! ## Overloads
//!
//! | Signature                    | Description         |
//! |------------------------------|---------------------|
//! | `(Poll[I32]) -> I32`         | Block on I32 future |
//! | `(Poll[I64]) -> I64`         | Block on I64 future |
//! | `(Poll[F64]) -> F64`         | Block on F64 future |
//! | `(Poll[Bool]) -> Bool`       | Block on Bool future|
//! | `(Poll[Unit]) -> Unit`       | Block on Unit future|
//! | `(Poll[Str]) -> Str`         | Block on Str future |
//!
//! The type system doesn't yet fully support generic builtins, so we
//! register overloads for common return types.

#include "types/env.hpp"

namespace tml::types {

void TypeEnv::init_builtin_async() {
    SourceSpan builtin_span{};

    // block_on[T](future: Poll[T]) -> T
    // Executes an async function synchronously and extracts the result.
    // In the current synchronous model, this simply unwraps Poll.Ready.

    // We register overloads for common return types since the type system
    // doesn't yet fully support generic builtins.

    // Helper lambda to create Poll[T] type
    auto make_poll = [this](TypePtr inner) -> TypePtr {
        return std::make_shared<Type>(NamedType{"Poll", "", {inner}});
    };

    // block_on(future: Poll[I32]) -> I32
    functions_["block_on"].push_back(
        FuncSig{.name = "block_on",
                .params = {make_poll(make_primitive(PrimitiveKind::I32))},
                .return_type = make_primitive(PrimitiveKind::I32),
                .type_params = {},
                .is_async = false,
                .span = builtin_span,
                .stability = StabilityLevel::Stable,
                .deprecated_message = "",
                .since_version = "1.0"});

    // block_on(future: Poll[I64]) -> I64
    functions_["block_on"].push_back(
        FuncSig{.name = "block_on",
                .params = {make_poll(make_primitive(PrimitiveKind::I64))},
                .return_type = make_primitive(PrimitiveKind::I64),
                .type_params = {},
                .is_async = false,
                .span = builtin_span,
                .stability = StabilityLevel::Stable,
                .deprecated_message = "",
                .since_version = "1.0"});

    // block_on(future: Poll[F64]) -> F64
    functions_["block_on"].push_back(
        FuncSig{.name = "block_on",
                .params = {make_poll(make_primitive(PrimitiveKind::F64))},
                .return_type = make_primitive(PrimitiveKind::F64),
                .type_params = {},
                .is_async = false,
                .span = builtin_span,
                .stability = StabilityLevel::Stable,
                .deprecated_message = "",
                .since_version = "1.0"});

    // block_on(future: Poll[Bool]) -> Bool
    functions_["block_on"].push_back(
        FuncSig{.name = "block_on",
                .params = {make_poll(make_primitive(PrimitiveKind::Bool))},
                .return_type = make_primitive(PrimitiveKind::Bool),
                .type_params = {},
                .is_async = false,
                .span = builtin_span,
                .stability = StabilityLevel::Stable,
                .deprecated_message = "",
                .since_version = "1.0"});

    // block_on(future: Poll[Str]) -> Str
    functions_["block_on"].push_back(
        FuncSig{.name = "block_on",
                .params = {make_poll(make_primitive(PrimitiveKind::Str))},
                .return_type = make_primitive(PrimitiveKind::Str),
                .type_params = {},
                .is_async = false,
                .span = builtin_span,
                .stability = StabilityLevel::Stable,
                .deprecated_message = "",
                .since_version = "1.0"});

    // block_on(future: Poll[Unit]) -> Unit
    functions_["block_on"].push_back(FuncSig{.name = "block_on",
                                             .params = {make_poll(make_unit())},
                                             .return_type = make_unit(),
                                             .type_params = {},
                                             .is_async = false,
                                             .span = builtin_span,
                                             .stability = StabilityLevel::Stable,
                                             .deprecated_message = "",
                                             .since_version = "1.0"});
}

} // namespace tml::types
