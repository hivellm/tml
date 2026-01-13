//! # Documentation Model
//!
//! This module defines the data structures for TML documentation.
//! These structures are used to represent extracted documentation
//! in a format suitable for generating HTML, JSON, and other outputs.
//!
//! ## Architecture
//!
//! The documentation model consists of:
//! - `DocItem`: Individual documented items (functions, types, etc.)
//! - `DocModule`: A module containing items and submodules
//! - `DocIndex`: The complete documentation database
//!
//! ## Usage
//!
//! ```cpp
//! DocExtractor extractor;
//! DocModule module = extractor.extract(ast_module);
//! JsonGenerator json_gen;
//! json_gen.generate(module, "output/docs.json");
//! ```

#ifndef TML_DOC_DOC_MODEL_HPP
#define TML_DOC_DOC_MODEL_HPP

#include "common.hpp"

#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace tml::doc {

// ============================================================================
// Documentation Tags
// ============================================================================

/// A parameter documentation entry.
///
/// Extracted from `@param name description` tags or inferred from function signature.
struct DocParam {
    std::string name;        ///< Parameter name.
    std::string type;        ///< Type as string (e.g., "I32", "ref Str").
    std::string description; ///< Documentation for this parameter.
};

/// Return value documentation.
///
/// Extracted from `@returns description` tag.
struct DocReturn {
    std::string type;        ///< Return type as string.
    std::string description; ///< Documentation for the return value.
};

/// A code example in documentation.
///
/// Extracted from `@example` tags or fenced code blocks.
struct DocExample {
    std::string code;        ///< The example code.
    std::string description; ///< Optional description of what the example demonstrates.
    std::string language;    ///< Language hint (default: "tml").
};

/// A thrown error/exception.
///
/// Extracted from `@throws ErrorType description` tags.
struct DocThrows {
    std::string error_type;  ///< The error type that may be thrown.
    std::string description; ///< When/why this error is thrown.
};

/// Deprecation information.
///
/// Extracted from `@deprecated message` tag.
struct DocDeprecation {
    std::string message;     ///< Deprecation message/reason.
    std::string since;       ///< Version when deprecated (if known).
    std::string replacement; ///< Suggested replacement (if any).
};

// ============================================================================
// Item Kinds
// ============================================================================

/// The kind of a documented item.
enum class DocItemKind {
    Function,       ///< A function declaration.
    Method,         ///< A method in an impl block.
    Struct,         ///< A struct type.
    Enum,           ///< An enum type.
    Variant,        ///< A variant of an enum.
    Field,          ///< A struct field.
    Trait,          ///< A behavior (trait).
    Impl,           ///< An impl block.
    TraitImpl,      ///< An impl block for a trait.
    TypeAlias,      ///< A type alias.
    Constant,       ///< A const declaration.
    AssociatedType, ///< An associated type in a behavior.
    Module,         ///< A module.
};

/// Converts DocItemKind to string.
[[nodiscard]] auto doc_item_kind_to_string(DocItemKind kind) -> std::string_view;

// ============================================================================
// Visibility
// ============================================================================

/// Visibility level of a documented item.
enum class DocVisibility {
    Public,  ///< Visible everywhere (`pub`).
    Crate,   ///< Visible within crate (`pub(crate)`).
    Private, ///< Not exported (private).
};

/// Converts DocVisibility to string.
[[nodiscard]] auto doc_visibility_to_string(DocVisibility vis) -> std::string_view;

// ============================================================================
// Generic Parameters
// ============================================================================

/// A generic type parameter.
struct DocGenericParam {
    std::string name;                         ///< Parameter name (e.g., "T").
    std::vector<std::string> bounds;          ///< Trait bounds (e.g., ["Display", "Clone"]).
    std::optional<std::string> default_value; ///< Default type/value if any.
    bool is_const = false;                    ///< True if this is a const generic.
};

// ============================================================================
// DocItem
// ============================================================================

/// A documented item (function, type, behavior, etc.).
///
/// This is the core unit of documentation, representing a single
/// item that can be documented and displayed.
struct DocItem {
    // Identification
    std::string id;           ///< Unique identifier: "core::slice::Slice::get".
    std::string name;         ///< Short name: "get".
    DocItemKind kind;         ///< What kind of item this is.
    std::string path;         ///< Module path: "core::slice".
    DocVisibility visibility; ///< Visibility level.

    // Signature
    std::string signature; ///< Full signature: "func get[T](this, idx: U64) -> Maybe[ref T]".
    std::vector<DocGenericParam> generics; ///< Generic parameters.

    // Documentation
    std::string doc;     ///< Full markdown documentation.
    std::string summary; ///< First paragraph (for listings).

    // Structured documentation (from tags)
    std::vector<DocParam> params;             ///< @param tags.
    std::optional<DocReturn> returns;         ///< @returns tag.
    std::vector<DocThrows> throws;            ///< @throws tags.
    std::vector<DocExample> examples;         ///< @example tags and code blocks.
    std::vector<std::string> see_also;        ///< @see references.
    std::optional<std::string> since;         ///< @since version.
    std::optional<DocDeprecation> deprecated; ///< @deprecated info.

    // Children (for types and behaviors)
    std::vector<DocItem> fields;            ///< Struct fields.
    std::vector<DocItem> variants;          ///< Enum variants.
    std::vector<DocItem> methods;           ///< Methods from impl blocks.
    std::vector<DocItem> associated_types;  ///< Associated types (for behaviors).
    std::vector<DocItem> associated_consts; ///< Associated consts (for behaviors).

    // Super traits (for behaviors)
    std::vector<std::string> super_traits; ///< Parent traits.

    // Source location
    std::string source_file;  ///< Source file path.
    uint32_t source_line = 0; ///< Line number in source.

    // Type-specific information
    std::optional<std::string> impl_for;     ///< For impls: the implementing type.
    std::optional<std::string> impl_trait;   ///< For impls: the implemented trait.
    std::optional<std::string> aliased_type; ///< For type aliases: the aliased type.

    // Modifiers
    bool is_async = false;  ///< For functions: is async.
    bool is_unsafe = false; ///< For functions: is lowlevel/unsafe.
};

// ============================================================================
// DocModule
// ============================================================================

/// A documented module.
///
/// Represents a module with its documentation, items, and submodules.
struct DocModule {
    std::string name;    ///< Module name: "slice".
    std::string path;    ///< Full path: "core::slice".
    std::string doc;     ///< Module-level documentation (from //!).
    std::string summary; ///< First paragraph of module doc.

    DocVisibility visibility; ///< Module visibility.

    std::vector<DocItem> items;        ///< Items in this module.
    std::vector<DocModule> submodules; ///< Child modules.

    std::string source_file; ///< Source file path.
};

// ============================================================================
// DocIndex
// ============================================================================

/// The complete documentation index.
///
/// Contains all documented modules and provides lookup functionality.
struct DocIndex {
    std::string crate_name;  ///< Name of the crate/package.
    std::string version;     ///< Version string.
    std::string description; ///< Crate description.

    std::vector<DocModule> modules; ///< All documented modules.

    /// Builds internal lookup indices after modules are added.
    void build_index();

    /// Finds an item by its ID.
    [[nodiscard]] auto find_item(const std::string& id) const -> const DocItem*;

    /// Finds a module by its path.
    [[nodiscard]] auto find_module(const std::string& path) const -> const DocModule*;

    /// Gets all items of a given kind.
    [[nodiscard]] auto items_by_kind(DocItemKind kind) const -> std::vector<const DocItem*>;

    /// Gets all public items.
    [[nodiscard]] auto public_items() const -> std::vector<const DocItem*>;

private:
    std::unordered_map<std::string, const DocItem*> item_index_;
    std::unordered_map<std::string, const DocModule*> module_index_;
};

// ============================================================================
// Search Support
// ============================================================================

/// A search result entry.
struct DocSearchResult {
    const DocItem* item;       ///< The matched item.
    float score;               ///< Relevance score (0.0 - 1.0).
    std::string match_context; ///< Context snippet showing match.
};

/// Search options.
struct DocSearchOptions {
    bool include_private = false;  ///< Include private items.
    bool search_docs = true;       ///< Search in documentation text.
    bool search_signatures = true; ///< Search in signatures.
    size_t max_results = 50;       ///< Maximum results to return.
};

} // namespace tml::doc

#endif // TML_DOC_DOC_MODEL_HPP
