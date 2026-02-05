//! # Derive Registry
//!
//! This module provides infrastructure for the @derive macro system.
//! It defines which traits can be derived and validates derivability.

#pragma once

#include <optional>
#include <string>
#include <unordered_set>
#include <vector>

namespace tml::derive {

/// Enumeration of all derivable traits/behaviors
enum class DerivableTrait {
    PartialEq,  // Field-by-field equality: eq(this, other: ref Self) -> Bool
    Eq,         // Marker trait, implies PartialEq (reflexive equality)
    PartialOrd, // Lexicographic comparison: partial_cmp(this, other: ref Self) -> Maybe[Ordering]
    Ord,        // Total ordering: cmp(this, other: ref Self) -> Ordering
    Duplicate,  // Field-by-field clone: duplicate(this) -> Self
    Copy,       // Marker trait, implies Duplicate (bitwise copy)
    Hash,       // Hash computation: hash(this) -> I64
    Default,    // Default construction: default() -> Self (static)
    Debug,      // Debug string: debug_string(this) -> Str
    Reflect     // Reflection: type_info(), runtime_type_info(), variant_name(), variant_tag()
};

/// Parse a trait name string to DerivableTrait enum
/// Returns std::nullopt if the trait name is not recognized
inline std::optional<DerivableTrait> parse_trait_name(const std::string& name) {
    if (name == "PartialEq")
        return DerivableTrait::PartialEq;
    if (name == "Eq")
        return DerivableTrait::Eq;
    if (name == "PartialOrd")
        return DerivableTrait::PartialOrd;
    if (name == "Ord")
        return DerivableTrait::Ord;
    if (name == "Duplicate")
        return DerivableTrait::Duplicate;
    if (name == "Copy")
        return DerivableTrait::Copy;
    if (name == "Hash")
        return DerivableTrait::Hash;
    if (name == "Default")
        return DerivableTrait::Default;
    if (name == "Debug")
        return DerivableTrait::Debug;
    if (name == "Reflect")
        return DerivableTrait::Reflect;
    return std::nullopt;
}

/// Get the string name of a derivable trait
inline std::string trait_name(DerivableTrait trait) {
    switch (trait) {
    case DerivableTrait::PartialEq:
        return "PartialEq";
    case DerivableTrait::Eq:
        return "Eq";
    case DerivableTrait::PartialOrd:
        return "PartialOrd";
    case DerivableTrait::Ord:
        return "Ord";
    case DerivableTrait::Duplicate:
        return "Duplicate";
    case DerivableTrait::Copy:
        return "Copy";
    case DerivableTrait::Hash:
        return "Hash";
    case DerivableTrait::Default:
        return "Default";
    case DerivableTrait::Debug:
        return "Debug";
    case DerivableTrait::Reflect:
        return "Reflect";
    }
    return "Unknown";
}

/// Get super-traits that must also be derived when deriving a given trait
/// For example, Eq requires PartialEq, Copy requires Duplicate
inline std::vector<DerivableTrait> get_super_traits(DerivableTrait trait) {
    switch (trait) {
    case DerivableTrait::Eq:
        return {DerivableTrait::PartialEq};
    case DerivableTrait::Copy:
        return {DerivableTrait::Duplicate};
    case DerivableTrait::Ord:
        return {DerivableTrait::PartialOrd, DerivableTrait::Eq};
    default:
        return {};
    }
}

/// Check if a trait is a marker trait (no methods to generate)
inline bool is_marker_trait(DerivableTrait trait) {
    return trait == DerivableTrait::Eq || trait == DerivableTrait::Copy;
}

/// Get the set of all derivable trait names for error messages
inline std::unordered_set<std::string> all_derivable_trait_names() {
    return {"PartialEq", "Eq",      "PartialOrd", "Ord",   "Duplicate",
            "Copy",      "Hash",    "Default",    "Debug", "Reflect"};
}

} // namespace tml::derive
