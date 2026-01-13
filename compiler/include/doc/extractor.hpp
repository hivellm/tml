//! # Documentation Extractor
//!
//! This module extracts documentation from AST modules and builds
//! a structured DocModule representation.

#ifndef TML_DOC_EXTRACTOR_HPP
#define TML_DOC_EXTRACTOR_HPP

#include "doc/doc_model.hpp"
#include "parser/ast.hpp"

#include <string>
#include <vector>

namespace tml::doc {

/// Configuration for documentation extraction.
struct ExtractorConfig {
    bool include_private = false;   ///< Include private items.
    bool include_internals = false; ///< Include items marked @internal.
    bool extract_examples = true;   ///< Extract code examples from docs.
    bool resolve_links = true;      ///< Resolve @see and inline links.
};

/// Extracts documentation from AST modules.
///
/// The extractor walks the AST and builds DocItem trees for each
/// documented item, parsing doc comments and generating signatures.
class Extractor {
public:
    /// Constructs an extractor with the given configuration.
    explicit Extractor(ExtractorConfig config = {});

    /// Extracts documentation from a parsed module.
    ///
    /// @param module The parsed AST module.
    /// @param module_path The module path (e.g., "core::slice").
    /// @returns A DocModule containing all documented items.
    [[nodiscard]] auto extract(const parser::Module& module, const std::string& module_path = "")
        -> DocModule;

    /// Extracts documentation from multiple modules into an index.
    ///
    /// @param modules Vector of (module, path) pairs.
    /// @returns A DocIndex containing all modules.
    [[nodiscard]] auto
    extract_all(const std::vector<std::pair<const parser::Module*, std::string>>& modules)
        -> DocIndex;

private:
    ExtractorConfig config_;
    std::string current_module_path_;

    // ========================================================================
    // Item Extraction
    // ========================================================================

    /// Extracts a function declaration.
    [[nodiscard]] auto extract_func(const parser::FuncDecl& func,
                                    const std::string& parent_path = "") -> DocItem;

    /// Extracts a struct declaration.
    [[nodiscard]] auto extract_struct(const parser::StructDecl& struct_decl) -> DocItem;

    /// Extracts an enum declaration.
    [[nodiscard]] auto extract_enum(const parser::EnumDecl& enum_decl) -> DocItem;

    /// Extracts a trait (behavior) declaration.
    [[nodiscard]] auto extract_trait(const parser::TraitDecl& trait) -> DocItem;

    /// Extracts an impl block.
    [[nodiscard]] auto extract_impl(const parser::ImplDecl& impl) -> DocItem;

    /// Extracts a type alias declaration.
    [[nodiscard]] auto extract_type_alias(const parser::TypeAliasDecl& alias) -> DocItem;

    /// Extracts a const declaration.
    [[nodiscard]] auto extract_const(const parser::ConstDecl& const_decl) -> DocItem;

    /// Extracts a struct field.
    [[nodiscard]] auto extract_field(const parser::StructField& field,
                                     const std::string& parent_path) -> DocItem;

    /// Extracts an enum variant.
    [[nodiscard]] auto extract_variant(const parser::EnumVariant& variant,
                                       const std::string& parent_path) -> DocItem;

    // ========================================================================
    // Helper Functions
    // ========================================================================

    /// Builds a full item path from components.
    [[nodiscard]] auto make_path(const std::string& parent, const std::string& name) const
        -> std::string;

    /// Converts AST visibility to doc visibility.
    [[nodiscard]] auto convert_visibility(parser::Visibility vis) const -> DocVisibility;

    /// Extracts generic parameters from AST.
    [[nodiscard]] auto extract_generics(const std::vector<parser::GenericParam>& params) const
        -> std::vector<DocGenericParam>;

    /// Extracts function parameters from AST.
    [[nodiscard]] auto extract_func_params(const std::vector<parser::FuncParam>& params) const
        -> std::vector<DocParam>;

    /// Checks if an item should be included based on config.
    [[nodiscard]] auto should_include(parser::Visibility vis,
                                      const std::optional<std::string>& doc) const -> bool;

    /// Parses doc comment and populates DocItem fields.
    void populate_doc_fields(DocItem& item, const std::optional<std::string>& doc);
};

} // namespace tml::doc

#endif // TML_DOC_EXTRACTOR_HPP
