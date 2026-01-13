//! # Documentation Output Generators Implementation
//!
//! This file implements JSON, Markdown, and HTML documentation generators.

#include "doc/generators.hpp"

#include <algorithm>
#include <fstream>
#include <functional>
#include <map>
#include <sstream>

namespace tml::doc {

// ============================================================================
// Helper Functions
// ============================================================================

namespace {

/// Check if a module name is a test module
bool is_test_module(const std::string& name) {
    // Check for .test suffix or _test suffix
    if (name.size() >= 5 && name.substr(name.size() - 5) == ".test") {
        return true;
    }
    if (name.size() >= 5 && name.substr(name.size() - 5) == "_test") {
        return true;
    }
    // Check for _test_ in the middle of the name (e.g., iter_test_simple)
    if (name.find("_test_") != std::string::npos) {
        return true;
    }
    return false;
}

/// Check if a module should be skipped (mod.tml files that are just re-exports)
bool is_reexport_module(const DocModule& module) {
    // Skip modules named exactly "mod" - these are typically just re-export files
    return module.name == "mod";
}

/// Extract library name from module path
std::string get_library_name(const DocModule& module) {
    // Path format: lib::core::src::... or similar
    // We want to extract "core", "std", "test" etc.
    const std::string& path = module.path;

    // Try to find lib:: prefix
    size_t lib_pos = path.find("lib::");
    if (lib_pos != std::string::npos) {
        size_t start = lib_pos + 5; // After "lib::"
        size_t end = path.find("::", start);
        if (end != std::string::npos) {
            return path.substr(start, end - start);
        }
        return path.substr(start);
    }

    // Check for common patterns in module name
    const std::string& name = module.name;

    // Default based on common patterns
    if (name.find("test") != std::string::npos) {
        return "test";
    }

    return "core"; // Default to core
}

/// Get library description
std::string get_library_description(const std::string& lib_name) {
    static const std::map<std::string, std::string> descriptions = {
        {"core", "Core library providing fundamental types, traits, and utilities."},
        {"std", "Standard library with collections, I/O, and system interfaces."},
        {"test", "Testing framework for writing and running tests."}};

    auto it = descriptions.find(lib_name);
    return it != descriptions.end() ? it->second : "";
}

} // namespace

std::vector<DocLibrary> organize_by_library(const DocIndex& index, bool include_tests) {
    std::map<std::string, DocLibrary> lib_map;

    for (const auto& module : index.modules) {
        // Skip test modules if not including tests
        if (!include_tests && is_test_module(module.name)) {
            continue;
        }

        // Skip re-export modules (mod.tml)
        if (is_reexport_module(module)) {
            continue;
        }

        // Get library name
        std::string lib_name = get_library_name(module);

        // Create library if it doesn't exist
        if (lib_map.find(lib_name) == lib_map.end()) {
            DocLibrary lib;
            lib.name = lib_name;
            lib.description = get_library_description(lib_name);
            lib_map[lib_name] = lib;
        }

        // Add module to library
        lib_map[lib_name].modules.push_back(&module);
    }

    // Convert to vector and sort
    std::vector<DocLibrary> result;
    result.reserve(lib_map.size());

    // Order: core first, then std, then test, then others alphabetically
    std::vector<std::string> priority = {"core", "std", "test"};
    for (const auto& name : priority) {
        auto it = lib_map.find(name);
        if (it != lib_map.end()) {
            result.push_back(std::move(it->second));
            lib_map.erase(it);
        }
    }

    // Add remaining libraries alphabetically
    for (auto& pair : lib_map) {
        result.push_back(std::move(pair.second));
    }

    // Sort modules within each library alphabetically
    for (auto& lib : result) {
        std::sort(lib.modules.begin(), lib.modules.end(),
                  [](const DocModule* a, const DocModule* b) { return a->name < b->name; });

        // Remove duplicates (modules with the same name - keep first)
        lib.modules.erase(
            std::unique(lib.modules.begin(), lib.modules.end(),
                        [](const DocModule* a, const DocModule* b) { return a->name == b->name; }),
            lib.modules.end());
    }

    return result;
}

// ============================================================================
// JSON Generator Implementation
// ============================================================================

JsonGenerator::JsonGenerator(GeneratorConfig config) : config_(std::move(config)) {}

void JsonGenerator::generate(const DocModule& module, std::ostream& out) {
    write_module(module, out, 0);
}

void JsonGenerator::generate(const DocIndex& index, std::ostream& out) {
    write_object_start(out);

    // Metadata
    write_indent(out, 1);
    out << "\"name\": ";
    write_string(index.crate_name, out);
    out << ",\n";

    write_indent(out, 1);
    out << "\"version\": ";
    write_string(index.version, out);
    out << ",\n";

    write_indent(out, 1);
    out << "\"description\": ";
    write_string(index.description, out);
    out << ",\n";

    // Modules
    write_indent(out, 1);
    out << "\"modules\": ";
    write_array_start(out);

    for (size_t i = 0; i < index.modules.size(); ++i) {
        if (i > 0) {
            out << ",";
        }
        out << "\n";
        write_indent(out, 2);
        write_module(index.modules[i], out, 2);
    }

    out << "\n";
    write_array_end(out, 1);
    out << "\n";

    write_object_end(out, 0);
}

void JsonGenerator::generate_file(const DocModule& module, const std::filesystem::path& path) {
    std::ofstream out(path);
    if (out) {
        generate(module, out);
    }
}

void JsonGenerator::generate_file(const DocIndex& index, const std::filesystem::path& path) {
    std::ofstream out(path);
    if (out) {
        generate(index, out);
    }
}

void JsonGenerator::write_item(const DocItem& item, std::ostream& out, int indent) {
    write_object_start(out);

    // Basic fields
    write_indent(out, indent + 1);
    out << "\"id\": ";
    write_string(item.id, out);
    out << ",\n";

    write_indent(out, indent + 1);
    out << "\"name\": ";
    write_string(item.name, out);
    out << ",\n";

    write_indent(out, indent + 1);
    out << "\"kind\": ";
    write_string(std::string(doc_item_kind_to_string(item.kind)), out);
    out << ",\n";

    write_indent(out, indent + 1);
    out << "\"path\": ";
    write_string(item.path, out);
    out << ",\n";

    write_indent(out, indent + 1);
    out << "\"visibility\": ";
    write_string(std::string(doc_visibility_to_string(item.visibility)), out);
    out << ",\n";

    write_indent(out, indent + 1);
    out << "\"signature\": ";
    write_string(item.signature, out);
    out << ",\n";

    // Documentation
    write_indent(out, indent + 1);
    out << "\"doc\": ";
    write_string(item.doc, out);
    out << ",\n";

    write_indent(out, indent + 1);
    out << "\"summary\": ";
    write_string(item.summary, out);

    // Parameters
    if (!item.params.empty()) {
        out << ",\n";
        write_indent(out, indent + 1);
        out << "\"params\": [\n";
        for (size_t i = 0; i < item.params.size(); ++i) {
            write_indent(out, indent + 2);
            out << "{\"name\": ";
            write_string(item.params[i].name, out);
            out << ", \"type\": ";
            write_string(item.params[i].type, out);
            out << ", \"description\": ";
            write_string(item.params[i].description, out);
            out << "}";
            if (i + 1 < item.params.size()) {
                out << ",";
            }
            out << "\n";
        }
        write_indent(out, indent + 1);
        out << "]";
    }

    // Returns
    if (item.returns) {
        out << ",\n";
        write_indent(out, indent + 1);
        out << "\"returns\": {\"type\": ";
        write_string(item.returns->type, out);
        out << ", \"description\": ";
        write_string(item.returns->description, out);
        out << "}";
    }

    // Generics
    if (!item.generics.empty()) {
        out << ",\n";
        write_indent(out, indent + 1);
        out << "\"generics\": [\n";
        for (size_t i = 0; i < item.generics.size(); ++i) {
            write_indent(out, indent + 2);
            out << "{\"name\": ";
            write_string(item.generics[i].name, out);
            out << ", \"bounds\": [";
            for (size_t j = 0; j < item.generics[i].bounds.size(); ++j) {
                if (j > 0) {
                    out << ", ";
                }
                write_string(item.generics[i].bounds[j], out);
            }
            out << "]";
            if (item.generics[i].default_value) {
                out << ", \"default\": ";
                write_string(*item.generics[i].default_value, out);
            }
            out << "}";
            if (i + 1 < item.generics.size()) {
                out << ",";
            }
            out << "\n";
        }
        write_indent(out, indent + 1);
        out << "]";
    }

    // Fields
    if (!item.fields.empty()) {
        out << ",\n";
        write_indent(out, indent + 1);
        out << "\"fields\": [\n";
        for (size_t i = 0; i < item.fields.size(); ++i) {
            write_indent(out, indent + 2);
            write_item(item.fields[i], out, indent + 2);
            if (i + 1 < item.fields.size()) {
                out << ",";
            }
            out << "\n";
        }
        write_indent(out, indent + 1);
        out << "]";
    }

    // Variants
    if (!item.variants.empty()) {
        out << ",\n";
        write_indent(out, indent + 1);
        out << "\"variants\": [\n";
        for (size_t i = 0; i < item.variants.size(); ++i) {
            write_indent(out, indent + 2);
            write_item(item.variants[i], out, indent + 2);
            if (i + 1 < item.variants.size()) {
                out << ",";
            }
            out << "\n";
        }
        write_indent(out, indent + 1);
        out << "]";
    }

    // Methods
    if (!item.methods.empty()) {
        out << ",\n";
        write_indent(out, indent + 1);
        out << "\"methods\": [\n";
        for (size_t i = 0; i < item.methods.size(); ++i) {
            write_indent(out, indent + 2);
            write_item(item.methods[i], out, indent + 2);
            if (i + 1 < item.methods.size()) {
                out << ",";
            }
            out << "\n";
        }
        write_indent(out, indent + 1);
        out << "]";
    }

    out << "\n";
    write_indent(out, indent);
    out << "}";
}

void JsonGenerator::write_module(const DocModule& module, std::ostream& out, int indent) {
    write_object_start(out);

    write_indent(out, indent + 1);
    out << "\"name\": ";
    write_string(module.name, out);
    out << ",\n";

    write_indent(out, indent + 1);
    out << "\"path\": ";
    write_string(module.path, out);
    out << ",\n";

    write_indent(out, indent + 1);
    out << "\"doc\": ";
    write_string(module.doc, out);
    out << ",\n";

    write_indent(out, indent + 1);
    out << "\"summary\": ";
    write_string(module.summary, out);
    out << ",\n";

    // Items
    write_indent(out, indent + 1);
    out << "\"items\": [\n";
    for (size_t i = 0; i < module.items.size(); ++i) {
        write_indent(out, indent + 2);
        write_item(module.items[i], out, indent + 2);
        if (i + 1 < module.items.size()) {
            out << ",";
        }
        out << "\n";
    }
    write_indent(out, indent + 1);
    out << "]\n";

    write_indent(out, indent);
    out << "}";
}

void JsonGenerator::write_string(const std::string& str, std::ostream& out) {
    out << "\"";
    for (char c : str) {
        switch (c) {
        case '"':
            out << "\\\"";
            break;
        case '\\':
            out << "\\\\";
            break;
        case '\n':
            out << "\\n";
            break;
        case '\r':
            out << "\\r";
            break;
        case '\t':
            out << "\\t";
            break;
        default:
            if (static_cast<unsigned char>(c) < 32) {
                // Control character - encode as \uXXXX
                char buf[8];
                snprintf(buf, sizeof(buf), "\\u%04x", static_cast<unsigned char>(c));
                out << buf;
            } else {
                out << c;
            }
            break;
        }
    }
    out << "\"";
}

void JsonGenerator::write_indent(std::ostream& out, int indent) {
    if (!config_.minify) {
        for (int i = 0; i < indent; ++i) {
            out << "  ";
        }
    }
}

void JsonGenerator::write_array_start(std::ostream& out) {
    out << "[";
}

void JsonGenerator::write_array_end(std::ostream& out, int indent) {
    write_indent(out, indent);
    out << "]";
}

void JsonGenerator::write_object_start(std::ostream& out) {
    out << "{\n";
}

void JsonGenerator::write_object_end(std::ostream& out, int indent) {
    write_indent(out, indent);
    out << "}";
}

// ============================================================================
// Markdown Generator Implementation
// ============================================================================

MarkdownGenerator::MarkdownGenerator(GeneratorConfig config) : config_(std::move(config)) {}

void MarkdownGenerator::generate(const DocModule& module, std::ostream& out) {
    // Title
    out << "# " << module.name << "\n\n";

    // Module documentation
    if (!module.doc.empty()) {
        out << module.doc << "\n\n";
    }

    // Group items by kind
    std::vector<const DocItem*> functions, structs, enums, traits, impls, consts, aliases;

    for (const auto& item : module.items) {
        switch (item.kind) {
        case DocItemKind::Function:
        case DocItemKind::Method:
            functions.push_back(&item);
            break;
        case DocItemKind::Struct:
            structs.push_back(&item);
            break;
        case DocItemKind::Enum:
            enums.push_back(&item);
            break;
        case DocItemKind::Trait:
            traits.push_back(&item);
            break;
        case DocItemKind::Impl:
        case DocItemKind::TraitImpl:
            impls.push_back(&item);
            break;
        case DocItemKind::Constant:
            consts.push_back(&item);
            break;
        case DocItemKind::TypeAlias:
            aliases.push_back(&item);
            break;
        default:
            break;
        }
    }

    // Structs
    if (!structs.empty()) {
        out << "## Structs\n\n";
        for (const auto* item : structs) {
            generate_item(*item, out);
        }
    }

    // Enums
    if (!enums.empty()) {
        out << "## Enums\n\n";
        for (const auto* item : enums) {
            generate_item(*item, out);
        }
    }

    // Behaviors
    if (!traits.empty()) {
        out << "## Behaviors\n\n";
        for (const auto* item : traits) {
            generate_item(*item, out);
        }
    }

    // Functions
    if (!functions.empty()) {
        out << "## Functions\n\n";
        for (const auto* item : functions) {
            generate_item(*item, out);
        }
    }

    // Type Aliases
    if (!aliases.empty()) {
        out << "## Type Aliases\n\n";
        for (const auto* item : aliases) {
            generate_item(*item, out);
        }
    }

    // Constants
    if (!consts.empty()) {
        out << "## Constants\n\n";
        for (const auto* item : consts) {
            generate_item(*item, out);
        }
    }
}

void MarkdownGenerator::generate_item(const DocItem& item, std::ostream& out) {
    out << "### " << item.name << "\n\n";

    write_signature(item, out);
    write_description(item, out);
    write_params(item, out);
    write_returns(item, out);
    write_fields(item, out);
    write_methods(item, out);
    write_examples(item, out);
    write_see_also(item, out);

    out << "---\n\n";
}

void MarkdownGenerator::generate_file(const DocModule& module, const std::filesystem::path& path) {
    std::ofstream out(path);
    if (out) {
        generate(module, out);
    }
}

void MarkdownGenerator::generate_directory(const DocIndex& index,
                                           const std::filesystem::path& dir) {
    std::filesystem::create_directories(dir);

    // Generate index
    std::ofstream index_file(dir / "README.md");
    if (index_file) {
        index_file << "# " << index.crate_name << "\n\n";
        if (!index.description.empty()) {
            index_file << index.description << "\n\n";
        }
        index_file << "## Modules\n\n";
        for (const auto& module : index.modules) {
            index_file << "- [" << module.name << "](" << module.name << ".md)";
            if (!module.summary.empty()) {
                index_file << " - " << module.summary;
            }
            index_file << "\n";
        }
    }

    // Generate each module
    for (const auto& module : index.modules) {
        std::string filename = module.name + ".md";
        // Replace :: with _ for nested modules
        for (auto& c : filename) {
            if (c == ':') {
                c = '_';
            }
        }
        generate_file(module, dir / filename);
    }
}

void MarkdownGenerator::write_signature(const DocItem& item, std::ostream& out) {
    out << "```tml\n" << item.signature << "\n```\n\n";
}

void MarkdownGenerator::write_description(const DocItem& item, std::ostream& out) {
    if (!item.doc.empty()) {
        // Write summary first, then details
        out << item.summary << "\n\n";
        // If doc is longer than summary, write the rest
        if (item.doc.length() > item.summary.length()) {
            // Skip first paragraph
            size_t pos = item.summary.length();
            while (pos < item.doc.length() && (item.doc[pos] == '\n' || item.doc[pos] == ' ')) {
                ++pos;
            }
            if (pos < item.doc.length()) {
                out << item.doc.substr(pos) << "\n\n";
            }
        }
    }

    // Deprecated warning
    if (item.deprecated) {
        out << "> **Deprecated**: " << item.deprecated->message << "\n\n";
    }
}

void MarkdownGenerator::write_params(const DocItem& item, std::ostream& out) {
    if (item.params.empty()) {
        return;
    }

    out << "#### Parameters\n\n";
    out << "| Name | Type | Description |\n";
    out << "|------|------|-------------|\n";
    for (const auto& param : item.params) {
        out << "| `" << param.name << "` | `" << param.type << "` | " << param.description
            << " |\n";
    }
    out << "\n";
}

void MarkdownGenerator::write_returns(const DocItem& item, std::ostream& out) {
    if (!item.returns) {
        return;
    }

    out << "#### Returns\n\n";
    out << "`" << item.returns->type << "`";
    if (!item.returns->description.empty()) {
        out << " - " << item.returns->description;
    }
    out << "\n\n";
}

void MarkdownGenerator::write_examples(const DocItem& item, std::ostream& out) {
    if (item.examples.empty()) {
        return;
    }

    out << "#### Examples\n\n";
    for (const auto& example : item.examples) {
        if (!example.description.empty()) {
            out << example.description << "\n\n";
        }
        out << "```" << (example.language.empty() ? "tml" : example.language) << "\n";
        out << example.code << "\n```\n\n";
    }
}

void MarkdownGenerator::write_see_also(const DocItem& item, std::ostream& out) {
    if (item.see_also.empty()) {
        return;
    }

    out << "#### See Also\n\n";
    for (const auto& ref : item.see_also) {
        out << "- `" << ref << "`\n";
    }
    out << "\n";
}

void MarkdownGenerator::write_fields(const DocItem& item, std::ostream& out) {
    if (item.fields.empty() && item.variants.empty()) {
        return;
    }

    if (!item.fields.empty()) {
        out << "#### Fields\n\n";
        out << "| Name | Type | Description |\n";
        out << "|------|------|-------------|\n";
        for (const auto& field : item.fields) {
            out << "| `" << field.name << "` | ";
            if (field.returns) {
                out << "`" << field.returns->type << "`";
            }
            out << " | " << field.summary << " |\n";
        }
        out << "\n";
    }

    if (!item.variants.empty()) {
        out << "#### Variants\n\n";
        for (const auto& variant : item.variants) {
            out << "- `" << variant.signature << "`";
            if (!variant.summary.empty()) {
                out << " - " << variant.summary;
            }
            out << "\n";
        }
        out << "\n";
    }
}

void MarkdownGenerator::write_methods(const DocItem& item, std::ostream& out) {
    if (item.methods.empty()) {
        return;
    }

    out << "#### Methods\n\n";
    for (const auto& method : item.methods) {
        out << "##### `" << method.name << "`\n\n";
        out << "```tml\n" << method.signature << "\n```\n\n";
        if (!method.summary.empty()) {
            out << method.summary << "\n\n";
        }
    }
}

// ============================================================================
// HTML Generator Implementation
// ============================================================================

HtmlGenerator::HtmlGenerator(GeneratorConfig config) : config_(std::move(config)) {}

void HtmlGenerator::generate_site(const DocIndex& index, const std::filesystem::path& output_dir) {
    std::filesystem::create_directories(output_dir);

    // Create pages subdirectory for module pages
    auto pages_dir = output_dir / "pages";
    std::filesystem::create_directories(pages_dir);

    // Organize modules by library, filtering out tests and mod files
    auto libraries = organize_by_library(index, config_.include_tests);

    // Generate index page (in root)
    {
        std::ofstream out(output_dir / "index.html");
        if (out) {
            generate_index_page_with_libraries(libraries, index, out);
        }
    }

    // Generate module pages (in /pages subdirectory) - only for filtered modules
    for (const auto& lib : libraries) {
        for (const auto* module : lib.modules) {
            std::string filename = module->name + ".html";
            for (auto& c : filename) {
                if (c == ':') {
                    c = '_';
                }
            }
            std::ofstream out(pages_dir / filename);
            if (out) {
                // Generate module page with full navigation and correct asset paths
                write_head(module->name, out, "../");

                out << R"(<body>
<div class="layout">
)";

                // Sidebar with full module navigation organized by library
                write_module_sidebar_with_libraries(module->name, *module, libraries, out, "../");

                // Main content
                out << R"(<main class="main-content">
<div class="page-header">
    <h1 class="page-title">)"
                    << escape_html(module->name) << R"(</h1>
)";
                if (!module->doc.empty()) {
                    out << "<p class=\"page-description\">" << escape_html(module->summary)
                        << "</p>\n";
                }
                out << "</div>\n";

                // Group items by kind for better organization
                std::vector<const DocItem*> structs, enums, traits, functions, methods, constants,
                    type_aliases;
                for (const auto& item : module->items) {
                    switch (item.kind) {
                    case DocItemKind::Struct:
                        structs.push_back(&item);
                        break;
                    case DocItemKind::Enum:
                        enums.push_back(&item);
                        break;
                    case DocItemKind::Trait:
                        traits.push_back(&item);
                        break;
                    case DocItemKind::Function:
                        functions.push_back(&item);
                        break;
                    case DocItemKind::Method:
                        methods.push_back(&item);
                        break;
                    case DocItemKind::Constant:
                        constants.push_back(&item);
                        break;
                    case DocItemKind::TypeAlias:
                        type_aliases.push_back(&item);
                        break;
                    default:
                        break;
                    }
                }

                // Structs
                if (!structs.empty()) {
                    out << "<section class=\"item-section\">\n";
                    out << "<h2 class=\"section-title\">Structs</h2>\n";
                    for (const auto* item : structs) {
                        write_item_card(*item, out);
                    }
                    out << "</section>\n";
                }

                // Enums
                if (!enums.empty()) {
                    out << "<section class=\"item-section\">\n";
                    out << "<h2 class=\"section-title\">Enums</h2>\n";
                    for (const auto* item : enums) {
                        write_item_card(*item, out);
                    }
                    out << "</section>\n";
                }

                // Behaviors
                if (!traits.empty()) {
                    out << "<section class=\"item-section\">\n";
                    out << "<h2 class=\"section-title\">Behaviors</h2>\n";
                    for (const auto* item : traits) {
                        write_item_card(*item, out);
                    }
                    out << "</section>\n";
                }

                // Functions
                if (!functions.empty()) {
                    out << "<section class=\"item-section\">\n";
                    out << "<h2 class=\"section-title\">Functions</h2>\n";
                    for (const auto* item : functions) {
                        write_item_card(*item, out);
                    }
                    out << "</section>\n";
                }

                // Constants
                if (!constants.empty()) {
                    out << "<section class=\"item-section\">\n";
                    out << "<h2 class=\"section-title\">Constants</h2>\n";
                    for (const auto* item : constants) {
                        write_item_card(*item, out);
                    }
                    out << "</section>\n";
                }

                // Type Aliases
                if (!type_aliases.empty()) {
                    out << "<section class=\"item-section\">\n";
                    out << "<h2 class=\"section-title\">Type Aliases</h2>\n";
                    for (const auto* item : type_aliases) {
                        write_item_card(*item, out);
                    }
                    out << "</section>\n";
                }

                write_footer(out);
                out << R"(</main>
</div>
<button class="mobile-toggle" aria-label="Toggle menu">☰</button>
)";
                write_scripts(out, "../");
                out << R"(</body>
</html>)";
            }
        }
    }

    // Generate search index (in root) - also filter
    {
        std::ofstream out(output_dir / "search-index.js");
        if (out) {
            generate_search_index_filtered(libraries, out);
        }
    }

    // Generate CSS (in root)
    {
        std::ofstream out(output_dir / "style.css");
        if (out) {
            write_css(out);
        }
    }

    // Generate search JavaScript (in root)
    {
        std::ofstream out(output_dir / "search.js");
        if (out) {
            write_search_js(out);
        }
    }
}

void HtmlGenerator::write_css(std::ostream& out) {
    out << R"(/* TML Documentation - Modern Dark Theme */
:root {
    --bg-primary: #0f1419;
    --bg-secondary: #1a1f25;
    --bg-tertiary: #242a32;
    --bg-hover: #2d353f;
    --text-primary: #e6e6e6;
    --text-secondary: #9ca3af;
    --text-muted: #6b7280;
    --accent-primary: #4fc3f7;
    --accent-secondary: #81d4fa;
    --accent-green: #4ade80;
    --accent-yellow: #fbbf24;
    --accent-purple: #a78bfa;
    --accent-pink: #f472b6;
    --accent-orange: #fb923c;
    --border-color: #374151;
    --code-bg: #1e252e;
    --search-bg: #1a1f25;
    --shadow: 0 4px 6px -1px rgba(0, 0, 0, 0.3);
    --font-sans: 'Inter', -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif;
    --font-mono: 'JetBrains Mono', 'Fira Code', 'Cascadia Code', Consolas, monospace;
}

* { box-sizing: border-box; margin: 0; padding: 0; }

html { scroll-behavior: smooth; }

body {
    font-family: var(--font-sans);
    background: var(--bg-primary);
    color: var(--text-primary);
    line-height: 1.7;
    min-height: 100vh;
}

/* Layout */
.layout {
    display: grid;
    grid-template-columns: 280px 1fr;
    min-height: 100vh;
}

@media (max-width: 900px) {
    .layout {
        grid-template-columns: 1fr;
    }
    .sidebar {
        position: fixed;
        left: -300px;
        transition: left 0.3s ease;
        z-index: 1000;
    }
    .sidebar.open { left: 0; }
    .mobile-toggle { display: flex !important; }
}

/* Sidebar */
.sidebar {
    background: var(--bg-secondary);
    border-right: 1px solid var(--border-color);
    height: 100vh;
    position: sticky;
    top: 0;
    overflow-y: auto;
    display: flex;
    flex-direction: column;
}

.sidebar-header {
    padding: 20px;
    border-bottom: 1px solid var(--border-color);
    background: var(--bg-tertiary);
}

.logo {
    display: flex;
    align-items: center;
    gap: 12px;
    text-decoration: none;
    color: var(--text-primary);
    font-weight: 700;
    font-size: 1.25rem;
}

.logo-icon {
    width: 32px;
    height: 32px;
    background: linear-gradient(135deg, var(--accent-primary), var(--accent-purple));
    border-radius: 8px;
    display: flex;
    align-items: center;
    justify-content: center;
    font-weight: 800;
    font-size: 14px;
}

.version-badge {
    font-size: 0.7rem;
    background: var(--accent-primary);
    color: var(--bg-primary);
    padding: 2px 8px;
    border-radius: 12px;
    font-weight: 600;
}

/* Search */
.search-container {
    padding: 16px 20px;
    border-bottom: 1px solid var(--border-color);
}

.search-box {
    position: relative;
    width: 100%;
}

.search-input {
    width: 100%;
    padding: 10px 16px 10px 40px;
    background: var(--bg-primary);
    border: 1px solid var(--border-color);
    border-radius: 8px;
    color: var(--text-primary);
    font-size: 0.9rem;
    font-family: var(--font-sans);
    transition: all 0.2s ease;
}

.search-input:focus {
    outline: none;
    border-color: var(--accent-primary);
    box-shadow: 0 0 0 3px rgba(79, 195, 247, 0.15);
}

.search-input::placeholder {
    color: var(--text-muted);
}

.search-icon {
    position: absolute;
    left: 12px;
    top: 50%;
    transform: translateY(-50%);
    color: var(--text-muted);
    width: 18px;
    height: 18px;
}

.search-shortcut {
    position: absolute;
    right: 12px;
    top: 50%;
    transform: translateY(-50%);
    background: var(--bg-tertiary);
    color: var(--text-muted);
    padding: 2px 8px;
    border-radius: 4px;
    font-size: 0.75rem;
    font-family: var(--font-mono);
    border: 1px solid var(--border-color);
}

/* Search Results Dropdown */
.search-results {
    position: absolute;
    top: 100%;
    left: 0;
    right: 0;
    margin-top: 8px;
    background: var(--bg-secondary);
    border: 1px solid var(--border-color);
    border-radius: 8px;
    box-shadow: var(--shadow);
    max-height: 400px;
    overflow-y: auto;
    z-index: 1000;
    display: none;
}

.search-results.active { display: block; }

.search-result-item {
    display: flex;
    align-items: center;
    gap: 12px;
    padding: 12px 16px;
    text-decoration: none;
    color: var(--text-primary);
    border-bottom: 1px solid var(--border-color);
    transition: background 0.15s ease;
}

.search-result-item:last-child { border-bottom: none; }
.search-result-item:hover,
.search-result-item.selected { background: var(--bg-hover); }

.result-kind {
    font-size: 0.7rem;
    font-weight: 600;
    padding: 3px 8px;
    border-radius: 4px;
    text-transform: uppercase;
    letter-spacing: 0.5px;
    flex-shrink: 0;
}

.result-kind.function { background: var(--accent-primary); color: var(--bg-primary); }
.result-kind.struct { background: var(--accent-green); color: var(--bg-primary); }
.result-kind.enum { background: var(--accent-yellow); color: var(--bg-primary); }
.result-kind.behavior { background: var(--accent-purple); color: var(--bg-primary); }
.result-kind.method { background: var(--accent-pink); color: var(--bg-primary); }
.result-kind.constant { background: var(--accent-orange); color: var(--bg-primary); }

.result-info { flex: 1; min-width: 0; }
.result-name { font-weight: 600; font-family: var(--font-mono); font-size: 0.9rem; }
.result-path { font-size: 0.8rem; color: var(--text-muted); margin-top: 2px; }

.search-empty {
    padding: 24px;
    text-align: center;
    color: var(--text-muted);
}

/* Navigation */
.nav-section {
    padding: 16px 20px;
    flex: 1;
    overflow-y: auto;
}

.nav-title {
    font-size: 0.7rem;
    font-weight: 700;
    text-transform: uppercase;
    letter-spacing: 1px;
    color: var(--text-muted);
    margin-bottom: 12px;
}

.nav-list { list-style: none; }

.nav-list-collapsed {
    max-height: 200px;
    overflow: hidden;
    position: relative;
}

.nav-list-collapsed::after {
    content: '';
    position: absolute;
    bottom: 0;
    left: 0;
    right: 0;
    height: 40px;
    background: linear-gradient(transparent, var(--bg-secondary));
    pointer-events: none;
}

.nav-list-collapsed.expanded {
    max-height: none;
    overflow: visible;
}

.nav-list-collapsed.expanded::after {
    display: none;
}

.nav-toggle {
    display: block;
    width: 100%;
    padding: 8px 12px;
    margin-top: 8px;
    background: var(--bg-tertiary);
    border: 1px solid var(--border-color);
    border-radius: 6px;
    color: var(--text-secondary);
    font-size: 0.8rem;
    cursor: pointer;
    transition: all 0.15s ease;
}

.nav-toggle:hover {
    background: var(--bg-hover);
    color: var(--text-primary);
}

.nav-item {
    margin-bottom: 4px;
}

.nav-item.active .nav-link {
    background: rgba(79, 195, 247, 0.15);
    color: var(--accent-primary);
    border-left: 3px solid var(--accent-primary);
    margin-left: -3px;
}

.nav-link {
    display: flex;
    align-items: center;
    gap: 10px;
    padding: 8px 12px;
    color: var(--text-secondary);
    text-decoration: none;
    border-radius: 6px;
    font-size: 0.9rem;
    transition: all 0.15s ease;
}

.nav-link:hover {
    background: var(--bg-hover);
    color: var(--text-primary);
}

.nav-link.active {
    background: var(--accent-primary);
    color: var(--bg-primary);
    font-weight: 600;
}

.nav-icon {
    width: 8px;
    height: 8px;
    border-radius: 2px;
    flex-shrink: 0;
}

.nav-icon.module { background: var(--accent-primary); }
.nav-icon.struct { background: var(--accent-green); }
.nav-icon.enum { background: var(--accent-yellow); }
.nav-icon.function { background: var(--accent-purple); }
.nav-icon.behavior { background: var(--accent-pink); }

/* Main Content */
.main-content {
    padding: 40px 60px;
    max-width: 1000px;
}

@media (max-width: 900px) {
    .main-content { padding: 20px; }
}

.mobile-toggle {
    display: none;
    position: fixed;
    bottom: 20px;
    right: 20px;
    width: 50px;
    height: 50px;
    background: var(--accent-primary);
    border: none;
    border-radius: 50%;
    color: var(--bg-primary);
    cursor: pointer;
    box-shadow: var(--shadow);
    align-items: center;
    justify-content: center;
    z-index: 999;
}

/* Page Header */
.page-header {
    margin-bottom: 40px;
    padding-bottom: 24px;
    border-bottom: 1px solid var(--border-color);
}

.page-title {
    font-size: 2.5rem;
    font-weight: 800;
    margin-bottom: 8px;
    background: linear-gradient(135deg, var(--accent-primary), var(--accent-purple));
    -webkit-background-clip: text;
    -webkit-text-fill-color: transparent;
    background-clip: text;
}

.page-description {
    font-size: 1.1rem;
    color: var(--text-secondary);
    line-height: 1.8;
}

/* Item Cards */
.item-section {
    margin-bottom: 48px;
}

.section-title {
    font-size: 1.4rem;
    font-weight: 700;
    margin-bottom: 20px;
    color: var(--text-primary);
    display: flex;
    align-items: center;
    gap: 12px;
}

.section-title::before {
    content: '';
    width: 4px;
    height: 24px;
    background: var(--accent-primary);
    border-radius: 2px;
}

.item-card {
    background: var(--bg-secondary);
    border: 1px solid var(--border-color);
    border-radius: 12px;
    padding: 24px;
    margin-bottom: 16px;
    transition: all 0.2s ease;
}

.item-card:hover {
    border-color: var(--accent-primary);
    box-shadow: 0 0 0 1px var(--accent-primary);
}

.item-header {
    display: flex;
    align-items: flex-start;
    gap: 16px;
    margin-bottom: 16px;
}

.item-kind-badge {
    font-size: 0.7rem;
    font-weight: 700;
    padding: 4px 10px;
    border-radius: 6px;
    text-transform: uppercase;
    letter-spacing: 0.5px;
    flex-shrink: 0;
}

.item-kind-badge.function { background: rgba(79, 195, 247, 0.15); color: var(--accent-primary); }
.item-kind-badge.struct { background: rgba(74, 222, 128, 0.15); color: var(--accent-green); }
.item-kind-badge.enum { background: rgba(251, 191, 36, 0.15); color: var(--accent-yellow); }
.item-kind-badge.behavior { background: rgba(167, 139, 250, 0.15); color: var(--accent-purple); }
.item-kind-badge.method { background: rgba(244, 114, 182, 0.15); color: var(--accent-pink); }
.item-kind-badge.constant { background: rgba(251, 146, 60, 0.15); color: var(--accent-orange); }
.item-kind-badge.field { background: rgba(156, 163, 175, 0.15); color: var(--text-secondary); }

.item-name {
    font-size: 1.2rem;
    font-weight: 700;
    font-family: var(--font-mono);
    color: var(--text-primary);
}

.item-name a {
    color: inherit;
    text-decoration: none;
}

.item-name a:hover { color: var(--accent-primary); }

/* Signature */
.signature {
    background: var(--code-bg);
    border: 1px solid var(--border-color);
    border-radius: 8px;
    padding: 16px 20px;
    margin-bottom: 16px;
    overflow-x: auto;
    font-family: var(--font-mono);
    font-size: 0.9rem;
    line-height: 1.6;
}

.sig-keyword { color: var(--accent-purple); font-weight: 600; }
.sig-name { color: var(--accent-primary); }
.sig-type { color: var(--accent-green); }
.sig-param { color: var(--accent-yellow); }
.sig-punct { color: var(--text-muted); }

/* Description */
.item-description {
    color: var(--text-secondary);
    line-height: 1.8;
}

.item-description p { margin-bottom: 12px; }
.item-description code {
    background: var(--code-bg);
    padding: 2px 6px;
    border-radius: 4px;
    font-family: var(--font-mono);
    font-size: 0.85em;
    color: var(--accent-primary);
}

/* Deprecated Warning */
.deprecated-warning {
    background: rgba(251, 146, 60, 0.1);
    border: 1px solid var(--accent-orange);
    border-radius: 8px;
    padding: 12px 16px;
    margin-bottom: 16px;
    display: flex;
    align-items: center;
    gap: 12px;
}

.deprecated-warning::before {
    content: '⚠';
    font-size: 1.2rem;
}

.deprecated-warning strong {
    color: var(--accent-orange);
}

/* Parameters & Returns */
.params-section, .returns-section {
    margin-top: 20px;
}

.params-title, .returns-title {
    font-size: 0.9rem;
    font-weight: 700;
    color: var(--text-muted);
    text-transform: uppercase;
    letter-spacing: 0.5px;
    margin-bottom: 12px;
}

.params-table {
    width: 100%;
    border-collapse: collapse;
    font-size: 0.9rem;
}

.params-table th,
.params-table td {
    padding: 12px 16px;
    text-align: left;
    border-bottom: 1px solid var(--border-color);
}

.params-table th {
    background: var(--bg-tertiary);
    font-weight: 600;
    color: var(--text-muted);
    text-transform: uppercase;
    font-size: 0.75rem;
    letter-spacing: 0.5px;
}

.params-table tr:last-child td { border-bottom: none; }
.params-table tr:hover td { background: var(--bg-hover); }

.param-name {
    font-family: var(--font-mono);
    color: var(--accent-yellow);
}

.param-type {
    font-family: var(--font-mono);
    color: var(--accent-green);
}

/* Methods Section */
.methods-section {
    margin-top: 24px;
    padding-top: 24px;
    border-top: 1px solid var(--border-color);
}

.methods-title {
    font-size: 1rem;
    font-weight: 700;
    margin-bottom: 16px;
    color: var(--text-primary);
}

.method-item {
    background: var(--bg-tertiary);
    border-radius: 8px;
    padding: 16px;
    margin-bottom: 12px;
}

.method-name {
    font-family: var(--font-mono);
    font-weight: 600;
    color: var(--accent-pink);
    margin-bottom: 8px;
}

.method-sig {
    font-family: var(--font-mono);
    font-size: 0.85rem;
    color: var(--text-secondary);
    background: var(--code-bg);
    padding: 8px 12px;
    border-radius: 4px;
    overflow-x: auto;
}

.method-desc {
    margin-top: 8px;
    color: var(--text-muted);
    font-size: 0.9rem;
}

/* Module List */
.module-list {
    display: grid;
    gap: 16px;
}

.module-card {
    background: var(--bg-secondary);
    border: 1px solid var(--border-color);
    border-radius: 12px;
    padding: 20px 24px;
    text-decoration: none;
    transition: all 0.2s ease;
}

.module-card:hover {
    border-color: var(--accent-primary);
    transform: translateY(-2px);
    box-shadow: var(--shadow);
}

.module-name {
    font-size: 1.1rem;
    font-weight: 700;
    font-family: var(--font-mono);
    color: var(--accent-primary);
    margin-bottom: 6px;
}

.module-summary {
    color: var(--text-secondary);
    font-size: 0.9rem;
}

/* Footer */
.footer {
    margin-top: 60px;
    padding: 24px 0;
    border-top: 1px solid var(--border-color);
    text-align: center;
    color: var(--text-muted);
    font-size: 0.85rem;
}

.footer a {
    color: var(--accent-primary);
    text-decoration: none;
}

.footer a:hover { text-decoration: underline; }

/* Scrollbar */
::-webkit-scrollbar { width: 8px; height: 8px; }
::-webkit-scrollbar-track { background: var(--bg-primary); }
::-webkit-scrollbar-thumb {
    background: var(--border-color);
    border-radius: 4px;
}
::-webkit-scrollbar-thumb:hover { background: var(--text-muted); }

/* Code Blocks */
pre {
    background: var(--code-bg);
    border: 1px solid var(--border-color);
    border-radius: 8px;
    padding: 16px 20px;
    overflow-x: auto;
    font-family: var(--font-mono);
    font-size: 0.9rem;
    line-height: 1.6;
    margin: 16px 0;
}

code {
    font-family: var(--font-mono);
}

/* Examples */
.examples-section {
    margin-top: 20px;
}

.example-block {
    margin-bottom: 16px;
}

.example-title {
    font-size: 0.85rem;
    font-weight: 600;
    color: var(--text-muted);
    margin-bottom: 8px;
}

/* Animations */
@keyframes fadeIn {
    from { opacity: 0; transform: translateY(10px); }
    to { opacity: 1; transform: translateY(0); }
}

.item-card { animation: fadeIn 0.3s ease; }

/* Focus States */
:focus-visible {
    outline: 2px solid var(--accent-primary);
    outline-offset: 2px;
}
)";
}

void HtmlGenerator::write_search_js(std::ostream& out) {
    out << R"(// TML Documentation Search
(function() {
    const searchInput = document.getElementById('search-input');
    const searchResults = document.getElementById('search-results');
    let selectedIndex = -1;
    let currentResults = [];

    if (!searchInput || !searchResults || !window.searchIndex) return;

    function escapeHtml(text) {
        const div = document.createElement('div');
        div.textContent = text;
        return div.innerHTML;
    }

    function getKindClass(kind) {
        const kindMap = {
            'function': 'function',
            'method': 'method',
            'struct': 'struct',
            'enum': 'enum',
            'behavior': 'behavior',
            'trait': 'behavior',
            'constant': 'constant',
            'field': 'field'
        };
        return kindMap[kind] || 'function';
    }

    function search(query) {
        if (!query.trim()) {
            searchResults.classList.remove('active');
            return [];
        }

        const q = query.toLowerCase();
        const results = window.searchIndex.filter(item => {
            const name = (item.name || '').toLowerCase();
            const path = (item.path || '').toLowerCase();
            return name.includes(q) || path.includes(q);
        }).slice(0, 15);

        return results;
    }

    function renderResults(results) {
        if (results.length === 0) {
            searchResults.innerHTML = '<div class="search-empty">No results found</div>';
            searchResults.classList.add('active');
            return;
        }

        searchResults.innerHTML = results.map((item, index) => `
            <a href="${item.module || 'index'}.html#${item.id || item.name}"
               class="search-result-item ${index === selectedIndex ? 'selected' : ''}"
               data-index="${index}">
                <span class="result-kind ${getKindClass(item.kind)}">${escapeHtml(item.kind)}</span>
                <div class="result-info">
                    <div class="result-name">${escapeHtml(item.name)}</div>
                    <div class="result-path">${escapeHtml(item.path || '')}</div>
                </div>
            </a>
        `).join('');
        searchResults.classList.add('active');
    }

    function updateSelection() {
        const items = searchResults.querySelectorAll('.search-result-item');
        items.forEach((item, index) => {
            item.classList.toggle('selected', index === selectedIndex);
        });
        if (selectedIndex >= 0 && items[selectedIndex]) {
            items[selectedIndex].scrollIntoView({ block: 'nearest' });
        }
    }

    searchInput.addEventListener('input', (e) => {
        selectedIndex = -1;
        currentResults = search(e.target.value);
        renderResults(currentResults);
    });

    searchInput.addEventListener('keydown', (e) => {
        const items = searchResults.querySelectorAll('.search-result-item');

        if (e.key === 'ArrowDown') {
            e.preventDefault();
            selectedIndex = Math.min(selectedIndex + 1, items.length - 1);
            updateSelection();
        } else if (e.key === 'ArrowUp') {
            e.preventDefault();
            selectedIndex = Math.max(selectedIndex - 1, -1);
            updateSelection();
        } else if (e.key === 'Enter') {
            e.preventDefault();
            if (selectedIndex >= 0 && items[selectedIndex]) {
                items[selectedIndex].click();
            }
        } else if (e.key === 'Escape') {
            searchResults.classList.remove('active');
            searchInput.blur();
        }
    });

    // Global shortcut: / to focus search
    document.addEventListener('keydown', (e) => {
        if (e.key === '/' && document.activeElement !== searchInput) {
            e.preventDefault();
            searchInput.focus();
        }
    });

    // Close on outside click
    document.addEventListener('click', (e) => {
        if (!searchInput.contains(e.target) && !searchResults.contains(e.target)) {
            searchResults.classList.remove('active');
        }
    });

    // Mobile toggle
    const mobileToggle = document.querySelector('.mobile-toggle');
    const sidebar = document.querySelector('.sidebar');
    if (mobileToggle && sidebar) {
        mobileToggle.addEventListener('click', () => {
            sidebar.classList.toggle('open');
        });
    }
})();

// Toggle modules list expand/collapse
function toggleModulesList() {
    const list = document.getElementById('modules-list');
    const btn = list.parentElement.querySelector('.nav-toggle');
    if (list && btn) {
        list.classList.toggle('expanded');
        btn.textContent = list.classList.contains('expanded') ? 'Show less' : 'Show all modules';
    }
}
)";
}

void HtmlGenerator::generate_module_page(const DocModule& module, std::ostream& out) {
    // Standalone module page (without full site navigation)
    // For full site generation with navigation, use generate_site()
    write_head(module.name, out, "");

    out << R"(<body>
<div class="layout">
<main class="main-content" style="margin-left: 0;">
<div class="page-header">
    <h1 class="page-title">)"
        << escape_html(module.name) << R"(</h1>
)";
    if (!module.doc.empty()) {
        out << "<p class=\"page-description\">" << escape_html(module.summary) << "</p>\n";
    }
    out << "</div>\n";

    // Group items by kind for better organization
    std::vector<const DocItem*> structs, enums, traits, functions, methods, constants, type_aliases;
    for (const auto& item : module.items) {
        switch (item.kind) {
        case DocItemKind::Struct:
            structs.push_back(&item);
            break;
        case DocItemKind::Enum:
            enums.push_back(&item);
            break;
        case DocItemKind::Trait:
            traits.push_back(&item);
            break;
        case DocItemKind::Function:
            functions.push_back(&item);
            break;
        case DocItemKind::Method:
            methods.push_back(&item);
            break;
        case DocItemKind::Constant:
            constants.push_back(&item);
            break;
        case DocItemKind::TypeAlias:
            type_aliases.push_back(&item);
            break;
        default:
            break;
        }
    }

    // Structs
    if (!structs.empty()) {
        out << "<section class=\"item-section\">\n";
        out << "<h2 class=\"section-title\">Structs</h2>\n";
        for (const auto* item : structs) {
            write_item_card(*item, out);
        }
        out << "</section>\n";
    }

    // Enums
    if (!enums.empty()) {
        out << "<section class=\"item-section\">\n";
        out << "<h2 class=\"section-title\">Enums</h2>\n";
        for (const auto* item : enums) {
            write_item_card(*item, out);
        }
        out << "</section>\n";
    }

    // Behaviors
    if (!traits.empty()) {
        out << "<section class=\"item-section\">\n";
        out << "<h2 class=\"section-title\">Behaviors</h2>\n";
        for (const auto* item : traits) {
            write_item_card(*item, out);
        }
        out << "</section>\n";
    }

    // Functions
    if (!functions.empty()) {
        out << "<section class=\"item-section\">\n";
        out << "<h2 class=\"section-title\">Functions</h2>\n";
        for (const auto* item : functions) {
            write_item_card(*item, out);
        }
        out << "</section>\n";
    }

    // Constants
    if (!constants.empty()) {
        out << "<section class=\"item-section\">\n";
        out << "<h2 class=\"section-title\">Constants</h2>\n";
        for (const auto* item : constants) {
            write_item_card(*item, out);
        }
        out << "</section>\n";
    }

    // Type Aliases
    if (!type_aliases.empty()) {
        out << "<section class=\"item-section\">\n";
        out << "<h2 class=\"section-title\">Type Aliases</h2>\n";
        for (const auto* item : type_aliases) {
            write_item_card(*item, out);
        }
        out << "</section>\n";
    }

    write_footer(out);
    out << R"(</main>
</div>
</body>
</html>)";
}

void HtmlGenerator::generate_item_html(const DocItem& item, std::ostream& out) {
    out << "<div class=\"item\">\n";
    write_signature_html(item, out);

    if (!item.summary.empty()) {
        out << "<p>" << escape_html(item.summary) << "</p>\n";
    }

    out << "</div>\n";
}

void HtmlGenerator::generate_index_page(const DocIndex& index, std::ostream& out) {
    write_head(index.crate_name, out, "");

    out << R"(<body>
<div class="layout">
)";

    // Sidebar
    write_sidebar_index(index, out);

    // Main content
    out << R"(<main class="main-content">
<div class="page-header">
    <h1 class="page-title">)"
        << escape_html(index.crate_name) << R"(</h1>
)";
    if (!index.description.empty()) {
        out << "<p class=\"page-description\">" << escape_html(index.description) << "</p>\n";
    }
    out << "</div>\n";

    // Modules section
    out << R"(<section class="item-section">
<h2 class="section-title">Modules</h2>
<div class="module-list">
)";

    for (const auto& module : index.modules) {
        std::string filename = "pages/" + module.name + ".html";
        for (auto& c : filename) {
            if (c == ':') {
                c = '_';
            }
        }
        out << "<a href=\"" << filename << "\" class=\"module-card\">\n";
        out << "  <div class=\"module-name\">" << escape_html(module.name) << "</div>\n";
        if (!module.summary.empty()) {
            out << "  <div class=\"module-summary\">" << escape_html(module.summary) << "</div>\n";
        }
        out << "</a>\n";
    }

    out << R"(</div>
</section>
)";

    write_footer(out);
    out << R"(</main>
</div>
<button class="mobile-toggle" aria-label="Toggle menu">☰</button>
)";
    write_scripts(out, "");
    out << R"(</body>
</html>)";
}

void HtmlGenerator::generate_index_page_with_libraries(const std::vector<DocLibrary>& libraries,
                                                       const DocIndex& index, std::ostream& out) {
    write_head(index.crate_name, out, "");

    out << R"(<body>
<div class="layout">
)";

    // Sidebar with libraries
    write_sidebar_index_with_libraries(libraries, out);

    // Main content
    out << R"(<main class="main-content">
<div class="page-header">
    <h1 class="page-title">)"
        << escape_html(index.crate_name) << R"(</h1>
)";
    if (!index.description.empty()) {
        out << "<p class=\"page-description\">" << escape_html(index.description) << "</p>\n";
    }
    out << "</div>\n";

    // Libraries section - each library is a separate section
    for (const auto& lib : libraries) {
        out << "<section class=\"item-section\">\n";
        out << "<h2 class=\"section-title\">" << escape_html(lib.name) << "</h2>\n";
        if (!lib.description.empty()) {
            out << "<p class=\"library-description\">" << escape_html(lib.description) << "</p>\n";
        }
        out << "<div class=\"module-list\">\n";

        for (const auto* module : lib.modules) {
            std::string filename = "pages/" + module->name + ".html";
            for (auto& c : filename) {
                if (c == ':') {
                    c = '_';
                }
            }
            out << "<a href=\"" << filename << "\" class=\"module-card\">\n";
            out << "  <div class=\"module-name\">" << escape_html(module->name) << "</div>\n";
            if (!module->summary.empty()) {
                out << "  <div class=\"module-summary\">" << escape_html(module->summary)
                    << "</div>\n";
            }
            out << "</a>\n";
        }

        out << "</div>\n</section>\n";
    }

    write_footer(out);
    out << R"(</main>
</div>
<button class="mobile-toggle" aria-label="Toggle menu">☰</button>
)";
    write_scripts(out, "");
    out << R"(</body>
</html>)";
}

void HtmlGenerator::write_sidebar_index_with_libraries(const std::vector<DocLibrary>& libraries,
                                                       std::ostream& out) {
    out << R"(<aside class="sidebar">
    <div class="sidebar-header">
        <a href="index.html" class="logo">
            <span class="logo-icon">TML</span>
            <span>Documentation</span>
        </a>
    </div>
    <div class="search-container">
        <div class="search-box">
            <svg class="search-icon" viewBox="0 0 20 20" fill="currentColor">
                <path fill-rule="evenodd" d="M8 4a4 4 0 100 8 4 4 0 000-8zM2 8a6 6 0 1110.89 3.476l4.817 4.817a1 1 0 01-1.414 1.414l-4.816-4.816A6 6 0 012 8z" clip-rule="evenodd"/>
            </svg>
            <input type="text" id="search-input" class="search-input" placeholder="Search docs..." autocomplete="off">
            <span class="search-shortcut">/</span>
            <div id="search-results" class="search-results"></div>
        </div>
    </div>
)";

    // Each library is a nav section
    for (const auto& lib : libraries) {
        out << "    <nav class=\"nav-section\">\n";
        out << "        <div class=\"nav-title\">" << escape_html(lib.name) << "</div>\n";
        out << "        <ul class=\"nav-list\">\n";

        for (const auto* module : lib.modules) {
            std::string filename = "pages/" + module->name + ".html";
            for (auto& c : filename) {
                if (c == ':')
                    c = '_';
            }
            out << "            <li class=\"nav-item\"><a href=\"" << filename
                << "\" class=\"nav-link\"><span class=\"nav-icon module\"></span>"
                << escape_html(module->name) << "</a></li>\n";
        }

        out << "        </ul>\n    </nav>\n";
    }

    out << "</aside>\n";
}

void HtmlGenerator::write_module_sidebar_with_libraries(const std::string& current_module,
                                                        const DocModule& module,
                                                        const std::vector<DocLibrary>& libraries,
                                                        std::ostream& out,
                                                        const std::string& asset_prefix) {
    out << R"(<aside class="sidebar">
    <div class="sidebar-header">
        <a href=")"
        << asset_prefix << R"(index.html" class="logo">
            <span class="logo-icon">TML</span>
            <span>Documentation</span>
        </a>
    </div>
    <div class="search-container">
        <div class="search-box">
            <svg class="search-icon" viewBox="0 0 20 20" fill="currentColor">
                <path fill-rule="evenodd" d="M8 4a4 4 0 100 8 4 4 0 000-8zM2 8a6 6 0 1110.89 3.476l4.817 4.817a1 1 0 01-1.414 1.414l-4.816-4.816A6 6 0 012 8z" clip-rule="evenodd"/>
            </svg>
            <input type="text" id="search-input" class="search-input" placeholder="Search..." autocomplete="off">
            <span class="search-shortcut">/</span>
            <div id="search-results" class="search-results"></div>
        </div>
    </div>
)";

    // Libraries navigation - collapsed by default, current library expanded
    for (const auto& lib : libraries) {
        bool lib_contains_current = false;
        for (const auto* mod : lib.modules) {
            if (mod->name == current_module) {
                lib_contains_current = true;
                break;
            }
        }

        out << "    <nav class=\"nav-section\">\n";
        out << "        <div class=\"nav-title\">" << escape_html(lib.name) << "</div>\n";
        out << "        <ul class=\"nav-list" << (lib_contains_current ? "" : " nav-list-collapsed")
            << "\" id=\"lib-" << escape_html(lib.name) << "\">\n";

        for (const auto* mod : lib.modules) {
            std::string filename = mod->name + ".html";
            for (auto& c : filename) {
                if (c == ':')
                    c = '_';
            }
            bool is_current = (mod->name == current_module);
            out << "            <li class=\"nav-item" << (is_current ? " active" : "") << "\">"
                << "<a href=\"" << filename << "\" class=\"nav-link\">"
                << "<span class=\"nav-icon module\"></span>" << escape_html(mod->name)
                << "</a></li>\n";
        }

        out << "        </ul>\n";
        if (!lib_contains_current && lib.modules.size() > 5) {
            out << "        <button class=\"nav-toggle\" onclick=\"toggleLibrary('" << lib.name
                << "')\">Show " << lib.name << "</button>\n";
        }
        out << "    </nav>\n";
    }

    // Current module items section
    out << R"(    <nav class="nav-section">
        <div class="nav-title">)"
        << escape_html(current_module) << R"(</div>
        <ul class="nav-list">
)";

    // Group items by kind
    std::vector<const DocItem*> structs, enums, traits, functions;
    for (const auto& item : module.items) {
        switch (item.kind) {
        case DocItemKind::Struct:
            structs.push_back(&item);
            break;
        case DocItemKind::Enum:
            enums.push_back(&item);
            break;
        case DocItemKind::Trait:
            traits.push_back(&item);
            break;
        case DocItemKind::Function:
            functions.push_back(&item);
            break;
        default:
            break;
        }
    }

    for (const auto* item : structs) {
        out << "            <li class=\"nav-item\"><a href=\"#" << escape_html(item->id)
            << "\" class=\"nav-link\"><span class=\"nav-icon struct\"></span>"
            << escape_html(item->name) << "</a></li>\n";
    }
    for (const auto* item : enums) {
        out << "            <li class=\"nav-item\"><a href=\"#" << escape_html(item->id)
            << "\" class=\"nav-link\"><span class=\"nav-icon enum\"></span>"
            << escape_html(item->name) << "</a></li>\n";
    }
    for (const auto* item : traits) {
        out << "            <li class=\"nav-item\"><a href=\"#" << escape_html(item->id)
            << "\" class=\"nav-link\"><span class=\"nav-icon behavior\"></span>"
            << escape_html(item->name) << "</a></li>\n";
    }
    for (const auto* item : functions) {
        out << "            <li class=\"nav-item\"><a href=\"#" << escape_html(item->id)
            << "\" class=\"nav-link\"><span class=\"nav-icon function\"></span>"
            << escape_html(item->name) << "</a></li>\n";
    }

    out << R"(        </ul>
    </nav>
</aside>
)";
}

void HtmlGenerator::generate_search_index_filtered(const std::vector<DocLibrary>& libraries,
                                                   std::ostream& out) {
    out << "window.searchIndex = [\n";

    bool first = true;

    auto add_item = [&](const DocItem& item, const std::string& module_name) {
        if (!first) {
            out << ",\n";
        }
        first = false;

        std::string escaped_summary;
        for (char c : item.summary) {
            if (c == '"')
                escaped_summary += "\\\"";
            else if (c == '\\')
                escaped_summary += "\\\\";
            else if (c == '\n')
                escaped_summary += " ";
            else if (c == '\r')
                continue;
            else
                escaped_summary += c;
        }

        std::string module_file = module_name;
        for (auto& c : module_file) {
            if (c == ':')
                c = '_';
        }

        out << "  {\"name\": \"" << item.name << "\", \"id\": \"" << item.id << "\", \"path\": \""
            << item.path << "\", \"kind\": \"" << doc_item_kind_to_string(item.kind)
            << "\", \"module\": \"pages/" << module_file << "\", \"summary\": \"" << escaped_summary
            << "\"}";
    };

    std::function<void(const std::vector<DocItem>&, const std::string&)> add_items;
    add_items = [&](const std::vector<DocItem>& items, const std::string& module_name) {
        for (const auto& item : items) {
            add_item(item, module_name);
            add_items(item.methods, module_name);
            add_items(item.fields, module_name);
            add_items(item.variants, module_name);
        }
    };

    for (const auto& lib : libraries) {
        for (const auto* module : lib.modules) {
            add_items(module->items, module->name);
        }
    }

    out << "\n];\n";
}

void HtmlGenerator::generate_search_index(const DocIndex& index, std::ostream& out) {
    out << "window.searchIndex = [\n";

    bool first = true;

    // Helper to add an item to the search index
    auto add_item = [&](const DocItem& item, const std::string& module_name) {
        if (!first) {
            out << ",\n";
        }
        first = false;

        // Escape strings for JSON
        std::string escaped_summary;
        for (char c : item.summary) {
            if (c == '"')
                escaped_summary += "\\\"";
            else if (c == '\\')
                escaped_summary += "\\\\";
            else if (c == '\n')
                escaped_summary += " ";
            else if (c == '\r')
                continue;
            else
                escaped_summary += c;
        }

        std::string module_file = module_name;
        for (auto& c : module_file) {
            if (c == ':')
                c = '_';
        }

        // URLs point to pages/ subdirectory
        out << "  {\"name\": \"" << item.name << "\", \"id\": \"" << item.id << "\", \"path\": \""
            << item.path << "\", \"kind\": \"" << doc_item_kind_to_string(item.kind)
            << "\", \"module\": \"pages/" << module_file << "\", \"summary\": \"" << escaped_summary
            << "\"}";
    };

    // Helper to recursively add items
    std::function<void(const std::vector<DocItem>&, const std::string&)> add_items;
    add_items = [&](const std::vector<DocItem>& items, const std::string& module_name) {
        for (const auto& item : items) {
            add_item(item, module_name);
            add_items(item.methods, module_name);
            add_items(item.fields, module_name);
            add_items(item.variants, module_name);
        }
    };

    for (const auto& module : index.modules) {
        add_items(module.items, module.name);
    }

    out << "\n];\n";
}

void HtmlGenerator::write_head(const std::string& title, std::ostream& out,
                               const std::string& asset_prefix) {
    out << "<!DOCTYPE html>\n<html lang=\"en\">\n<head>\n";
    out << "  <meta charset=\"UTF-8\">\n";
    out << "  <meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">\n";
    out << "  <title>" << escape_html(title) << " - TML Documentation</title>\n";
    out << "  <link rel=\"stylesheet\" href=\"" << asset_prefix << "style.css\">\n";
    out << "</head>\n";
}

void HtmlGenerator::write_scripts(std::ostream& out, const std::string& asset_prefix) {
    out << "<script src=\"" << asset_prefix << "search-index.js\"></script>\n";
    out << "<script src=\"" << asset_prefix << "search.js\"></script>\n";
}

void HtmlGenerator::write_navigation(const DocIndex& index, std::ostream& out) {
    out << "<nav>\n";
    out << "  <a href=\"index.html\">Home</a>\n";
    for (const auto& module : index.modules) {
        std::string filename = module.name + ".html";
        out << "  <a href=\"" << filename << "\">" << escape_html(module.name) << "</a>\n";
    }
    out << "</nav>\n";
}

void HtmlGenerator::write_item_section(const DocItem& item, std::ostream& out) {
    out << "<div class=\"item\" id=\"" << escape_html(item.id) << "\">\n";

    // Title
    out << "<h3>" << escape_html(item.name) << "</h3>\n";

    write_signature_html(item, out);

    // Deprecated warning
    if (item.deprecated) {
        out << "<div class=\"deprecated\"><strong>Deprecated:</strong> "
            << escape_html(item.deprecated->message) << "</div>\n";
    }

    // Description
    if (!item.doc.empty()) {
        out << "<div class=\"description\">" << markdown_to_html(item.doc) << "</div>\n";
    }

    // Parameters
    if (!item.params.empty()) {
        out << "<h4>Parameters</h4>\n<table>\n";
        out << "<tr><th>Name</th><th>Type</th><th>Description</th></tr>\n";
        for (const auto& param : item.params) {
            out << "<tr><td><code>" << escape_html(param.name) << "</code></td>";
            out << "<td><code>" << escape_html(param.type) << "</code></td>";
            out << "<td>" << escape_html(param.description) << "</td></tr>\n";
        }
        out << "</table>\n";
    }

    // Returns
    if (item.returns) {
        out << "<h4>Returns</h4>\n";
        out << "<p><code>" << escape_html(item.returns->type) << "</code>";
        if (!item.returns->description.empty()) {
            out << " - " << escape_html(item.returns->description);
        }
        out << "</p>\n";
    }

    // Methods
    if (!item.methods.empty()) {
        out << "<h4>Methods</h4>\n";
        for (const auto& method : item.methods) {
            out << "<div class=\"method\">\n";
            out << "<h5>" << escape_html(method.name) << "</h5>\n";
            out << "<pre class=\"signature\">" << escape_html(method.signature) << "</pre>\n";
            if (!method.summary.empty()) {
                out << "<p>" << escape_html(method.summary) << "</p>\n";
            }
            out << "</div>\n";
        }
    }

    out << "</div>\n";
}

void HtmlGenerator::write_signature_html(const DocItem& item, std::ostream& out) {
    out << "<pre class=\"signature\">" << escape_html(item.signature) << "</pre>\n";
}

void HtmlGenerator::write_module_sidebar(const std::string& current_module, const DocModule& module,
                                         const DocIndex& index, std::ostream& out,
                                         const std::string& asset_prefix) {
    out << R"(<aside class="sidebar">
    <div class="sidebar-header">
        <a href=")"
        << asset_prefix << R"(index.html" class="logo">
            <span class="logo-icon">TML</span>
            <span>Documentation</span>
        </a>
    </div>
    <div class="search-container">
        <div class="search-box">
            <svg class="search-icon" viewBox="0 0 20 20" fill="currentColor">
                <path fill-rule="evenodd" d="M8 4a4 4 0 100 8 4 4 0 000-8zM2 8a6 6 0 1110.89 3.476l4.817 4.817a1 1 0 01-1.414 1.414l-4.816-4.816A6 6 0 012 8z" clip-rule="evenodd"/>
            </svg>
            <input type="text" id="search-input" class="search-input" placeholder="Search..." autocomplete="off">
            <span class="search-shortcut">/</span>
            <div id="search-results" class="search-results"></div>
        </div>
    </div>
)";

    // All modules navigation section
    out << R"(    <nav class="nav-section">
        <div class="nav-title">Modules</div>
        <ul class="nav-list nav-list-collapsed" id="modules-list">
)";

    for (const auto& mod : index.modules) {
        std::string filename = mod.name + ".html";
        for (auto& c : filename) {
            if (c == ':')
                c = '_';
        }
        bool is_current = (mod.name == current_module);
        out << "            <li class=\"nav-item" << (is_current ? " active" : "") << "\">"
            << "<a href=\"" << filename << "\" class=\"nav-link\">"
            << "<span class=\"nav-icon module\"></span>" << escape_html(mod.name) << "</a></li>\n";
    }

    out << R"HTML(        </ul>
        <button class="nav-toggle" onclick="toggleModulesList()">Show all modules</button>
    </nav>
)HTML";

    // Current module items section
    out << R"(    <nav class="nav-section">
        <div class="nav-title">)"
        << escape_html(current_module) << R"(</div>
        <ul class="nav-list">
)";

    // Group items by kind
    std::vector<const DocItem*> structs, enums, traits, functions;
    for (const auto& item : module.items) {
        switch (item.kind) {
        case DocItemKind::Struct:
            structs.push_back(&item);
            break;
        case DocItemKind::Enum:
            enums.push_back(&item);
            break;
        case DocItemKind::Trait:
            traits.push_back(&item);
            break;
        case DocItemKind::Function:
            functions.push_back(&item);
            break;
        default:
            break;
        }
    }

    // Structs
    for (const auto* item : structs) {
        out << "            <li class=\"nav-item\"><a href=\"#" << escape_html(item->id)
            << "\" class=\"nav-link\"><span class=\"nav-icon struct\"></span>"
            << escape_html(item->name) << "</a></li>\n";
    }

    // Enums
    for (const auto* item : enums) {
        out << "            <li class=\"nav-item\"><a href=\"#" << escape_html(item->id)
            << "\" class=\"nav-link\"><span class=\"nav-icon enum\"></span>"
            << escape_html(item->name) << "</a></li>\n";
    }

    // Behaviors
    for (const auto* item : traits) {
        out << "            <li class=\"nav-item\"><a href=\"#" << escape_html(item->id)
            << "\" class=\"nav-link\"><span class=\"nav-icon behavior\"></span>"
            << escape_html(item->name) << "</a></li>\n";
    }

    // Functions
    for (const auto* item : functions) {
        out << "            <li class=\"nav-item\"><a href=\"#" << escape_html(item->id)
            << "\" class=\"nav-link\"><span class=\"nav-icon function\"></span>"
            << escape_html(item->name) << "</a></li>\n";
    }

    out << R"(        </ul>
    </nav>
</aside>
)";
}

void HtmlGenerator::write_sidebar_index(const DocIndex& index, std::ostream& out) {
    out << R"(<aside class="sidebar">
    <div class="sidebar-header">
        <a href="index.html" class="logo">
            <span class="logo-icon">TML</span>
            <span>Documentation</span>
        </a>
)";
    if (!index.version.empty()) {
        out << "        <span class=\"version-badge\">v" << escape_html(index.version)
            << "</span>\n";
    }
    out << R"(    </div>
    <div class="search-container">
        <div class="search-box">
            <svg class="search-icon" viewBox="0 0 20 20" fill="currentColor">
                <path fill-rule="evenodd" d="M8 4a4 4 0 100 8 4 4 0 000-8zM2 8a6 6 0 1110.89 3.476l4.817 4.817a1 1 0 01-1.414 1.414l-4.816-4.816A6 6 0 012 8z" clip-rule="evenodd"/>
            </svg>
            <input type="text" id="search-input" class="search-input" placeholder="Search docs..." autocomplete="off">
            <span class="search-shortcut">/</span>
            <div id="search-results" class="search-results"></div>
        </div>
    </div>
    <nav class="nav-section">
        <div class="nav-title">Modules</div>
        <ul class="nav-list">
)";

    for (const auto& module : index.modules) {
        std::string filename = "pages/" + module.name + ".html";
        for (auto& c : filename) {
            if (c == ':')
                c = '_';
        }
        out << "            <li class=\"nav-item\"><a href=\"" << filename
            << "\" class=\"nav-link\"><span class=\"nav-icon module\"></span>"
            << escape_html(module.name) << "</a></li>\n";
    }

    out << R"(        </ul>
    </nav>
</aside>
)";
}

void HtmlGenerator::write_item_card(const DocItem& item, std::ostream& out) {
    std::string kind_class;
    switch (item.kind) {
    case DocItemKind::Function:
        kind_class = "function";
        break;
    case DocItemKind::Method:
        kind_class = "method";
        break;
    case DocItemKind::Struct:
        kind_class = "struct";
        break;
    case DocItemKind::Enum:
        kind_class = "enum";
        break;
    case DocItemKind::Trait:
        kind_class = "behavior";
        break;
    case DocItemKind::Constant:
        kind_class = "constant";
        break;
    case DocItemKind::Field:
        kind_class = "field";
        break;
    default:
        kind_class = "function";
        break;
    }

    out << "<article class=\"item-card\" id=\"" << escape_html(item.id) << "\">\n";

    // Header with badge and name
    out << "  <div class=\"item-header\">\n";
    out << "    <span class=\"item-kind-badge " << kind_class << "\">"
        << doc_item_kind_to_string(item.kind) << "</span>\n";
    out << "    <h3 class=\"item-name\"><a href=\"#" << escape_html(item.id) << "\">"
        << escape_html(item.name) << "</a></h3>\n";
    out << "  </div>\n";

    // Signature
    out << "  <div class=\"signature\">" << escape_html(item.signature) << "</div>\n";

    // Deprecated warning
    if (item.deprecated) {
        out << "  <div class=\"deprecated-warning\"><strong>Deprecated:</strong> "
            << escape_html(item.deprecated->message) << "</div>\n";
    }

    // Description
    if (!item.doc.empty()) {
        out << "  <div class=\"item-description\">" << markdown_to_html(item.doc) << "</div>\n";
    }

    // Parameters
    if (!item.params.empty()) {
        out << "  <div class=\"params-section\">\n";
        out << "    <div class=\"params-title\">Parameters</div>\n";
        out << "    <table class=\"params-table\">\n";
        out << "      <thead><tr><th>Name</th><th>Type</th><th>Description</th></tr></thead>\n";
        out << "      <tbody>\n";
        for (const auto& param : item.params) {
            out << "        <tr><td class=\"param-name\">" << escape_html(param.name)
                << "</td><td class=\"param-type\">" << escape_html(param.type) << "</td><td>"
                << escape_html(param.description) << "</td></tr>\n";
        }
        out << "      </tbody>\n";
        out << "    </table>\n";
        out << "  </div>\n";
    }

    // Returns
    if (item.returns) {
        out << "  <div class=\"returns-section\">\n";
        out << "    <div class=\"returns-title\">Returns</div>\n";
        out << "    <p><code class=\"param-type\">" << escape_html(item.returns->type) << "</code>";
        if (!item.returns->description.empty()) {
            out << " &mdash; " << escape_html(item.returns->description);
        }
        out << "</p>\n";
        out << "  </div>\n";
    }

    // Methods
    if (!item.methods.empty()) {
        out << "  <div class=\"methods-section\">\n";
        out << "    <div class=\"methods-title\">Methods</div>\n";
        for (const auto& method : item.methods) {
            out << "    <div class=\"method-item\" id=\"" << escape_html(method.id) << "\">\n";
            out << "      <div class=\"method-name\">" << escape_html(method.name) << "</div>\n";
            out << "      <div class=\"method-sig\">" << escape_html(method.signature)
                << "</div>\n";
            if (!method.summary.empty()) {
                out << "      <div class=\"method-desc\">" << escape_html(method.summary)
                    << "</div>\n";
            }
            out << "    </div>\n";
        }
        out << "  </div>\n";
    }

    // Variants (for enums)
    if (!item.variants.empty()) {
        out << "  <div class=\"methods-section\">\n";
        out << "    <div class=\"methods-title\">Variants</div>\n";
        for (const auto& variant : item.variants) {
            out << "    <div class=\"method-item\">\n";
            out << "      <div class=\"method-name\">" << escape_html(variant.name) << "</div>\n";
            out << "      <div class=\"method-sig\">" << escape_html(variant.signature)
                << "</div>\n";
            if (!variant.summary.empty()) {
                out << "      <div class=\"method-desc\">" << escape_html(variant.summary)
                    << "</div>\n";
            }
            out << "    </div>\n";
        }
        out << "  </div>\n";
    }

    // Fields (for structs)
    if (!item.fields.empty()) {
        out << "  <div class=\"methods-section\">\n";
        out << "    <div class=\"methods-title\">Fields</div>\n";
        for (const auto& field : item.fields) {
            out << "    <div class=\"method-item\">\n";
            out << "      <div class=\"method-name\">" << escape_html(field.name) << "</div>\n";
            if (field.returns) {
                out << "      <div class=\"method-sig\">" << escape_html(field.returns->type)
                    << "</div>\n";
            }
            if (!field.summary.empty()) {
                out << "      <div class=\"method-desc\">" << escape_html(field.summary)
                    << "</div>\n";
            }
            out << "    </div>\n";
        }
        out << "  </div>\n";
    }

    out << "</article>\n";
}

void HtmlGenerator::write_footer(std::ostream& out) {
    out << R"(<footer class="footer">
    <p>Generated by <a href="https://github.com/tml-lang/tml">TML Documentation Generator</a></p>
</footer>
)";
}

std::string HtmlGenerator::escape_html(const std::string& text) {
    std::string result;
    result.reserve(text.size());
    for (char c : text) {
        switch (c) {
        case '&':
            result += "&amp;";
            break;
        case '<':
            result += "&lt;";
            break;
        case '>':
            result += "&gt;";
            break;
        case '"':
            result += "&quot;";
            break;
        case '\'':
            result += "&#39;";
            break;
        default:
            result += c;
            break;
        }
    }
    return result;
}

std::string HtmlGenerator::markdown_to_html(const std::string& markdown) {
    // Simple markdown to HTML conversion
    // For production, use a proper markdown library
    std::string result;
    std::istringstream stream(markdown);
    std::string line;
    bool in_code_block = false;
    bool in_paragraph = false;

    while (std::getline(stream, line)) {
        // Code blocks
        if (line.starts_with("```")) {
            if (in_code_block) {
                result += "</pre>\n";
                in_code_block = false;
            } else {
                if (in_paragraph) {
                    result += "</p>\n";
                    in_paragraph = false;
                }
                result += "<pre><code>";
                in_code_block = true;
            }
            continue;
        }

        if (in_code_block) {
            result += escape_html(line) + "\n";
            continue;
        }

        // Headers
        if (line.starts_with("### ")) {
            if (in_paragraph) {
                result += "</p>\n";
                in_paragraph = false;
            }
            result += "<h4>" + escape_html(line.substr(4)) + "</h4>\n";
            continue;
        }
        if (line.starts_with("## ")) {
            if (in_paragraph) {
                result += "</p>\n";
                in_paragraph = false;
            }
            result += "<h3>" + escape_html(line.substr(3)) + "</h3>\n";
            continue;
        }
        if (line.starts_with("# ")) {
            if (in_paragraph) {
                result += "</p>\n";
                in_paragraph = false;
            }
            result += "<h2>" + escape_html(line.substr(2)) + "</h2>\n";
            continue;
        }

        // Empty line ends paragraph
        if (line.empty()) {
            if (in_paragraph) {
                result += "</p>\n";
                in_paragraph = false;
            }
            continue;
        }

        // Regular text
        if (!in_paragraph) {
            result += "<p>";
            in_paragraph = true;
        } else {
            result += " ";
        }
        result += escape_html(line);
    }

    if (in_paragraph) {
        result += "</p>\n";
    }
    if (in_code_block) {
        result += "</code></pre>\n";
    }

    return result;
}

} // namespace tml::doc
