TML_MODULE("compiler")

//! # Normalize Array Length Pass
//!
//! Replaces `array.len()` calls on fixed-size arrays with constant values.
//! Also normalizes `slice.len()` where the slice was created from a
//! known-size array.

#include "mir/passes/normalize_array_len.hpp"

#include <unordered_map>

namespace tml::mir {

auto NormalizeArrayLenPass::run_on_function(Function& func) -> bool {
    bool changed = false;

    // Step 1: Collect known array sizes from allocas and array init instructions.
    // Maps ValueId -> array size
    std::unordered_map<ValueId, int64_t> array_sizes;

    for (const auto& block : func.blocks) {
        for (const auto& inst : block.instructions) {
            if (auto* alloca = std::get_if<AllocaInst>(&inst.inst)) {
                if (alloca->alloc_type) {
                    if (auto* arr_type = std::get_if<MirArrayType>(&alloca->alloc_type->kind)) {
                        array_sizes[inst.result] = static_cast<int64_t>(arr_type->size);
                    }
                }
            } else if (auto* arr_init = std::get_if<ArrayInitInst>(&inst.inst)) {
                array_sizes[inst.result] = static_cast<int64_t>(arr_init->elements.size());

                // Also check the result_type for the array size
                if (arr_init->result_type) {
                    if (auto* arr_type = std::get_if<MirArrayType>(&arr_init->result_type->kind)) {
                        array_sizes[inst.result] = static_cast<int64_t>(arr_type->size);
                    }
                }
            }
        }
    }

    if (array_sizes.empty()) {
        return false;
    }

    // Step 2: Find method calls to "len" (or "length", "size") on known-size arrays
    // and replace them with constants.
    for (auto& block : func.blocks) {
        for (auto& inst : block.instructions) {
            if (auto* method = std::get_if<MethodCallInst>(&inst.inst)) {
                if (method->method_name != "len") {
                    continue;
                }

                // Check if receiver is a known-size array
                auto it = array_sizes.find(method->receiver.id);
                if (it == array_sizes.end()) {
                    continue;
                }

                int64_t size = it->second;

                // Replace the method call with a constant
                inst.inst = ConstantInst{ConstInt{size}};
                changed = true;
            }

            // Also check CallInst patterns like "Array_len", "Array[I32]_len"
            if (auto* call = std::get_if<CallInst>(&inst.inst)) {
                // Check for function names ending in "_len" or "::len"
                const auto& name = call->func_name;
                bool is_len_call = false;

                if (name.size() >= 4 && name.substr(name.size() - 4) == "_len") {
                    is_len_call = true;
                } else if (name.size() >= 5 && name.substr(name.size() - 5) == "::len") {
                    is_len_call = true;
                }

                if (!is_len_call || call->args.empty()) {
                    continue;
                }

                // The first argument is usually the array/slice
                auto it = array_sizes.find(call->args[0].id);
                if (it == array_sizes.end()) {
                    continue;
                }

                int64_t size = it->second;
                inst.inst = ConstantInst{ConstInt{size}};
                changed = true;
            }
        }
    }

    return changed;
}

} // namespace tml::mir
