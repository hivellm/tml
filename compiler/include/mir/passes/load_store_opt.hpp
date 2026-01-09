//! # Load-Store Optimization Pass
//!
//! Eliminates redundant loads and stores within basic blocks.
//!
//! ## Optimizations
//!
//! - **Redundant Load Elimination**: If we load from address A, and later load
//!   from A again with no intervening store to A, reuse the first load.
//!
//! - **Dead Store Elimination**: If we store to address A, and later store to A
//!   again with no intervening load from A, eliminate the first store.
//!
//! - **Store-to-Load Forwarding**: If we store value V to address A, and later
//!   load from A with no intervening store, use V directly.

#pragma once

#include "mir/mir.hpp"
#include "mir/mir_pass.hpp"

#include <unordered_map>

namespace tml::mir {

class LoadStoreOptPass : public FunctionPass {
  public:
    [[nodiscard]] auto name() const -> std::string override { return "LoadStoreOpt"; }

  protected:
    auto run_on_function(Function& func) -> bool override;

  private:
    // Track the last value stored to an address
    struct MemState {
        ValueId stored_value{0};  // Last value stored (0 = unknown)
        ValueId loaded_value{0};  // Last value loaded (0 = unknown)
        bool has_store{false};
        bool has_load{false};
    };

    auto optimize_block(Function& func, BasicBlock& block) -> bool;
    auto may_alias(ValueId ptr1, ValueId ptr2) -> bool;
    auto invalidate_all(std::unordered_map<ValueId, MemState>& mem_state) -> void;
};

} // namespace tml::mir
