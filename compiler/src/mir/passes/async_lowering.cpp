TML_MODULE("compiler")

//! # Async Lowering Pass
//!
//! Transforms async functions into state machines for suspension/resume.
//!
//! ## State Machine Structure
//!
//! ```text
//! struct FuncName_state {
//!     _state: I32,        // Current state index
//!     saved_local_1: T1,  // Locals live across await
//!     saved_local_2: T2,
//!     ...
//! }
//! ```
//!
//! ## Await Point Analysis
//!
//! For each `await` instruction:
//! 1. Record block before/after suspension
//! 2. Identify values live across the await
//! 3. Add those values to saved locals
//!
//! ## Generated Poll Function
//!
//! ```text
//! func poll(state: *FuncName_state) -> Poll[T] {
//!     switch state._state {
//!         0 => { /* initial code */ }
//!         1 => { /* after first await */ }
//!         ...
//!     }
//! }
//! ```
//!
//! ## Return Types
//!
//! - `Poll::Ready(value)` - computation complete
//! - `Poll::Pending` - needs to be polled again

#include "mir/passes/async_lowering.hpp"

#include <algorithm>
#include <iostream>

namespace tml::mir {

// ============================================================================
// Helper Functions
// ============================================================================

auto needs_async_lowering(const Function& func) -> bool {
    if (!func.is_async) {
        return false;
    }

    // Check if function has any await instructions
    for (const auto& block : func.blocks) {
        for (const auto& inst_data : block.instructions) {
            if (std::holds_alternative<AwaitInst>(inst_data.inst)) {
                return true;
            }
        }
    }
    return false;
}

auto get_poll_inner_type(const MirTypePtr& poll_type) -> MirTypePtr {
    if (!poll_type) {
        return nullptr;
    }

    if (auto* enum_type = std::get_if<MirEnumType>(&poll_type->kind)) {
        if (enum_type->name == "Poll" && !enum_type->type_args.empty()) {
            return enum_type->type_args[0];
        }
    }

    return nullptr;
}

auto make_poll_type(const MirTypePtr& inner_type) -> MirTypePtr {
    auto poll = std::make_shared<MirType>();
    poll->kind = MirEnumType{"Poll", {inner_type}};
    return poll;
}

// ============================================================================
// Async Analysis
// ============================================================================

AsyncAnalysis::AsyncAnalysis(const Function& func) : func_(func) {}

void AsyncAnalysis::analyze() {
    find_suspensions();
    if (!suspensions_.empty()) {
        analyze_saved_locals();
    }
}

auto AsyncAnalysis::suspension_points() const -> const std::vector<SuspensionPoint>& {
    return suspensions_;
}

auto AsyncAnalysis::saved_locals() const -> const std::vector<SavedLocal>& {
    return saved_locals_;
}

auto AsyncAnalysis::has_suspensions() const -> bool {
    return !suspensions_.empty();
}

void AsyncAnalysis::find_suspensions() {
    uint32_t suspension_id = 0;

    for (const auto& block : func_.blocks) {
        for (size_t i = 0; i < block.instructions.size(); ++i) {
            const auto& inst_data = block.instructions[i];

            if (auto* await = std::get_if<AwaitInst>(&inst_data.inst)) {
                SuspensionPoint sp;
                sp.id = suspension_id++;
                sp.block_before = block.id;
                sp.block_after = block.id; // Will be split during transformation
                sp.awaited_value = await->poll_value.id;
                sp.result_value = inst_data.result;
                sp.result_type = await->result_type;
                sp.span = inst_data.span;

                suspensions_.push_back(sp);
            }
        }
    }
}

void AsyncAnalysis::analyze_saved_locals() {
    // Collect all defined values and their defining blocks
    std::unordered_map<ValueId, uint32_t> def_block;
    std::unordered_map<ValueId, std::string> value_names;
    std::unordered_map<ValueId, MirTypePtr> value_types;

    // Parameters are defined in the entry block
    for (const auto& param : func_.params) {
        def_block[param.value_id] = 0;
        value_names[param.value_id] = param.name;
        value_types[param.value_id] = param.type;
    }

    // Collect definitions from instructions
    for (const auto& block : func_.blocks) {
        for (const auto& inst_data : block.instructions) {
            if (inst_data.result != INVALID_VALUE) {
                def_block[inst_data.result] = block.id;
                value_types[inst_data.result] = inst_data.type;

                // Try to get name from AllocaInst
                if (auto* alloca = std::get_if<AllocaInst>(&inst_data.inst)) {
                    value_names[inst_data.result] = alloca->name;
                }
            }
        }
    }

    // Collect all uses
    std::unordered_map<ValueId, std::vector<uint32_t>> use_blocks;

    auto record_use = [&](ValueId val, uint32_t block_id) {
        if (val != INVALID_VALUE) {
            use_blocks[val].push_back(block_id);
        }
    };

    for (const auto& block : func_.blocks) {
        for (const auto& inst_data : block.instructions) {
            std::visit(
                [&](const auto& inst) {
                    using T = std::decay_t<decltype(inst)>;

                    if constexpr (std::is_same_v<T, BinaryInst>) {
                        record_use(inst.left.id, block.id);
                        record_use(inst.right.id, block.id);
                    } else if constexpr (std::is_same_v<T, UnaryInst>) {
                        record_use(inst.operand.id, block.id);
                    } else if constexpr (std::is_same_v<T, LoadInst>) {
                        record_use(inst.ptr.id, block.id);
                    } else if constexpr (std::is_same_v<T, StoreInst>) {
                        record_use(inst.ptr.id, block.id);
                        record_use(inst.value.id, block.id);
                    } else if constexpr (std::is_same_v<T, GetElementPtrInst>) {
                        record_use(inst.base.id, block.id);
                        for (const auto& idx : inst.indices) {
                            record_use(idx.id, block.id);
                        }
                    } else if constexpr (std::is_same_v<T, ExtractValueInst>) {
                        record_use(inst.aggregate.id, block.id);
                    } else if constexpr (std::is_same_v<T, InsertValueInst>) {
                        record_use(inst.aggregate.id, block.id);
                        record_use(inst.value.id, block.id);
                    } else if constexpr (std::is_same_v<T, CallInst>) {
                        for (const auto& arg : inst.args) {
                            record_use(arg.id, block.id);
                        }
                    } else if constexpr (std::is_same_v<T, MethodCallInst>) {
                        record_use(inst.receiver.id, block.id);
                        for (const auto& arg : inst.args) {
                            record_use(arg.id, block.id);
                        }
                    } else if constexpr (std::is_same_v<T, CastInst>) {
                        record_use(inst.operand.id, block.id);
                    } else if constexpr (std::is_same_v<T, PhiInst>) {
                        for (const auto& [val, _] : inst.incoming) {
                            record_use(val.id, block.id);
                        }
                    } else if constexpr (std::is_same_v<T, SelectInst>) {
                        record_use(inst.condition.id, block.id);
                        record_use(inst.true_val.id, block.id);
                        record_use(inst.false_val.id, block.id);
                    } else if constexpr (std::is_same_v<T, StructInitInst>) {
                        for (const auto& field : inst.fields) {
                            record_use(field.id, block.id);
                        }
                    } else if constexpr (std::is_same_v<T, EnumInitInst>) {
                        for (const auto& p : inst.payload) {
                            record_use(p.id, block.id);
                        }
                    } else if constexpr (std::is_same_v<T, TupleInitInst>) {
                        for (const auto& elem : inst.elements) {
                            record_use(elem.id, block.id);
                        }
                    } else if constexpr (std::is_same_v<T, ArrayInitInst>) {
                        for (const auto& elem : inst.elements) {
                            record_use(elem.id, block.id);
                        }
                    } else if constexpr (std::is_same_v<T, AwaitInst>) {
                        record_use(inst.poll_value.id, block.id);
                    } else if constexpr (std::is_same_v<T, ClosureInitInst>) {
                        for (const auto& cap : inst.captures) {
                            record_use(cap.second.id, block.id);
                        }
                    }
                },
                inst_data.inst);
        }

        // Check terminator for uses
        if (block.terminator) {
            std::visit(
                [&](const auto& term) {
                    using T = std::decay_t<decltype(term)>;
                    if constexpr (std::is_same_v<T, ReturnTerm>) {
                        if (term.value) {
                            record_use(term.value->id, block.id);
                        }
                    } else if constexpr (std::is_same_v<T, CondBranchTerm>) {
                        record_use(term.condition.id, block.id);
                    } else if constexpr (std::is_same_v<T, SwitchTerm>) {
                        record_use(term.discriminant.id, block.id);
                    }
                },
                *block.terminator);
        }
    }

    // For each value, check if it's defined before a suspension and used after
    for (const auto& [value_id, defining_block] : def_block) {
        std::vector<uint32_t> live_at_suspensions;

        for (const auto& sp : suspensions_) {
            // Value is defined before this suspension?
            bool defined_before = defining_block <= sp.block_before;

            // Value is used after this suspension?
            bool used_after = false;
            if (use_blocks.count(value_id)) {
                for (uint32_t use_block : use_blocks.at(value_id)) {
                    if (use_block > sp.block_before) {
                        used_after = true;
                        break;
                    }
                }
            }

            if (defined_before && used_after) {
                live_at_suspensions.push_back(sp.id);
            }
        }

        if (!live_at_suspensions.empty()) {
            SavedLocal saved;
            saved.name = value_names.count(value_id) ? value_names.at(value_id)
                                                     : "_v" + std::to_string(value_id);
            saved.value_id = value_id;
            saved.type = value_types.count(value_id) ? value_types.at(value_id) : make_i64_type();
            saved.live_at = std::move(live_at_suspensions);

            saved_locals_.push_back(saved);
        }
    }
}

auto AsyncAnalysis::is_live_at(ValueId /*value*/, uint32_t /*block_id*/) const -> bool {
    // A value is live at a block if it's defined before and used after
    // This is a simplified check
    return true; // Placeholder
}

auto AsyncAnalysis::get_defs_before(uint32_t block_id) const -> std::set<ValueId> {
    std::set<ValueId> defs;
    for (const auto& block : func_.blocks) {
        if (block.id >= block_id) {
            break;
        }
        for (const auto& inst : block.instructions) {
            if (inst.result != INVALID_VALUE) {
                defs.insert(inst.result);
            }
        }
    }
    return defs;
}

auto AsyncAnalysis::get_uses_after(uint32_t /*block_id*/) const -> std::set<ValueId> {
    std::set<ValueId> uses;
    // Placeholder - would need full use analysis
    return uses;
}

// ============================================================================
// Async Lowering Pass
// ============================================================================

auto AsyncLoweringPass::run_on_function(Function& func) -> bool {
    if (!func.is_async) {
        return false;
    }

    AsyncAnalysis analysis(func);
    analysis.analyze();

    if (!analysis.has_suspensions()) {
        // No suspension points - async function always returns Ready
        // This is already handled by the existing codegen
        return false;
    }

    // Build the state machine metadata
    AsyncStateMachine sm;
    sm.state_struct_name = func.name + "_state";
    sm.suspensions = analysis.suspension_points();
    sm.saved_locals = analysis.saved_locals();
    sm.inner_return_type = func.return_type;
    sm.poll_return_type = make_poll_type(func.return_type);

    func.state_machine = std::move(sm);

    // Note: The actual code transformation happens during LLVM IR generation
    // This pass only populates the metadata needed for that transformation

    return true;
}

auto AsyncLoweringPass::generate_state_struct(const Function& func, const AsyncAnalysis& analysis)
    -> StructDef {
    StructDef state_struct;
    state_struct.name = func.name + "_state";

    // Field 0: state index (i32)
    state_struct.fields.push_back(StructField{"_state", make_i32_type()});

    // Fields for saved locals
    for (const auto& local : analysis.saved_locals()) {
        state_struct.fields.push_back(StructField{local.name, local.type});
    }

    return state_struct;
}

void AsyncLoweringPass::transform_to_state_machine(Function& /*func*/,
                                                   const AsyncAnalysis& /*analysis*/) {
    // This would perform the actual MIR transformation
    // For now, we just set up the metadata and let codegen handle it
}

void AsyncLoweringPass::generate_state_blocks(Function& /*func*/,
                                              const AsyncAnalysis& /*analysis*/) {
    // Would generate separate blocks for each state
}

void AsyncLoweringPass::generate_await_handler(Function& /*func*/, const SuspensionPoint& /*sp*/,
                                               uint32_t /*state_ptr*/) {
    // Would generate the poll check and branch
}

} // namespace tml::mir
