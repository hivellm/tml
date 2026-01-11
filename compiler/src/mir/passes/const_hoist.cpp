//! # Constant Hoisting Pass
//!
//! Moves expensive constant materialization out of loops.

#include "mir/passes/const_hoist.hpp"

#include <algorithm>
#include <queue>

namespace tml::mir {

auto ConstantHoistPass::run_on_function(Function& func) -> bool {
    bool changed = false;

    auto loops = find_loops(func);

    for (auto& loop : loops) {
        loop.preheader = find_preheader(func, loop);
        if (loop.preheader != 0) {
            changed |= hoist_constants(func, loop);
        }
    }

    return changed;
}

auto ConstantHoistPass::find_loops(const Function& func) -> std::vector<LoopInfo> {
    std::vector<LoopInfo> loops;

    // Find back edges to detect loops
    for (size_t i = 0; i < func.blocks.size(); ++i) {
        const auto& block = func.blocks[i];

        for (uint32_t succ : block.successors) {
            int succ_idx = get_block_index(func, succ);
            if (succ_idx < 0)
                continue;

            // Back edge: successor dominates current block
            if (static_cast<size_t>(succ_idx) <= i) {
                LoopInfo loop;
                loop.header = succ;

                // Collect loop blocks via reverse traversal
                std::unordered_set<uint32_t> visited;
                std::queue<uint32_t> worklist;

                visited.insert(loop.header);
                worklist.push(block.id);

                while (!worklist.empty()) {
                    uint32_t curr = worklist.front();
                    worklist.pop();

                    if (visited.count(curr))
                        continue;
                    visited.insert(curr);

                    int curr_idx = get_block_index(func, curr);
                    if (curr_idx >= 0) {
                        for (uint32_t pred :
                             func.blocks[static_cast<size_t>(curr_idx)].predecessors) {
                            if (!visited.count(pred)) {
                                worklist.push(pred);
                            }
                        }
                    }
                }

                loop.blocks = std::move(visited);
                loops.push_back(std::move(loop));
            }
        }
    }

    return loops;
}

auto ConstantHoistPass::find_preheader(const Function& func, const LoopInfo& loop) -> uint32_t {
    int header_idx = get_block_index(func, loop.header);
    if (header_idx < 0)
        return 0;

    const auto& header = func.blocks[static_cast<size_t>(header_idx)];

    // Find a predecessor that's not in the loop
    for (uint32_t pred : header.predecessors) {
        if (loop.blocks.count(pred) == 0) {
            return pred;
        }
    }

    return 0;
}

auto ConstantHoistPass::hoist_constants(Function& func, const LoopInfo& loop) -> bool {
    bool changed = false;

    int preheader_idx = get_block_index(func, loop.preheader);
    if (preheader_idx < 0)
        return false;

    auto& preheader = func.blocks[static_cast<size_t>(preheader_idx)];

    // Find expensive constants in loop blocks
    std::unordered_map<std::string, ValueId> hoisted_constants;

    for (uint32_t block_id : loop.blocks) {
        int block_idx = get_block_index(func, block_id);
        if (block_idx < 0)
            continue;

        auto& block = func.blocks[static_cast<size_t>(block_idx)];
        std::vector<size_t> to_remove;

        for (size_t i = 0; i < block.instructions.size(); ++i) {
            auto& inst = block.instructions[i];

            if (auto* c = std::get_if<ConstantInst>(&inst.inst)) {
                if (is_expensive_constant(*c)) {
                    // Create a key for this constant
                    std::string key = std::visit(
                        [](const auto& v) -> std::string {
                            using T = std::decay_t<decltype(v)>;
                            if constexpr (std::is_same_v<T, int64_t>) {
                                return "i64_" + std::to_string(v);
                            } else if constexpr (std::is_same_v<T, uint64_t>) {
                                return "u64_" + std::to_string(v);
                            } else if constexpr (std::is_same_v<T, double>) {
                                return "f64_" + std::to_string(v);
                            } else if constexpr (std::is_same_v<T, bool>) {
                                return "bool_" + std::to_string(v);
                            } else if constexpr (std::is_same_v<T, std::string>) {
                                return "str_" + v;
                            } else if constexpr (std::is_same_v<T, char>) {
                                return "char_" + std::to_string(static_cast<int>(v));
                            } else if constexpr (std::is_same_v<T, std::monostate>) {
                                return "unit";
                            } else {
                                return "unknown";
                            }
                        },
                        c->value);

                    auto it = hoisted_constants.find(key);
                    if (it != hoisted_constants.end()) {
                        // Already hoisted - replace uses with hoisted value
                        ValueId old_val = inst.result;
                        ValueId new_val = it->second;

                        // Replace uses in this block
                        for (size_t j = i + 1; j < block.instructions.size(); ++j) {
                            std::visit(
                                [old_val, new_val](auto& inner) {
                                    using T = std::decay_t<decltype(inner)>;
                                    if constexpr (std::is_same_v<T, BinaryInst>) {
                                        if (inner.left.id == old_val)
                                            inner.left.id = new_val;
                                        if (inner.right.id == old_val)
                                            inner.right.id = new_val;
                                    } else if constexpr (std::is_same_v<T, UnaryInst>) {
                                        if (inner.operand.id == old_val)
                                            inner.operand.id = new_val;
                                    } else if constexpr (std::is_same_v<T, StoreInst>) {
                                        if (inner.value.id == old_val)
                                            inner.value.id = new_val;
                                    } else if constexpr (std::is_same_v<T, CallInst>) {
                                        for (auto& arg : inner.args) {
                                            if (arg.id == old_val)
                                                arg.id = new_val;
                                        }
                                    }
                                },
                                block.instructions[j].inst);
                        }

                        to_remove.push_back(i);
                        changed = true;
                    } else {
                        // Hoist this constant to preheader
                        InstructionData new_inst;
                        new_inst.result = func.next_value_id++;
                        new_inst.inst = *c;

                        // Insert at end of preheader (before terminator)
                        preheader.instructions.push_back(std::move(new_inst));

                        hoisted_constants[key] = preheader.instructions.back().result;

                        // Replace original with hoisted version
                        ValueId old_val = inst.result;
                        ValueId new_val = hoisted_constants[key];

                        for (size_t j = i + 1; j < block.instructions.size(); ++j) {
                            std::visit(
                                [old_val, new_val](auto& inner) {
                                    using T = std::decay_t<decltype(inner)>;
                                    if constexpr (std::is_same_v<T, BinaryInst>) {
                                        if (inner.left.id == old_val)
                                            inner.left.id = new_val;
                                        if (inner.right.id == old_val)
                                            inner.right.id = new_val;
                                    } else if constexpr (std::is_same_v<T, UnaryInst>) {
                                        if (inner.operand.id == old_val)
                                            inner.operand.id = new_val;
                                    } else if constexpr (std::is_same_v<T, StoreInst>) {
                                        if (inner.value.id == old_val)
                                            inner.value.id = new_val;
                                    } else if constexpr (std::is_same_v<T, CallInst>) {
                                        for (auto& arg : inner.args) {
                                            if (arg.id == old_val)
                                                arg.id = new_val;
                                        }
                                    }
                                },
                                block.instructions[j].inst);
                        }

                        to_remove.push_back(i);
                        changed = true;
                    }
                }
            }
        }

        // Remove hoisted constants from loop block
        std::sort(to_remove.begin(), to_remove.end(), std::greater<size_t>());
        for (size_t idx : to_remove) {
            block.instructions.erase(block.instructions.begin() + static_cast<std::ptrdiff_t>(idx));
        }
    }

    return changed;
}

auto ConstantHoistPass::is_expensive_constant(const ConstantInst& c) -> bool {
    // Consider constants expensive if they require multiple instructions
    // to materialize (large immediates, floats, strings)
    return std::visit(
        [](const auto& v) -> bool {
            using T = std::decay_t<decltype(v)>;
            if constexpr (std::is_same_v<T, int64_t>) {
                // Large integers are expensive
                return v > 0x7FFFFFFF || v < -0x80000000LL;
            } else if constexpr (std::is_same_v<T, uint64_t>) {
                return v > 0xFFFFFFFF;
            } else if constexpr (std::is_same_v<T, double>) {
                // Floats often require memory loads
                return true;
            } else if constexpr (std::is_same_v<T, std::string>) {
                return true;
            } else {
                return false;
            }
        },
        c.value);
}

auto ConstantHoistPass::get_block_index(const Function& func, uint32_t id) -> int {
    for (size_t i = 0; i < func.blocks.size(); ++i) {
        if (func.blocks[i].id == id) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

} // namespace tml::mir
