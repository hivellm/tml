// MIR Optimization Pass Infrastructure Implementation

#include "mir/mir_pass.hpp"

#include "mir/passes/common_subexpression_elimination.hpp"
#include "mir/passes/constant_folding.hpp"
#include "mir/passes/constant_propagation.hpp"
#include "mir/passes/copy_propagation.hpp"
#include "mir/passes/dead_code_elimination.hpp"
#include "mir/passes/unreachable_code_elimination.hpp"

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

    // O1 and above: basic optimizations
    add_pass(std::make_unique<ConstantFoldingPass>());
    add_pass(std::make_unique<ConstantPropagationPass>());

    if (level_ >= OptLevel::O2) {
        // O2 and above: standard optimizations
        add_pass(std::make_unique<CommonSubexpressionEliminationPass>());
        add_pass(std::make_unique<CopyPropagationPass>());
        add_pass(std::make_unique<DeadCodeEliminationPass>());
        add_pass(std::make_unique<UnreachableCodeEliminationPass>());
    }

    if (level_ >= OptLevel::O3) {
        // O3: aggressive optimizations (run passes again for more thorough optimization)
        add_pass(std::make_unique<ConstantFoldingPass>());
        add_pass(std::make_unique<ConstantPropagationPass>());
        add_pass(std::make_unique<DeadCodeEliminationPass>());
        // Future: add_pass(std::make_unique<InliningPass>());
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
