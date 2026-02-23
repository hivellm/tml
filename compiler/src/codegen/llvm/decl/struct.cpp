TML_MODULE("codegen_x86")

//! # LLVM IR Generator - Struct Declarations
//!
//! This file implements struct declaration and instantiation code generation.

#include "codegen/llvm/llvm_ir_gen.hpp"

namespace tml::codegen {

void LLVMIRGen::gen_struct_decl(const parser::StructDecl& s) {
    // Store struct declaration for all structs (needed for default field values)
    struct_decls_[s.name] = &s;

    // If struct has generic parameters, defer generation until instantiated
    if (!s.generics.empty()) {
        pending_generic_structs_[s.name] = &s;
        return;
    }

    // Skip builtin types that are already declared in the runtime
    if (s.name == "Ordering") {
        // Register field info for builtin structs but don't emit type definition
        std::string type_name = "%struct." + s.name;
        std::vector<FieldInfo> fields;
        for (size_t i = 0; i < s.fields.size(); ++i) {
            std::string ft = llvm_type_ptr(s.fields[i].type);
            // Function pointer fields use fat pointer to support closures
            if (s.fields[i].type && s.fields[i].type->is<parser::FuncType>()) {
                ft = "{ ptr, ptr }";
            }
            types::TypePtr sem_type = resolve_parser_type_with_subs(*s.fields[i].type, {});
            fields.push_back({s.fields[i].name, static_cast<int>(i), ft, sem_type});
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
        // Function pointer fields use fat pointer { fn_ptr, env_ptr } to support closures
        if (s.fields[i].type && s.fields[i].type->is<parser::FuncType>()) {
            ft = "{ ptr, ptr }";
        }
        field_types.push_back(ft);
        types::TypePtr sem_type = resolve_parser_type_with_subs(*s.fields[i].type, {});
        fields.push_back({s.fields[i].name, static_cast<int>(i), ft, sem_type});
    }

    // Register first to prevent duplicates from recursive types
    struct_types_[s.name] = type_name;
    struct_fields_[s.name] = fields;

    // Check for @simd annotation — emit LLVM vector type instead of struct
    bool is_simd = false;
    for (const auto& deco : s.decorators) {
        if (deco.name == "simd") {
            is_simd = true;
            break;
        }
    }

    if (is_simd && !field_types.empty()) {
        // All fields must be the same type for SIMD vectors
        std::string elem_type = field_types[0];
        int lane_count = static_cast<int>(field_types.size());
        std::string def =
            type_name + " = type <" + std::to_string(lane_count) + " x " + elem_type + ">";
        type_defs_buffer_ << def << "\n";
        simd_types_[s.name] = {elem_type, lane_count};
    } else {
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

    // Generate @derive support if decorated
    gen_derive_reflect_struct(s);
    gen_derive_partial_eq_struct(s);
    gen_derive_duplicate_struct(s);
    gen_derive_hash_struct(s);
    gen_derive_default_struct(s);
    gen_derive_partial_ord_struct(s);
    gen_derive_ord_struct(s);
    gen_derive_debug_struct(s);
    gen_derive_display_struct(s);
    gen_derive_serialize_struct(s);
    gen_derive_deserialize_struct(s);
    gen_derive_fromstr_struct(s);
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
        // Function pointer fields use fat pointer { fn_ptr, env_ptr } to support closures
        if (field_type && field_type->is<types::FuncType>()) {
            ft = "{ ptr, ptr }";
        }
        field_types.push_back(ft);
        fields.push_back({decl.fields[i].name, static_cast<int>(i), ft, field_type});
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
    // Check for unresolved generic types in type_args
    // If any type argument contains unresolved generics, we cannot instantiate yet
    // This prevents creating invalid struct types with incomplete type arguments
    // First, try to apply current type substitutions to resolve any unresolved generics
    std::vector<types::TypePtr> resolved_args = type_args;
    if (!current_type_subs_.empty()) {
        for (auto& arg : resolved_args) {
            arg = apply_type_substitutions(arg, current_type_subs_);
        }
    }

    for (const auto& arg : resolved_args) {
        if (contains_unresolved_generic(arg)) {
            // Define a dummy struct with fields matching Mutex to avoid index errors
            // This is a workaround until we fix the root cause
            std::string placeholder_name = base_name + "__UNRESOLVED";
            if (struct_types_.find(placeholder_name) == struct_types_.end()) {
                // Mutex has 3 fields: data (ptr), handle (ptr), is_locked (i1)
                std::string type_def = "%struct." + placeholder_name + " = type { ptr, ptr, i1 }";
                type_defs_buffer_ << type_def << "\n";
                struct_types_[placeholder_name] = "%struct." + placeholder_name;
                // Register dummy fields
                struct_fields_[placeholder_name] = {{"data", 0, "ptr", types::make_i64()},
                                                    {"handle", 1, "ptr", types::make_i64()},
                                                    {"is_locked", 2, "i1", types::make_bool()}};
            }
            return placeholder_name;
        }
    }

    // Use resolved_args for the rest of the function instead of type_args
    const std::vector<types::TypePtr>& final_type_args = resolved_args;

    // Generate mangled name
    std::string mangled = mangle_struct_name(base_name, final_type_args);

    // Check if already registered
    auto it = struct_instantiations_.find(mangled);
    if (it != struct_instantiations_.end()) {
        return mangled; // Already queued or generated
    }

    // If the base (unmangled) type already exists in struct_types_ (e.g., library code
    // emitted %struct.BTreeMap during emit_module_pure_tml_functions), create a type alias
    // from the mangled name to the existing type. This ensures user code that references
    // %struct.BTreeMap__I64 resolves to the same type as library functions using %struct.BTreeMap.
    // Without this, tml run/build fails with "Cannot allocate unsized type" because the mangled
    // type is never defined while the unmangled version is.
    if (mangled != base_name && struct_types_.find(base_name) != struct_types_.end() &&
        struct_types_.find(mangled) == struct_types_.end()) {
        // The base type already has a definition (e.g., library code emitted %struct.HashMapIter).
        // Emit the mangled type with the same field layout so both names are valid in IR.
        // Without this, code paths that use the mangled name directly (e.g., struct literal
        // construction via current_ret_type_) would reference an undefined type.
        std::string mangled_type = "%struct." + mangled;
        // Build field list from the base type's registered fields
        auto base_fields_it = struct_fields_.find(base_name);
        if (base_fields_it != struct_fields_.end()) {
            std::string def = mangled_type + " = type { ";
            for (size_t i = 0; i < base_fields_it->second.size(); ++i) {
                if (i > 0)
                    def += ", ";
                def += base_fields_it->second[i].llvm_type;
            }
            def += " }";
            type_defs_buffer_ << def << "\n";
        } else {
            // Fallback: single ptr field (common for handle-based types)
            type_defs_buffer_ << mangled_type << " = type { ptr }\n";
        }
        struct_types_[mangled] = mangled_type;
        // Copy field info from the base type
        if (base_fields_it != struct_fields_.end()) {
            struct_fields_[mangled] = base_fields_it->second;
        }
        struct_instantiations_[mangled] =
            GenericInstantiation{base_name, final_type_args, mangled, true};
        return mangled;
    }

    // RawPtr[T] and RawMutPtr[T] are type-erased pointer wrappers — always { i64 }
    // regardless of the type parameter. Handle them like other runtime-backed types
    // (List, HashMap) to ensure the type definition is always emitted correctly.
    if (base_name == "RawPtr" || base_name == "RawMutPtr") {
        struct_instantiations_[mangled] =
            GenericInstantiation{base_name, final_type_args, mangled, true};
        std::string type_name = "%struct." + mangled;
        std::string def = type_name + " = type { i64 }";
        type_defs_buffer_ << def << "\n";
        struct_types_[mangled] = type_name;
        struct_fields_[mangled] = {{"addr", 0, "i64", types::make_i64()}};
        return mangled;
    }

    // Register new instantiation (mark as generated since we'll generate immediately)
    struct_instantiations_[mangled] = GenericInstantiation{
        base_name, final_type_args, mangled,
        true // Mark as generated since we'll generate it immediately
    };

    // Register field info and generate type definition immediately
    auto decl_it = pending_generic_structs_.find(base_name);
    if (decl_it != pending_generic_structs_.end()) {
        const parser::StructDecl* decl = decl_it->second;

        // Create substitution map
        std::unordered_map<std::string, types::TypePtr> subs;
        for (size_t i = 0; i < decl->generics.size() && i < final_type_args.size(); ++i) {
            subs[decl->generics[i].name] = final_type_args[i];
        }

        // Register field info
        std::vector<FieldInfo> fields;
        for (size_t i = 0; i < decl->fields.size(); ++i) {
            types::TypePtr field_type = resolve_parser_type_with_subs(*decl->fields[i].type, subs);
            // Use for_data=true since struct fields need concrete types (Unit -> {} not void)
            std::string ft = llvm_type_from_semantic(field_type, true);
            fields.push_back({decl->fields[i].name, static_cast<int>(i), ft, field_type});
        }
        struct_fields_[mangled] = fields;

        // Recursively instantiate type arguments that are generic types
        // This ensures that types like LinkedListNode[I64] in List[LinkedListNode[I64]]
        // are instantiated before they're used in method bodies
        for (const auto& arg : final_type_args) {
            if (arg && arg->is<types::NamedType>()) {
                const auto& named = arg->as<types::NamedType>();
                if (!named.type_args.empty()) {
                    require_struct_instantiation(named.name, named.type_args);
                }
            }
        }

        // Generate type definition immediately to type_defs_buffer_
        gen_struct_instantiation(*decl, final_type_args);
    }
    // Handle imported generic structs from module registry
    else if (env_.module_registry()) {
        const auto& all_modules = env_.module_registry()->get_all_modules();
        bool found_in_registry = false;
        for (const auto& [mod_name, mod] : all_modules) {
            // Check public structs first
            auto struct_it = mod.structs.find(base_name);
            bool found = struct_it != mod.structs.end() && !struct_it->second.type_params.empty();

            // Also check internal structs (for module-internal types like ArcInner)
            if (!found) {
                struct_it = mod.internal_structs.find(base_name);
                found = struct_it != mod.internal_structs.end() &&
                        !struct_it->second.type_params.empty();
            }

            if (found) {
                found_in_registry = true;
                // Found imported generic struct - use its semantic definition
                const auto& struct_def = struct_it->second;

                // Create substitution map from type params
                std::unordered_map<std::string, types::TypePtr> subs;
                for (size_t i = 0; i < struct_def.type_params.size() && i < final_type_args.size();
                     ++i) {
                    subs[struct_def.type_params[i]] = final_type_args[i];
                }

                // Register field info using the semantic struct definition
                std::vector<FieldInfo> fields;
                std::vector<std::string> field_types_vec;
                int field_idx = 0;
                for (const auto& field : struct_def.fields) {
                    // Apply type substitution to field type
                    types::TypePtr resolved_type = apply_type_substitutions(field.type, subs);
                    std::string ft = llvm_type_from_semantic(resolved_type, true);
                    fields.push_back({field.name, field_idx++, ft, resolved_type});
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

                // Recursively instantiate type arguments that are generic types
                // This ensures that types like LinkedListNode[I64] in List[LinkedListNode[I64]]
                // are instantiated before they're used in method bodies
                for (const auto& arg : final_type_args) {
                    if (arg && arg->is<types::NamedType>()) {
                        const auto& named = arg->as<types::NamedType>();
                        if (!named.type_args.empty()) {
                            require_struct_instantiation(named.name, named.type_args);
                        }
                    }
                }
                break;
            }
        }

        // Fallback: if not found in registry, check if it's a known runtime-backed collection type
        // These types have well-defined layouts regardless of their type parameter
        if (!found_in_registry) {
            if (base_name == "List" || base_name == "Vec" || base_name == "Array") {
                // List[T] = type { handle: *Unit } - all instantiations are { ptr }
                std::string type_name = "%struct." + mangled;
                std::string def = type_name + " = type { ptr }";
                type_defs_buffer_ << def << "\n";
                struct_types_[mangled] = type_name;
                struct_fields_[mangled] = {
                    {"handle", 0, "ptr", types::make_ptr(types::make_unit())}};

                // Recursively instantiate type arguments that are generic types
                // This ensures that types like LinkedListNode[I64] in List[LinkedListNode[I64]]
                // are instantiated before they're used in method bodies
                for (const auto& arg : final_type_args) {
                    if (arg && arg->is<types::NamedType>()) {
                        const auto& named = arg->as<types::NamedType>();
                        if (!named.type_args.empty()) {
                            require_struct_instantiation(named.name, named.type_args);
                        }
                    }
                }
            }
            // Note: HashMap removed — now uses normal generic struct instantiation path
            // (HashMap[K,V] { handle: *Unit } naturally produces { ptr })
        }
    }
    // Fallback for when module registry isn't available
    else {
        if (base_name == "List" || base_name == "Vec" || base_name == "Array") {
            std::string type_name = "%struct." + mangled;
            std::string def = type_name + " = type { ptr }";
            type_defs_buffer_ << def << "\n";
            struct_types_[mangled] = type_name;
            struct_fields_[mangled] = {{"handle", 0, "ptr", types::make_ptr(types::make_unit())}};

            // Recursively instantiate type arguments
            for (const auto& arg : final_type_args) {
                if (arg && arg->is<types::NamedType>()) {
                    const auto& named = arg->as<types::NamedType>();
                    if (!named.type_args.empty()) {
                        require_struct_instantiation(named.name, named.type_args);
                    }
                }
            }
        } else if (base_name == "HashMap" || base_name == "Map" || base_name == "Dict") {
            std::string type_name = "%struct." + mangled;
            std::string def = type_name + " = type { ptr }";
            type_defs_buffer_ << def << "\n";
            struct_types_[mangled] = type_name;
            struct_fields_[mangled] = {{"handle", 0, "ptr", types::make_ptr(types::make_unit())}};

            // Recursively instantiate type arguments
            for (const auto& arg : final_type_args) {
                if (arg && arg->is<types::NamedType>()) {
                    const auto& named = arg->as<types::NamedType>();
                    if (!named.type_args.empty()) {
                        require_struct_instantiation(named.name, named.type_args);
                    }
                }
            }
        }
    }

    return mangled;
}

void LLVMIRGen::gen_union_decl(const parser::UnionDecl& u) {
    // Union types are stored like structs but with is_union flag
    // In LLVM, unions are represented as a byte array of the max field size

    std::string type_name = "%union." + u.name;

    // Check if already emitted
    if (struct_types_.find(u.name) != struct_types_.end()) {
        return;
    }

    // First pass: ensure all field types are defined and calculate max size
    int64_t max_size = 0;
    int64_t max_align = 1;
    std::vector<std::string> field_llvm_types;

    for (const auto& field : u.fields) {
        ensure_type_defined(field.type);
        std::string ft = llvm_type_ptr(field.type);
        if (ft == "void")
            ft = "{}";
        field_llvm_types.push_back(ft);

        // Calculate size of this field type
        int64_t field_size = get_type_size(ft);
        if (field_size > 0 && field_size > max_size) {
            max_size = field_size;
        }
        // Track alignment (simplified - use same as size for primitives)
        if (field_size > max_align) {
            max_align = field_size;
        }
    }

    // Minimum size of 1 byte for empty unions
    if (max_size <= 0) {
        max_size = 1;
    }

    // Register field info - all fields are at index 0 (they overlap)
    std::vector<FieldInfo> fields;
    for (size_t i = 0; i < u.fields.size(); ++i) {
        types::TypePtr sem_type = resolve_parser_type_with_subs(*u.fields[i].type, {});
        // All union fields are at "index 0" since they all start at the same memory location
        fields.push_back({u.fields[i].name, 0, field_llvm_types[i], sem_type});
    }

    // Register first to prevent duplicates
    struct_types_[u.name] = type_name;
    struct_fields_[u.name] = fields;
    union_types_.insert(u.name); // Mark as union for field access codegen

    // Emit union type definition as a byte array
    // The union is represented as { [N x i8] } where N is the max field size
    std::string def = type_name + " = type { [" + std::to_string(max_size) + " x i8] }";
    type_defs_buffer_ << def << "\n";
}

} // namespace tml::codegen
