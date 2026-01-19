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

#include <iostream>

#include "mir/passes/adce.hpp"
#include "mir/passes/block_merge.hpp"
#include "mir/passes/common_subexpression_elimination.hpp"
#include "mir/passes/const_hoist.hpp"
#include "mir/passes/constant_folding.hpp"
#include "mir/passes/constant_propagation.hpp"
#include "mir/passes/copy_propagation.hpp"
#include "mir/passes/dead_arg_elim.hpp"
#include "mir/passes/dead_code_elimination.hpp"
#include "mir/passes/dead_function_elimination.hpp"
#include "mir/passes/early_cse.hpp"
#include "mir/passes/gvn.hpp"
#include "mir/passes/inlining.hpp"
#include "mir/passes/inst_simplify.hpp"
#include "mir/passes/jump_threading.hpp"
#include "mir/passes/licm.hpp"
#include "mir/passes/load_store_opt.hpp"
#include "mir/passes/loop_rotate.hpp"
#include "mir/passes/loop_unroll.hpp"
#include "mir/passes/match_simplify.hpp"
#include "mir/passes/mem2reg.hpp"
#include "mir/passes/merge_returns.hpp"
#include "mir/passes/narrowing.hpp"
#include "mir/passes/peephole.hpp"
#include "mir/passes/reassociate.hpp"
#include "mir/passes/simplify_cfg.hpp"
#include "mir/passes/simplify_select.hpp"
#include "mir/passes/sinking.hpp"
#include "mir/passes/sroa.hpp"
#include "mir/passes/strength_reduction.hpp"
#include "mir/passes/tail_call.hpp"
#include "mir/passes/unreachable_code_elimination.hpp"

// OOP optimization passes
#include "mir/passes/batch_destruction.hpp"
#include "mir/passes/builder_opt.hpp"
#include "mir/passes/constructor_fusion.hpp"
#include "mir/passes/dead_method_elimination.hpp"
#include "mir/passes/destructor_hoist.hpp"
#include "mir/passes/devirtualization.hpp"
#include "mir/passes/escape_analysis.hpp"
#include "types/env.hpp"

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

// MIR verification helper - checks for basic SSA validity
static bool verify_mir(const Module& module, const std::string& after_pass) {
    for (const auto& func : module.functions) {
        // Build set of all defined values
        std::unordered_set<ValueId> defined;

        // Parameters are defined at entry
        for (const auto& param : func.params) {
            defined.insert(param.value_id);
        }

        // Check each block
        for (const auto& block : func.blocks) {
            // Phi nodes can use values from predecessors (not checked here for simplicity)
            // but other instructions must use already-defined values

            for (const auto& inst : block.instructions) {
                // First, mark this instruction's result as defined
                if (inst.result != INVALID_VALUE) {
                    defined.insert(inst.result);
                }
            }
        }

        // Now check uses - for simplicity, just check that comparison operands exist
        for (const auto& block : func.blocks) {
            for (const auto& inst : block.instructions) {
                if (auto* bin = std::get_if<BinaryInst>(&inst.inst)) {
                    if (defined.find(bin->left.id) == defined.end()) {
                        std::cerr << "MIR VERIFICATION FAILED after " << after_pass
                                  << ": undefined value %" << bin->left.id
                                  << " used in " << func.name << " block " << block.id << "\n";
                        return false;
                    }
                    if (defined.find(bin->right.id) == defined.end()) {
                        std::cerr << "MIR VERIFICATION FAILED after " << after_pass
                                  << ": undefined value %" << bin->right.id
                                  << " used in " << func.name << " block " << block.id << "\n";
                        return false;
                    }
                }
            }
        }
    }
    return true;
}

auto PassManager::run(Module& module) -> int {
    int changes = 0;
    bool debug_mir = false; // Set to true to debug MIR passes

    for (auto& pass : passes_) {
        if (pass->run(module)) {
            ++changes;

            if (debug_mir) {
                if (!verify_mir(module, pass->name())) {
                    std::cerr << "Pass " << pass->name() << " corrupted MIR!\n";
                }
            }
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

    // Early CSE: catch simple redundancies before other passes
    add_pass(std::make_unique<EarlyCSEPass>());

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

        // Peephole optimizations (algebraic simplifications)
        add_pass(std::make_unique<PeepholePass>());

        // Simplify select instructions
        add_pass(std::make_unique<SimplifySelectPass>());

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

        // Merge basic blocks after CFG simplification
        add_pass(std::make_unique<BlockMergePass>());

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

        // Load-store optimization: eliminate redundant memory operations
        add_pass(std::make_unique<LoadStoreOptPass>());

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

        // Constant hoisting: move expensive constants out of loops
        add_pass(std::make_unique<ConstantHoistPass>());

        // Loop optimization
        add_pass(std::make_unique<LICMPass>());

        // Loop rotation: transform loops for better optimization
        add_pass(std::make_unique<LoopRotatePass>());

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

        // Final block merging
        add_pass(std::make_unique<BlockMergePass>());

        // Final dead function elimination
        add_pass(std::make_unique<DeadFunctionEliminationPass>());

        // Dead argument elimination (inter-procedural)
        add_pass(std::make_unique<DeadArgEliminationPass>());

        // Merge multiple returns into single exit
        add_pass(std::make_unique<MergeReturnsPass>());
    }
}

void PassManager::configure_standard_pipeline(types::TypeEnv& env) {
    // Clear existing passes - we'll build a custom pipeline with OOP optimizations
    // integrated at the right points for maximum effectiveness
    passes_.clear();

    if (level_ == OptLevel::O0) {
        return; // No optimizations
    }

    // ==========================================================================
    // Phase 1: Early Cleanup and Preparation
    // ==========================================================================
    add_pass(std::make_unique<EarlyCSEPass>());
    add_pass(std::make_unique<InstSimplifyPass>());
    add_pass(std::make_unique<ConstantFoldingPass>());
    add_pass(std::make_unique<ConstantPropagationPass>());
    add_pass(std::make_unique<SimplifyCfgPass>());
    add_pass(std::make_unique<DeadCodeEliminationPass>());

    if (level_ >= OptLevel::O2) {
        // ======================================================================
        // Phase 2: OOP Pre-Inlining Optimizations (CRITICAL ORDER!)
        // ======================================================================
        // These MUST run BEFORE inlining so devirtualized calls can be inlined

        // Dead method elimination: Remove unused virtual methods early
        add_pass(std::make_unique<DeadMethodEliminationPass>(env));

        // FIRST: Devirtualization - convert virtual calls to direct calls
        // This enables inlining to work on the devirtualized calls
        add_pass(std::make_unique<DevirtualizationPass>(env));

        // Cleanup after devirtualization
        add_pass(std::make_unique<DeadCodeEliminationPass>());

        // ======================================================================
        // Phase 3: Standard Optimizations with Inlining
        // ======================================================================
        add_pass(std::make_unique<SROAPass>());
        add_pass(std::make_unique<Mem2RegPass>());
        add_pass(std::make_unique<InstSimplifyPass>());
        add_pass(std::make_unique<PeepholePass>());
        add_pass(std::make_unique<SimplifySelectPass>());
        add_pass(std::make_unique<StrengthReductionPass>());
        add_pass(std::make_unique<ReassociatePass>());
        add_pass(std::make_unique<GVNPass>());
        add_pass(std::make_unique<CopyPropagationPass>());
        add_pass(std::make_unique<DeadCodeEliminationPass>());
        add_pass(std::make_unique<SimplifyCfgPass>());
        add_pass(std::make_unique<BlockMergePass>());
        add_pass(std::make_unique<MatchSimplifyPass>());

        // Inlining with OOP-aware options (higher bonus for devirtualized calls)
        InliningOptions inline_opts;
        inline_opts.optimization_level = 2;
        inline_opts.base_threshold = 75; // Slightly higher for OOP
        inline_opts.max_callee_size = 40;
        inline_opts.devirt_bonus = 100;
        inline_opts.devirt_exact_bonus = 150;
        inline_opts.devirt_sealed_bonus = 120;
        inline_opts.prioritize_devirt = true;
        inline_opts.constructor_bonus = 200;
        inline_opts.base_constructor_bonus = 250;
        inline_opts.prioritize_constructors = true;
        inline_opts.always_inline_single_expr = true;
        inline_opts.single_expr_max_size = 5;
        add_pass(std::make_unique<InliningPass>(inline_opts));

        // Post-inlining cleanup
        add_pass(std::make_unique<SimplifyCfgPass>());
        add_pass(std::make_unique<Mem2RegPass>());
        add_pass(std::make_unique<InstSimplifyPass>());
        add_pass(std::make_unique<ConstantFoldingPass>());
        add_pass(std::make_unique<ConstantPropagationPass>());
        add_pass(std::make_unique<GVNPass>());
        add_pass(std::make_unique<DeadCodeEliminationPass>());

        // ======================================================================
        // Phase 4: Post-Inlining OOP Optimizations
        // ======================================================================

        // Second devirtualization pass - inlining may expose new opportunities
        add_pass(std::make_unique<DevirtualizationPass>(env));

        // Constructor fusion: Merge field stores after inlining
        add_pass(std::make_unique<ConstructorFusionPass>(env));

        // Builder pattern optimization: Optimize method chaining
        add_pass(std::make_unique<BuilderOptPass>());

        // Escape analysis + stack promotion: Promote non-escaping objects
        add_pass(std::make_unique<EscapeAndPromotePass>());

        // Destructor hoisting: Move destructors for better scheduling
        add_pass(std::make_unique<DestructorHoistPass>(env));

        // Batch destruction: Combine multiple destructor calls
        add_pass(std::make_unique<BatchDestructionPass>(env));

        // Cleanup after OOP optimizations
        add_pass(std::make_unique<DeadCodeEliminationPass>());
        add_pass(std::make_unique<SimplifyCfgPass>());

        // ======================================================================
        // Phase 5: Final Standard Optimizations
        // ======================================================================
        add_pass(std::make_unique<LoadStoreOptPass>());
        add_pass(std::make_unique<LICMPass>());
        add_pass(std::make_unique<JumpThreadingPass>());
        add_pass(std::make_unique<SimplifyCfgPass>());
        add_pass(std::make_unique<TailCallPass>());
        add_pass(std::make_unique<UnreachableCodeEliminationPass>());
        add_pass(std::make_unique<DeadFunctionEliminationPass>());
    }

    if (level_ >= OptLevel::O3) {
        // ======================================================================
        // O3: Aggressive Optimizations
        // ======================================================================

        // Third round of devirtualization after aggressive inlining
        add_pass(std::make_unique<DevirtualizationPass>(env));

        // More aggressive dead method elimination
        add_pass(std::make_unique<DeadMethodEliminationPass>(env));

        add_pass(std::make_unique<SROAPass>());
        add_pass(std::make_unique<Mem2RegPass>());
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
        inline_opts_o3.base_threshold = 150;
        inline_opts_o3.max_callee_size = 75;
        inline_opts_o3.devirt_bonus = 150;
        inline_opts_o3.devirt_exact_bonus = 200;
        inline_opts_o3.devirt_sealed_bonus = 180;
        inline_opts_o3.prioritize_devirt = true;
        inline_opts_o3.constructor_bonus = 300;
        inline_opts_o3.base_constructor_bonus = 350;
        inline_opts_o3.prioritize_constructors = true;
        inline_opts_o3.always_inline_single_expr = true;
        inline_opts_o3.single_expr_max_size = 8;
        add_pass(std::make_unique<InliningPass>(inline_opts_o3));

        // Post-aggressive-inlining cleanup
        add_pass(std::make_unique<SimplifyCfgPass>());
        add_pass(std::make_unique<Mem2RegPass>());
        add_pass(std::make_unique<InstSimplifyPass>());
        add_pass(std::make_unique<ConstantFoldingPass>());
        add_pass(std::make_unique<ConstantPropagationPass>());
        add_pass(std::make_unique<CopyPropagationPass>());
        add_pass(std::make_unique<GVNPass>());
        add_pass(std::make_unique<DeadCodeEliminationPass>());

        // Final OOP optimization round at O3
        // Note: EscapeAndPromotePass runs only once (earlier) to avoid duplicate allocas
        add_pass(std::make_unique<DevirtualizationPass>(env));
        add_pass(std::make_unique<ConstructorFusionPass>(env));
        add_pass(std::make_unique<BuilderOptPass>());
        add_pass(std::make_unique<DestructorHoistPass>(env));
        add_pass(std::make_unique<BatchDestructionPass>(env));

        // Aggressive final optimizations
        add_pass(std::make_unique<NarrowingPass>());
        add_pass(std::make_unique<ConstantHoistPass>());
        add_pass(std::make_unique<LICMPass>());
        add_pass(std::make_unique<LoopRotatePass>());
        add_pass(std::make_unique<LoopUnrollPass>());
        add_pass(std::make_unique<SinkingPass>());
        add_pass(std::make_unique<MatchSimplifyPass>());
        add_pass(std::make_unique<JumpThreadingPass>());
        add_pass(std::make_unique<SimplifyCfgPass>());
        add_pass(std::make_unique<TailCallPass>());
        add_pass(std::make_unique<ADCEPass>());
        add_pass(std::make_unique<BlockMergePass>());
        add_pass(std::make_unique<DeadFunctionEliminationPass>());
        add_pass(std::make_unique<DeadArgEliminationPass>());
        add_pass(std::make_unique<MergeReturnsPass>());
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
