//! # Destination Propagation Pass
//!
//! Eliminates intermediate copies by propagating destinations through
//! store-load-store chains. Equivalent to Rust's `DestinationPropagation`
//! pass which runs at mir-opt-level >= 1 (O0 default).
//!
//! ## Pattern
//!
//! Before:
//! ```mir
//!   %tmp = alloca T
//!   %val = struct_init { ... }
//!   store %val -> %tmp
//!   %loaded = load %tmp
//!   store %loaded -> %dest
//! ```
//!
//! After:
//! ```mir
//!   store %val -> %dest
//! ```
//!
//! ## How It Works
//!
//! 1. Find allocas that are used exactly once as a store target and once
//!    as a load source (single-store, single-load temporaries)
//! 2. Verify the store dominates the load (same block, store before load)
//! 3. Replace uses of the loaded value with the stored value
//! 4. Remove the dead store, load, and alloca
//!
//! ## Safety
//!
//! This pass is safe because:
//! - Only operates on single-use temporaries (no aliasing possible)
//! - Volatile loads/stores are never optimized
//! - The alloca is confirmed dead after the transformation

#pragma once

#include "mir/mir_pass.hpp"

namespace tml::mir {

class DestinationPropagationPass : public FunctionPass {
public:
    [[nodiscard]] auto name() const -> std::string override {
        return "DestinationPropagation";
    }

protected:
    auto run_on_function(Function& func) -> bool override;
};

} // namespace tml::mir
