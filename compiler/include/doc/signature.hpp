//! # Signature Generation
//!
//! This module generates human-readable signatures from AST nodes.
//! Signatures are used in documentation to show the full declaration
//! of an item including types, generics, and constraints.
//!
//! ## Examples
//!
//! - Function: `func get[T](this, index: U64) -> Maybe[ref T]`
//! - Struct: `type Vec[T]`
//! - Behavior: `behavior Iterator[Item]`
//! - Impl: `impl[T: Display] Display for Vec[T]`

#ifndef TML_DOC_SIGNATURE_HPP
#define TML_DOC_SIGNATURE_HPP

#include "parser/ast.hpp"

#include <string>

namespace tml::doc {

/// Generates a signature string for a function declaration.
[[nodiscard]] auto generate_func_signature(const parser::FuncDecl& func) -> std::string;

/// Generates a signature string for a struct declaration.
[[nodiscard]] auto generate_struct_signature(const parser::StructDecl& struct_decl) -> std::string;

/// Generates a signature string for an enum declaration.
[[nodiscard]] auto generate_enum_signature(const parser::EnumDecl& enum_decl) -> std::string;

/// Generates a signature string for a behavior (trait) declaration.
[[nodiscard]] auto generate_trait_signature(const parser::TraitDecl& trait) -> std::string;

/// Generates a signature string for an impl block.
[[nodiscard]] auto generate_impl_signature(const parser::ImplDecl& impl) -> std::string;

/// Generates a signature string for a type alias.
[[nodiscard]] auto generate_type_alias_signature(const parser::TypeAliasDecl& alias) -> std::string;

/// Generates a signature string for a constant.
[[nodiscard]] auto generate_const_signature(const parser::ConstDecl& const_decl) -> std::string;

/// Generates a signature string for a struct field.
[[nodiscard]] auto generate_field_signature(const parser::StructField& field) -> std::string;

/// Generates a signature string for an enum variant.
[[nodiscard]] auto generate_variant_signature(const parser::EnumVariant& variant) -> std::string;

/// Converts a type AST node to a string representation.
[[nodiscard]] auto type_to_string(const parser::Type& type) -> std::string;

/// Generates the generic parameters string (e.g., "[T, U: Clone]").
[[nodiscard]] auto generics_to_string(const std::vector<parser::GenericParam>& generics)
    -> std::string;

/// Generates a where clause string.
[[nodiscard]] auto where_clause_to_string(const std::optional<parser::WhereClause>& where_clause)
    -> std::string;

} // namespace tml::doc

#endif // TML_DOC_SIGNATURE_HPP
