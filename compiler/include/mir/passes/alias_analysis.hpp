//! # Alias Analysis Pass
//!
//! This pass performs alias analysis to determine whether two memory locations
//! may refer to the same memory. This information is used by other passes
//! (LICM, GVN, LoadStoreOpt) to optimize memory operations.
//!
//! ## Alias Results
//!
//! | Result | Meaning |
//! |--------|---------|
//! | NoAlias | Pointers never alias (safe to reorder) |
//! | MayAlias | Pointers might alias (must be conservative) |
//! | MustAlias | Pointers always refer to same location |
//! | PartialAlias | Pointers overlap but are not identical |
//!
//! ## Analysis Levels
//!
//! 1. **Basic**: Stack vs global, different allocas don't alias
//! 2. **Type-based (TBAA)**: Different types don't alias (strict aliasing)
//! 3. **Field-sensitive**: Different struct fields don't alias
//! 4. **Flow-sensitive**: Track aliases through CFG
//!
//! ## Example
//!
//! ```tml
//! var x: I32 = 1
//! var y: I32 = 2
//! // x and y have NoAlias - they're different stack variables
//!
//! var arr: [I32; 10]
//! let p1 = ref arr[0]
//! let p2 = ref arr[1]
//! // p1 and p2 have NoAlias - different array elements at constant indices
//!
//! func foo(p1: mut ref I32, p2: mut ref I32) {
//!     // p1 and p2 have MayAlias - could be the same reference
//! }
//! ```

#pragma once

#include "mir/mir.hpp"
#include "mir/mir_pass.hpp"

#include <optional>
#include <unordered_map>
#include <unordered_set>

namespace tml::mir {

/// Result of alias query
enum class AliasResult {
    NoAlias,     ///< Definitely do not alias
    MayAlias,    ///< Might alias, be conservative
    MustAlias,   ///< Always refer to same memory
    PartialAlias ///< Overlap but not identical
};

/// Memory location descriptor
struct MemoryLocation {
    ValueId base;       ///< Base pointer
    int64_t offset = 0; ///< Offset from base (if known)
    int64_t size = -1;  ///< Size of access (-1 = unknown)
    MirTypePtr type;    ///< Type being accessed

    /// Check if this is a null location
    [[nodiscard]] auto is_null() const -> bool {
        return base == INVALID_VALUE;
    }

    /// Create a location from a value
    static auto from_value(ValueId val, MirTypePtr ty = nullptr) -> MemoryLocation {
        return MemoryLocation{val, 0, -1, std::move(ty)};
    }
};

/// Pointer origin tracking for alias analysis
enum class PointerOrigin {
    Unknown,        ///< Origin not tracked
    StackAlloca,    ///< From alloca instruction
    GlobalVariable, ///< From global variable
    HeapAlloc,      ///< From heap allocation
    FunctionArg,    ///< From function argument (may alias)
    GEP,            ///< From GetElementPtr
    FieldAccess     ///< From struct field access
};

/// Information about a pointer value
struct PointerInfo {
    PointerOrigin origin = PointerOrigin::Unknown;
    ValueId base = INVALID_VALUE; ///< Ultimate base pointer
    std::vector<int64_t> offsets; ///< Known offsets
    MirTypePtr pointee_type;      ///< Type pointed to
    bool is_restrict = false;     ///< Marked as restrict/noalias
};

/// Statistics for alias analysis
struct AliasAnalysisStats {
    size_t queries_total = 0;
    size_t no_alias_results = 0;
    size_t may_alias_results = 0;
    size_t must_alias_results = 0;
    size_t partial_alias_results = 0;

    void reset() {
        *this = AliasAnalysisStats{};
    }
};

/// Alias Analysis Pass
///
/// Performs inter- and intra-procedural alias analysis to determine
/// whether memory accesses may interfere with each other.
class AliasAnalysisPass : public FunctionPass {
public:
    [[nodiscard]] auto name() const -> std::string override {
        return "AliasAnalysis";
    }

    /// Query alias relationship between two memory locations
    [[nodiscard]] auto query(const MemoryLocation& loc1, const MemoryLocation& loc2) const
        -> AliasResult;

    /// Query alias relationship between two pointer values
    [[nodiscard]] auto alias(ValueId ptr1, ValueId ptr2) const -> AliasResult;

    /// Get pointer information
    [[nodiscard]] auto get_pointer_info(ValueId ptr) const -> const PointerInfo*;

    /// Check if a pointer is known to be a stack allocation
    [[nodiscard]] auto is_stack_pointer(ValueId ptr) const -> bool;

    /// Check if two pointers are provably from different allocations
    [[nodiscard]] auto are_distinct_allocations(ValueId ptr1, ValueId ptr2) const -> bool;

    /// Get statistics
    [[nodiscard]] auto stats() const -> const AliasAnalysisStats& {
        return stats_;
    }

protected:
    auto run_on_function(Function& func) -> bool override;

private:
    mutable AliasAnalysisStats stats_;

    /// Map from value to pointer information
    std::unordered_map<ValueId, PointerInfo> pointer_info_;

    /// Set of known stack allocations
    std::unordered_set<ValueId> stack_allocas_;

    /// Set of known global variables
    std::unordered_set<ValueId> global_vars_;

    /// Set of known heap allocations
    std::unordered_set<ValueId> heap_allocs_;

    // ============ Analysis Methods ============

    /// Build pointer information for all values in function
    void analyze_pointers(const Function& func);

    /// Analyze a single instruction for pointer info
    void analyze_instruction(const InstructionData& inst);

    /// Trace a pointer back to its origin
    auto trace_pointer_origin(ValueId ptr) const -> PointerOrigin;

    /// Get the base pointer for a derived pointer
    auto get_base_pointer(ValueId ptr) const -> ValueId;

    // ============ Alias Query Helpers ============

    /// Basic alias analysis: stack vs global, different allocas
    [[nodiscard]] auto basic_alias(ValueId ptr1, ValueId ptr2) const -> AliasResult;

    /// Type-based alias analysis (TBAA)
    [[nodiscard]] auto type_based_alias(ValueId ptr1, ValueId ptr2) const -> AliasResult;

    /// Field-sensitive alias analysis
    [[nodiscard]] auto field_alias(ValueId ptr1, ValueId ptr2) const -> AliasResult;

    /// Check if two GEP-derived pointers may alias
    [[nodiscard]] auto gep_alias(const PointerInfo& info1, const PointerInfo& info2) const
        -> AliasResult;
};

/// Module-level alias analysis that tracks cross-function information
class ModuleAliasAnalysis : public MirPass {
public:
    [[nodiscard]] auto name() const -> std::string override {
        return "ModuleAliasAnalysis";
    }

    auto run(Module& module) -> bool override;

    /// Query alias for a specific function
    [[nodiscard]] auto query_in_function(const std::string& func_name, const MemoryLocation& loc1,
                                         const MemoryLocation& loc2) const -> AliasResult;

private:
    /// Per-function alias analysis results
    std::unordered_map<std::string, std::unique_ptr<AliasAnalysisPass>> function_analyses_;
};

} // namespace tml::mir
