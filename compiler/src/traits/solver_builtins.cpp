//! # Builtin Behavior Implementations
//!
//! Defines which primitive and builtin types implement which behaviors
//! without requiring explicit `extend` declarations.

#include "traits/solver.hpp"

namespace tml::traits {

// ============================================================================
// Builtin behavior tables
// ============================================================================

namespace {

/// Behaviors implemented by all signed integer types (I8, I16, I32, I64, I128).
const std::vector<std::string> SIGNED_INT_BEHAVIORS = {
    "Numeric", "Eq",    "PartialEq", "Ord",       "PartialOrd", "Hash",
    "Display", "Debug", "Default",   "Duplicate", "Sized",
};

/// Behaviors implemented by all unsigned integer types (U8, U16, U32, U64, U128).
const std::vector<std::string> UNSIGNED_INT_BEHAVIORS = {
    "Numeric", "Eq",    "PartialEq", "Ord",       "PartialOrd", "Hash",
    "Display", "Debug", "Default",   "Duplicate", "Sized",
};

/// Behaviors implemented by floating point types (F32, F64).
const std::vector<std::string> FLOAT_BEHAVIORS = {
    "Numeric", "PartialEq", "PartialOrd", "Display", "Debug", "Default", "Duplicate", "Sized",
};

/// Behaviors implemented by Bool.
const std::vector<std::string> BOOL_BEHAVIORS = {
    "Eq",      "PartialEq", "Ord",     "PartialOrd", "Hash",
    "Display", "Debug",     "Default", "Duplicate",  "Sized",
};

/// Behaviors implemented by Char.
const std::vector<std::string> CHAR_BEHAVIORS = {
    "Eq", "PartialEq", "Ord", "PartialOrd", "Hash", "Display", "Debug", "Duplicate", "Sized",
};

/// Behaviors implemented by Str.
const std::vector<std::string> STR_BEHAVIORS = {
    "Eq", "PartialEq", "Ord", "PartialOrd", "Hash", "Display", "Debug", "Duplicate", "Sized",
};

/// Behaviors implemented by Unit type.
const std::vector<std::string> UNIT_BEHAVIORS = {
    "Eq", "PartialEq", "Ord", "PartialOrd", "Hash", "Debug", "Default", "Sized", "Send", "Sync",
};

/// Check if a PrimitiveKind is a signed integer.
bool is_signed_int(types::PrimitiveKind kind) {
    return kind == types::PrimitiveKind::I8 || kind == types::PrimitiveKind::I16 ||
           kind == types::PrimitiveKind::I32 || kind == types::PrimitiveKind::I64 ||
           kind == types::PrimitiveKind::I128;
}

/// Check if a PrimitiveKind is an unsigned integer.
bool is_unsigned_int(types::PrimitiveKind kind) {
    return kind == types::PrimitiveKind::U8 || kind == types::PrimitiveKind::U16 ||
           kind == types::PrimitiveKind::U32 || kind == types::PrimitiveKind::U64 ||
           kind == types::PrimitiveKind::U128;
}

/// Check if a PrimitiveKind is a float.
bool is_float(types::PrimitiveKind kind) {
    return kind == types::PrimitiveKind::F32 || kind == types::PrimitiveKind::F64;
}

/// Get the builtin behaviors for a primitive kind.
const std::vector<std::string>& behaviors_for_primitive(types::PrimitiveKind kind) {
    static const std::vector<std::string> EMPTY;
    static const std::vector<std::string> NEVER_BEHAVIORS = {"Sized"};

    if (is_signed_int(kind))
        return SIGNED_INT_BEHAVIORS;
    if (is_unsigned_int(kind))
        return UNSIGNED_INT_BEHAVIORS;
    if (is_float(kind))
        return FLOAT_BEHAVIORS;

    switch (kind) {
    case types::PrimitiveKind::Bool:
        return BOOL_BEHAVIORS;
    case types::PrimitiveKind::Char:
        return CHAR_BEHAVIORS;
    case types::PrimitiveKind::Str:
        return STR_BEHAVIORS;
    case types::PrimitiveKind::Unit:
        return UNIT_BEHAVIORS;
    case types::PrimitiveKind::Never:
        return NEVER_BEHAVIORS;
    default:
        return EMPTY;
    }
}

} // anonymous namespace

// ============================================================================
// Public API
// ============================================================================

auto has_builtin_impl(const types::TypePtr& type, const std::string& behavior_name) -> bool {
    if (!type)
        return false;

    // Primitive types
    if (type->is<types::PrimitiveType>()) {
        const auto& prim = type->as<types::PrimitiveType>();
        const auto& behaviors = behaviors_for_primitive(prim.kind);
        for (const auto& b : behaviors) {
            if (b == behavior_name)
                return true;
        }
        return false;
    }

    // Tuple types implement various behaviors if all elements do
    if (type->is<types::TupleType>()) {
        // Tuples implement Eq, PartialEq, Ord, PartialOrd, Hash, Debug, Default, Duplicate, Sized
        // (if all elements do) — but we can't check element-level here without a solver reference.
        // For now, Sized is always true for tuples.
        return behavior_name == "Sized";
    }

    // Array types implement Sized
    if (type->is<types::ArrayType>()) {
        return behavior_name == "Sized";
    }

    // Reference types implement Sized and Duplicate (for shared refs)
    if (type->is<types::RefType>()) {
        if (behavior_name == "Sized" || behavior_name == "Duplicate")
            return true;
        return false;
    }

    // Pointer types implement Sized
    if (type->is<types::PtrType>()) {
        return behavior_name == "Sized";
    }

    // Function types implement Sized and Fn/FnOnce
    if (type->is<types::FuncType>()) {
        return behavior_name == "Sized" || behavior_name == "Fn" || behavior_name == "FnOnce";
    }

    // Closure types implement Fn/FnMut/FnOnce based on captures
    if (type->is<types::ClosureType>()) {
        return behavior_name == "Sized" || behavior_name == "FnOnce" || behavior_name == "FnMut" ||
               behavior_name == "Fn";
    }

    return false;
}

auto builtin_behaviors_for_type(const types::TypePtr& type) -> std::vector<std::string> {
    if (!type)
        return {};

    if (type->is<types::PrimitiveType>()) {
        const auto& prim = type->as<types::PrimitiveType>();
        return behaviors_for_primitive(prim.kind);
    }

    // For non-primitives, return basic structural behaviors
    std::vector<std::string> result;
    if (!type->is<types::DynBehaviorType>()) {
        result.push_back("Sized");
    }
    return result;
}

} // namespace tml::traits
