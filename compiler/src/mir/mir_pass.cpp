//! # MIR Optimization Pass Infrastructure
//!
//! This file implements the pass infrastructure and pass manager.
//!
//! ## Pass Types
//!
//! - `FunctionPass::run()`: Iterates over functions
//! - `BlockPass::run()`: Iterates over all blocks
//!
//! ## PassManager
//!
//! Coordinates optimization passes with standard pipelines:
//!
//! | Level | Passes                                   |
//! |-------|------------------------------------------|
//! | O0    | (none)                                   |
//! | O1    | Constant folding/propagation             |
//! | O2    | O1 + CSE, copy prop, DCE, UCE            |
//! | O3    | O2 + second optimization round           |
//!
//! ## Analysis Utilities
//!
//! - `is_value_used()`: Check if value is referenced
//! - `has_side_effects()`: Check if instruction is pure
//! - `is_constant()`: Check for constant instructions
//! - `get_constant_int/bool()`: Extract constant values

#include "mir/mir_pass.hpp"

#include "mir/passes/common_subexpression_elimination.hpp"
#include "mir/passes/constant_folding.hpp"
#include "mir/passes/constant_propagation.hpp"
#include "mir/passes/copy_propagation.hpp"
#include "mir/passes/dead_code_elimination.hpp"
#include "mir/passes/dead_function_elimination.hpp"
#include "mir/passes/gvn.hpp"
#include "mir/passes/inlining.hpp"
#include "mir/passes/inst_simplify.hpp"
#include "mir/passes/jump_threading.hpp"
#include "mir/passes/licm.hpp"
#include "mir/passes/match_simplify.hpp"
#include "mir/passes/mem2reg.hpp"
#include "mir/passes/narrowing.hpp"
#include "mir/passes/reassociate.hpp"
#include "mir/passes/simplify_cfg.hpp"
#include "mir/passes/sroa.hpp"
#include "mir/passes/strength_reduction.hpp"
#include "mir/passes/tail_call.hpp"
#include "mir/passes/unreachable_code_elimination.hpp"
#include "mir/passes/loop_unroll.hpp"
#include "mir/passes/sinking.hpp"
#include "mir/passes/adce.hpp"

namespace tml::mir {

// ============================================================================
// FunctionPass Implementation
// ============================================================================

auto FunctionPass::run(Module& module) -> bool {
    bool changed = false;
    for (auto& func : module.functions) {
        changed |= run_on_function(func);
    }
    return changed;
}

// ============================================================================
// BlockPass Implementation
// ============================================================================

auto BlockPass::run(Module& module) -> bool {
    bool changed = false;
    for (auto& func : module.functions) {
        for (auto& block : func.blocks) {
            changed |= run_on_block(block, func);
        }
    }
    return changed;
}

// ============================================================================
// PassManager Implementation
// ============================================================================

PassManager::PassManager(OptLevel level) : level_(level) {}

void PassManager::add_pass(std::unique_ptr<MirPass> pass) {
    passes_.push_back(std::move(pass));
}

auto PassManager::run(Module& module) -> int {
    int changes = 0;
    for (auto& pass : passes_) {
        if (pass->run(module)) {
            ++changes;
        }
    }
    return changes;
}

void PassManager::configure_standard_pipeline() {
    // Clear existing passes
    passes_.clear();

    if (level_ == OptLevel::O0) {
        // No optimizations
        return;
    }

    // ==========================================================================
    // O1 and above: basic optimizations
    // ==========================================================================

    // Early instruction simplification (peephole)
    add_pass(std::make_unique<InstSimplifyPass>());

    // Constant folding and propagation
    add_pass(std::make_unique<ConstantFoldingPass>());
    add_pass(std::make_unique<ConstantPropagationPass>());

    // Simplify CFG after constant propagation may have created dead branches
    add_pass(std::make_unique<SimplifyCfgPass>());

    // Dead code elimination
    add_pass(std::make_unique<DeadCodeEliminationPass>());

    if (level_ >= OptLevel::O2) {
        // ======================================================================
        // O2 and above: standard optimizations (similar to Rust's pipeline)
        // ======================================================================

        // SROA: Break up aggregate allocas early for better optimization
        add_pass(std::make_unique<SROAPass>());

        // Mem2Reg: Promote allocas to SSA registers
        add_pass(std::make_unique<Mem2RegPass>());

        // More instruction simplification after initial optimizations
        add_pass(std::make_unique<InstSimplifyPass>());

        // Strength reduction (mul by power of 2 -> shift, etc.)
        add_pass(std::make_unique<StrengthReductionPass>());

        // Reassociate: Reorder associative operations for optimization
        add_pass(std::make_unique<ReassociatePass>());

        // GVN instead of just local CSE (works across blocks)
        add_pass(std::make_unique<GVNPass>());
        add_pass(std::make_unique<CopyPropagationPass>());

        // Cleanup after GVN/CopyProp
        add_pass(std::make_unique<DeadCodeEliminationPass>());
        add_pass(std::make_unique<SimplifyCfgPass>());

        // Match/switch simplification
        add_pass(std::make_unique<MatchSimplifyPass>());

        // Inlining at O2 with conservative thresholds
        InliningOptions inline_opts;
        inline_opts.optimization_level = 2;
        inline_opts.base_threshold = 50;
        inline_opts.max_callee_size = 30;
        add_pass(std::make_unique<InliningPass>(inline_opts));

        // Post-inlining cleanup (critical for removing blocks created by inlining)
        add_pass(std::make_unique<SimplifyCfgPass>());
        add_pass(std::make_unique<Mem2RegPass>());
        add_pass(std::make_unique<InstSimplifyPass>());
        add_pass(std::make_unique<ConstantFoldingPass>());
        add_pass(std::make_unique<ConstantPropagationPass>());
        add_pass(std::make_unique<GVNPass>());
        add_pass(std::make_unique<DeadCodeEliminationPass>());

        // LICM: Move loop-invariant code out of loops
        add_pass(std::make_unique<LICMPass>());

        // Jump threading after CFG is simplified
        add_pass(std::make_unique<JumpThreadingPass>());
        add_pass(std::make_unique<SimplifyCfgPass>());

        // Tail call optimization
        add_pass(std::make_unique<TailCallPass>());

        // Unreachable code elimination
        add_pass(std::make_unique<UnreachableCodeEliminationPass>());

        // Remove dead functions after inlining
        add_pass(std::make_unique<DeadFunctionEliminationPass>());
    }

    if (level_ >= OptLevel::O3) {
        // ======================================================================
        // O3: aggressive optimizations (run passes again)
        // ======================================================================

        // Second SROA pass after inlining may expose new opportunities
        add_pass(std::make_unique<SROAPass>());
        add_pass(std::make_unique<Mem2RegPass>());

        // Second round of optimizations
        add_pass(std::make_unique<InstSimplifyPass>());
        add_pass(std::make_unique<StrengthReductionPass>());
        add_pass(std::make_unique<ReassociatePass>());
        add_pass(std::make_unique<ConstantFoldingPass>());
        add_pass(std::make_unique<ConstantPropagationPass>());
        add_pass(std::make_unique<SimplifyCfgPass>());
        add_pass(std::make_unique<DeadCodeEliminationPass>());

        // More aggressive inlining at O3
        InliningOptions inline_opts_o3;
        inline_opts_o3.optimization_level = 3;
        inline_opts_o3.base_threshold = 100;
        inline_opts_o3.max_callee_size = 50;
        add_pass(std::make_unique<InliningPass>(inline_opts_o3));

        // Final cleanup passes after aggressive inlining
        add_pass(std::make_unique<SimplifyCfgPass>());
        add_pass(std::make_unique<Mem2RegPass>());
        add_pass(std::make_unique<InstSimplifyPass>());
        add_pass(std::make_unique<ConstantFoldingPass>());
        add_pass(std::make_unique<ConstantPropagationPass>());
        add_pass(std::make_unique<CopyPropagationPass>());
        add_pass(std::make_unique<GVNPass>());
        add_pass(std::make_unique<DeadCodeEliminationPass>());

        // Narrowing: Use smaller types when safe
        add_pass(std::make_unique<NarrowingPass>());

        // Loop optimization
        add_pass(std::make_unique<LICMPass>());

        // Loop unrolling (small constant-bound loops)
        add_pass(std::make_unique<LoopUnrollPass>());

        // Code sinking: move computations closer to uses
        add_pass(std::make_unique<SinkingPass>());

        // Match simplification
        add_pass(std::make_unique<MatchSimplifyPass>());

        // Final jump threading
        add_pass(std::make_unique<JumpThreadingPass>());
        add_pass(std::make_unique<SimplifyCfgPass>());

        // Tail call optimization
        add_pass(std::make_unique<TailCallPass>());

        // Aggressive dead code elimination (final cleanup)
        add_pass(std::make_unique<ADCEPass>());

        // Final dead function elimination
        add_pass(std::make_unique<DeadFunctionEliminationPass>());
    }
}

// ============================================================================
// Analysis Utilities Implementation
// ============================================================================

auto is_value_used(const Function& func, ValueId value) -> bool {
    for (const auto& block : func.blocks) {
        for (const auto& inst : block.instructions) {
            // Check if any operand uses this value
            bool used = std::visit(
                [value](const auto& i) -> bool {
                    using T = std::decay_t<decltype(i)>;

                    if constexpr (std::is_same_v<T, BinaryInst>) {
                        return i.left.id == value || i.right.id == value;
                    } else if constexpr (std::is_same_v<T, UnaryInst>) {
                        return i.operand.id == value;
                    } else if constexpr (std::is_same_v<T, LoadInst>) {
                        return i.ptr.id == value;
                    } else if constexpr (std::is_same_v<T, StoreInst>) {
                        return i.ptr.id == value || i.value.id == value;
                    } else if constexpr (std::is_same_v<T, GetElementPtrInst>) {
                        if (i.base.id == value)
                            return true;
                        for (const auto& idx : i.indices) {
                            if (idx.id == value)
                                return true;
                        }
                        return false;
                    } else if constexpr (std::is_same_v<T, ExtractValueInst>) {
                        return i.aggregate.id == value;
                    } else if constexpr (std::is_same_v<T, InsertValueInst>) {
                        return i.aggregate.id == value || i.value.id == value;
                    } else if constexpr (std::is_same_v<T, CallInst>) {
                        for (const auto& arg : i.args) {
                            if (arg.id == value)
                                return true;
                        }
                        return false;
                    } else if constexpr (std::is_same_v<T, MethodCallInst>) {
                        if (i.receiver.id == value)
                            return true;
                        for (const auto& arg : i.args) {
                            if (arg.id == value)
                                return true;
                        }
                        return false;
                    } else if constexpr (std::is_same_v<T, CastInst>) {
                        return i.operand.id == value;
                    } else if constexpr (std::is_same_v<T, PhiInst>) {
                        for (const auto& [val, _] : i.incoming) {
                            if (val.id == value)
                                return true;
                        }
                        return false;
                    } else if constexpr (std::is_same_v<T, SelectInst>) {
                        return i.condition.id == value || i.true_val.id == value ||
                               i.false_val.id == value;
                    } else if constexpr (std::is_same_v<T, StructInitInst>) {
                        for (const auto& field : i.fields) {
                            if (field.id == value)
                                return true;
                        }
                        return false;
                    } else if constexpr (std::is_same_v<T, EnumInitInst>) {
                        for (const auto& p : i.payload) {
                            if (p.id == value)
                                return true;
                        }
                        return false;
                    } else if constexpr (std::is_same_v<T, TupleInitInst>) {
                        for (const auto& elem : i.elements) {
                            if (elem.id == value)
                                return true;
                        }
                        return false;
                    } else if constexpr (std::is_same_v<T, ArrayInitInst>) {
                        for (const auto& elem : i.elements) {
                            if (elem.id == value)
                                return true;
                        }
                        return false;
                    } else if constexpr (std::is_same_v<T, AwaitInst>) {
                        return i.poll_value.id == value;
                    } else if constexpr (std::is_same_v<T, ClosureInitInst>) {
                        for (const auto& cap : i.captures) {
                            if (cap.second.id == value)
                                return true;
                        }
                        return false;
                    } else {
                        return false;
                    }
                },
                inst.inst);

            if (used)
                return true;
        }

        // Check terminator
        if (block.terminator.has_value()) {
            bool used = std::visit(
                [value](const auto& t) -> bool {
                    using T = std::decay_t<decltype(t)>;

                    if constexpr (std::is_same_v<T, ReturnTerm>) {
                        return t.value.has_value() && t.value->id == value;
                    } else if constexpr (std::is_same_v<T, CondBranchTerm>) {
                        return t.condition.id == value;
                    } else if constexpr (std::is_same_v<T, SwitchTerm>) {
                        return t.discriminant.id == value;
                    } else {
                        return false;
                    }
                },
                *block.terminator);

            if (used)
                return true;
        }
    }

    return false;
}

auto has_side_effects(const Instruction& inst) -> bool {
    return std::visit(
        [](const auto& i) -> bool {
            using T = std::decay_t<decltype(i)>;

            // Instructions with side effects:
            // - Store: writes to memory
            // - Call: may have side effects
            // - MethodCall: may have side effects

            if constexpr (std::is_same_v<T, StoreInst>) {
                return true;
            } else if constexpr (std::is_same_v<T, CallInst>) {
                // Conservatively assume all calls have side effects
                // Could be refined with purity analysis
                return true;
            } else if constexpr (std::is_same_v<T, MethodCallInst>) {
                return true;
            } else {
                return false;
            }
        },
        inst);
}

auto is_constant(const Instruction& inst) -> bool {
    return std::holds_alternative<ConstantInst>(inst);
}

auto get_constant_int(const Instruction& inst) -> std::optional<int64_t> {
    if (auto* ci = std::get_if<ConstantInst>(&inst)) {
        if (auto* int_val = std::get_if<ConstInt>(&ci->value)) {
            return int_val->value;
        }
    }
    return std::nullopt;
}

auto get_constant_bool(const Instruction& inst) -> std::optional<bool> {
    if (auto* ci = std::get_if<ConstantInst>(&inst)) {
        if (auto* bool_val = std::get_if<ConstBool>(&ci->value)) {
            return bool_val->value;
        }
    }
    return std::nullopt;
}

} // namespace tml::mir
