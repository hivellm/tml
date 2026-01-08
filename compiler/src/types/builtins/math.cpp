//! # Builtin Math Functions
//!
//! This file registers mathematical intrinsics.
//!
//! ## Math Operations
//!
//! | Function | Signature            | Description     |
//! |----------|----------------------|-----------------|
//! | `sqrt`   | `(I32) -> I32`       | Square root     |
//! | `pow`    | `(I32, I32) -> I32`  | Exponentiation  |
//! | `abs`    | `(I32) -> I32`       | Absolute value  |
//! | `floor`  | `(I32) -> I32`       | Floor           |
//! | `ceil`   | `(I32) -> I32`       | Ceiling         |
//! | `round`  | `(I32) -> I32`       | Round           |
//!
//! ## Optimization Barriers
//!
//! | Function       | Signature        | Description                |
//! |----------------|------------------|----------------------------|
//! | `black_box`    | `(I32) -> I32`   | Prevent optimization (I32) |
//! | `black_box_i64`| `(I64) -> I64`   | Prevent optimization (I64) |
//!
//! Black box functions are used in benchmarks to prevent the compiler
//! from optimizing away computations.

#include "types/env.hpp"

namespace tml::types {

void TypeEnv::init_builtin_math() {
    SourceSpan builtin_span{};

    // ============ Math Functions ============

    // sqrt(x: I32) -> I32 - Square root
    functions_["sqrt"].push_back(
        FuncSig{"sqrt", {make_i32()}, make_i32(), {}, false, builtin_span});

    // pow(base: I32, exp: I32) -> I32 - Power
    functions_["pow"].push_back(
        FuncSig{"pow", {make_i32(), make_i32()}, make_i32(), {}, false, builtin_span});

    // abs(x: I32) -> I32 - Absolute value
    functions_["abs"].push_back(FuncSig{"abs", {make_i32()}, make_i32(), {}, false, builtin_span});

    // floor(x: I32) -> I32 - Floor
    functions_["floor"].push_back(
        FuncSig{"floor", {make_i32()}, make_i32(), {}, false, builtin_span});

    // ceil(x: I32) -> I32 - Ceiling
    functions_["ceil"].push_back(
        FuncSig{"ceil", {make_i32()}, make_i32(), {}, false, builtin_span});

    // round(x: I32) -> I32 - Round
    functions_["round"].push_back(
        FuncSig{"round", {make_i32()}, make_i32(), {}, false, builtin_span});

    // ============ Black Box (prevent optimization) ============

    // black_box(x: I32) -> I32 - Prevent optimization
    functions_["black_box"].push_back(
        FuncSig{"black_box", {make_i32()}, make_i32(), {}, false, builtin_span});

    // black_box_i64(x: I64) -> I64 - Prevent optimization for I64
    functions_["black_box_i64"].push_back(
        FuncSig{"black_box_i64", {make_i64()}, make_i64(), {}, false, builtin_span});
}

} // namespace tml::types
