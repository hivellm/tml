//! # Scalar Replacement of Aggregates (SROA) Pass
//!
//! Breaks up alloca of aggregates (structs, tuples, arrays) into multiple
//! scalar allocas when the aggregate is only accessed field-by-field.
//!
//! ## Example
//!
//! Before:
//! ```
//! %point = alloca Point         // {x: i32, y: i32}
//! %gep_x = getelementptr %point, 0, 0
//! store i32 10, %gep_x
//! %gep_y = getelementptr %point, 0, 1
//! store i32 20, %gep_y
//! ```
//!
//! After:
//! ```
//! %point_x = alloca i32
//! %point_y = alloca i32
//! store i32 10, %point_x
//! store i32 20, %point_y
//! ```
//!
//! ## Benefits
//!
//! - Enables mem2reg to promote fields to SSA values
//! - Reduces memory traffic
//! - Enables further optimizations (constant propagation, DCE)

#pragma once

#include "mir/mir.hpp"
#include "mir/mir_pass.hpp"

#include <unordered_map>
#include <unordered_set>

namespace tml::mir {

class SROAPass : public FunctionPass {
public:
    [[nodiscard]] auto name() const -> std::string override {
        return "SROA";
    }

protected:
    auto run_on_function(Function& func) -> bool override;

private:
    // Information about an alloca candidate
    struct AllocaInfo {
        ValueId alloca_id;
        MirTypePtr alloc_type;
        std::string name;
        size_t inst_index;
        size_t block_index;
        bool can_split = true;

        // For struct types: field indices that are accessed
        std::unordered_set<uint32_t> accessed_fields;

        // For array types: element indices that are accessed (empty = all accessed dynamically)
        std::unordered_set<size_t> accessed_elements;
        bool has_dynamic_access = false;
    };

    // Replacement mapping: old GEP result -> new alloca
    struct SplitAlloca {
        ValueId new_alloca_id;
        MirTypePtr field_type;
        std::string name;
    };

    // Analyze an alloca to see if it's a candidate for SROA
    auto analyze_alloca(Function& func, ValueId alloca_id, const AllocaInst& alloca,
                        size_t block_idx, size_t inst_idx) -> AllocaInfo;

    // Check if an alloca is only accessed through simple GEPs
    auto is_simple_access(const Function& func, ValueId alloca_id, AllocaInfo& info) -> bool;

    // Check if a type can be split (struct, tuple, or small array)
    auto can_split_type(const MirTypePtr& type) -> bool;

    // Get the number of fields/elements in a splittable type
    auto get_field_count(const MirTypePtr& type, const Function& func) -> size_t;

    // Get field type for a struct/tuple at given index
    auto get_field_type(const MirTypePtr& type, uint32_t index, const Function& func) -> MirTypePtr;

    // Split an alloca into multiple scalar allocas
    auto split_alloca(Function& func, const AllocaInfo& info)
        -> std::unordered_map<uint32_t, SplitAlloca>;

    // Rewrite uses of the original alloca to use split allocas
    auto rewrite_uses(Function& func, const AllocaInfo& info,
                      const std::unordered_map<uint32_t, SplitAlloca>& splits) -> void;

    // Remove the original alloca and dead GEPs
    auto cleanup(Function& func, const AllocaInfo& info,
                 const std::unordered_set<ValueId>& dead_values) -> void;

    // Cache of struct definitions for field lookup
    std::unordered_map<std::string, const StructDef*> struct_cache_;
};

} // namespace tml::mir
