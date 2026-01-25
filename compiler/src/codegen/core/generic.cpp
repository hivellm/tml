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
        while (!pending_impl_method_instantiations_.empty()) {
            auto pending = std::move(pending_impl_method_instantiations_);
            pending_impl_method_instantiations_.clear();

            for (const auto& pim : pending) {
                // First check locally defined impls
                auto impl_it = pending_generic_impls_.find(pim.base_type_name);
                if (impl_it != pending_generic_impls_.end()) {
                    const auto& impl = *impl_it->second;

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
                                for (const auto& concrete_binding : concrete_impl.type_bindings) {
                                    auto concrete_resolved =
                                        resolve_parser_type_with_subs(*concrete_binding.type, {});
                                    current_associated_types_[concrete_binding.name] =
                                        concrete_resolved;
                                }
                            }
                        }
                    }

                    // Now resolve the impl's own type bindings with the substitutions
                    for (const auto& binding : impl.type_bindings) {
                        // Resolve the binding type with the current type substitutions
                        auto resolved = resolve_parser_type_with_subs(*binding.type, pim.type_subs);
                        current_associated_types_[binding.name] = resolved;
                    }

                    // Find the method in the impl block
                    for (const auto& m : impl.methods) {
                        if (m.name == pim.method_name) {
                            gen_impl_method_instantiation(pim.mangled_type_name, m, pim.type_subs,
                                                          impl.generics, pim.method_type_suffix,
                                                          pim.is_library_type, pim.base_type_name);
                            break;
                        }
                    }

                    // Restore associated types
                    current_associated_types_ = saved_associated_types;
                } else if (env_.module_registry()) {
                    // Check imported modules - need to re-parse to get impl AST
                    const auto& all_modules = env_.module_registry()->get_all_modules();
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

                        // Re-parse the module source to get impl AST
                        if (mod.source_code.empty())
                            continue;

                        auto source = lexer::Source::from_string(mod.source_code, mod.file_path);
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

                        const auto& parsed_mod = std::get<parser::Module>(parse_result);

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

                            // Then resolve the impl's own type bindings
                            for (const auto& binding : impl_decl.type_bindings) {
                                auto resolved =
                                    resolve_parser_type_with_subs(*binding.type, pim.type_subs);
                                current_associated_types_[binding.name] = resolved;
                            }

                            // Find the method
                            for (size_t mi = 0; mi < impl_decl.methods.size(); ++mi) {
                                const auto& method_decl = impl_decl.methods[mi];
                                if (method_decl.name == pim.method_name) {
                                    gen_impl_method_instantiation(
                                        pim.mangled_type_name, method_decl, pim.type_subs,
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
