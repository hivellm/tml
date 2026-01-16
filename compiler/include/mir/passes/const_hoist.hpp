//! # Constant Hoisting Pass
//!
//! Moves expensive constant materialization out of loops to reduce
//! redundant computation. Large constants that appear multiple times
//! in a loop are hoisted to a single location in the preheader.
//!
//! ## Example
//!
//! Before:
//! ```
//! loop:
//!     %1 = const 0x123456789ABCDEF
//!     use %1
//!     goto loop
//! ```
//!
//! After:
//! ```
//! preheader:
//!     %hoisted = const 0x123456789ABCDEF
//! loop:
//!     use %hoisted
//!     goto loop
//! ```

#pragma once

#include "mir/mir.hpp"
#include "mir/mir_pass.hpp"

#include <unordered_map>
#include <unordered_set>

namespace tml::mir {

class ConstantHoistPass : public FunctionPass {
public:
    [[nodiscard]] auto name() const -> std::string override {
        return "ConstHoist";
    }

protected:
    auto run_on_function(Function& func) -> bool override;

private:
    struct LoopInfo {
        uint32_t header;
        std::unordered_set<uint32_t> blocks;
        uint32_t preheader{0}; // Block before loop header (0 = none)
    };

    auto find_loops(const Function& func) -> std::vector<LoopInfo>;
    auto find_preheader(const Function& func, const LoopInfo& loop) -> uint32_t;
    auto hoist_constants(Function& func, const LoopInfo& loop) -> bool;
    auto is_expensive_constant(const ConstantInst& c) -> bool;
    auto get_block_index(const Function& func, uint32_t id) -> int;
};

} // namespace tml::mir
