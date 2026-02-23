TML_MODULE("tools")

//! # Documentation Model Implementation
//!
//! This file implements the utility functions for the documentation model.

#include "doc/doc_model.hpp"

#include <functional>

namespace tml::doc {

auto doc_item_kind_to_string(DocItemKind kind) -> std::string_view {
    switch (kind) {
    case DocItemKind::Function:
        return "function";
    case DocItemKind::Method:
        return "method";
    case DocItemKind::Struct:
        return "struct";
    case DocItemKind::Enum:
        return "enum";
    case DocItemKind::Variant:
        return "variant";
    case DocItemKind::Field:
        return "field";
    case DocItemKind::Trait:
        return "behavior";
    case DocItemKind::Impl:
        return "impl";
    case DocItemKind::TraitImpl:
        return "trait_impl";
    case DocItemKind::TypeAlias:
        return "type_alias";
    case DocItemKind::Constant:
        return "constant";
    case DocItemKind::AssociatedType:
        return "associated_type";
    case DocItemKind::Module:
        return "module";
    }
    return "unknown";
}

auto doc_visibility_to_string(DocVisibility vis) -> std::string_view {
    switch (vis) {
    case DocVisibility::Public:
        return "public";
    case DocVisibility::Crate:
        return "crate";
    case DocVisibility::Private:
        return "private";
    }
    return "unknown";
}

void DocIndex::build_index() {
    // Clear existing indices
    item_index_.clear();
    module_index_.clear();

    // Helper to index items recursively
    std::function<void(const std::vector<DocItem>&)> index_items;
    index_items = [&](const std::vector<DocItem>& items) {
        for (const auto& item : items) {
            item_index_[item.id] = &item;
            index_items(item.fields);
            index_items(item.variants);
            index_items(item.methods);
            index_items(item.associated_types);
            index_items(item.associated_consts);
        }
    };

    // Helper to index modules recursively
    std::function<void(const std::vector<DocModule>&)> index_modules;
    index_modules = [&](const std::vector<DocModule>& mods) {
        for (const auto& mod : mods) {
            module_index_[mod.path] = &mod;
            index_items(mod.items);
            index_modules(mod.submodules);
        }
    };

    index_modules(modules);
}

auto DocIndex::find_item(const std::string& id) const -> const DocItem* {
    // Use index if available
    auto it = item_index_.find(id);
    if (it != item_index_.end()) {
        return it->second;
    }

    // Fallback to recursive search
    std::function<const DocItem*(const std::vector<DocItem>&)> search_items;
    search_items = [&](const std::vector<DocItem>& items) -> const DocItem* {
        for (const auto& item : items) {
            if (item.id == id) {
                return &item;
            }
            // Search in children
            if (auto* found = search_items(item.fields)) {
                return found;
            }
            if (auto* found = search_items(item.variants)) {
                return found;
            }
            if (auto* found = search_items(item.methods)) {
                return found;
            }
            if (auto* found = search_items(item.associated_types)) {
                return found;
            }
            if (auto* found = search_items(item.associated_consts)) {
                return found;
            }
        }
        return nullptr;
    };

    // Helper to search in modules
    std::function<const DocItem*(const std::vector<DocModule>&)> search_modules;
    search_modules = [&](const std::vector<DocModule>& mods) -> const DocItem* {
        for (const auto& mod : mods) {
            if (auto* found = search_items(mod.items)) {
                return found;
            }
            if (auto* found = search_modules(mod.submodules)) {
                return found;
            }
        }
        return nullptr;
    };

    return search_modules(modules);
}

auto DocIndex::find_module(const std::string& path) const -> const DocModule* {
    // Use index if available
    auto it = module_index_.find(path);
    if (it != module_index_.end()) {
        return it->second;
    }

    // Fallback to recursive search
    std::function<const DocModule*(const std::vector<DocModule>&)> search;
    search = [&](const std::vector<DocModule>& mods) -> const DocModule* {
        for (const auto& mod : mods) {
            if (mod.path == path) {
                return &mod;
            }
            if (auto* found = search(mod.submodules)) {
                return found;
            }
        }
        return nullptr;
    };
    return search(modules);
}

auto DocIndex::items_by_kind(DocItemKind kind) const -> std::vector<const DocItem*> {
    std::vector<const DocItem*> results;

    std::function<void(const std::vector<DocItem>&)> collect_items;
    collect_items = [&](const std::vector<DocItem>& items) {
        for (const auto& item : items) {
            if (item.kind == kind) {
                results.push_back(&item);
            }
            collect_items(item.fields);
            collect_items(item.variants);
            collect_items(item.methods);
            collect_items(item.associated_types);
            collect_items(item.associated_consts);
        }
    };

    std::function<void(const std::vector<DocModule>&)> collect_from_modules;
    collect_from_modules = [&](const std::vector<DocModule>& mods) {
        for (const auto& mod : mods) {
            collect_items(mod.items);
            collect_from_modules(mod.submodules);
        }
    };

    collect_from_modules(modules);
    return results;
}

auto DocIndex::public_items() const -> std::vector<const DocItem*> {
    std::vector<const DocItem*> results;

    std::function<void(const std::vector<DocItem>&)> collect_items;
    collect_items = [&](const std::vector<DocItem>& items) {
        for (const auto& item : items) {
            if (item.visibility == DocVisibility::Public) {
                results.push_back(&item);
            }
            collect_items(item.fields);
            collect_items(item.variants);
            collect_items(item.methods);
            collect_items(item.associated_types);
            collect_items(item.associated_consts);
        }
    };

    std::function<void(const std::vector<DocModule>&)> collect_from_modules;
    collect_from_modules = [&](const std::vector<DocModule>& mods) {
        for (const auto& mod : mods) {
            collect_items(mod.items);
            collect_from_modules(mod.submodules);
        }
    };

    collect_from_modules(modules);
    return results;
}

} // namespace tml::doc
