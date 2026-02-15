//! # MCP Documentation Get/List/Resolve Handlers
//!
//! Handlers for docs/get, docs/list, and docs/resolve tools.
//! These provide direct access to the documentation index.

#include "doc/doc_model.hpp"
#include "doc/extractor.hpp"
#include "mcp_tools_internal.hpp"

#include <algorithm>
#include <cctype>
#include <iomanip>
#include <map>

namespace tml::mcp {

// ============================================================================
// Documentation Get/List/Resolve Tool Definitions
// ============================================================================

auto make_docs_get_tool() -> Tool {
    return Tool{.name = "docs/get",
                .description = "Get full documentation for an item by its qualified path",
                .parameters = {
                    {"id", "string", "Fully qualified item path (e.g. core::str::split)", true},
                }};
}

auto make_docs_list_tool() -> Tool {
    return Tool{
        .name = "docs/list",
        .description = "List all documentation items in a module",
        .parameters = {
            {"module", "string", "Module path (e.g. core::str, std::json)", true},
            {"kind", "string",
             "Filter by item kind: function, method, struct, enum, behavior, constant", false},
        }};
}

auto make_docs_resolve_tool() -> Tool {
    return Tool{.name = "docs/resolve",
                .description = "Resolve a short name to its fully qualified path(s)",
                .parameters = {
                    {"name", "string", "Short name to resolve (e.g. HashMap, split)", true},
                    {"limit", "number", "Maximum results (default: 5)", false},
                }};
}

// ============================================================================
// Full Item Formatter
// ============================================================================

/// Formats a full documentation view for a single item (used by docs/get).
/// Shows all fields: full doc text, params, returns, examples, children, etc.
static void format_full_item(std::stringstream& out, const doc::DocItem& item,
                             const std::string& module_path) {
    auto kind_str = doc::doc_item_kind_to_string(item.kind);
    auto vis_str = doc::doc_visibility_to_string(item.visibility);

    out << "# " << module_path << "::" << item.name << "\n\n";
    out << "Kind:       " << kind_str << "\n";
    out << "Visibility: " << vis_str << "\n";
    out << "Module:     " << module_path << "\n";

    if (!item.source_file.empty()) {
        out << "Source:     " << item.source_file;
        if (item.source_line > 0)
            out << ":" << item.source_line;
        out << "\n";
    }

    if (!item.signature.empty()) {
        out << "\n```tml\n" << item.signature << "\n```\n";
    }

    // Full documentation text
    if (!item.doc.empty()) {
        out << "\n" << item.doc << "\n";
    }

    // Parameters
    if (!item.params.empty()) {
        out << "\n## Parameters\n\n";
        for (const auto& p : item.params) {
            if (p.name == "this")
                continue;
            out << "- **" << p.name << "**";
            if (!p.type.empty())
                out << ": `" << p.type << "`";
            if (!p.description.empty())
                out << " - " << p.description;
            out << "\n";
        }
    }

    // Returns
    if (item.returns) {
        out << "\n## Returns\n\n";
        if (!item.returns->type.empty())
            out << "Type: `" << item.returns->type << "`\n";
        if (!item.returns->description.empty())
            out << item.returns->description << "\n";
    }

    // Throws
    if (!item.throws.empty()) {
        out << "\n## Throws\n\n";
        for (const auto& t : item.throws) {
            out << "- **" << t.error_type << "**";
            if (!t.description.empty())
                out << " - " << t.description;
            out << "\n";
        }
    }

    // Examples
    if (!item.examples.empty()) {
        out << "\n## Examples\n\n";
        for (const auto& ex : item.examples) {
            if (!ex.description.empty())
                out << ex.description << "\n\n";
            out << "```" << (ex.language.empty() ? "tml" : ex.language) << "\n";
            out << ex.code << "\n```\n\n";
        }
    }

    // Deprecation
    if (item.deprecated) {
        out << "\n## Deprecated\n\n";
        out << item.deprecated->message << "\n";
        if (!item.deprecated->since.empty())
            out << "Since: " << item.deprecated->since << "\n";
        if (!item.deprecated->replacement.empty())
            out << "Use instead: " << item.deprecated->replacement << "\n";
    }

    // Generic parameters
    if (!item.generics.empty()) {
        out << "\n## Type Parameters\n\n";
        for (const auto& g : item.generics) {
            out << "- **" << g.name << "**";
            if (!g.bounds.empty()) {
                out << ": ";
                for (size_t i = 0; i < g.bounds.size(); ++i) {
                    if (i > 0)
                        out << " + ";
                    out << g.bounds[i];
                }
            }
            if (g.default_value)
                out << " = " << *g.default_value;
            out << "\n";
        }
    }

    // Fields (for structs)
    if (!item.fields.empty()) {
        out << "\n## Fields\n\n";
        for (const auto& f : item.fields) {
            out << "- **" << f.name << "**";
            if (!f.signature.empty())
                out << ": `" << f.signature << "`";
            if (!f.summary.empty())
                out << " - " << f.summary;
            out << "\n";
        }
    }

    // Variants (for enums)
    if (!item.variants.empty()) {
        out << "\n## Variants\n\n";
        for (const auto& v : item.variants) {
            out << "- **" << v.name << "**";
            if (!v.signature.empty())
                out << "(" << v.signature << ")";
            if (!v.summary.empty())
                out << " - " << v.summary;
            out << "\n";
        }
    }

    // Methods
    if (!item.methods.empty()) {
        out << "\n## Methods\n\n";
        for (const auto& m : item.methods) {
            out << "- `" << m.signature << "`";
            if (!m.summary.empty())
                out << " - " << m.summary;
            out << "\n";
        }
    }

    // Super traits (for behaviors)
    if (!item.super_traits.empty()) {
        out << "\n## Super Traits\n\n";
        for (const auto& t : item.super_traits) {
            out << "- " << t << "\n";
        }
    }

    // Associated types
    if (!item.associated_types.empty()) {
        out << "\n## Associated Types\n\n";
        for (const auto& at : item.associated_types) {
            out << "- **" << at.name << "**";
            if (!at.summary.empty())
                out << " - " << at.summary;
            out << "\n";
        }
    }

    // See also
    if (!item.see_also.empty()) {
        out << "\n## See Also\n\n";
        for (const auto& s : item.see_also) {
            out << "- " << s << "\n";
        }
    }

    // Since
    if (item.since) {
        out << "\nSince: " << *item.since << "\n";
    }
}

// ============================================================================
// Documentation Get/List/Resolve Handlers
// ============================================================================

auto handle_docs_get(const json::JsonValue& params) -> ToolResult {
    auto* id_param = params.get("id");
    if (id_param == nullptr || !id_param->is_string()) {
        return ToolResult::error("Missing or invalid 'id' parameter");
    }
    std::string id = id_param->as_string();

    ensure_doc_index();
    if (!is_doc_cache_initialized()) {
        return ToolResult::error("Documentation index not available");
    }

    const auto& all_items = get_doc_all_items();

    // Search for item by qualified path (exact match preferred)
    const doc::DocItem* best_match = nullptr;
    std::string best_mod_path;
    int best_priority = 0; // higher = better match

    for (const auto& [item, mod_path] : all_items) {
        std::string qualified = mod_path + "::" + item->name;

        if (qualified == id) {
            // Exact qualified match — best possible
            best_match = item;
            best_mod_path = mod_path;
            break;
        }
        if (item->path == id && best_priority < 3) {
            best_match = item;
            best_mod_path = mod_path;
            best_priority = 3;
        }
        if (item->name == id && best_priority < 1) {
            best_match = item;
            best_mod_path = mod_path;
            best_priority = 1;
        }
    }

    if (best_match != nullptr) {
        std::stringstream out;
        format_full_item(out, *best_match, best_mod_path);
        return ToolResult::text(out.str());
    }

    return ToolResult::text("Item not found: " + id +
                            "\n\nTip: Use docs/search to find the correct qualified name.");
}

auto handle_docs_list(const json::JsonValue& params) -> ToolResult {
    auto* module_param = params.get("module");
    if (module_param == nullptr || !module_param->is_string()) {
        return ToolResult::error("Missing or invalid 'module' parameter");
    }
    std::string module_path = module_param->as_string();

    std::optional<doc::DocItemKind> kind_filter;
    auto* kind_param = params.get("kind");
    if (kind_param != nullptr && kind_param->is_string()) {
        kind_filter = parse_kind_filter(kind_param->as_string());
    }

    ensure_doc_index();
    if (!is_doc_cache_initialized()) {
        return ToolResult::error("Documentation index not available");
    }

    const auto& all_items = get_doc_all_items();

    // Group items by kind for organized output
    std::map<doc::DocItemKind, std::vector<const doc::DocItem*>> by_kind;
    int total = 0;

    for (const auto& [item, mod_path] : all_items) {
        if (!icontains(mod_path, module_path))
            continue;
        if (kind_filter && item->kind != *kind_filter)
            continue;
        by_kind[item->kind].push_back(item);
        ++total;
    }

    std::stringstream out;
    out << "# Module: " << module_path << "\n\n";

    if (total == 0) {
        out << "No items found in module '" << module_path << "'.\n";
        out << "\nAvailable modules: core, core::str, core::num, core::slice, "
               "core::iter, core::cmp, core::fmt, std::json, std::hash, "
               "std::collections, std::os, std::crypto, std::search, ...\n";
        return ToolResult::text(out.str());
    }

    // Display order for item kinds
    const doc::DocItemKind kind_order[] = {
        doc::DocItemKind::Struct,    doc::DocItemKind::Enum,     doc::DocItemKind::Trait,
        doc::DocItemKind::TypeAlias, doc::DocItemKind::Function, doc::DocItemKind::Method,
        doc::DocItemKind::Constant,  doc::DocItemKind::Impl,     doc::DocItemKind::TraitImpl};

    for (auto kind : kind_order) {
        auto it = by_kind.find(kind);
        if (it == by_kind.end())
            continue;

        auto kind_str = doc::doc_item_kind_to_string(kind);
        out << "## " << kind_str << "s (" << it->second.size() << ")\n\n";

        for (const auto* item : it->second) {
            auto vis_str = doc::doc_visibility_to_string(item->visibility);
            out << "  " << vis_str << " " << item->name;
            if (!item->signature.empty()) {
                out << " — " << item->signature;
            }
            out << "\n";
            if (!item->summary.empty()) {
                out << "    " << item->summary << "\n";
            }
        }
        out << "\n";
    }

    out << "(" << total << " item(s) total)\n";
    out << "\nUse docs/get with a qualified name for full documentation.\n";
    return ToolResult::text(out.str());
}

auto handle_docs_resolve(const json::JsonValue& params) -> ToolResult {
    auto* name_param = params.get("name");
    if (name_param == nullptr || !name_param->is_string()) {
        return ToolResult::error("Missing or invalid 'name' parameter");
    }
    std::string name = name_param->as_string();

    int64_t limit = 5;
    auto* limit_param = params.get("limit");
    if (limit_param != nullptr && limit_param->is_integer()) {
        limit = limit_param->as_i64();
    }

    ensure_doc_index();
    if (!is_doc_cache_initialized()) {
        return ToolResult::error("Documentation index not available");
    }

    const auto& all_items = get_doc_all_items();

    std::stringstream out;
    out << "Resolving: " << name << "\n\n";

    int count = 0;
    for (const auto& [item, mod_path] : all_items) {
        if (count >= limit)
            break;
        if (!icontains(item->name, name))
            continue;

        auto kind_str = doc::doc_item_kind_to_string(item->kind);
        out << "  " << mod_path << "::" << item->name << " (" << kind_str << ")\n";
        if (!item->summary.empty()) {
            out << "    " << item->summary << "\n";
        }
        count++;
    }

    if (count == 0) {
        out << "No items found matching: " << name << "\n";
    } else {
        out << "\n(" << count << " match(es))\n";
    }

    return ToolResult::text(out.str());
}

} // namespace tml::mcp
