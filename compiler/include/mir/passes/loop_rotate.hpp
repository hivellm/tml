//! # Loop Rotation Pass
//!
//! Transforms loops to expose more optimization opportunities.
//!
//! ## Transformation
//!
//! Converts:
//! ```
//! loop:
//!     if (cond) goto exit
//!     body
//!     goto loop
//! exit:
//! ```
//!
//! To:
//! ```
//! if (cond) goto exit
//! loop:
//!     body
//!     if (!cond) goto loop
//! exit:
//! ```
//!
//! This form is better because:
//! - The loop body is executed without an initial check
//! - Better for branch prediction
//! - Exposes opportunities for LICM and other optimizations

#pragma once

#include "mir/mir.hpp"
#include "mir/mir_pass.hpp"

#include <unordered_set>

namespace tml::mir {

class LoopRotatePass : public FunctionPass {
  public:
    [[nodiscard]] auto name() const -> std::string override { return "LoopRotate"; }

  protected:
    auto run_on_function(Function& func) -> bool override;

  private:
    struct LoopInfo {
        uint32_t header;
        uint32_t latch;  // Back edge source
        std::unordered_set<uint32_t> blocks;
    };

    auto find_loops(const Function& func) -> std::vector<LoopInfo>;
    auto is_rotatable(const Function& func, const LoopInfo& loop) -> bool;
    auto rotate_loop(Function& func, const LoopInfo& loop) -> bool;
    auto get_block_index(const Function& func, uint32_t id) -> int;
};

} // namespace tml::mir
