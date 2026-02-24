//! # Normalize Array Length Pass
//!
//! Normalizes array length computations. For fixed-size arrays where the
//! size is known at compile time, replaces `array.len()` method calls
//! with constant values. Equivalent to Rust's `NormalizeArrayLen` pass
//! which runs at mir-opt-level >= 1.
//!
//! ## Pattern
//!
//! Before:
//! ```mir
//!   %arr = alloca [I32; 10]
//!   %len = method_call %arr, "len" -> USize
//! ```
//!
//! After:
//! ```mir
//!   %arr = alloca [I32; 10]
//!   %len = const 10 : USize
//! ```
//!
//! ## How It Works
//!
//! 1. Scan for allocas with array types (known size from MirArrayType)
//! 2. Scan for ArrayInitInst (known size from element count)
//! 3. Find method calls to "len" on those values
//! 4. Replace the method call with a constant integer
//!
//! ## Safety
//!
//! Safe because fixed-size arrays have immutable length — the size is part
//! of the type and cannot change at runtime.

#pragma once

#include "mir/mir_pass.hpp"

namespace tml::mir {

class NormalizeArrayLenPass : public FunctionPass {
public:
    [[nodiscard]] auto name() const -> std::string override {
        return "NormalizeArrayLen";
    }

protected:
    auto run_on_function(Function& func) -> bool override;
};

} // namespace tml::mir
