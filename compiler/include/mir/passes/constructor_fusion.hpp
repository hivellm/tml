//! # Constructor Initialization Fusion Pass
//!
//! Fuses multiple stores to adjacent fields during object construction
//! into more efficient memory operations.
//!
//! ## Optimizations
//!
//! 1. **Store Fusion**: Multiple stores to adjacent fields are combined
//!    into a single memcpy or aggregate store when possible.
//!
//! 2. **Vtable Store Elimination**: Redundant vtable pointer stores
//!    are eliminated (e.g., when constructing derived class, the base
//!    vtable pointer is immediately overwritten).
//!
//! ## Example
//!
//! Before:
//! ```
//! %obj = call @Circle_create(5.0)
//! // stores: vtable, id=1, radius=5.0
//! ```
//!
//! After (with fusion):
//! ```
//! %obj = alloca %Circle
//! store %Circle { vtable.Circle, 1, 5.0 }, ptr %obj
//! ```

#pragma once

#include "mir/mir.hpp"
#include "mir/mir_pass.hpp"
#include "types/env.hpp"

#include <unordered_map>
#include <unordered_set>

namespace tml::mir {

/// Statistics for constructor fusion optimization.
struct ConstructorFusionStats {
    size_t constructors_analyzed = 0;    ///< Total constructors examined.
    size_t stores_fused = 0;             ///< Stores combined into aggregate init.
    size_t vtable_stores_eliminated = 0; ///< Redundant vtable stores removed.
    size_t base_constructor_inlined = 0; ///< Base constructors inlined.
};

/// Constructor initialization fusion pass.
class ConstructorFusionPass : public FunctionPass {
public:
    /// Creates a constructor fusion pass.
    explicit ConstructorFusionPass(types::TypeEnv& env) : env_(env) {}

    [[nodiscard]] auto name() const -> std::string override {
        return "ConstructorFusion";
    }

    /// Returns optimization statistics.
    [[nodiscard]] auto get_stats() const -> ConstructorFusionStats {
        return stats_;
    }

protected:
    auto run_on_function(Function& func) -> bool override;

private:
    types::TypeEnv& env_;
    ConstructorFusionStats stats_;

    /// Information about a store sequence to an object
    struct StoreSequence {
        ValueId object_ptr;                  ///< Base object pointer
        std::string class_name;              ///< Class being constructed
        std::vector<size_t> store_indices;   ///< Indices of store instructions
        std::vector<uint32_t> field_indices; ///< Fields being stored to
        std::vector<Value> values;           ///< Values being stored
        bool is_complete = false;            ///< All fields initialized?
    };

    /// Analyze a basic block for constructor patterns.
    auto analyze_block(Function& func, BasicBlock& block) -> std::vector<StoreSequence>;

    /// Check if a sequence of stores can be fused.
    auto can_fuse_stores(const StoreSequence& seq, const Function& func) -> bool;

    /// Fuse a sequence of stores into a struct initialization.
    auto fuse_stores(Function& func, BasicBlock& block, const StoreSequence& seq) -> bool;

    /// Detect and eliminate redundant vtable stores.
    auto eliminate_vtable_stores(Function& func, BasicBlock& block) -> bool;

    /// Check if an instruction is a vtable pointer store.
    auto is_vtable_store(const InstructionData& inst, ValueId obj_ptr) -> bool;

    /// Get the class name from an alloca or GEP instruction.
    auto get_class_name(const Function& func, ValueId ptr) -> std::optional<std::string>;
};

} // namespace tml::mir
