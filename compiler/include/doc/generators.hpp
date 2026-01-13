//! # Documentation Output Generators
//!
//! This module provides generators for outputting documentation in
//! various formats: JSON, HTML, and Markdown.

#ifndef TML_DOC_GENERATORS_HPP
#define TML_DOC_GENERATORS_HPP

#include "doc/doc_model.hpp"

#include <filesystem>
#include <ostream>
#include <string>

namespace tml::doc {

// ============================================================================
// Generator Configuration
// ============================================================================

/// Configuration for documentation output generation.
struct GeneratorConfig {
    std::string title;            ///< Documentation title.
    std::string version;          ///< Version string.
    bool include_private = false; ///< Include private items.
    bool include_source = false;  ///< Include source links.
    bool include_tests = false;   ///< Include test modules (.test.tml).
    bool minify = false;          ///< Minify output (for JSON).
};

/// Represents a library grouping for documentation.
struct DocLibrary {
    std::string name;                      ///< Library name (e.g., "core", "std").
    std::string description;               ///< Library description.
    std::vector<const DocModule*> modules; ///< Modules in this library.
};

/// Helper to organize modules by library.
std::vector<DocLibrary> organize_by_library(const DocIndex& index, bool include_tests = false);

// ============================================================================
// JSON Generator
// ============================================================================

/// Generates JSON documentation output.
///
/// The JSON format is suitable for:
/// - IDE integration
/// - Search indexing
/// - Static site generators
/// - API consumption
class JsonGenerator {
public:
    /// Constructs a JSON generator with the given configuration.
    explicit JsonGenerator(GeneratorConfig config = {});

    /// Generates JSON for a single module.
    void generate(const DocModule& module, std::ostream& out);

    /// Generates JSON for the entire index.
    void generate(const DocIndex& index, std::ostream& out);

    /// Generates JSON to a file.
    void generate_file(const DocModule& module, const std::filesystem::path& path);
    void generate_file(const DocIndex& index, const std::filesystem::path& path);

private:
    GeneratorConfig config_;

    void write_item(const DocItem& item, std::ostream& out, int indent);
    void write_module(const DocModule& module, std::ostream& out, int indent);
    void write_string(const std::string& str, std::ostream& out);
    void write_indent(std::ostream& out, int indent);
    void write_array_start(std::ostream& out);
    void write_array_end(std::ostream& out, int indent);
    void write_object_start(std::ostream& out);
    void write_object_end(std::ostream& out, int indent);
};

// ============================================================================
// Markdown Generator
// ============================================================================

/// Generates Markdown documentation output.
///
/// The Markdown format is suitable for:
/// - GitHub/GitLab wikis
/// - Static documentation sites (Jekyll, Hugo, etc.)
/// - README files
class MarkdownGenerator {
public:
    /// Constructs a Markdown generator with the given configuration.
    explicit MarkdownGenerator(GeneratorConfig config = {});

    /// Generates Markdown for a single module.
    void generate(const DocModule& module, std::ostream& out);

    /// Generates Markdown for a single item.
    void generate_item(const DocItem& item, std::ostream& out);

    /// Generates Markdown to a file.
    void generate_file(const DocModule& module, const std::filesystem::path& path);

    /// Generates Markdown documentation to a directory (one file per module).
    void generate_directory(const DocIndex& index, const std::filesystem::path& dir);

private:
    GeneratorConfig config_;

    void write_signature(const DocItem& item, std::ostream& out);
    void write_description(const DocItem& item, std::ostream& out);
    void write_params(const DocItem& item, std::ostream& out);
    void write_returns(const DocItem& item, std::ostream& out);
    void write_examples(const DocItem& item, std::ostream& out);
    void write_see_also(const DocItem& item, std::ostream& out);
    void write_fields(const DocItem& item, std::ostream& out);
    void write_methods(const DocItem& item, std::ostream& out);
};

// ============================================================================
// HTML Generator
// ============================================================================

/// Generates HTML documentation output.
///
/// The HTML format is suitable for:
/// - Standalone documentation websites
/// - Offline documentation viewing
/// - IDE hover documentation
class HtmlGenerator {
public:
    /// Constructs an HTML generator with the given configuration.
    explicit HtmlGenerator(GeneratorConfig config = {});

    /// Generates a complete HTML documentation site.
    void generate_site(const DocIndex& index, const std::filesystem::path& output_dir);

    /// Generates HTML for a single module page.
    void generate_module_page(const DocModule& module, std::ostream& out);

    /// Generates HTML for a single item (for inline/hover docs).
    void generate_item_html(const DocItem& item, std::ostream& out);

    /// Generates the index/landing page.
    void generate_index_page(const DocIndex& index, std::ostream& out);

    /// Generates the search index JavaScript file.
    void generate_search_index(const DocIndex& index, std::ostream& out);

private:
    GeneratorConfig config_;

    void write_head(const std::string& title, std::ostream& out,
                    const std::string& asset_prefix = "");
    void write_navigation(const DocIndex& index, std::ostream& out);
    void write_item_section(const DocItem& item, std::ostream& out);
    void write_signature_html(const DocItem& item, std::ostream& out);
    void write_footer(std::ostream& out);
    void write_css(std::ostream& out);
    void write_search_js(std::ostream& out);
    void write_module_sidebar(const std::string& current_module, const DocModule& module,
                              const DocIndex& index, std::ostream& out,
                              const std::string& asset_prefix = "");
    void write_module_sidebar_with_libraries(const std::string& current_module,
                                             const DocModule& module,
                                             const std::vector<DocLibrary>& libraries,
                                             std::ostream& out,
                                             const std::string& asset_prefix = "");
    void write_sidebar_index(const DocIndex& index, std::ostream& out);
    void write_sidebar_index_with_libraries(const std::vector<DocLibrary>& libraries,
                                            std::ostream& out);
    void write_item_card(const DocItem& item, std::ostream& out);
    void write_scripts(std::ostream& out, const std::string& asset_prefix = "");
    void generate_index_page_with_libraries(const std::vector<DocLibrary>& libraries,
                                            const DocIndex& index, std::ostream& out);
    void generate_search_index_filtered(const std::vector<DocLibrary>& libraries,
                                        std::ostream& out);
    std::string escape_html(const std::string& text);
    std::string markdown_to_html(const std::string& markdown);
};

} // namespace tml::doc

#endif // TML_DOC_GENERATORS_HPP
