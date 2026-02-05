//! # LLVM IR Generator - Generic Instantiation
//!
//! This file implements monomorphization of generic types and functions.
//!
//! ## Monomorphization Strategy
//!
//! TML uses monomorphization like Rust: each use of a generic with concrete
//! types generates a specialized version of the code.
//!
//! ## Instantiation Pipeline
//!
//! | Phase | What Happens                                    |
//! |-------|------------------------------------------------|
//! | 1     | Collect pending struct instantiations          |
//! | 2     | Collect pending enum instantiations            |
//! | 3     | Collect pending function instantiations        |
//! | 4     | Loop until no new instantiations (handles recursion) |
//!
//! ## Key Methods
//!
//! | Method                        | Purpose                          |
//! |-------------------------------|----------------------------------|
//! | `generate_pending_instantiations` | Main instantiation loop      |
//! | `require_struct_instantiation`| Queue struct for instantiation   |
//! | `require_enum_instantiation`  | Queue enum for instantiation     |
//! | `require_func_instantiation`  | Queue function for instantiation |
//!
//! ## Naming Convention
//!
//! Instantiated names include type arguments: `List_I32`, `HashMap_Str_I32`

#include "codegen/llvm_ir_gen.hpp"
#include "lexer/lexer.hpp"
#include "parser/parser.hpp"

namespace tml::codegen {

// Helper: Parse a mangled type string back into a semantic type
// e.g., "ptr_ChannelNode__I32" -> PtrType{inner=NamedType{name="ChannelNode", type_args=[I32]}}
static types::TypePtr parse_mangled_type_string(const std::string& s) {
    // Handle primitive types
    if (s == "I8")
        return types::make_primitive(types::PrimitiveKind::I8);
    if (s == "I16")
        return types::make_primitive(types::PrimitiveKind::I16);
    if (s == "I32")
        return types::make_i32();
    if (s == "I64")
        return types::make_i64();
    if (s == "I128")
        return types::make_primitive(types::PrimitiveKind::I128);
    if (s == "U8")
        return types::make_primitive(types::PrimitiveKind::U8);
    if (s == "U16")
        return types::make_primitive(types::PrimitiveKind::U16);
    if (s == "U32")
        return types::make_primitive(types::PrimitiveKind::U32);
    if (s == "U64")
        return types::make_primitive(types::PrimitiveKind::U64);
    if (s == "U128")
        return types::make_primitive(types::PrimitiveKind::U128);
    if (s == "F32")
        return types::make_primitive(types::PrimitiveKind::F32);
    if (s == "F64")
        return types::make_f64();
    if (s == "Bool")
        return types::make_bool();
    if (s == "Str")
        return types::make_str();
    if (s == "Unit")
        return types::make_unit();
    if (s == "Usize")
        return types::make_primitive(types::PrimitiveKind::U64);
    if (s == "Isize")
        return types::make_primitive(types::PrimitiveKind::I64);

    // Check for pointer prefix (e.g., ptr_ChannelNode__I32 -> Ptr[ChannelNode[I32]])
    if (s.size() > 4 && s.substr(0, 4) == "ptr_") {
        std::string inner_str = s.substr(4);
        auto inner = parse_mangled_type_string(inner_str);
        if (inner) {
            auto t = std::make_shared<types::Type>();
            t->kind = types::PtrType{.is_mut = false, .inner = inner};
            return t;
        }
    }

    // Check for mutable pointer prefix
    if (s.size() > 7 && s.substr(0, 7) == "mutptr_") {
        std::string inner_str = s.substr(7);
        auto inner = parse_mangled_type_string(inner_str);
        if (inner) {
            auto t = std::make_shared<types::Type>();
            t->kind = types::PtrType{.is_mut = true, .inner = inner};
            return t;
        }
    }

    // Check for ref prefix
    if (s.size() > 4 && s.substr(0, 4) == "ref_") {
        std::string inner_str = s.substr(4);
        auto inner = parse_mangled_type_string(inner_str);
        if (inner) {
            auto t = std::make_shared<types::Type>();
            t->kind = types::RefType{.is_mut = false, .inner = inner};
            return t;
        }
    }

    // Check for mutable ref prefix
    if (s.size() > 7 && s.substr(0, 7) == "mutref_") {
        std::string inner_str = s.substr(7);
        auto inner = parse_mangled_type_string(inner_str);
        if (inner) {
            auto t = std::make_shared<types::Type>();
            t->kind = types::RefType{.is_mut = true, .inner = inner};
            return t;
        }
    }

    // Check for nested generic (e.g., Mutex__I32, ChannelNode__I32)
    auto delim = s.find("__");
    if (delim != std::string::npos) {
        std::string base = s.substr(0, delim);
        std::string arg_str = s.substr(delim + 2);

        // Parse all type arguments (separated by __)
        std::vector<types::TypePtr> type_args;
        size_t pos = 0;
        while (pos < arg_str.size()) {
            // Find next __ delimiter
            auto next_delim = arg_str.find("__", pos);
            std::string arg_part;
            if (next_delim == std::string::npos) {
                arg_part = arg_str.substr(pos);
                pos = arg_str.size();
            } else {
                arg_part = arg_str.substr(pos, next_delim - pos);
                pos = next_delim + 2;
            }

            auto arg_type = parse_mangled_type_string(arg_part);
            if (arg_type) {
                type_args.push_back(arg_type);
            } else {
                // Fallback: create NamedType
                auto t = std::make_shared<types::Type>();
                t->kind = types::NamedType{arg_part, "", {}};
                type_args.push_back(t);
            }
        }

        auto t = std::make_shared<types::Type>();
        t->kind = types::NamedType{base, "", std::move(type_args)};
        return t;
    }

    // Simple struct type (no generics, no prefix)
    auto t = std::make_shared<types::Type>();
    t->kind = types::NamedType{s, "", {}};
    return t;
}

// ============ Generate Pending Generic Instantiations ============
// Iteratively generate all pending struct/enum/func instantiations
// Loops until no new instantiations are added (handles recursive types)

void LLVMIRGen::generate_pending_instantiations() {
    const int MAX_ITERATIONS = 100; // Prevent infinite loops
    int iterations = 0;

    // First pass: generate ALL type definitions (structs and enums)
    // This must complete before any functions that use these types
    bool types_changed = true;
    while (types_changed && iterations < MAX_ITERATIONS) {
        types_changed = false;
        ++iterations;

        // Generate pending struct instantiations
        for (auto& [key, inst] : struct_instantiations_) {
            if (!inst.generated) {
                inst.generated = true;

                // Find the generic struct declaration
                auto it = pending_generic_structs_.find(inst.base_name);
                if (it != pending_generic_structs_.end()) {
                    gen_struct_instantiation(*it->second, inst.type_args);
                    types_changed = true;
                }
            }
        }

        // Generate pending enum instantiations
        for (auto& [key, inst] : enum_instantiations_) {
            if (!inst.generated) {
                inst.generated = true;

                auto it = pending_generic_enums_.find(inst.base_name);
                if (it != pending_generic_enums_.end()) {
                    gen_enum_instantiation(*it->second, inst.type_args);
                    types_changed = true;
                }
            }
        }

        // Generate pending class instantiations
        for (auto& [key, inst] : class_instantiations_) {
            if (!inst.generated) {
                inst.generated = true;

                auto it = pending_generic_classes_.find(inst.base_name);
                if (it != pending_generic_classes_.end()) {
                    gen_class_instantiation(*it->second, inst.type_args);
                    types_changed = true;
                }
            }
        }
    }

    // Second pass: generate functions (may discover new types, so we loop)
    iterations = 0;
    bool changed = true;
    while (changed && iterations < MAX_ITERATIONS) {
        changed = false;
        ++iterations;

        // Generate pending function instantiations
        for (auto& [key, inst] : func_instantiations_) {
            if (!inst.generated) {
                inst.generated = true;

                auto it = pending_generic_funcs_.find(inst.base_name);
                if (it != pending_generic_funcs_.end()) {
                    gen_func_instantiation(*it->second, inst.type_args);
                    changed = true;
                }
            }
        }

        // If new types were discovered during function generation, emit them now
        // before continuing with more functions
        bool new_types = false;
        for (auto& [key, inst] : struct_instantiations_) {
            if (!inst.generated) {
                inst.generated = true;
                auto it = pending_generic_structs_.find(inst.base_name);
                if (it != pending_generic_structs_.end()) {
                    gen_struct_instantiation(*it->second, inst.type_args);
                    new_types = true;
                }
            }
        }
        for (auto& [key, inst] : enum_instantiations_) {
            if (!inst.generated) {
                inst.generated = true;
                auto it = pending_generic_enums_.find(inst.base_name);
                if (it != pending_generic_enums_.end()) {
                    gen_enum_instantiation(*it->second, inst.type_args);
                    new_types = true;
                }
            }
        }
        if (new_types)
            changed = true;

        // Generate pending impl method instantiations
        // Track processed methods to avoid duplicate lookups (expensive module searches)
        std::unordered_set<std::string> processed_impl_methods;

        while (!pending_impl_method_instantiations_.empty()) {
            auto pending = std::move(pending_impl_method_instantiations_);
            pending_impl_method_instantiations_.clear();

            for (const auto& pim : pending) {
                // Build deduplication key
                std::string method_key = pim.mangled_type_name + "::" + pim.method_name;
                if (!pim.method_type_suffix.empty()) {
                    method_key += "__" + pim.method_type_suffix;
                }

                // Skip if already processed or already generated
                if (processed_impl_methods.count(method_key) > 0) {
                    continue;
                }

                // Also check if already generated (from previous compilation phases)
                // Only check generated_impl_methods_output_ which tracks ACTUALLY generated
                // methods. Don't check generated_impl_methods_ here - that's for queue
                // deduplication only.
                std::string generated_key = "tml_" + pim.mangled_type_name + "_" + pim.method_name;
                if (!pim.method_type_suffix.empty()) {
                    generated_key += "__" + pim.method_type_suffix;
                }
                if (generated_impl_methods_output_.count(generated_key) > 0) {
                    processed_impl_methods.insert(method_key);
                    continue;
                }

                // NOTE: GlobalLibraryIRCache is DISABLED for now.
                // The cache was causing issues where only declarations were emitted
                // but implementations were missing when compiling multiple test suites.
                // Each suite needs its own complete implementation of library methods.
                // TODO: Revisit caching strategy - perhaps cache at the object file level
                // instead of the IR level, or clear cache between suites.

                processed_impl_methods.insert(method_key);

                TML_DEBUG_LN("[IMPL_INST] Looking for "
                             << pim.base_type_name << "::" << pim.method_name
                             << " (mangled: " << pim.mangled_type_name << ")"
                             << " is_library_type=" << (pim.is_library_type ? "true" : "false")
                             << " method_type_suffix=" << pim.method_type_suffix);

                bool method_generated = false;

                // First check locally defined impls
                auto impl_it = pending_generic_impls_.find(pim.base_type_name);
                if (impl_it != pending_generic_impls_.end()) {
                    const auto& impl = *impl_it->second;

                    // Check if this impl has the method we're looking for BEFORE
                    // doing any processing. This handles the case where multiple
                    // modules define the same type (e.g., core::range::Range vs
                    // core::ops::range::Range) but with different methods.
                    bool has_method = false;
                    for (const auto& m : impl.methods) {
                        if (m.name == pim.method_name) {
                            has_method = true;
                            break;
                        }
                    }

                    // DEBUG: Log method lookup result
                    if (pim.base_type_name == "RangeInclusive" || pim.base_type_name == "Range") {
                        std::cerr << "[DEBUG GENERIC] " << pim.base_type_name
                                  << "::" << pim.method_name << " - local impl has "
                                  << impl.methods.size()
                                  << " methods, has_method=" << (has_method ? "yes" : "no")
                                  << ", generics=";
                        for (const auto& g : impl.generics) {
                            std::cerr << g.name << " ";
                        }
                        std::cerr << "\n";
                    }

                    if (has_method) {
                        // Process associated type bindings from the impl block
                        // e.g., `type Item = I::Item` becomes `Item -> I64` when I -> RangeIterI64
                        auto saved_associated_types = current_associated_types_;
                        current_associated_types_.clear();

                        // First, we need to find the associated types from the concrete types
                        // that the generic params were substituted to
                        // For example: if I -> RangeIterI64, look up RangeIterI64's Item type
                        for (const auto& [param_name, concrete_type] : pim.type_subs) {
                            if (concrete_type && concrete_type->is<types::NamedType>()) {
                                const auto& concrete_named = concrete_type->as<types::NamedType>();
                                // Find the impl block for this concrete type to get its associated
                                // types
                                auto concrete_impl_it =
                                    pending_generic_impls_.find(concrete_named.name);
                                if (concrete_impl_it != pending_generic_impls_.end()) {
                                    const auto& concrete_impl = *concrete_impl_it->second;
                                    for (const auto& concrete_binding :
                                         concrete_impl.type_bindings) {
                                        auto concrete_resolved = resolve_parser_type_with_subs(
                                            *concrete_binding.type, {});
                                        current_associated_types_[concrete_binding.name] =
                                            concrete_resolved;
                                    }
                                }
                            }
                        }

                        // Recover type_subs from mangled_type_name if empty
                        // For example: mangled_type_name="Range__I64", base_type_name="Range"
                        // Extract "I64" and map to impl generics (e.g., T -> I64)
                        auto effective_type_subs = pim.type_subs;
                        if (effective_type_subs.empty() && !impl.generics.empty() &&
                            pim.mangled_type_name.length() > pim.base_type_name.length() + 2) {
                            std::string suffix =
                                pim.mangled_type_name.substr(pim.base_type_name.length());
                            if (suffix.starts_with("__")) {
                                suffix = suffix.substr(2);
                                // For single type param, use entire suffix
                                if (impl.generics.size() == 1) {
                                    auto type_arg = parse_mangled_type_string(suffix);
                                    if (type_arg) {
                                        effective_type_subs[impl.generics[0].name] = type_arg;
                                        TML_DEBUG_LN(
                                            "[IMPL_INST] Recovered type_subs from mangled name: "
                                            << impl.generics[0].name << " -> " << suffix);
                                    }
                                } else {
                                    // Multiple type params - split on "__"
                                    std::vector<std::string> parts;
                                    size_t pos = 0;
                                    while (pos < suffix.size()) {
                                        size_t next = suffix.find("__", pos);
                                        if (next == std::string::npos) {
                                            parts.push_back(suffix.substr(pos));
                                            break;
                                        }
                                        parts.push_back(suffix.substr(pos, next - pos));
                                        pos = next + 2;
                                    }
                                    for (size_t i = 0; i < impl.generics.size() && i < parts.size();
                                         ++i) {
                                        auto type_arg = parse_mangled_type_string(parts[i]);
                                        if (type_arg) {
                                            effective_type_subs[impl.generics[i].name] = type_arg;
                                        }
                                    }
                                }
                            }
                        }

                        // Now resolve the impl's own type bindings with the substitutions
                        for (const auto& binding : impl.type_bindings) {
                            // Resolve the binding type with the current type substitutions
                            auto resolved =
                                resolve_parser_type_with_subs(*binding.type, effective_type_subs);
                            current_associated_types_[binding.name] = resolved;
                        }

                        // Find the method in the impl block and generate it
                        for (const auto& m : impl.methods) {
                            if (m.name == pim.method_name) {
                                gen_impl_method_instantiation(
                                    pim.mangled_type_name, m, effective_type_subs, impl.generics,
                                    pim.method_type_suffix, pim.is_library_type,
                                    pim.base_type_name);
                                method_generated = true;
                                break;
                            }
                        }

                        // Restore associated types
                        current_associated_types_ = saved_associated_types;
                    }
                }

                // If not found in local impls, check imported modules
                if (!method_generated && env_.module_registry()) {
                    // Check imported modules - need to re-parse to get impl AST
                    const auto& all_modules = env_.module_registry()->get_all_modules();
                    TML_DEBUG_LN("[IMPL_INST]   Not in local impls, searching "
                                 << all_modules.size() << " modules for " << pim.base_type_name
                                 << "::" << pim.method_name);

                    bool found = false;
                    for (const auto& [mod_name, mod] : all_modules) {
                        if (found)
                            break;

                        // Check if this module has the struct (for exported types)
                        // For library-internal types (pim.is_library_type), skip this check
                        // and search the source code directly
                        auto struct_it = mod.structs.find(pim.base_type_name);
                        if (struct_it == mod.structs.end() && !pim.is_library_type)
                            continue;

                        TML_DEBUG_LN("[IMPL_INST]   Checking module: "
                                     << mod_name << " has_source="
                                     << (!mod.source_code.empty() ? "yes" : "no") << " has_struct="
                                     << (struct_it != mod.structs.end() ? "yes" : "no"));

                        // Get parsed AST from global cache or parse if not cached
                        if (mod.source_code.empty()) {
                            TML_DEBUG_LN("[IMPL_INST]   Module has no source_code, skipping");
                            continue;
                        }

                        const parser::Module* parsed_mod_ptr = nullptr;

                        // Check global AST cache first
                        if (GlobalASTCache::should_cache(mod_name)) {
                            parsed_mod_ptr = GlobalASTCache::instance().get(mod_name);
                            if (parsed_mod_ptr) {
                                TML_DEBUG_LN("[IMPL_INST]   AST cache hit for: " << mod_name);
                            }
                        }

                        // If not in cache, parse the module
                        parser::Module local_parsed_mod;
                        if (!parsed_mod_ptr) {
                            auto source =
                                lexer::Source::from_string(mod.source_code, mod.file_path);
                            lexer::Lexer lex(source);
                            auto tokens = lex.tokenize();
                            if (lex.has_errors())
                                continue;

                            parser::Parser mod_parser(std::move(tokens));
                            auto module_name_stem = mod.name;
                            if (auto pos = module_name_stem.rfind("::"); pos != std::string::npos) {
                                module_name_stem = module_name_stem.substr(pos + 2);
                            }
                            auto parse_result = mod_parser.parse_module(module_name_stem);
                            if (!std::holds_alternative<parser::Module>(parse_result))
                                continue;

                            local_parsed_mod = std::get<parser::Module>(std::move(parse_result));

                            // Store in global cache for library modules
                            if (GlobalASTCache::should_cache(mod_name)) {
                                GlobalASTCache::instance().put(mod_name,
                                                               std::move(local_parsed_mod));
                                parsed_mod_ptr = GlobalASTCache::instance().get(mod_name);
                                TML_DEBUG_LN("[IMPL_INST]   AST cached: " << mod_name);
                            } else {
                                parsed_mod_ptr = &local_parsed_mod;
                            }
                        }

                        if (!parsed_mod_ptr)
                            continue;

                        const auto& parsed_mod = *parsed_mod_ptr;

                        // Find the impl block for this type
                        for (const auto& decl : parsed_mod.decls) {
                            if (!decl->is<parser::ImplDecl>())
                                continue;
                            const auto& impl_decl = decl->as<parser::ImplDecl>();

                            // Check if this impl is for our type
                            if (!impl_decl.self_type)
                                continue;
                            if (!impl_decl.self_type->is<parser::NamedType>())
                                continue;
                            const auto& target = impl_decl.self_type->as<parser::NamedType>();
                            if (target.path.segments.empty())
                                continue;
                            if (target.path.segments.back() != pim.base_type_name)
                                continue;

                            // For TryFrom/From on primitive types, match the behavior type
                            // parameter e.g., for I32::try_from(I64), find impl TryFrom[I64] for
                            // I32
                            if (!pim.method_type_suffix.empty() && impl_decl.trait_type) {
                                if (impl_decl.trait_type->is<parser::NamedType>()) {
                                    const auto& trait =
                                        impl_decl.trait_type->as<parser::NamedType>();
                                    std::string trait_name = trait.path.segments.empty()
                                                                 ? ""
                                                                 : trait.path.segments.back();
                                    // Only check for TryFrom/From behaviors
                                    if ((trait_name == "TryFrom" || trait_name == "From") &&
                                        trait.generics.has_value() &&
                                        !trait.generics->args.empty()) {
                                        // Extract the behavior type parameter
                                        bool matches = false;
                                        for (const auto& arg : trait.generics->args) {
                                            if (arg.is_type() &&
                                                arg.as_type()->is<parser::NamedType>()) {
                                                const auto& arg_named =
                                                    arg.as_type()->as<parser::NamedType>();
                                                std::string arg_type_name =
                                                    arg_named.path.segments.empty()
                                                        ? ""
                                                        : arg_named.path.segments.back();
                                                if (arg_type_name == pim.method_type_suffix) {
                                                    matches = true;
                                                    break;
                                                }
                                            }
                                        }
                                        if (!matches) {
                                            continue; // Wrong behavior type parameter, skip this
                                                      // impl
                                        }
                                    }
                                }
                            }

                            TML_DEBUG_LN("[IMPL_INST]   Found impl for "
                                         << pim.base_type_name
                                         << ", methods: " << impl_decl.methods.size());

                            // Process associated type bindings from the imported impl
                            auto saved_associated_types = current_associated_types_;
                            current_associated_types_.clear();

                            // First, find associated types from concrete types in substitutions
                            for (const auto& [param_name, concrete_type] : pim.type_subs) {
                                if (concrete_type && concrete_type->is<types::NamedType>()) {
                                    const auto& concrete_named =
                                        concrete_type->as<types::NamedType>();
                                    // Check pending_generic_impls_ for local impls
                                    auto concrete_impl_it =
                                        pending_generic_impls_.find(concrete_named.name);
                                    if (concrete_impl_it != pending_generic_impls_.end()) {
                                        const auto& concrete_impl = *concrete_impl_it->second;
                                        for (const auto& concrete_binding :
                                             concrete_impl.type_bindings) {
                                            auto concrete_resolved = resolve_parser_type_with_subs(
                                                *concrete_binding.type, {});
                                            current_associated_types_[concrete_binding.name] =
                                                concrete_resolved;
                                        }
                                    }
                                }
                            }

                            // Recover type_subs from mangled_type_name if empty
                            auto effective_type_subs = pim.type_subs;
                            if (effective_type_subs.empty() && !impl_decl.generics.empty() &&
                                pim.mangled_type_name.length() > pim.base_type_name.length() + 2) {
                                std::string suffix =
                                    pim.mangled_type_name.substr(pim.base_type_name.length());
                                if (suffix.starts_with("__")) {
                                    suffix = suffix.substr(2);
                                    if (impl_decl.generics.size() == 1) {
                                        auto type_arg = parse_mangled_type_string(suffix);
                                        if (type_arg) {
                                            effective_type_subs[impl_decl.generics[0].name] =
                                                type_arg;
                                            TML_DEBUG_LN(
                                                "[IMPL_INST] Recovered type_subs (imported): "
                                                << impl_decl.generics[0].name << " -> " << suffix);
                                        }
                                    } else {
                                        std::vector<std::string> parts;
                                        size_t pos = 0;
                                        while (pos < suffix.size()) {
                                            size_t next = suffix.find("__", pos);
                                            if (next == std::string::npos) {
                                                parts.push_back(suffix.substr(pos));
                                                break;
                                            }
                                            parts.push_back(suffix.substr(pos, next - pos));
                                            pos = next + 2;
                                        }
                                        for (size_t i = 0;
                                             i < impl_decl.generics.size() && i < parts.size();
                                             ++i) {
                                            auto type_arg = parse_mangled_type_string(parts[i]);
                                            if (type_arg) {
                                                effective_type_subs[impl_decl.generics[i].name] =
                                                    type_arg;
                                            }
                                        }
                                    }
                                }
                            }

                            // Then resolve the impl's own type bindings
                            for (const auto& binding : impl_decl.type_bindings) {
                                auto resolved = resolve_parser_type_with_subs(*binding.type,
                                                                              effective_type_subs);
                                current_associated_types_[binding.name] = resolved;
                            }

                            // Find the method
                            for (size_t mi = 0; mi < impl_decl.methods.size(); ++mi) {
                                const auto& method_decl = impl_decl.methods[mi];
                                if (method_decl.name == pim.method_name) {
                                    gen_impl_method_instantiation(
                                        pim.mangled_type_name, method_decl, effective_type_subs,
                                        impl_decl.generics, pim.method_type_suffix,
                                        pim.is_library_type, pim.base_type_name);
                                    found = true;
                                    break;
                                }
                            }

                            // Restore associated types
                            current_associated_types_ = saved_associated_types;

                            if (found)
                                break;
                        }
                    }
                }
            }
            changed = true;
        }

        // Generate pending generic class method instantiations
        while (!pending_generic_class_method_insts_.empty()) {
            auto pending = std::move(pending_generic_class_method_insts_);
            pending_generic_class_method_insts_.clear();

            for (const auto& p : pending) {
                gen_generic_class_static_method(*p.class_decl, *p.method, p.method_suffix,
                                                p.type_subs);
            }
            changed = true;
        }
    }
}

// Request enum instantiation - returns mangled name
// Immediately generates the type definition to type_defs_buffer_ if not already generated
auto LLVMIRGen::require_enum_instantiation(const std::string& base_name,
                                           const std::vector<types::TypePtr>& type_args)
    -> std::string {
    std::string mangled = mangle_struct_name(base_name, type_args);

    auto it = enum_instantiations_.find(mangled);
    if (it != enum_instantiations_.end()) {
        return mangled;
    }

    enum_instantiations_[mangled] = GenericInstantiation{
        base_name, type_args, mangled,
        true // Mark as generated since we'll generate it immediately
    };

    // Register enum variants and generate type definition immediately
    auto decl_it = pending_generic_enums_.find(base_name);
    if (decl_it != pending_generic_enums_.end()) {
        const parser::EnumDecl* decl = decl_it->second;

        // Register variant tags with mangled enum name
        int tag = 0;
        for (const auto& variant : decl->variants) {
            std::string key = mangled + "::" + variant.name;
            enum_variants_[key] = tag++;
        }

        // Generate type definition immediately to type_defs_buffer_
        gen_enum_instantiation(*decl, type_args);
    }

    return mangled;
}

// Placeholder for function instantiation (will implement when adding generic functions)
auto LLVMIRGen::require_func_instantiation(const std::string& base_name,
                                           const std::vector<types::TypePtr>& type_args)
    -> std::string {
    std::string mangled = mangle_func_name(base_name, type_args);

    // Register the instantiation if not already registered
    if (func_instantiations_.find(mangled) == func_instantiations_.end()) {
        func_instantiations_[mangled] = GenericInstantiation{
            base_name, type_args, mangled,
            false // not generated yet
        };
    }

    return mangled;
}

// Request class instantiation - returns mangled name
// Records the instantiation request; actual generation is deferred to
// generate_pending_instantiations
auto LLVMIRGen::require_class_instantiation(const std::string& base_name,
                                            const std::vector<types::TypePtr>& type_args)
    -> std::string {
    // Use the same mangling as structs for consistency
    std::string mangled = mangle_struct_name(base_name, type_args);

    auto it = class_instantiations_.find(mangled);
    if (it != class_instantiations_.end()) {
        return mangled;
    }

    // Record instantiation request - generation is deferred to generate_pending_instantiations
    class_instantiations_[mangled] = GenericInstantiation{
        base_name, type_args, mangled,
        false // Mark as NOT generated - will be generated in generate_pending_instantiations
    };

    return mangled;
}

// Generate a monomorphized class instance from a generic class declaration
void LLVMIRGen::gen_class_instantiation(const parser::ClassDecl& c,
                                        const std::vector<types::TypePtr>& type_args) {
    // Build mangled name
    std::string mangled = mangle_struct_name(c.name, type_args);

    // Skip if already generated
    if (class_types_.find(mangled) != class_types_.end()) {
        return;
    }

    // Create type substitution map
    std::unordered_map<std::string, types::TypePtr> type_subs;
    for (size_t i = 0; i < c.generics.size() && i < type_args.size(); ++i) {
        type_subs[c.generics[i].name] = type_args[i];
    }

    // Save and set current type substitutions for field type resolution
    auto saved_subs = current_type_subs_;
    current_type_subs_ = type_subs;

    // Generate LLVM type name
    std::string type_name = "%class." + mangled;

    // Collect field types with substituted generic parameters
    std::vector<std::string> field_types;
    field_types.push_back("ptr"); // Vtable pointer is always first

    // If class extends another, include base class as embedded struct
    if (c.extends) {
        std::string base_name = c.extends->segments.back();
        field_types.push_back("%class." + base_name);
    }

    // Add own instance fields with generic substitution
    std::vector<ClassFieldInfo> field_info;
    size_t field_offset = field_types.size();

    for (const auto& field : c.fields) {
        if (field.is_static)
            continue;

        // Resolve field type with generic substitution - always use substitution
        // to handle generic type parameters like T -> I32
        auto resolved = resolve_parser_type_with_subs(*field.type, type_subs);
        std::string ft = llvm_type_from_semantic(resolved);
        if (ft == "void")
            ft = "{}";
        field_types.push_back(ft);

        field_info.push_back(
            {field.name, static_cast<int>(field_offset++), ft, field.vis, false, {}});
    }

    // Emit class type definition
    std::string def = type_name + " = type { ";
    for (size_t i = 0; i < field_types.size(); ++i) {
        if (i > 0)
            def += ", ";
        def += field_types[i];
    }
    def += " }";
    type_defs_buffer_ << def << "\n";

    // Register class type and fields
    class_types_[mangled] = type_name;
    class_fields_[mangled] = field_info;

    // Generate vtable for this instantiation
    std::string vtable_type_name = "%vtable." + mangled;
    std::string vtable_name = "@vtable." + mangled;

    // Collect virtual methods and their function names
    std::vector<std::string> vtable_func_names;
    std::vector<VirtualMethodInfo> vtable_methods;
    size_t vtable_idx = 0;
    for (const auto& method : c.methods) {
        if (method.is_virtual || method.is_abstract) {
            // Do NOT use suite prefix for generic class method instantiations
            // These are library methods shared across all test files in a suite
            std::string method_func_name = "@tml_" + mangled + "_" + method.name;
            vtable_func_names.push_back(method_func_name);
            vtable_methods.push_back({method.name, mangled, mangled, vtable_idx++});
        }
    }

    if (!vtable_func_names.empty()) {
        // Generate vtable type
        std::string vtable_type = vtable_type_name + " = type { ";
        for (size_t i = 0; i < vtable_func_names.size(); ++i) {
            if (i > 0)
                vtable_type += ", ";
            vtable_type += "ptr";
        }
        vtable_type += " }";
        type_defs_buffer_ << vtable_type << "\n";

        // Generate vtable global
        std::string vtable_value = "{ ";
        for (size_t i = 0; i < vtable_func_names.size(); ++i) {
            if (i > 0)
                vtable_value += ", ";
            vtable_value += "ptr " + vtable_func_names[i];
        }
        vtable_value += " }";
        type_defs_buffer_ << vtable_name << " = internal constant " << vtable_type_name << " "
                          << vtable_value << "\n";
    } else {
        // Empty vtable - just emit a pointer placeholder
        type_defs_buffer_ << vtable_type_name << " = type { ptr }\n";
        type_defs_buffer_ << vtable_name << " = internal constant " << vtable_type_name
                          << " { ptr null }\n";
    }

    // Store vtable layout
    class_vtable_layout_[mangled] = vtable_methods;

    // Generate constructors with mangled name
    for (const auto& ctor : c.constructors) {
        gen_class_constructor_instantiation(c, ctor, mangled, type_subs);
    }

    // Generate methods with mangled name
    for (const auto& method : c.methods) {
        if (!method.is_abstract) {
            gen_class_method_instantiation(c, method, mangled, type_subs);
        }
    }

    // Restore type substitutions
    current_type_subs_ = saved_subs;
}

} // namespace tml::codegen
