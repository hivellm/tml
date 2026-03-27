TML_MODULE("codegen_x86")

//! # LLVM IR Generator - Generic Instantiation Helpers
//!
//! This file contains the small helper methods for generic monomorphization.
//! The main instantiation loop (`generate_pending_instantiations`) lives in
//! `generic_instantiate.cpp`.
//!
//! ## Methods
//!
//! | Method                              | Purpose                                        |
//! |-------------------------------------|------------------------------------------------|
//! | `ensure_generic_types_instantiated` | Recursively ensure all type args are instantiated |
//! | `require_enum_instantiation`        | Immediately generate a generic enum instance   |
//! | `require_func_instantiation`        | Queue a generic function for instantiation     |
//! | `require_class_instantiation`       | Defer a generic class for instantiation        |
//! | `gen_class_instantiation`           | Generate a monomorphized class + vtable        |

#include "codegen/llvm/llvm_ir_gen.hpp"
#include "lexer/lexer.hpp"
#include "parser/parser.hpp"
#include "types/module_binary.hpp"

#include <fstream>
#include <functional>

namespace tml::codegen {

// Recursively walk a semantic type tree and ensure all generic enums/structs are instantiated.
// This is needed for default behavior method generation where return types like
// Outcome[Unit, I64] or (I64, Maybe[I64]) may not have been instantiated yet.
void LLVMIRGen::ensure_generic_types_instantiated(const types::TypePtr& type) {
    if (!type)
        return;

    if (type->is<types::NamedType>()) {
        const auto& named = type->as<types::NamedType>();
        if (!named.type_args.empty()) {
            // Try as enum first, then as struct
            if (env_.all_enums().find(named.name) != env_.all_enums().end()) {
                require_enum_instantiation(named.name, named.type_args);
            } else if (env_.lookup_struct(named.name)) {
                require_struct_instantiation(named.name, named.type_args);
            }
            // Recursively check type args
            for (const auto& arg : named.type_args) {
                ensure_generic_types_instantiated(arg);
            }
        }
    } else if (type->is<types::TupleType>()) {
        const auto& tuple = type->as<types::TupleType>();
        for (const auto& elem : tuple.elements) {
            ensure_generic_types_instantiated(elem);
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
        pending_func_keys_.push_back(mangled);
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
    pending_class_keys_.push_back(mangled);

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
            // Use mangled name for generic class method instantiations
            std::string method_func_name = "@" + mangle_impl_method(mangled, method.name);
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
