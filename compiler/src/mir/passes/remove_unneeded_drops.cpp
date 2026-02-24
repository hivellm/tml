TML_MODULE("compiler")

//! # Remove Unneeded Drops Pass
//!
//! Eliminates drop calls for types that don't need dropping.
//! Equivalent to Rust's `RemoveUnneededDrops` at mir-opt-level=1.

#include "mir/passes/remove_unneeded_drops.hpp"

#include <algorithm>
#include <set>

namespace tml::mir {

// Primitive types that never need dropping.
// These types have no destructor logic — drop calls for them are pure waste.
static const std::set<std::string> PRIMITIVE_TYPES = {
    "I8",   "I16", "I32", "I64",  "I128", "U8",   "U16",   "U32",   "U64",
    "U128", "F32", "F64", "Bool", "Char", "Unit", "ISize", "USize",
};

auto RemoveUnneededDropsPass::is_drop_call(const std::string& func_name) -> bool {
    // Match patterns: "Type::drop", "Type_drop"
    if (func_name.size() < 5) {
        return false;
    }
    // Check for "::drop" suffix
    if (func_name.size() >= 6 && func_name.substr(func_name.size() - 6) == "::drop") {
        return true;
    }
    // Check for "_drop" suffix
    if (func_name.size() >= 5 && func_name.substr(func_name.size() - 5) == "_drop") {
        return true;
    }
    return false;
}

auto RemoveUnneededDropsPass::extract_drop_type(const std::string& func_name) -> std::string {
    // "Type::drop" -> "Type"
    auto pos = func_name.rfind("::drop");
    if (pos != std::string::npos) {
        return func_name.substr(0, pos);
    }
    // "Type_drop" -> "Type"
    pos = func_name.rfind("_drop");
    if (pos != std::string::npos) {
        return func_name.substr(0, pos);
    }
    return {};
}

auto RemoveUnneededDropsPass::type_needs_drop(const std::string& type_name) -> bool {
    // Primitives never need drop
    if (PRIMITIVE_TYPES.count(type_name) > 0) {
        return false;
    }

    // Str DOES need drop (heap-allocated)
    if (type_name == "Str") {
        return true;
    }

    // Generic types: check if it's a known non-droppable wrapper
    // Tuples of primitives don't need drop, but we can't determine tuple
    // contents from the name alone, so be conservative and keep them.

    // Without TypeEnv, be conservative — assume unknown types need drop
    return true;
}

auto RemoveUnneededDropsPass::run_on_function(Function& func) -> bool {
    bool changed = false;

    for (auto& block : func.blocks) {
        std::vector<size_t> to_remove;

        for (size_t i = 0; i < block.instructions.size(); ++i) {
            auto& inst = block.instructions[i];

            // Check if this is a CallInst that's a drop call
            auto* call = std::get_if<CallInst>(&inst.inst);
            if (!call) {
                continue;
            }

            if (!is_drop_call(call->func_name)) {
                continue;
            }

            // Extract the type being dropped
            std::string drop_type = extract_drop_type(call->func_name);
            if (drop_type.empty()) {
                continue;
            }

            // If the type doesn't need dropping, remove the call
            if (!type_needs_drop(drop_type)) {
                to_remove.push_back(i);
            }
        }

        // Remove in reverse order to preserve indices
        for (auto it = to_remove.rbegin(); it != to_remove.rend(); ++it) {
            block.instructions.erase(block.instructions.begin() + static_cast<std::ptrdiff_t>(*it));
            changed = true;
        }
    }

    return changed;
}

} // namespace tml::mir
