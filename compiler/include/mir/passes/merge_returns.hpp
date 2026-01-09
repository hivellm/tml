//! # MergeReturns Pass
//!
//! Combines multiple return statements into a single exit block.
//! This simplifies the CFG and can enable other optimizations.
//!
//! ## Transformation
//!
//! Before:
//! ```
//! bb1:
//!     return %1
//! bb2:
//!     return %2
//! ```
//!
//! After:
//! ```
//! bb1:
//!     goto exit(%1)
//! bb2:
//!     goto exit(%2)
//! exit:
//!     %ret = phi [%1, bb1], [%2, bb2]
//!     return %ret
//! ```

#pragma once

#include "mir/mir.hpp"
#include "mir/mir_pass.hpp"

namespace tml::mir {

class MergeReturnsPass : public FunctionPass {
  public:
    [[nodiscard]] auto name() const -> std::string override { return "MergeReturns"; }

  protected:
    auto run_on_function(Function& func) -> bool override;

  private:
    auto find_return_blocks(const Function& func) -> std::vector<size_t>;
    auto create_unified_exit(Function& func, const std::vector<size_t>& return_blocks) -> bool;
};

} // namespace tml::mir
