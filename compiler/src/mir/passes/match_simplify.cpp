//! # Match Branch Simplification Pass
//!
//! Simplifies switch/match statements.

#include "mir/passes/match_simplify.hpp"

namespace tml::mir {

auto MatchSimplifyPass::run_on_function(Function& func) -> bool {
    bool changed = false;
    bool made_progress = true;

    while (made_progress) {
        made_progress = false;

        for (auto& block : func.blocks) {
            if (simplify_switch(func, block)) {
                made_progress = true;
                changed = true;
            }
        }
    }

    return changed;
}

auto MatchSimplifyPass::simplify_switch(Function& func, BasicBlock& block) -> bool {
    if (!block.terminator) {
        return false;
    }

    auto* switch_term = std::get_if<SwitchTerm>(&*block.terminator);
    if (!switch_term) {
        return false;
    }

    bool changed = false;

    // Try to fold constant discriminant
    if (fold_constant_switch(func, block, *switch_term)) {
        return true; // Terminator was replaced
    }

    // Remove cases that go to the default block
    if (remove_redundant_cases(*switch_term)) {
        changed = true;
    }

    // If no cases left, convert to unconditional branch
    if (switch_term->cases.empty()) {
        block.terminator = BranchTerm{switch_term->default_block};
        return true;
    }

    // If only one case, consider converting to conditional branch
    if (switch_term->cases.size() == 1) {
        if (convert_to_conditional(func, block, *switch_term)) {
            return true;
        }
    }

    return changed;
}

auto MatchSimplifyPass::blocks_equivalent(const Function& func, uint32_t block1_id,
                                           uint32_t block2_id) -> bool {
    if (block1_id == block2_id) {
        return true;
    }

    const BasicBlock* block1 = nullptr;
    const BasicBlock* block2 = nullptr;

    for (const auto& b : func.blocks) {
        if (b.id == block1_id) block1 = &b;
        if (b.id == block2_id) block2 = &b;
    }

    if (!block1 || !block2) {
        return false;
    }

    // Simple equivalence check: both blocks are empty with same terminator
    if (block1->instructions.empty() && block2->instructions.empty()) {
        if (block1->terminator && block2->terminator) {
            // Check if terminators are equivalent
            if (auto* br1 = std::get_if<BranchTerm>(&*block1->terminator)) {
                if (auto* br2 = std::get_if<BranchTerm>(&*block2->terminator)) {
                    return br1->target == br2->target;
                }
            }
            if (auto* ret1 = std::get_if<ReturnTerm>(&*block1->terminator)) {
                if (auto* ret2 = std::get_if<ReturnTerm>(&*block2->terminator)) {
                    // Both return same value or both return nothing
                    if (!ret1->value && !ret2->value) {
                        return true;
                    }
                    if (ret1->value && ret2->value) {
                        return ret1->value->id == ret2->value->id;
                    }
                }
            }
        }
    }

    return false;
}

auto MatchSimplifyPass::convert_to_conditional(Function& /*func*/, BasicBlock& block,
                                                SwitchTerm& switch_term) -> bool {
    if (switch_term.cases.size() != 1) {
        return false;
    }

    auto& [value, target_block] = switch_term.cases[0];

    // If the single case goes to the same place as default, just branch unconditionally
    if (target_block == switch_term.default_block) {
        block.terminator = BranchTerm{switch_term.default_block};
        return true;
    }

    // Create a comparison: discriminant == value
    // For simplicity, we keep the switch as-is if we'd need to create new instructions
    // A more complete implementation would create:
    //   %cmp = icmp eq %discriminant, value
    //   br i1 %cmp, label %target, label %default

    // For now, only convert if value == 0 (common pattern)
    if (value == 0) {
        // We'd need to create the comparison instruction
        // For now, leave as switch - the LLVM backend handles this well
        return false;
    }

    return false;
}

auto MatchSimplifyPass::remove_redundant_cases(SwitchTerm& switch_term) -> bool {
    bool changed = false;

    // Remove cases that go to the default block
    auto it = switch_term.cases.begin();
    while (it != switch_term.cases.end()) {
        if (it->second == switch_term.default_block) {
            it = switch_term.cases.erase(it);
            changed = true;
        } else {
            ++it;
        }
    }

    return changed;
}

auto MatchSimplifyPass::fold_constant_switch(Function& func, BasicBlock& block,
                                              SwitchTerm& switch_term) -> bool {
    auto const_val = get_constant_discriminant(func, switch_term.discriminant.id);
    if (!const_val) {
        return false;
    }

    // Find which case matches
    uint32_t target = switch_term.default_block;
    for (const auto& [case_val, case_block] : switch_term.cases) {
        if (case_val == *const_val) {
            target = case_block;
            break;
        }
    }

    // Replace switch with unconditional branch
    block.terminator = BranchTerm{target};
    return true;
}

auto MatchSimplifyPass::get_constant_discriminant(const Function& func, ValueId id)
    -> std::optional<int64_t> {
    for (const auto& block : func.blocks) {
        for (const auto& inst : block.instructions) {
            if (inst.result == id) {
                if (auto* ci = std::get_if<ConstantInst>(&inst.inst)) {
                    if (auto* int_val = std::get_if<ConstInt>(&ci->value)) {
                        return int_val->value;
                    }
                }
                return std::nullopt;
            }
        }
    }
    return std::nullopt;
}

} // namespace tml::mir
