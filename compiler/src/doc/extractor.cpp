//! # Documentation Extractor Implementation
//!
//! This file implements the AST to DocModel conversion.

#include "doc/extractor.hpp"

#include "doc/doc_parser.hpp"
#include "doc/signature.hpp"

#include <algorithm>
#include <sstream>

namespace tml::doc {

Extractor::Extractor(ExtractorConfig config) : config_(std::move(config)) {}

auto Extractor::extract(const parser::Module& module, const std::string& module_path) -> DocModule {
    current_module_path_ = module_path;

    DocModule doc_module;
    doc_module.name = module.name;
    doc_module.path = module_path.empty() ? module.name : module_path;

    // Extract module-level documentation (join all //! comments)
    if (!module.module_docs.empty()) {
        std::ostringstream oss;
        for (size_t i = 0; i < module.module_docs.size(); ++i) {
            if (i > 0) {
                oss << "\n";
            }
            oss << module.module_docs[i];
        }
        doc_module.doc = oss.str();
        doc_module.summary = extract_summary(doc_module.doc);
    }

    // Process all declarations
    for (const auto& decl : module.decls) {
        if (!decl) {
            continue;
        }

        // Function
        if (decl->is<parser::FuncDecl>()) {
            const auto& func = decl->as<parser::FuncDecl>();
            if (should_include(func.vis, func.doc)) {
                doc_module.items.push_back(extract_func(func));
            }
        }
        // Struct
        else if (decl->is<parser::StructDecl>()) {
            const auto& struct_decl = decl->as<parser::StructDecl>();
            if (should_include(struct_decl.vis, struct_decl.doc)) {
                doc_module.items.push_back(extract_struct(struct_decl));
            }
        }
        // Enum
        else if (decl->is<parser::EnumDecl>()) {
            const auto& enum_decl = decl->as<parser::EnumDecl>();
            if (should_include(enum_decl.vis, enum_decl.doc)) {
                doc_module.items.push_back(extract_enum(enum_decl));
            }
        }
        // Trait (Behavior)
        else if (decl->is<parser::TraitDecl>()) {
            const auto& trait = decl->as<parser::TraitDecl>();
            if (should_include(trait.vis, trait.doc)) {
                doc_module.items.push_back(extract_trait(trait));
            }
        }
        // Impl
        else if (decl->is<parser::ImplDecl>()) {
            const auto& impl = decl->as<parser::ImplDecl>();
            // Impl blocks are always included if they have public methods
            doc_module.items.push_back(extract_impl(impl));
        }
        // Type Alias
        else if (decl->is<parser::TypeAliasDecl>()) {
            const auto& alias = decl->as<parser::TypeAliasDecl>();
            if (should_include(alias.vis, alias.doc)) {
                doc_module.items.push_back(extract_type_alias(alias));
            }
        }
        // Const
        else if (decl->is<parser::ConstDecl>()) {
            const auto& const_decl = decl->as<parser::ConstDecl>();
            if (should_include(const_decl.vis, const_decl.doc)) {
                doc_module.items.push_back(extract_const(const_decl));
            }
        }
        // Use declarations - skip (they don't generate documentation)
        // Module declarations - could be processed recursively
    }

    return doc_module;
}

auto Extractor::extract_all(
    const std::vector<std::pair<const parser::Module*, std::string>>& modules) -> DocIndex {
    DocIndex index;

    for (const auto& [module, path] : modules) {
        if (module) {
            index.modules.push_back(extract(*module, path));
        }
    }

    // Build the index
    index.build_index();

    return index;
}

// ============================================================================
// Item Extraction
// ============================================================================

auto Extractor::extract_func(const parser::FuncDecl& func, const std::string& parent_path)
    -> DocItem {
    DocItem item;
    item.name = func.name;
    item.path = make_path(parent_path.empty() ? current_module_path_ : parent_path, func.name);
    item.id = item.path;
    item.kind = func.params.empty() || !func.params[0].pattern->is<parser::IdentPattern>() ||
                        func.params[0].pattern->as<parser::IdentPattern>().name != "this"
                    ? DocItemKind::Function
                    : DocItemKind::Method;
    item.visibility = convert_visibility(func.vis);
    item.signature = generate_func_signature(func);
    item.generics = extract_generics(func.generics);
    item.params = extract_func_params(func.params);

    // Return type
    if (func.return_type) {
        DocReturn ret;
        ret.type = type_to_string(**func.return_type);
        item.returns = ret;
    }

    // Async/unsafe markers
    item.is_async = func.is_async;
    item.is_unsafe = func.is_unsafe;

    // Documentation
    populate_doc_fields(item, func.doc);

    return item;
}

auto Extractor::extract_struct(const parser::StructDecl& struct_decl) -> DocItem {
    DocItem item;
    item.name = struct_decl.name;
    item.path = make_path(current_module_path_, struct_decl.name);
    item.id = item.path;
    item.kind = DocItemKind::Struct;
    item.visibility = convert_visibility(struct_decl.vis);
    item.signature = generate_struct_signature(struct_decl);
    item.generics = extract_generics(struct_decl.generics);

    // Extract fields
    for (const auto& field : struct_decl.fields) {
        if (config_.include_private || field.vis == parser::Visibility::Public ||
            field.vis == parser::Visibility::PubCrate) {
            item.fields.push_back(extract_field(field, item.path));
        }
    }

    // Documentation
    populate_doc_fields(item, struct_decl.doc);

    return item;
}

auto Extractor::extract_enum(const parser::EnumDecl& enum_decl) -> DocItem {
    DocItem item;
    item.name = enum_decl.name;
    item.path = make_path(current_module_path_, enum_decl.name);
    item.id = item.path;
    item.kind = DocItemKind::Enum;
    item.visibility = convert_visibility(enum_decl.vis);
    item.signature = generate_enum_signature(enum_decl);
    item.generics = extract_generics(enum_decl.generics);

    // Extract variants
    for (const auto& variant : enum_decl.variants) {
        item.variants.push_back(extract_variant(variant, item.path));
    }

    // Documentation
    populate_doc_fields(item, enum_decl.doc);

    return item;
}

auto Extractor::extract_trait(const parser::TraitDecl& trait) -> DocItem {
    DocItem item;
    item.name = trait.name;
    item.path = make_path(current_module_path_, trait.name);
    item.id = item.path;
    item.kind = DocItemKind::Trait;
    item.visibility = convert_visibility(trait.vis);
    item.signature = generate_trait_signature(trait);
    item.generics = extract_generics(trait.generics);

    // Extract super traits
    for (const auto& super_trait : trait.super_traits) {
        item.super_traits.push_back(type_to_string(*super_trait));
    }

    // Extract methods
    for (const auto& method : trait.methods) {
        item.methods.push_back(extract_func(method, item.path));
    }

    // Extract associated types
    for (const auto& assoc_type : trait.associated_types) {
        DocItem type_item;
        type_item.name = assoc_type.name;
        type_item.path = make_path(item.path, assoc_type.name);
        type_item.id = type_item.path;
        type_item.kind = DocItemKind::AssociatedType;
        type_item.visibility = DocVisibility::Public;

        // Build signature
        std::string sig = "type " + assoc_type.name;
        if (!assoc_type.generics.empty()) {
            sig += generics_to_string(assoc_type.generics);
        }
        if (!assoc_type.bounds.empty()) {
            sig += ": ";
            for (size_t i = 0; i < assoc_type.bounds.size(); ++i) {
                if (i > 0) {
                    sig += " + ";
                }
                sig += type_to_string(*assoc_type.bounds[i]);
            }
        }
        if (assoc_type.default_type) {
            sig += " = " + type_to_string(**assoc_type.default_type);
        }
        type_item.signature = sig;

        item.associated_types.push_back(std::move(type_item));
    }

    // Documentation
    populate_doc_fields(item, trait.doc);

    return item;
}

auto Extractor::extract_impl(const parser::ImplDecl& impl) -> DocItem {
    DocItem item;

    // Generate name from self type and optional trait
    std::string self_type_str = type_to_string(*impl.self_type);
    if (impl.trait_type) {
        item.name = type_to_string(*impl.trait_type) + " for " + self_type_str;
        item.kind = DocItemKind::TraitImpl;
        item.impl_trait = type_to_string(*impl.trait_type);
    } else {
        item.name = self_type_str;
        item.kind = DocItemKind::Impl;
    }

    item.path = make_path(current_module_path_, "impl_" + self_type_str);
    item.id = item.path;
    item.impl_for = self_type_str;
    item.signature = generate_impl_signature(impl);
    item.generics = extract_generics(impl.generics);

    // Extract methods
    for (const auto& method : impl.methods) {
        if (should_include(method.vis, method.doc)) {
            item.methods.push_back(extract_func(method, item.path));
        }
    }

    // Extract associated constants
    for (const auto& const_decl : impl.constants) {
        if (should_include(const_decl.vis, const_decl.doc)) {
            item.associated_consts.push_back(extract_const(const_decl));
        }
    }

    // Extract associated type bindings
    for (const auto& binding : impl.type_bindings) {
        DocItem type_item;
        type_item.name = binding.name;
        type_item.path = make_path(item.path, binding.name);
        type_item.id = type_item.path;
        type_item.kind = DocItemKind::AssociatedType;
        type_item.visibility = DocVisibility::Public;
        type_item.signature = "type " + binding.name + " = " + type_to_string(*binding.type);
        type_item.aliased_type = type_to_string(*binding.type);
        item.associated_types.push_back(std::move(type_item));
    }

    // Documentation
    populate_doc_fields(item, impl.doc);

    return item;
}

auto Extractor::extract_type_alias(const parser::TypeAliasDecl& alias) -> DocItem {
    DocItem item;
    item.name = alias.name;
    item.path = make_path(current_module_path_, alias.name);
    item.id = item.path;
    item.kind = DocItemKind::TypeAlias;
    item.visibility = convert_visibility(alias.vis);
    item.signature = generate_type_alias_signature(alias);
    item.generics = extract_generics(alias.generics);
    item.aliased_type = type_to_string(*alias.type);

    // Documentation
    populate_doc_fields(item, alias.doc);

    return item;
}

auto Extractor::extract_const(const parser::ConstDecl& const_decl) -> DocItem {
    DocItem item;
    item.name = const_decl.name;
    item.path = make_path(current_module_path_, const_decl.name);
    item.id = item.path;
    item.kind = DocItemKind::Constant;
    item.visibility = convert_visibility(const_decl.vis);
    item.signature = generate_const_signature(const_decl);

    // Type info
    if (const_decl.type) {
        DocReturn ret;
        ret.type = type_to_string(*const_decl.type);
        item.returns = ret;
    }

    // Documentation
    populate_doc_fields(item, const_decl.doc);

    return item;
}

auto Extractor::extract_field(const parser::StructField& field, const std::string& parent_path)
    -> DocItem {
    DocItem item;
    item.name = field.name;
    item.path = make_path(parent_path, field.name);
    item.id = item.path;
    item.kind = DocItemKind::Field;
    item.visibility = convert_visibility(field.vis);
    item.signature = generate_field_signature(field);

    // Type info
    if (field.type) {
        DocReturn ret;
        ret.type = type_to_string(*field.type);
        item.returns = ret;
    }

    // Documentation
    populate_doc_fields(item, field.doc);

    return item;
}

auto Extractor::extract_variant(const parser::EnumVariant& variant, const std::string& parent_path)
    -> DocItem {
    DocItem item;
    item.name = variant.name;
    item.path = make_path(parent_path, variant.name);
    item.id = item.path;
    item.kind = DocItemKind::Variant;
    item.visibility = DocVisibility::Public; // Variants are always public within enum
    item.signature = generate_variant_signature(variant);

    // Extract variant fields if struct variant
    if (variant.struct_fields) {
        for (const auto& field : *variant.struct_fields) {
            DocItem field_item;
            field_item.name = field.name;
            field_item.path = make_path(item.path, field.name);
            field_item.id = field_item.path;
            field_item.kind = DocItemKind::Field;
            field_item.visibility = DocVisibility::Public;
            field_item.signature = generate_field_signature(field);
            if (field.type) {
                DocReturn ret;
                ret.type = type_to_string(*field.type);
                field_item.returns = ret;
            }
            item.fields.push_back(std::move(field_item));
        }
    }

    // Documentation
    populate_doc_fields(item, variant.doc);

    return item;
}

// ============================================================================
// Helper Functions
// ============================================================================

auto Extractor::make_path(const std::string& parent, const std::string& name) const -> std::string {
    if (parent.empty()) {
        return name;
    }
    return parent + "::" + name;
}

auto Extractor::convert_visibility(parser::Visibility vis) const -> DocVisibility {
    switch (vis) {
    case parser::Visibility::Public:
        return DocVisibility::Public;
    case parser::Visibility::PubCrate:
        return DocVisibility::Crate;
    case parser::Visibility::Private:
    default:
        return DocVisibility::Private;
    }
}

auto Extractor::extract_generics(const std::vector<parser::GenericParam>& params) const
    -> std::vector<DocGenericParam> {
    std::vector<DocGenericParam> result;
    result.reserve(params.size());

    for (const auto& param : params) {
        DocGenericParam doc_param;
        doc_param.name = param.name;
        doc_param.is_const = param.is_const;

        // Convert bounds
        for (const auto& bound : param.bounds) {
            doc_param.bounds.push_back(type_to_string(*bound));
        }

        // Default type
        if (param.default_type) {
            doc_param.default_value = type_to_string(**param.default_type);
        }

        result.push_back(std::move(doc_param));
    }

    return result;
}

auto Extractor::extract_func_params(const std::vector<parser::FuncParam>& params) const
    -> std::vector<DocParam> {
    std::vector<DocParam> result;
    result.reserve(params.size());

    for (const auto& param : params) {
        DocParam doc_param;

        // Extract parameter name from pattern
        if (param.pattern->is<parser::IdentPattern>()) {
            const auto& ident = param.pattern->as<parser::IdentPattern>();
            doc_param.name = ident.name;
        } else {
            doc_param.name = "_";
        }

        // Type
        doc_param.type = type_to_string(*param.type);

        result.push_back(std::move(doc_param));
    }

    return result;
}

auto Extractor::should_include(parser::Visibility vis, const std::optional<std::string>& doc) const
    -> bool {
    // Check visibility
    if (!config_.include_private && vis == parser::Visibility::Private) {
        return false;
    }

    // Check for @internal tag
    if (!config_.include_internals && doc) {
        if (doc->find("@internal") != std::string::npos) {
            return false;
        }
    }

    return true;
}

void Extractor::populate_doc_fields(DocItem& item, const std::optional<std::string>& doc) {
    if (!doc || doc->empty()) {
        return;
    }

    item.doc = *doc;
    item.summary = extract_summary(*doc);

    // Parse structured tags
    auto parsed = parse_doc_comment(*doc);

    // Merge parsed params with extracted params (add descriptions)
    for (auto& param : item.params) {
        auto it = std::find_if(parsed.params.begin(), parsed.params.end(),
                               [&](const DocParam& p) { return p.name == param.name; });
        if (it != parsed.params.end()) {
            param.description = it->description;
        }
    }

    // Return description
    if (item.returns && parsed.returns) {
        item.returns->description = parsed.returns->description;
    }

    // Other parsed fields
    item.throws = std::move(parsed.throws);
    item.see_also = std::move(parsed.see_also);
    item.since = parsed.since;
    item.deprecated = parsed.deprecated;

    // Extract examples if configured
    if (config_.extract_examples) {
        item.examples = extract_code_blocks(*doc);
    }
}

} // namespace tml::doc
