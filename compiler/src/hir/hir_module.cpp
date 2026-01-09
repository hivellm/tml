//! # HIR Module Implementation
//!
//! This file implements the HirModule lookup methods.
//!
//! ## Overview
//!
//! An HirModule represents a compilation unit after AST lowering. It contains:
//! - Struct definitions
//! - Enum definitions
//! - Function definitions (including monomorphized specializations)
//! - Constant definitions
//! - Type aliases
//! - Behavior implementations
//!
//! ## Lookup Methods
//!
//! The `find_*` methods search for items by name. They accept both:
//! - Source names (e.g., "Vec", "push")
//! - Mangled names (e.g., "Vec_I32", "Vec_I32_push")
//!
//! This dual lookup enables both source-level queries and linking.
//!
//! ## Module Lifecycle
//!
//! ```text
//! AST Source File  ->  HirBuilder  ->  HirModule  ->  MIR Lowering
//!                                          |
//!                                          v
//!                                   Serialization (cache)
//! ```
//!
//! ## See Also
//!
//! - `hir_module.hpp` - Module type definitions
//! - `hir_builder.cpp` - Module construction from AST
//! - `hir_serialize.hpp` - Module serialization

#include "hir/hir_module.hpp"

namespace tml::hir {

// ============================================================================
// HirModule Lookup Methods
// ============================================================================
//
// Linear search through module items. Matches against both source name and
// mangled name to support both symbolic lookup and link-time resolution.
// Returns nullptr if not found.
//
// Note: For large modules, consider indexing by name for O(1) lookup.

auto HirModule::find_struct(const std::string& search_name) const -> const HirStruct* {
    for (const auto& s : structs) {
        if (s.name == search_name || s.mangled_name == search_name) {
            return &s;
        }
    }
    return nullptr;
}

auto HirModule::find_enum(const std::string& search_name) const -> const HirEnum* {
    for (const auto& e : enums) {
        if (e.name == search_name || e.mangled_name == search_name) {
            return &e;
        }
    }
    return nullptr;
}

auto HirModule::find_function(const std::string& search_name) const -> const HirFunction* {
    for (const auto& f : functions) {
        if (f.name == search_name || f.mangled_name == search_name) {
            return &f;
        }
    }
    return nullptr;
}

auto HirModule::find_const(const std::string& search_name) const -> const HirConst* {
    for (const auto& c : constants) {
        if (c.name == search_name) {
            return &c;
        }
    }
    return nullptr;
}

} // namespace tml::hir
