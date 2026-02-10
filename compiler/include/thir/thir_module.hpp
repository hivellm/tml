//! # THIR Module
//!
//! Top-level compilation unit for THIR. Mirrors `HirModule` structure.

#pragma once

#include "thir/thir_expr.hpp"

#include <string>
#include <vector>

namespace tml::thir {

// ============================================================================
// Declaration Types
// ============================================================================

/// A function parameter.
struct ThirParam {
    std::string name;
    ThirType type;
    bool is_mut;
    SourceSpan span;
};

/// A function declaration in THIR.
struct ThirFunction {
    ThirId id;
    std::string name;
    std::string mangled_name;
    std::vector<ThirParam> params;
    ThirType return_type;
    std::optional<ThirExprPtr> body;
    bool is_public;
    bool is_async;
    bool is_extern;
    std::optional<std::string> extern_abi;
    std::vector<std::string> attributes;
    SourceSpan span;
};

/// A struct field.
struct ThirField {
    std::string name;
    ThirType type;
    bool is_public;
    SourceSpan span;
};

/// A struct declaration.
struct ThirStruct {
    ThirId id;
    std::string name;
    std::string mangled_name;
    std::vector<ThirField> fields;
    bool is_public;
    SourceSpan span;
};

/// An enum variant.
struct ThirVariant {
    std::string name;
    int index;
    std::vector<ThirType> payload_types;
    SourceSpan span;
};

/// An enum declaration.
struct ThirEnum {
    ThirId id;
    std::string name;
    std::string mangled_name;
    std::vector<ThirVariant> variants;
    bool is_public;
    SourceSpan span;
};

/// A behavior method signature.
struct ThirBehaviorMethod {
    std::string name;
    std::vector<ThirParam> params;
    ThirType return_type;
    bool has_default_impl;
    std::optional<ThirExprPtr> default_body;
    SourceSpan span;
};

/// A behavior (trait) declaration.
struct ThirBehavior {
    ThirId id;
    std::string name;
    std::vector<ThirBehaviorMethod> methods;
    std::vector<std::string> super_behaviors;
    bool is_public;
    SourceSpan span;
};

/// An impl block.
struct ThirImpl {
    ThirId id;
    std::optional<std::string> behavior_name;
    std::string type_name;
    ThirType self_type;
    std::vector<ThirFunction> methods;
    SourceSpan span;
};

/// A constant declaration.
struct ThirConst {
    ThirId id;
    std::string name;
    ThirType type;
    ThirExprPtr value;
    bool is_public;
    SourceSpan span;
};

// ============================================================================
// THIR Module
// ============================================================================

/// A complete THIR module (compilation unit).
///
/// Same structure as HirModule but contains THIR declarations with
/// explicit coercions and resolved method dispatch.
struct ThirModule {
    std::string name;
    std::string source_path;

    std::vector<ThirStruct> structs;
    std::vector<ThirEnum> enums;
    std::vector<ThirBehavior> behaviors;
    std::vector<ThirImpl> impls;
    std::vector<ThirFunction> functions;
    std::vector<ThirConst> constants;
    std::vector<std::string> imports;

    /// Find a struct by name.
    [[nodiscard]] auto find_struct(const std::string& search_name) const -> const ThirStruct*;

    /// Find an enum by name.
    [[nodiscard]] auto find_enum(const std::string& search_name) const -> const ThirEnum*;

    /// Find a function by name.
    [[nodiscard]] auto find_function(const std::string& search_name) const -> const ThirFunction*;

    /// Find a constant by name.
    [[nodiscard]] auto find_const(const std::string& search_name) const -> const ThirConst*;
};

} // namespace tml::thir
