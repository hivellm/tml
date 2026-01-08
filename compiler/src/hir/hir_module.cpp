//! # HIR Module Implementation
//!
//! This file implements the HirModule lookup methods.

#include "hir/hir_module.hpp"

namespace tml::hir {

// ============================================================================
// HirModule Lookup Methods
// ============================================================================

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
