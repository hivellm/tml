//! # Dead Code Elimination (DCE) Pass
//!
//! This pass removes instructions whose results are never used.
//!
//! ## Algorithm
//!
//! 1. Find instructions with unused results
//! 2. Check if instruction has side effects
//! 3. Remove dead instructions
//! 4. Repeat until no changes (fixed-point)
//!
//! ## Pure Functions
//!
//! Calls to known pure functions can be eliminated if unused:
//! - Math: abs, sqrt, sin, cos, floor, ceil, etc.
//! - String: len, contains, trim, parse_int, etc.
//! - Collection: get, first, last, capacity
//! - Conversion: to_string, to_i32, to_f64, etc.
//!
//! ## Non-Removable Instructions
//!
//! | Instruction  | Reason                      |
//! |--------------|-----------------------------|
//! | Store        | Memory side effect          |
//! | Call         | Unknown side effects        |
//! | MethodCall   | Unknown side effects        |
//!
//! Pure calls can still be removed if their result is unused.

#include "mir/passes/dead_code_elimination.hpp"

#include <algorithm>
#include <unordered_set>

namespace tml::mir {

// Set of known pure functions (no side effects)
// These can be safely removed if their result is unused
static const std::unordered_set<std::string> pure_functions = {
    // Math functions
    "abs",
    "sqrt",
    "cbrt",
    "pow",
    "exp",
    "exp2",
    "log",
    "log2",
    "log10",
    "sin",
    "cos",
    "tan",
    "asin",
    "acos",
    "atan",
    "atan2",
    "sinh",
    "cosh",
    "tanh",
    "floor",
    "ceil",
    "round",
    "trunc",
    "fmod",
    "min",
    "max",
    "clamp",
    // String functions (that don't modify in-place)
    "len",
    "is_empty",
    "contains",
    "starts_with",
    "ends_with",
    "to_uppercase",
    "to_lowercase",
    "trim",
    "trim_start",
    "trim_end",
    "parse_int",
    "parse_float",
    // Collection accessors
    "get",
    "first",
    "last",
    "capacity",
    "count",
    // Type conversions
    "to_string",
    "to_i32",
    "to_i64",
    "to_f32",
    "to_f64",
    "to_bool",
    // Comparison helpers
    "cmp",
    "eq",
    "ne",
    "lt",
    "le",
    "gt",
    "ge",
};

// Check if a function is known to be pure (no side effects)
static bool is_pure_function(const std::string& name) {
    // Check direct match
    if (pure_functions.count(name)) {
        return true;
    }

    // Check for method calls (e.g., "String::len" -> check "len")
    size_t colon_pos = name.rfind("::");
    if (colon_pos != std::string::npos) {
        std::string method_name = name.substr(colon_pos + 2);
        if (pure_functions.count(method_name)) {
            return true;
        }
    }

    // Check for generic instantiations (e.g., "abs[I32]" -> check "abs")
    size_t bracket_pos = name.find('[');
    if (bracket_pos != std::string::npos) {
        std::string base_name = name.substr(0, bracket_pos);
        if (pure_functions.count(base_name)) {
            return true;
        }
    }

    return false;
}

auto DeadCodeEliminationPass::run_on_function(Function& func) -> bool {
    bool changed = false;
    bool made_progress = true;

    // Keep iterating until no more progress is made
    while (made_progress) {
        made_progress = false;

        for (auto& block : func.blocks) {
            // Find instructions to remove
            std::vector<size_t> to_remove;

            for (size_t i = 0; i < block.instructions.size(); ++i) {
                const auto& inst = block.instructions[i];

                // Skip instructions with no result (like stores)
                if (inst.result == INVALID_VALUE) {
                    continue;
                }

                // Check if the result is used
                if (!is_used(func, inst.result)) {
                    // Check if the instruction can be removed
                    if (can_remove(inst.inst)) {
                        to_remove.push_back(i);
                        made_progress = true;
                        changed = true;
                    }
                }
            }

            // Remove dead instructions (in reverse order to maintain indices)
            for (auto it = to_remove.rbegin(); it != to_remove.rend(); ++it) {
                block.instructions.erase(block.instructions.begin() +
                                         static_cast<std::ptrdiff_t>(*it));
            }
        }
    }

    return changed;
}

auto DeadCodeEliminationPass::can_remove(const Instruction& inst) -> bool {
    return std::visit(
        [](const auto& i) -> bool {
            using T = std::decay_t<decltype(i)>;

            // Instructions that CAN be removed (no side effects):
            // - Constants
            // - Binary/Unary operations
            // - Load (but only if result unused)
            // - Alloca (but careful with this one)
            // - GEP
            // - ExtractValue
            // - InsertValue
            // - Cast
            // - Phi
            // - Select
            // - Struct/Enum/Tuple/Array init

            // Instructions that CANNOT be removed:
            // - Store (writes to memory)
            // - Call (may have side effects, unless pure)
            // - MethodCall (may have side effects, unless pure)

            if constexpr (std::is_same_v<T, StoreInst>) {
                return false;
            } else if constexpr (std::is_same_v<T, CallInst>) {
                // Check if the function is known to be pure (no side effects)
                // Pure functions can be safely removed if their result is unused
                if (!i.func_name.empty() && is_pure_function(i.func_name)) {
                    return true;
                }
                // Conservatively assume unknown calls have side effects
                return false;
            } else if constexpr (std::is_same_v<T, MethodCallInst>) {
                // Check if the method is known to be pure
                if (is_pure_function(i.method_name)) {
                    return true;
                }
                // Also check fully qualified name
                if (!i.receiver_type.empty() &&
                    is_pure_function(i.receiver_type + "::" + i.method_name)) {
                    return true;
                }
                return false;
            } else if constexpr (std::is_same_v<T, AllocaInst>) {
                // Alloca can be removed if the allocated memory is never used
                // For now, be conservative
                return true;
            } else {
                return true;
            }
        },
        inst);
}

auto DeadCodeEliminationPass::is_used(const Function& func, ValueId id) -> bool {
    // Use the utility function from mir_pass.hpp
    return is_value_used(func, id);
}

} // namespace tml::mir
