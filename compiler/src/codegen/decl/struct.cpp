//! # LLVM IR Generator - Struct Declarations
//!
//! This file implements struct declaration and instantiation code generation.

#include "codegen/llvm_ir_gen.hpp"

namespace tml::codegen {

void LLVMIRGen::gen_struct_decl(const parser::StructDecl& s) {
    // If struct has generic parameters, defer generation until instantiated
    if (!s.generics.empty()) {
        pending_generic_structs_[s.name] = &s;
        return;
    }

    // Skip builtin types that are already declared in the runtime
    if (s.name == "File" || s.name == "Path" || s.name == "Ordering") {
        // Register field info for builtin structs but don't emit type definition
        std::string type_name = "%struct." + s.name;
        std::vector<FieldInfo> fields;
        for (size_t i = 0; i < s.fields.size(); ++i) {
            std::string ft = llvm_type_ptr(s.fields[i].type);
            fields.push_back({s.fields[i].name, static_cast<int>(i), ft});
        }
        struct_types_[s.name] = type_name;
        struct_fields_[s.name] = fields;
        return;
    }

    // Non-generic struct: generate immediately
    std::string type_name = "%struct." + s.name;

    // Check if already emitted (can happen with re-exports across modules)
    if (struct_types_.find(s.name) != struct_types_.end()) {
        return;
    }

    // First pass: ensure all field types are defined
    // This handles cases where a struct references types from other modules
    // that haven't been processed yet
    for (size_t i = 0; i < s.fields.size(); ++i) {
        ensure_type_defined(s.fields[i].type);
    }

    // Collect field types and register field info
    std::vector<std::string> field_types;
    std::vector<FieldInfo> fields;
    for (size_t i = 0; i < s.fields.size(); ++i) {
        std::string ft = llvm_type_ptr(s.fields[i].type);
        // Unit type as struct field must be {} not void (LLVM doesn't allow void in structs)
        if (ft == "void")
            ft = "{}";
        field_types.push_back(ft);
        fields.push_back({s.fields[i].name, static_cast<int>(i), ft});
    }

    // Register first to prevent duplicates from recursive types
    struct_types_[s.name] = type_name;
    struct_fields_[s.name] = fields;

    // Emit struct type definition to type_defs_buffer_ (ensures types before functions)
    std::string def = type_name + " = type { ";
    for (size_t i = 0; i < field_types.size(); ++i) {
        if (i > 0)
            def += ", ";
        def += field_types[i];
    }
    def += " }";
    type_defs_buffer_ << def << "\n";
}

// Generate a specialized version of a generic struct
void LLVMIRGen::gen_struct_instantiation(const parser::StructDecl& decl,
                                         const std::vector<types::TypePtr>& type_args) {
    // 1. Create substitution map: T -> I32, K -> Str, etc.
    std::unordered_map<std::string, types::TypePtr> subs;
    for (size_t i = 0; i < decl.generics.size() && i < type_args.size(); ++i) {
        subs[decl.generics[i].name] = type_args[i];
    }

    // 2. Generate mangled name: Pair[I32] -> Pair__I32
    std::string mangled = mangle_struct_name(decl.name, type_args);
    std::string type_name = "%struct." + mangled;

    // 3. Collect field types with substitution and register field info
    std::vector<std::string> field_types;
    std::vector<FieldInfo> fields;
    for (size_t i = 0; i < decl.fields.size(); ++i) {
        // Resolve field type and apply substitution
        types::TypePtr field_type = resolve_parser_type_with_subs(*decl.fields[i].type, subs);
        // Use for_data=true since struct fields need concrete types (Unit -> {} not void)
        std::string ft = llvm_type_from_semantic(field_type, true);
        field_types.push_back(ft);
        fields.push_back({decl.fields[i].name, static_cast<int>(i), ft});
    }

    // 4. Emit struct type definition to type_defs_buffer_ (ensures types before functions)
    std::string def = type_name + " = type { ";
    for (size_t i = 0; i < field_types.size(); ++i) {
        if (i > 0)
            def += ", ";
        def += field_types[i];
    }
    def += " }";
    type_defs_buffer_ << def << "\n";

    // 5. Register for later use
    struct_types_[mangled] = type_name;
    struct_fields_[mangled] = fields;
}

// Request instantiation of a generic struct - returns mangled name
// Immediately generates the type definition to type_defs_buffer_ if not already generated
auto LLVMIRGen::require_struct_instantiation(const std::string& base_name,
                                             const std::vector<types::TypePtr>& type_args)
    -> std::string {
    // Generate mangled name
    std::string mangled = mangle_struct_name(base_name, type_args);

    // Check if already registered
    auto it = struct_instantiations_.find(mangled);
    if (it != struct_instantiations_.end()) {
        return mangled; // Already queued or generated
    }

    // Register new instantiation (mark as generated since we'll generate immediately)
    struct_instantiations_[mangled] = GenericInstantiation{
        base_name, type_args, mangled,
        true // Mark as generated since we'll generate it immediately
    };

    // Register field info and generate type definition immediately
    auto decl_it = pending_generic_structs_.find(base_name);
    if (decl_it != pending_generic_structs_.end()) {
        const parser::StructDecl* decl = decl_it->second;

        // Create substitution map
        std::unordered_map<std::string, types::TypePtr> subs;
        for (size_t i = 0; i < decl->generics.size() && i < type_args.size(); ++i) {
            subs[decl->generics[i].name] = type_args[i];
        }

        // Register field info
        std::vector<FieldInfo> fields;
        for (size_t i = 0; i < decl->fields.size(); ++i) {
            types::TypePtr field_type = resolve_parser_type_with_subs(*decl->fields[i].type, subs);
            // Use for_data=true since struct fields need concrete types (Unit -> {} not void)
            std::string ft = llvm_type_from_semantic(field_type, true);
            fields.push_back({decl->fields[i].name, static_cast<int>(i), ft});
        }
        struct_fields_[mangled] = fields;

        // Generate type definition immediately to type_defs_buffer_
        gen_struct_instantiation(*decl, type_args);
    }
    // Handle imported generic structs from module registry
    else if (env_.module_registry()) {
        const auto& all_modules = env_.module_registry()->get_all_modules();
        for (const auto& [mod_name, mod] : all_modules) {
            auto struct_it = mod.structs.find(base_name);
            if (struct_it != mod.structs.end() && !struct_it->second.type_params.empty()) {
                // Found imported generic struct - use its semantic definition
                const auto& struct_def = struct_it->second;

                // Create substitution map from type params
                std::unordered_map<std::string, types::TypePtr> subs;
                for (size_t i = 0; i < struct_def.type_params.size() && i < type_args.size(); ++i) {
                    subs[struct_def.type_params[i]] = type_args[i];
                }

                // Register field info using the semantic struct definition
                std::vector<FieldInfo> fields;
                std::vector<std::string> field_types_vec;
                int field_idx = 0;
                for (const auto& [field_name, field_type] : struct_def.fields) {
                    // Apply type substitution to field type
                    types::TypePtr resolved_type = apply_type_substitutions(field_type, subs);
                    std::string ft = llvm_type_from_semantic(resolved_type, true);
                    fields.push_back({field_name, field_idx++, ft});
                    field_types_vec.push_back(ft);
                }
                struct_fields_[mangled] = fields;

                // Emit struct type definition
                std::string type_name = "%struct." + mangled;
                std::string def = type_name + " = type { ";
                for (size_t i = 0; i < field_types_vec.size(); ++i) {
                    if (i > 0)
                        def += ", ";
                    def += field_types_vec[i];
                }
                def += " }";
                type_defs_buffer_ << def << "\n";
                struct_types_[mangled] = type_name;
                break;
            }
        }
    }

    return mangled;
}

} // namespace tml::codegen
