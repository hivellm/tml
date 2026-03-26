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

    // 4. Ensure all referenced struct/enum types are defined.
    // When a field references %struct.BorrowState (a module-internal enum), that type
    // must be emitted before this struct. Search for undefined %struct.X references
    // and emit them by computing the enum layout from the module registry.
    for (const auto& ft : field_types) {
        if (ft.starts_with("%struct.") && struct_types_.find(ft.substr(8)) == struct_types_.end()) {
            std::string dep_name = ft.substr(8); // e.g., "BorrowState"
            if (env_.module_registry()) {
                for (const auto& [mod_path, mod] : env_.module_registry()->get_all_modules()) {
                    // Lambda to find and emit an enum definition from a map
                    auto find_and_emit =
                        [&](const std::unordered_map<std::string, types::EnumDef>& enum_map)
                        -> bool {
                        auto it = enum_map.find(dep_name);
                        if (it == enum_map.end())
                            return false;
                        const auto& edef = it->second;
                        // Compute enum layout: { i32 tag, [max_payload] }
                        int max_payload_bytes = 0;
                        for (const auto& [vname, vtypes] : edef.variants) {
                            int variant_size = 0;
                            for (const auto& vt : vtypes) {
                                std::string llvm_ft = llvm_type_from_semantic(vt);
                                if (llvm_ft == "i8")
                                    variant_size += 1;
                                else if (llvm_ft == "i16")
                                    variant_size += 2;
                                else if (llvm_ft == "i32" || llvm_ft == "float")
                                    variant_size += 4;
                                else
                                    variant_size += 8;
                            }
                            if (variant_size > max_payload_bytes)
                                max_payload_bytes = variant_size;
                        }
                        std::string dep_type_name = "%struct." + dep_name;
                        std::string enum_def;
                        if (max_payload_bytes == 0)
                            enum_def = dep_type_name + " = type { i32 }";
                        else if (max_payload_bytes <= 4)
                            enum_def = dep_type_name + " = type { i32, i32 }";
                        else
                            enum_def = dep_type_name + " = type { i32, [1 x i64] }";
                        type_defs_buffer_ << enum_def << "\n";
                        struct_types_[dep_name] = dep_type_name;
                        int tag = 0;
                        for (const auto& [vname, vtypes] : edef.variants) {
                            enum_variants_[dep_name + "::" + vname] = tag++;
                        }
                        return true;
                    };
                    if (find_and_emit(mod.enums) || find_and_emit(mod.internal_enums))
                        break;
                }
            }
        }
    }

    // 5. Emit struct type definition to type_defs_buffer_ (ensures types before functions)
    std::string def = type_name + " = type { ";
    for (size_t i = 0; i < field_types.size(); ++i) {
        if (i > 0)
            def += ", ";
        def += field_types[i];
    }
    def += " }";
    type_defs_buffer_ << def << "\n";

    // 6. Register for later use
    struct_types_[mangled] = type_name;
    struct_fields_[mangled] = fields;
}

// Request instantiation of a generic struct - returns mangled name
// Immediately generates the type definition to type_defs_buffer_ if not already generated
auto LLVMIRGen::require_struct_instantiation(const std::string& raw_name,
                                             const std::vector<types::TypePtr>& type_args)
    -> std::string {
    // Strip module qualifiers (e.g., "core::marker::PhantomData" -> "PhantomData")
    // LLVM identifiers cannot contain "::" so we must use only the simple name.
    std::string base_name = raw_name;
    auto last_colon = base_name.rfind("::");
    if (last_colon != std::string::npos) {
        base_name = base_name.substr(last_colon + 2);
    }

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
    //
    // IMPORTANT: Skip this shortcut for generic structs that have type-dependent field layouts.
    // For types like Take[I] where field `inner: I` depends on the type parameter,
    // copying the base type's fields would give wrong types (e.g., i32 instead of
    // %struct.Repeat__I32). Only use this alias path for types where all instantiations
    // share the same layout (like runtime-backed collections with { ptr } layout).
    bool base_is_generic_struct = false;
    if (env_.module_registry()) {
        const auto& all_mods = env_.module_registry()->get_all_modules();
        for (const auto& [mod_name, mod] : all_mods) {
            auto sit = mod.structs.find(base_name);
            if (sit != mod.structs.end() && !sit->second.type_params.empty()) {
                base_is_generic_struct = true;
                break;
            }
            sit = mod.internal_structs.find(base_name);
            if (sit != mod.internal_structs.end() && !sit->second.type_params.empty()) {
                base_is_generic_struct = true;
                break;
            }
        }
    }
    if (!base_is_generic_struct &&
        pending_generic_structs_.find(base_name) != pending_generic_structs_.end()) {
        base_is_generic_struct = true;
    }
    // Secondary check: if the base type has fields whose LLVM types could vary
    // with generic parameters (e.g., %struct.X fields), mark as generic to avoid
    // the alias shortcut producing inconsistent layouts across compilation units.
    // This fixes suite merging symbol collisions when max_per_suite > 1.
    if (!base_is_generic_struct && mangled != base_name) {
        auto base_fields_check = struct_fields_.find(base_name);
        if (base_fields_check != struct_fields_.end()) {
            for (const auto& field : base_fields_check->second) {
                const auto& ft = field.llvm_type;
                // A field type is layout-stable if it's a primitive LLVM type
                // that doesn't depend on generic parameters. If any field has
                // a struct type (%struct.*) or other complex type, the layout
                // may vary with different generic instantiations.
                if (ft.find("%struct.") != std::string::npos ||
                    ft.find("%union.") != std::string::npos ||
                    ft.find("%enum.") != std::string::npos) {
                    base_is_generic_struct = true;
                    break;
                }
            }
        }
    }
    if (!base_is_generic_struct && mangled != base_name &&
        struct_types_.find(base_name) != struct_types_.end() &&
        struct_types_.find(mangled) == struct_types_.end()) {
        // The base type already has a definition with layout-stable fields
        // (all primitives like ptr, i64, etc. — no %struct.* references).
        // Safe to create a type alias since all instantiations share the same layout.
        // Examples: BTreeMap { ptr }, HashMapIter { ptr, ptr, i64, i64 }, etc.
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

    // Slice[T] and MutSlice[T] are fat pointers — always { ptr, i64 }
    if (base_name == "Slice" || base_name == "MutSlice") {
        struct_instantiations_[mangled] =
            GenericInstantiation{base_name, final_type_args, mangled, true};
        std::string type_name = "%struct." + mangled;
        std::string def = type_name + " = type { ptr, i64 }";
        type_defs_buffer_ << def << "\n";
        struct_types_[mangled] = type_name;
        struct_fields_[mangled] = {{"data", 0, "ptr", types::make_ptr(types::make_unit())},
                                   {"len", 1, "i64", types::make_i64()}};
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
    // When multiple structs share the same simple name (e.g., iter::Take vs async_iter::Take),
    // pending_generic_structs_ may have the wrong definition. Prefer the module registry when
    // it has a matching generic struct, as it provides the correct semantic field definitions.
    bool use_pending_generic = decl_it != pending_generic_structs_.end();

    if (use_pending_generic && env_.module_registry() &&
        local_generic_struct_names_.find(base_name) == local_generic_struct_names_.end()) {
        // Check if multiple modules define a struct with this name — if so, there's
        // ambiguity and pending_generic_structs_ may have the wrong one.
        // Skip this check for local structs — the local definition always wins.
        int module_count = 0;
        const auto& all_modules = env_.module_registry()->get_all_modules();
        for (const auto& [mod_name, mod] : all_modules) {
            auto sit = mod.structs.find(base_name);
            if (sit != mod.structs.end() && !sit->second.type_params.empty()) {
                ++module_count;
            } else {
                sit = mod.internal_structs.find(base_name);
                if (sit != mod.internal_structs.end() && !sit->second.type_params.empty()) {
                    ++module_count;
                }
            }
        }
        if (module_count > 1) {
            // Multiple modules define this generic struct — pending_generic_structs_
            // doesn't track which module, so fall through to module registry path
            // which can disambiguate using type_args.
            use_pending_generic = false;
        }
    }

    if (use_pending_generic) {
        const parser::StructDecl* decl = decl_it->second;

        // Create substitution map and const generic values map
        std::unordered_map<std::string, types::TypePtr> subs;
        auto saved_const_values = current_const_generic_values_;
        for (size_t i = 0; i < decl->generics.size() && i < final_type_args.size(); ++i) {
            subs[decl->generics[i].name] = final_type_args[i];
            // For const generic params, extract the resolved value
            if (decl->generics[i].is_const && final_type_args[i] &&
                final_type_args[i]->is<types::ConstGenericType>()) {
                const auto& cgt = final_type_args[i]->as<types::ConstGenericType>();
                if (cgt.resolved_value.has_value()) {
                    current_const_generic_values_[decl->generics[i].name] = *cgt.resolved_value;
                }
            }
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
        current_const_generic_values_ = saved_const_values;
    }
    // Handle imported generic structs from module registry
    else if (env_.module_registry()) {
        const auto& all_modules = env_.module_registry()->get_all_modules();
        bool found_in_registry = false;

        // When multiple modules define the same struct name (e.g., iter::Take vs
        // async_iter::Take), prefer the module that also contains the type_arg types.
        // For Take[Repeat[I32]], if Repeat is in async_iter, prefer async_iter::Take.
        std::string preferred_module;
        for (const auto& arg : final_type_args) {
            if (arg && arg->is<types::NamedType>()) {
                const auto& arg_named = arg->as<types::NamedType>();
                for (const auto& [mn, m] : all_modules) {
                    if (m.structs.count(arg_named.name) || m.enums.count(arg_named.name)) {
                        preferred_module = mn;
                        break;
                    }
                }
                if (!preferred_module.empty()) {
                    break;
                }
            }
        }

        // First pass: try preferred module if set
        // Second pass: try all modules
        for (int pass = 0; pass < 2 && !found_in_registry; ++pass) {
            for (const auto& [mod_name, mod] : all_modules) {
                if (pass == 0 && !preferred_module.empty() && mod_name != preferred_module) {
                    continue;
                }
                if (pass == 1 && mod_name == preferred_module) {
                    continue; // Already tried in pass 0
                }
                // Check public structs first
                auto struct_it = mod.structs.find(base_name);
                bool found =
                    struct_it != mod.structs.end() && !struct_it->second.type_params.empty();

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

                    // Create substitution map from type params and const params
                    std::unordered_map<std::string, types::TypePtr> subs;
                    // Map type params first: type_params[0..n] -> final_type_args[0..n]
                    size_t type_param_count = struct_def.type_params.size();
                    for (size_t i = 0; i < type_param_count && i < final_type_args.size(); ++i) {
                        subs[struct_def.type_params[i]] = final_type_args[i];
                    }
                    // Map const params: const_params[0..m] -> final_type_args[n..n+m]
                    for (size_t i = 0; i < struct_def.const_params.size(); ++i) {
                        size_t arg_idx = type_param_count + i;
                        if (arg_idx < final_type_args.size()) {
                            subs[struct_def.const_params[i].name] = final_type_args[arg_idx];
                        }
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
        } // end pass loop

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
