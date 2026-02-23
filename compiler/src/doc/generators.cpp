TML_MODULE("tools")

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

} // namespace tml::doc
