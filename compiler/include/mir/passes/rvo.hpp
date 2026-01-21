//! # Return Value Optimization (RVO) Pass
//!
//! This pass implements Named Return Value Optimization (NRVO) by identifying
//! local variables that are returned from a function and eliminating copies.
//!
//! ## NRVO Pattern
//!
//! When a local variable is returned from all return paths, the compiler can
//! construct it directly in the caller's return slot:
//!
//! ```tml
//! func create() -> MyStruct {
//!     var result = MyStruct { x: 0 }  // Construct in return slot
//!     result.x = 42                    // Modify return slot directly
//!     return result                    // No copy needed
//! }
//! ```
//!
//! ## Multiple Return Paths
//!
//! For functions with multiple returns, all paths must return the same
//! variable (or compatible struct literals) for NRVO to apply:
//!
//! ```tml
//! func create(flag: Bool) -> MyStruct {
//!     var result: MyStruct
//!     if flag {
//!         result = MyStruct { x: 1 }
//!         return result  // Returns result
//!     }
//!     result = MyStruct { x: 2 }
//!     return result      // Returns result - same variable, NRVO applies
//! }
//! ```
//!
//! ## Hidden Return Pointer (sret)
//!
//! For large structs (> 8 bytes), the calling convention uses a hidden
//! pointer parameter where the callee constructs the return value:
//!
//! ```llvm
//! ; Instead of: define %struct.Big @func()
//! ; Use:        define void @func(ptr sret(%struct.Big) %retval)
//! ```

#pragma once

#include "mir/mir.hpp"
#include "mir/mir_pass.hpp"

#include <unordered_map>
#include <unordered_set>

namespace tml::mir {

/// Information about a return statement
struct ReturnInfo {
    uint32_t block_id;      ///< Block containing the return
    size_t inst_index;      ///< Index of the return instruction
    ValueId returned_value; ///< Value being returned
    bool is_local_var;      ///< True if returning a local variable
    bool is_struct_literal; ///< True if returning a struct literal
};

/// Statistics for RVO optimization
struct RvoStats {
    size_t functions_analyzed = 0;       ///< Functions examined
    size_t nrvo_applied = 0;             ///< NRVO optimizations applied
    size_t copy_elision_applied = 0;     ///< Copy elisions applied
    size_t sret_conversions = 0;         ///< Functions converted to sret
    size_t multiple_returns_unified = 0; ///< Multiple returns to same var

    void reset() {
        *this = RvoStats{};
    }
};

/// Return Value Optimization Pass
///
/// Identifies opportunities for NRVO and copy elision, and prepares
/// the MIR for codegen to use hidden return pointers for large structs.
class RvoPass : public FunctionPass {
public:
    /// Constructor with optional size threshold for sret
    explicit RvoPass(size_t sret_threshold = 16) : sret_threshold_(sret_threshold) {}

    [[nodiscard]] auto name() const -> std::string override {
        return "RVO";
    }

    /// Get optimization statistics
    [[nodiscard]] auto stats() const -> const RvoStats& {
        return stats_;
    }

protected:
    auto run_on_function(Function& func) -> bool override;

private:
    RvoStats stats_;
    [[maybe_unused]] size_t sret_threshold_; ///< Size threshold for sret conversion (bytes)

    /// Map from local variable to all instructions that assign to it
    std::unordered_map<ValueId, std::vector<size_t>> local_assignments_;

    /// Set of return values that are local variables
    std::unordered_set<ValueId> returned_locals_;

    // ============ Analysis Methods ============

    /// Find all return statements in the function
    auto find_returns(const Function& func) -> std::vector<ReturnInfo>;

    /// Check if all returns return the same local variable
    auto all_returns_same_local(const std::vector<ReturnInfo>& returns) -> std::optional<ValueId>;

    /// Check if a value is a local variable (not a parameter)
    auto is_local_variable(const Function& func, ValueId value) const -> bool;

    /// Check if a value was created by a struct literal (StructInitInst)
    auto is_struct_literal(const Function& func, ValueId value) const -> bool;

    /// Check if the returned type is large enough to benefit from sret
    auto should_use_sret(const Function& func) const -> bool;

    // ============ Transformation Methods ============

    /// Mark a local variable for return slot optimization
    void mark_for_return_slot(Function& func, ValueId local_var);

    /// Convert function to use sret parameter for large returns
    auto convert_to_sret(Function& func) -> bool;

    /// Apply NRVO by eliminating copy to return
    auto apply_nrvo(Function& func, ValueId local_var) -> bool;
};

/// Module-level RVO pass that also handles sret calling convention
class ModuleRvoPass : public MirPass {
public:
    [[nodiscard]] auto name() const -> std::string override {
        return "ModuleRVO";
    }

    auto run(Module& module) -> bool override;

    [[nodiscard]] auto stats() const -> const RvoStats& {
        return stats_;
    }

private:
    RvoStats stats_;

    /// Track which functions return large structs
    std::unordered_set<std::string> sret_functions_;
};

/// Sret Conversion Pass - Runs AFTER all inlining
///
/// This pass converts functions returning large structs to use the sret
/// calling convention. It must run after all inlining is complete to avoid
/// breaking inlined code.
class SretConversionPass : public FunctionPass {
public:
    [[nodiscard]] auto name() const -> std::string override {
        return "SretConversion";
    }

    /// Get the number of functions converted
    [[nodiscard]] auto conversions() const -> size_t {
        return conversions_;
    }

protected:
    auto run_on_function(Function& func) -> bool override;

private:
    size_t conversions_ = 0;

    /// Check if the returned type is large enough to benefit from sret
    auto should_use_sret(const Function& func) const -> bool;

    /// Convert function to use sret parameter for large returns
    auto convert_to_sret(Function& func) -> bool;
};

} // namespace tml::mir
