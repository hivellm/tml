//! # Builtin Math Functions
//!
//! This file registers mathematical intrinsics.
//!
//! ## Math Operations (Integer)
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
//! ## Math Operations (Float)
//!
//! | Function       | Signature            | Description              |
//! |----------------|----------------------|--------------------------|
//! | `sqrt`         | `(F64) -> F64`       | Square root (float)      |
//! | `pow`          | `(F64, I32) -> F64`  | Exponentiation (float)   |
//! | `int_to_float` | `(I32) -> F64`       | Integer to float         |
//! | `float_to_int` | `(F64) -> I32`       | Float to integer         |
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

    // ============ Math Functions (Integer) ============

    // sqrt(x: I32) -> I32 - Square root (integer)
    functions_["sqrt"].push_back(
        FuncSig{"sqrt", {make_i32()}, make_i32(), {}, false, builtin_span});

    // pow(base: I32, exp: I32) -> I32 - Power (integer)
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

    // ============ Math Functions (Float) ============

    // sqrt(x: F64) -> F64 - Square root (float)
    functions_["sqrt"].push_back(
        FuncSig{"sqrt", {make_f64()}, make_f64(), {}, false, builtin_span});

    // pow(base: F64, exp: I32) -> F64 - Power (float base, int exponent)
    functions_["pow"].push_back(
        FuncSig{"pow", {make_f64(), make_i32()}, make_f64(), {}, false, builtin_span});

    // pow(base: F64, exp: I64) -> F64 - Power (float base, int exponent, I64 version)
    functions_["pow"].push_back(
        FuncSig{"pow", {make_f64(), make_i64()}, make_f64(), {}, false, builtin_span});

    // int_to_float(x: I32) -> F64 - Convert integer to float
    functions_["int_to_float"].push_back(
        FuncSig{"int_to_float", {make_i32()}, make_f64(), {}, false, builtin_span});

    // int_to_float(x: I64) -> F64 - Convert I64 to float
    functions_["int_to_float"].push_back(
        FuncSig{"int_to_float", {make_i64()}, make_f64(), {}, false, builtin_span});

    // float_to_int(x: F64) -> I32 - Convert float to integer (truncates)
    functions_["float_to_int"].push_back(
        FuncSig{"float_to_int", {make_f64()}, make_i32(), {}, false, builtin_span});

    // ============ Black Box (prevent optimization) ============

    // black_box(x: I32) -> I32 - Prevent optimization
    functions_["black_box"].push_back(
        FuncSig{"black_box", {make_i32()}, make_i32(), {}, false, builtin_span});

    // black_box_i64(x: I64) -> I64 - Prevent optimization for I64
    functions_["black_box_i64"].push_back(
        FuncSig{"black_box_i64", {make_i64()}, make_i64(), {}, false, builtin_span});
}

} // namespace tml::types
