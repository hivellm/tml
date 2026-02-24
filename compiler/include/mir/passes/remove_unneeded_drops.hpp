//! # Remove Unneeded Drops Pass
//!
//! Eliminates drop calls for types that don't implement Drop and have no
//! fields that need dropping. This is the MIR equivalent of Rust's
//! `RemoveUnneededDrops` pass which runs at mir-opt-level >= 1 (O0 default).
//!
//! ## Pattern
//!
//! Before:
//! ```mir
//!   call @I32::drop(%x)        ; I32 doesn't implement Drop — remove
//!   call @Point::drop(%p)      ; Point{x: I32, y: I32} — no Drop, no droppable fields — remove
//!   call @List::drop(%list)    ; List implements Drop — KEEP
//! ```
//!
//! After:
//! ```mir
//!   call @List::drop(%list)    ; Only real drops remain
//! ```
//!
//! ## How It Works
//!
//! Drop calls are identified by function names ending in `::drop` or `_drop`.
//! For each drop call, we check if the target type:
//! 1. Explicitly implements the `Drop` behavior
//! 2. Contains fields that transitively need dropping
//!
//! If neither condition holds, the drop call is dead and can be removed.
//!
//! ## Safety
//!
//! This pass is safe because:
//! - It only removes calls to functions that would be no-ops anyway
//! - The type checker already validates Drop implementations
//! - Primitive types (I32, Bool, F64, etc.) never need drops

#pragma once

#include "mir/mir_pass.hpp"

namespace tml::mir {

class RemoveUnneededDropsPass : public FunctionPass {
public:
    [[nodiscard]] auto name() const -> std::string override {
        return "RemoveUnneededDrops";
    }

protected:
    auto run_on_function(Function& func) -> bool override;

private:
    /// Check if a function name looks like a drop call
    static auto is_drop_call(const std::string& func_name) -> bool;

    /// Extract the type name from a drop function name
    /// e.g., "I32::drop" -> "I32", "List_drop" -> "List"
    static auto extract_drop_type(const std::string& func_name) -> std::string;

    /// Check if a type actually needs dropping (has Drop impl or droppable fields)
    static auto type_needs_drop(const std::string& type_name) -> bool;
};

} // namespace tml::mir
