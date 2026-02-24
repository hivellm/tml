TML_MODULE("codegen_x86")

//! # LLVM IR Generator - Drop/RAII Support
//!
//! This file implements automatic destructor calls at scope exit.
//!
//! ## RAII in TML
//!
//! Types implementing the `Drop` behavior have their `drop()` method
//! called automatically when they go out of scope.
//!
//! ## Drop Scope Stack
//!
//! `drop_scopes_` tracks variables needing drop per lexical scope:
//!
//! | Method            | Action                              |
//! |-------------------|-------------------------------------|
//! | `push_drop_scope` | Enter new scope (e.g., block)       |
//! | `pop_drop_scope`  | Exit scope                          |
//! | `register_for_drop`| Track variable for later drop      |
//! | `emit_scope_drops`| Emit drop calls at scope exit       |
//!
//! ## Drop Order
//!
//! Drops are emitted in LIFO order (last declared, first dropped),
//! matching Rust's drop semantics.
//!
//! ## Generated Code
//!
//! ```llvm
//! ; At scope exit:
//! call void @tml_Resource_drop(ptr %resource)
//! ```

#include "codegen/llvm/llvm_ir_gen.hpp"

namespace tml::codegen {

// Helper to parse mangled type strings for drop instantiation
// Handles: primitives, ptr_X (Ptr[X]), Nested__Y (Nested[Y])
static types::TypePtr parse_mangled_type_for_drop(const std::string& s) {
    // Primitives
    if (s == "I32")
        return types::make_i32();
    if (s == "I64")
        return types::make_i64();
    if (s == "Bool")
        return types::make_bool();
    if (s == "F32")
        return types::make_primitive(types::PrimitiveKind::F32);
    if (s == "F64")
        return types::make_f64();
    if (s == "Str")
        return types::make_str();
    if (s == "I8")
        return types::make_primitive(types::PrimitiveKind::I8);
    if (s == "I16")
        return types::make_primitive(types::PrimitiveKind::I16);
    if (s == "U8")
        return types::make_primitive(types::PrimitiveKind::U8);
    if (s == "U16")
        return types::make_primitive(types::PrimitiveKind::U16);
    if (s == "U32")
        return types::make_primitive(types::PrimitiveKind::U32);
    if (s == "U64")
        return types::make_primitive(types::PrimitiveKind::U64);

    // Pointer prefix: ptr_X -> Ptr[X]
    if (s.size() > 4 && s.substr(0, 4) == "ptr_") {
        auto inner = parse_mangled_type_for_drop(s.substr(4));
        if (inner) {
            auto t = std::make_shared<types::Type>();
            t->kind = types::PtrType{.inner = inner};
            return t;
        }
    }

    // Nested generic: Base__Arg -> Base[Arg]
    auto delim = s.find("__");
    if (delim != std::string::npos) {
        std::string base = s.substr(0, delim);
        std::string arg_str = s.substr(delim + 2);
        auto inner = parse_mangled_type_for_drop(arg_str);
        if (inner) {
            auto t = std::make_shared<types::Type>();
            t->kind = types::NamedType{base, "", {inner}};
            return t;
        }
    }

    // Simple struct type
    auto t = std::make_shared<types::Type>();
    t->kind = types::NamedType{s, "", {}};
    return t;
}

void LLVMIRGen::mark_var_consumed(const std::string& var_name) {
    consumed_vars_.insert(var_name);
}

void LLVMIRGen::mark_field_consumed(const std::string& var_name, const std::string& field_name) {
    // Mark the specific field as consumed: "var.field"
    consumed_vars_.insert(var_name + "." + field_name);
    TML_DEBUG_LN("[DROP] Marked field as consumed: " << var_name << "." << field_name);
}

bool LLVMIRGen::has_consumed_fields(const std::string& var_name) const {
    // Check if any field of this variable has been consumed
    std::string prefix = var_name + ".";
    for (const auto& consumed : consumed_vars_) {
        if (consumed.size() > prefix.size() && consumed.substr(0, prefix.size()) == prefix) {
            return true;
        }
    }
    return false;
}

void LLVMIRGen::push_drop_scope() {
    drop_scopes_.push_back({});
}

void LLVMIRGen::pop_drop_scope() {
    if (!drop_scopes_.empty()) {
        drop_scopes_.pop_back();
    }
}

void LLVMIRGen::register_for_drop(const std::string& var_name, const std::string& var_reg,
                                  const std::string& type_name, const std::string& llvm_type) {
    if (type_name.empty()) {
        return;
    }

    // Check if the type directly implements Drop
    bool has_drop = env_.type_implements(type_name, "Drop");

    // For generic types (MutexGuard__I32), check the base type too
    if (!has_drop) {
        auto sep_pos = type_name.find("__");
        if (sep_pos != std::string::npos) {
            std::string base_type = type_name.substr(0, sep_pos);
            has_drop = env_.type_implements(base_type, "Drop");

            // Also check source code for generic Drop impls
            if (!has_drop && env_.module_registry()) {
                const auto& all_modules = env_.module_registry()->get_all_modules();
                for (const auto& [mod_name, mod] : all_modules) {
                    if (!mod.source_code.empty()) {
                        std::string pattern = "Drop for " + base_type + "[";
                        if (mod.source_code.find(pattern) != std::string::npos) {
                            has_drop = true;
                            break;
                        }
                    }
                }
            }
        }
    }

    // If no direct Drop impl, check if any field needs drop (recursive analysis).
    // Types like Wrapper { res: Resource } where Resource impls Drop need
    // field-level drops even though Wrapper doesn't have its own Drop impl.
    bool needs_field_drops = false;
    if (!has_drop) {
        // First try the type environment (works for imported/library types)
        needs_field_drops = env_.type_needs_drop(type_name);

        // Fallback: check local struct_fields_ for test-local types.
        // env_.type_needs_drop() only knows about types registered in TypeEnv,
        // not locally-defined structs from the current compilation unit.
        if (!needs_field_drops) {
            auto fields_it = struct_fields_.find(type_name);
            if (fields_it != struct_fields_.end()) {
                for (const auto& field : fields_it->second) {
                    // Check for Str fields (llvm_type == "ptr" with Str semantic type)
                    if (field.llvm_type == "ptr" && field.semantic_type &&
                        field.semantic_type->is<types::PrimitiveType>() &&
                        field.semantic_type->as<types::PrimitiveType>().kind ==
                            types::PrimitiveKind::Str) {
                        needs_field_drops = true;
                        break;
                    }
                    // Extract field type name from LLVM type
                    std::string ft_name;
                    if (field.llvm_type.starts_with("%struct.")) {
                        ft_name = field.llvm_type.substr(8);
                    }
                    if (!ft_name.empty()) {
                        // Check if field type implements Drop or recursively needs drop
                        if (env_.type_implements(ft_name, "Drop")) {
                            needs_field_drops = true;
                            break;
                        }
                        // Check generic base type (e.g., Mutex from Mutex__I32)
                        auto sep = ft_name.find("__");
                        if (sep != std::string::npos) {
                            std::string base = ft_name.substr(0, sep);
                            if (env_.type_implements(base, "Drop")) {
                                needs_field_drops = true;
                                break;
                            }
                        }
                        // Recurse: check if field type itself has droppable fields
                        auto sub_fields = struct_fields_.find(ft_name);
                        if (sub_fields != struct_fields_.end()) {
                            // Recursively check (limited depth via struct_fields_ lookup)
                            for (const auto& sf : sub_fields->second) {
                                std::string sft_name;
                                if (sf.llvm_type.starts_with("%struct.")) {
                                    sft_name = sf.llvm_type.substr(8);
                                }
                                if (!sft_name.empty() && (env_.type_implements(sft_name, "Drop") ||
                                                          env_.type_needs_drop(sft_name))) {
                                    needs_field_drops = true;
                                    break;
                                }
                                // Check generic base
                                if (!sft_name.empty()) {
                                    auto sep2 = sft_name.find("__");
                                    if (sep2 != std::string::npos) {
                                        std::string base2 = sft_name.substr(0, sep2);
                                        if (env_.type_implements(base2, "Drop")) {
                                            needs_field_drops = true;
                                            break;
                                        }
                                    }
                                }
                            }
                        }
                    }
                    if (needs_field_drops)
                        break;
                }
            }
        }
    }

    // Detect enum types that need variant-aware drop.
    // Enums aren't in struct_fields_, so needs_field_drops from struct lookup won't work.
    // Check if this type is an enum with droppable variant payloads.
    bool needs_enum_drop = false;
    if (!has_drop) {
        // Check base name (strip mangling for generic enums like IntList or Expr)
        std::string base_type = type_name;
        auto sep_pos = type_name.find("__");
        if (sep_pos != std::string::npos) {
            base_type = type_name.substr(0, sep_pos);
        }
        // Check if it's a registered enum in TypeEnv
        auto enum_def = env_.lookup_enum(base_type);
        if (enum_def.has_value() && env_.type_needs_drop(type_name)) {
            needs_enum_drop = true;
            needs_field_drops = false; // Override: use enum drop, not field drop
        }
        // Also check pending generic enums (for locally-defined generic enums)
        if (!needs_enum_drop && pending_generic_enums_.count(base_type) > 0) {
            if (env_.type_needs_drop(type_name) || env_.type_needs_drop(base_type)) {
                needs_enum_drop = true;
                needs_field_drops = false;
            }
        }
    }

    if (!has_drop && !needs_field_drops && !needs_enum_drop) {
        return;
    }

    TML_DEBUG_LN("[DROP] Registering " << var_name << " for drop, type=" << type_name
                                       << (needs_enum_drop ? " (enum-drop)" : "")
                                       << (needs_field_drops ? " (field-level)" : ""));

    if (!drop_scopes_.empty()) {
        DropInfo di{var_name, var_reg, type_name, llvm_type};
        di.needs_field_drops = needs_field_drops;
        di.needs_enum_drop = needs_enum_drop;
        drop_scopes_.back().push_back(di);

        // For generic imported types, request Drop method instantiation
        // This handles types like MutexGuard__I32 from std::sync
        auto sep_pos = type_name.find("__");
        if (sep_pos != std::string::npos) {
            std::string base_type = type_name.substr(0, sep_pos);

            // Check if Drop impl method already generated
            std::string drop_key = "tml_" + type_name + "_drop";
            TML_DEBUG_LN("[DROP]   base_type=" << base_type << ", drop_key=" << drop_key
                                               << ", already_generated="
                                               << (generated_impl_methods_.find(drop_key) !=
                                                   generated_impl_methods_.end()));
            if (generated_impl_methods_.find(drop_key) == generated_impl_methods_.end()) {
                // Pass empty type_subs — generic.cpp's recovery logic will parse
                // the mangled name and map type params correctly using the impl's
                // actual generic param names (handles both single [T] and multi [K,V]).
                std::unordered_map<std::string, types::TypePtr> empty_subs;
                pending_impl_method_instantiations_.push_back(
                    PendingImplMethod{type_name, "drop", empty_subs, base_type, "",
                                      /*is_library_type=*/true});
                generated_impl_methods_.insert(drop_key);

                // Pre-register in functions_ so emit_drop_call can find it
                // Library types don't use suite prefix
                std::string method_name = type_name + "_drop";
                std::string func_llvm_name = "tml_" + type_name + "_drop";
                functions_[method_name] =
                    FuncInfo{"@" + func_llvm_name, "void (ptr)", "void", {"ptr"}};
            }
        } else {
            // Non-generic type with Drop impl (e.g., Condvar, DroppableResource)
            // Need to ensure the drop function exists and is findable
            std::string method_name = type_name + "_drop";
            if (functions_.find(method_name) == functions_.end()) {
                // Determine if this is a library type or a local (test-defined) type
                // Library types are from imported modules (std::*, core::*)
                // Local types are defined in the current compilation unit
                bool is_library = false;
                if (env_.module_registry()) {
                    const auto& all_modules = env_.module_registry()->get_all_modules();
                    for (const auto& [mod_name, mod] : all_modules) {
                        if (mod_name.starts_with("std::") || mod_name.starts_with("core::")) {
                            if (mod.structs.count(type_name) || mod.classes.count(type_name)) {
                                is_library = true;
                                break;
                            }
                        }
                    }
                }

                std::string prefix = is_library ? "" : get_suite_prefix();
                std::string func_llvm_name = "tml_" + prefix + type_name + "_drop";
                functions_[method_name] =
                    FuncInfo{"@" + func_llvm_name, "void (ptr)", "void", {"ptr"}};
                // Request instantiation
                std::unordered_map<std::string, types::TypePtr> empty_subs;
                pending_impl_method_instantiations_.push_back(
                    PendingImplMethod{type_name, "drop", empty_subs, type_name, "",
                                      /*is_library_type=*/is_library});
                generated_impl_methods_.insert(func_llvm_name);
            }
        }
    }
}

void LLVMIRGen::register_heap_str_for_drop(const std::string& var_name,
                                           const std::string& var_reg) {
    if (!drop_scopes_.empty()) {
        DropInfo di;
        di.var_name = var_name;
        di.var_reg = var_reg;
        di.type_name = "Str";
        di.llvm_type = "ptr";
        di.is_heap_str = true;
        drop_scopes_.back().push_back(di);
    }
}

void LLVMIRGen::emit_drop_call(const DropInfo& info) {
    // Heap-allocated Str: safely free via tml_str_free.
    // tml_str_free validates the pointer is a genuine heap allocation
    // before calling free(), so it's safe for global string constants too.
    if (info.is_heap_str) {
        require_runtime_decl("tml_str_free");
        std::string ptr_val = fresh_reg();
        emit_line("  " + ptr_val + " = load ptr, ptr " + info.var_reg);
        emit_line("  call void @tml_str_free(ptr " + ptr_val + ")");
        return;
    }

    // Enum variant-aware drops: switch on discriminant, drop active variant's fields.
    if (info.needs_enum_drop) {
        emit_enum_variant_drops(info);
        return;
    }

    // Field-level drops: type doesn't implement Drop itself, but contains
    // fields that need dropping. Emit GEP + drop for each droppable field.
    if (info.needs_field_drops) {
        emit_field_level_drops(info);
        return;
    }

    // Load the value from the variable's alloca
    std::string value_reg = fresh_reg();
    emit_line("  " + value_reg + " = load " + info.llvm_type + ", ptr " + info.var_reg);

    // Create a pointer to pass to drop (drop takes mut this)
    // Actually, for drop we pass the pointer directly since it's `mut this`
    // The drop function signature is: void @tml_TypeName_drop(ptr %this)
    // Look up in functions_ to get the correct LLVM name
    std::string drop_lookup_key = info.type_name + "_drop";
    auto drop_it = functions_.find(drop_lookup_key);
    std::string drop_func;
    if (drop_it != functions_.end()) {
        drop_func = drop_it->second.llvm_name;
    } else {
        // Fallback: use suite prefix for test-local types
        drop_func = "@tml_" + get_suite_prefix() + info.type_name + "_drop";
    }
    emit_line("  call void " + drop_func + "(ptr " + info.var_reg + ")");

    // For @pool(thread_local: true) classes, release to thread-local pool
    if (tls_pool_classes_.count(info.type_name) > 0) {
        // Get class type for size calculation
        std::string class_type = "%class." + info.type_name;
        emit_line("  call void @tls_pool_release(ptr @pool.name." + info.type_name + ", ptr " +
                  info.var_reg + ", i64 ptrtoint (" + class_type + "* getelementptr (" +
                  class_type + ", " + class_type + "* null, i32 1) to i64))");
    }
    // For @pool classes (non-thread-local), release to global pool
    else if (pool_classes_.count(info.type_name) > 0) {
        emit_line("  call void @pool_release(ptr @pool." + info.type_name + ", ptr " +
                  info.var_reg + ")");
    }
}

void LLVMIRGen::emit_field_level_drops(const DropInfo& info) {
    // Look up struct fields
    auto fields_it = struct_fields_.find(info.type_name);
    if (fields_it == struct_fields_.end()) {
        TML_DEBUG_LN("[DROP] No field info for " << info.type_name << ", skipping field drops");
        return;
    }

    const auto& fields = fields_it->second;
    std::string struct_type = info.llvm_type;

    // Drop fields in reverse order (last field first, matching Rust semantics)
    for (auto it = fields.rbegin(); it != fields.rend(); ++it) {
        const auto& field = *it;

        // Get the field's TML type name to check if it needs drop.
        // For generic types (Mutex[I32]), use the mangled LLVM type name
        // (%struct.Mutex__I32 -> Mutex__I32) since that's what register_for_drop uses.
        std::string field_type_name;
        bool is_str_field = false;
        if (field.llvm_type.starts_with("%struct.")) {
            field_type_name = field.llvm_type.substr(8); // "%struct.X" -> "X"
        } else if (field.semantic_type) {
            if (field.semantic_type->is<types::NamedType>()) {
                field_type_name = field.semantic_type->as<types::NamedType>().name;
            } else if (field.semantic_type->is<types::ClassType>()) {
                field_type_name = field.semantic_type->as<types::ClassType>().name;
            } else if (field.semantic_type->is<types::PrimitiveType>() &&
                       field.semantic_type->as<types::PrimitiveType>().kind ==
                           types::PrimitiveKind::Str) {
                field_type_name = "Str";
                is_str_field = true;
            }
        }

        if (field_type_name.empty()) {
            continue;
        }

        // Str fields: emit tml_str_free directly (no Drop impl lookup needed)
        if (is_str_field) {
            std::string field_ptr = fresh_reg();
            emit_line("  " + field_ptr + " = getelementptr inbounds " + struct_type + ", ptr " +
                      info.var_reg + ", i32 0, i32 " + std::to_string(field.index));
            require_runtime_decl("tml_str_free");
            std::string str_val = fresh_reg();
            emit_line("  " + str_val + " = load ptr, ptr " + field_ptr);
            emit_line("  call void @tml_str_free(ptr " + str_val + ")");
            continue;
        }

        // Check if this specific field needs drop
        bool field_has_drop = env_.type_implements(field_type_name, "Drop");
        // For generic types (List__Arg), check the base type too
        if (!field_has_drop) {
            auto sep_pos = field_type_name.find("__");
            if (sep_pos != std::string::npos) {
                std::string base_type = field_type_name.substr(0, sep_pos);
                field_has_drop = env_.type_implements(base_type, "Drop");
            }
        }
        bool field_needs_recursive = false;
        if (!field_has_drop) {
            field_needs_recursive = env_.type_needs_drop(field_type_name);
        }

        if (!field_has_drop && !field_needs_recursive) {
            continue;
        }

        TML_DEBUG_LN("[DROP]   Field " << info.type_name << "." << field.name
                                       << " (type=" << field_type_name << ")"
                                       << (field_needs_recursive ? " [recursive]" : ""));

        // GEP to get field pointer
        std::string field_ptr = fresh_reg();
        emit_line("  " + field_ptr + " = getelementptr inbounds " + struct_type + ", ptr " +
                  info.var_reg + ", i32 0, i32 " + std::to_string(field.index));

        if (field_needs_recursive) {
            // Recursively emit field-level drops for nested structs
            DropInfo field_info;
            field_info.var_name = info.var_name + "." + field.name;
            field_info.var_reg = field_ptr;
            field_info.type_name = field_type_name;
            field_info.llvm_type = field.llvm_type;
            field_info.needs_field_drops = true;
            emit_field_level_drops(field_info);
        } else {
            // Field directly implements Drop — call its drop method.
            // Look up in functions_ first; if not found, determine the correct
            // function name based on whether it's a library or local type.
            std::string drop_lookup_key = field_type_name + "_drop";
            auto drop_it = functions_.find(drop_lookup_key);
            std::string drop_func;
            if (drop_it != functions_.end()) {
                drop_func = drop_it->second.llvm_name;
            } else {
                // Check if library type (no suite prefix) or local type (suite prefix)
                bool is_library = false;
                if (env_.module_registry()) {
                    const auto& all_modules = env_.module_registry()->get_all_modules();
                    for (const auto& [mod_name, mod] : all_modules) {
                        if (mod_name.starts_with("std::") || mod_name.starts_with("core::")) {
                            if (mod.structs.count(field_type_name) ||
                                mod.classes.count(field_type_name)) {
                                is_library = true;
                                break;
                            }
                        }
                    }
                }
                // Also check generic base types (e.g., Mutex from Mutex__I32)
                std::string base_type_name;
                auto sep = field_type_name.find("__");
                if (sep != std::string::npos) {
                    base_type_name = field_type_name.substr(0, sep);
                    if (!is_library && env_.module_registry()) {
                        const auto& all_modules = env_.module_registry()->get_all_modules();
                        for (const auto& [mod_name, mod] : all_modules) {
                            if (mod_name.starts_with("std::") || mod_name.starts_with("core::")) {
                                if (mod.structs.count(base_type_name) ||
                                    mod.classes.count(base_type_name)) {
                                    is_library = true;
                                    break;
                                }
                            }
                        }
                    }
                }
                std::string prefix = is_library ? "" : get_suite_prefix();
                std::string func_llvm_name = "tml_" + prefix + field_type_name + "_drop";
                drop_func = "@" + func_llvm_name;

                // Register in functions_ so future lookups find it
                functions_[drop_lookup_key] = FuncInfo{drop_func, "void (ptr)", "void", {"ptr"}};

                // For generic types, queue instantiation so the drop method
                // body is actually generated (prevents UNRESOLVED references)
                if (!base_type_name.empty() &&
                    generated_impl_methods_.find(func_llvm_name) == generated_impl_methods_.end()) {
                    std::unordered_map<std::string, types::TypePtr> empty_subs;
                    pending_impl_method_instantiations_.push_back(
                        PendingImplMethod{field_type_name, "drop", empty_subs, base_type_name, "",
                                          /*is_library_type=*/is_library});
                    generated_impl_methods_.insert(func_llvm_name);
                    TML_DEBUG_LN("[DROP]   Queued generic drop instantiation for field type: "
                                 << field_type_name << " (base=" << base_type_name << ")");
                } else if (base_type_name.empty()) {
                    // Non-generic field type with Drop — also queue instantiation
                    if (generated_impl_methods_.find(func_llvm_name) ==
                        generated_impl_methods_.end()) {
                        std::unordered_map<std::string, types::TypePtr> empty_subs;
                        pending_impl_method_instantiations_.push_back(PendingImplMethod{
                            field_type_name, "drop", empty_subs, field_type_name, "",
                            /*is_library_type=*/is_library});
                        generated_impl_methods_.insert(func_llvm_name);
                    }
                }
            }
            emit_line("  call void " + drop_func + "(ptr " + field_ptr + ")");
        }
    }
}

// Helper: get a mangled type name from a semantic TypePtr for drop purposes.
// E.g., NamedType{"Heap", type_args=[NamedType{"Expr"}]} → "Heap__Expr"
static std::string mangled_type_name_for_drop(const types::TypePtr& type) {
    if (!type)
        return "";
    if (type->is<types::NamedType>()) {
        const auto& named = type->as<types::NamedType>();
        if (named.type_args.empty())
            return named.name;
        std::string result = named.name;
        for (const auto& arg : named.type_args) {
            result += "__" + mangled_type_name_for_drop(arg);
        }
        return result;
    }
    if (type->is<types::PrimitiveType>()) {
        const auto& prim = type->as<types::PrimitiveType>();
        switch (prim.kind) {
        case types::PrimitiveKind::I32:
            return "I32";
        case types::PrimitiveKind::I64:
            return "I64";
        case types::PrimitiveKind::Bool:
            return "Bool";
        case types::PrimitiveKind::Str:
            return "Str";
        case types::PrimitiveKind::F32:
            return "F32";
        case types::PrimitiveKind::F64:
            return "F64";
        case types::PrimitiveKind::I8:
            return "I8";
        case types::PrimitiveKind::I16:
            return "I16";
        case types::PrimitiveKind::U8:
            return "U8";
        case types::PrimitiveKind::U16:
            return "U16";
        case types::PrimitiveKind::U32:
            return "U32";
        case types::PrimitiveKind::U64:
            return "U64";
        case types::PrimitiveKind::I128:
            return "I128";
        case types::PrimitiveKind::U128:
            return "U128";
        default:
            return "";
        }
    }
    if (type->is<types::PtrType>()) {
        return "ptr_" + mangled_type_name_for_drop(type->as<types::PtrType>().inner);
    }
    if (type->is<types::RefType>()) {
        return "ref_" + mangled_type_name_for_drop(type->as<types::RefType>().inner);
    }
    return "";
}

// Helper: get the LLVM type string for a semantic type in drop context.
// Returns "ptr" for Str, pointer types; "i32"/"i64"/etc for primitives;
// "%struct.Name" for named types.
static std::string llvm_type_for_drop(const types::TypePtr& type) {
    if (!type)
        return "ptr";
    if (type->is<types::PrimitiveType>()) {
        const auto& prim = type->as<types::PrimitiveType>();
        switch (prim.kind) {
        case types::PrimitiveKind::I32:
            return "i32";
        case types::PrimitiveKind::I64:
            return "i64";
        case types::PrimitiveKind::Bool:
            return "i1";
        case types::PrimitiveKind::Str:
            return "ptr";
        case types::PrimitiveKind::F32:
            return "float";
        case types::PrimitiveKind::F64:
            return "double";
        case types::PrimitiveKind::I8:
            return "i8";
        case types::PrimitiveKind::I16:
            return "i16";
        case types::PrimitiveKind::U8:
            return "i8";
        case types::PrimitiveKind::U16:
            return "i16";
        case types::PrimitiveKind::U32:
            return "i32";
        case types::PrimitiveKind::U64:
            return "i64";
        case types::PrimitiveKind::I128:
            return "i128";
        case types::PrimitiveKind::U128:
            return "i128";
        default:
            return "ptr";
        }
    }
    if (type->is<types::NamedType>()) {
        return "%struct." + mangled_type_name_for_drop(type);
    }
    if (type->is<types::PtrType>() || type->is<types::RefType>()) {
        return "ptr";
    }
    return "ptr";
}

// Helper: check if a semantic type needs drop.
static bool type_needs_drop_for_variant(const types::TypePtr& type,
                                        const tml::types::TypeEnv& env) {
    if (!type)
        return false;
    if (type->is<types::PrimitiveType>()) {
        auto kind = type->as<types::PrimitiveType>().kind;
        return kind == types::PrimitiveKind::Str; // Str needs tml_str_free
    }
    if (type->is<types::NamedType>()) {
        const auto& named = type->as<types::NamedType>();
        std::string mangled = mangled_type_name_for_drop(type);
        // Check direct Drop impl
        if (env.type_implements(mangled, "Drop"))
            return true;
        if (env.type_implements(named.name, "Drop"))
            return true;
        // Check recursive needs_drop
        if (env.type_needs_drop(mangled))
            return true;
        if (env.type_needs_drop(named.name))
            return true;
    }
    return false;
}

void LLVMIRGen::emit_enum_variant_drops(const DropInfo& info) {
    // Generate (or find) a standalone enum drop function and call it.
    // Using a standalone function avoids infinite recursion for recursive enums
    // like SimpleList { Node(Heap[SimpleList]), End }.
    ensure_enum_drop_function(info.type_name);

    std::string drop_key = info.type_name + "_drop";
    auto drop_it = functions_.find(drop_key);
    std::string drop_func;
    if (drop_it != functions_.end()) {
        drop_func = drop_it->second.llvm_name;
    } else {
        drop_func = "@tml_" + get_suite_prefix() + info.type_name + "_drop";
    }
    emit_line("  call void " + drop_func + "(ptr " + info.var_reg + ")");
}

void LLVMIRGen::ensure_enum_drop_function(const std::string& enum_type_name) {
    std::string prefix = get_suite_prefix();
    std::string func_name = "tml_" + prefix + enum_type_name + "_drop";

    // Already generated or in progress?
    if (generated_enum_drop_functions_.count(func_name)) {
        return;
    }
    // Mark immediately to prevent infinite recursion for self-referential enums
    generated_enum_drop_functions_.insert(func_name);

    // Register in functions_ so callers can find it
    std::string drop_key = enum_type_name + "_drop";
    functions_[drop_key] = FuncInfo{"@" + func_name, "void (ptr)", "void", {"ptr"}};

    // Look up enum definition
    std::string base_type = enum_type_name;
    auto sep_pos = enum_type_name.find("__");
    if (sep_pos != std::string::npos) {
        base_type = enum_type_name.substr(0, sep_pos);
    }
    auto enum_def = env_.lookup_enum(base_type);
    if (!enum_def.has_value()) {
        return;
    }

    // For generic enums (e.g., UnaryTree__I32), build a substitution map
    // to resolve type parameters (T -> I32) in variant payload types
    std::unordered_map<std::string, types::TypePtr> type_subs;
    bool is_generic = (enum_type_name.find("__") != std::string::npos);
    if (is_generic) {
        // Try enum_instantiations_ first
        auto inst_it = enum_instantiations_.find(enum_type_name);
        if (inst_it != enum_instantiations_.end()) {
            // Get the generic param names from the enum decl
            auto gen_enum_it = pending_generic_enums_.find(base_type);
            if (gen_enum_it != pending_generic_enums_.end()) {
                const auto& gen_decl = *gen_enum_it->second;
                for (size_t g = 0;
                     g < gen_decl.generics.size() && g < inst_it->second.type_args.size(); ++g) {
                    type_subs[gen_decl.generics[g].name] = inst_it->second.type_args[g];
                }
            }
        }
    }

    // Helper to substitute type params in a TypePtr
    // e.g., Heap[UnaryTree[T]] with T->I32 becomes Heap[UnaryTree[I32]]
    std::function<types::TypePtr(const types::TypePtr&)> resolve_type;
    resolve_type = [&](const types::TypePtr& ty) -> types::TypePtr {
        if (!ty)
            return ty;
        if (ty->is<types::NamedType>()) {
            const auto& named = ty->as<types::NamedType>();
            // Check if this is a type parameter
            auto sub_it = type_subs.find(named.name);
            if (sub_it != type_subs.end()) {
                return sub_it->second;
            }
            // Recurse into type args
            if (!named.type_args.empty()) {
                auto result = std::make_shared<types::Type>();
                std::vector<types::TypePtr> resolved_args;
                for (const auto& arg : named.type_args) {
                    resolved_args.push_back(resolve_type(arg));
                }
                result->kind =
                    types::NamedType{named.name, named.module_path, std::move(resolved_args)};
                return result;
            }
        }
        return ty;
    };

    // Build the function IR into a local stringstream to avoid conflicting
    // with the current function being generated in output_
    std::stringstream fn;
    auto e = [&fn](const std::string& line) { fn << line << "\n"; };
    // Use a local counter to avoid conflicting with main codegen
    int lc = temp_counter_;
    auto fr = [&lc]() -> std::string { return "%edt" + std::to_string(lc++); };

    e("\ndefine internal void @" + func_name + "(ptr %this) #0 {");
    e("entry:");

    // Build droppable variants list
    struct DroppableVariant {
        int tag;
        std::string variant_name;
        std::vector<std::pair<size_t, types::TypePtr>> droppable_fields;
    };
    std::vector<DroppableVariant> droppable_variants;

    int tag = 0;
    for (const auto& [variant_name, payload_types] : enum_def->variants) {
        DroppableVariant dv;
        dv.tag = tag++;
        dv.variant_name = variant_name;
        for (size_t i = 0; i < payload_types.size(); ++i) {
            // Resolve generic type params before checking droppability
            auto resolved = is_generic ? resolve_type(payload_types[i]) : payload_types[i];
            if (type_needs_drop_for_variant(resolved, env_)) {
                dv.droppable_fields.push_back({i, resolved});
            }
        }
        if (!dv.droppable_fields.empty()) {
            droppable_variants.push_back(std::move(dv));
        }
    }

    if (droppable_variants.empty()) {
        e("  ret void");
        e("}");
        enum_drop_output_ << fn.str();
        temp_counter_ = lc;
        return;
    }

    std::string enum_llvm_type = "%struct." + enum_type_name;

    // Load discriminant
    std::string tag_ptr = fr();
    e("  " + tag_ptr + " = getelementptr inbounds " + enum_llvm_type + ", ptr %this, i32 0, i32 0");
    std::string tag_val = fr();
    e("  " + tag_val + " = load i32, ptr " + tag_ptr);

    // Switch
    std::string end_label = "edf_end" + std::to_string(lc++);
    std::string default_label = "edf_default" + std::to_string(lc++);

    std::string switch_str = "  switch i32 " + tag_val + ", label %" + default_label + " [";
    int switch_counter = lc;
    for (const auto& dv : droppable_variants) {
        std::string case_label =
            "edf_v" + std::to_string(dv.tag) + "_" + std::to_string(switch_counter);
        switch_str += "\n    i32 " + std::to_string(dv.tag) + ", label %" + case_label;
    }
    switch_str += "\n  ]";
    e(switch_str);

    // Emit each variant's drop
    for (const auto& dv : droppable_variants) {
        std::string case_label =
            "edf_v" + std::to_string(dv.tag) + "_" + std::to_string(switch_counter);
        e(case_label + ":");

        std::string payload_ptr = fr();
        e("  " + payload_ptr + " = getelementptr inbounds " + enum_llvm_type +
          ", ptr %this, i32 0, i32 1");

        for (const auto& [field_idx, field_type] : dv.droppable_fields) {
            std::string mangled = mangled_type_name_for_drop(field_type);
            std::string llvm_ty = llvm_type_for_drop(field_type);
            bool is_str = field_type->is<types::PrimitiveType>() &&
                          field_type->as<types::PrimitiveType>().kind == types::PrimitiveKind::Str;

            std::string field_ptr;
            if (dv.droppable_fields.size() == 1 && field_idx == 0 &&
                enum_def->variants[dv.tag].second.size() == 1) {
                field_ptr = payload_ptr;
            } else {
                std::string tuple_type = "{ ";
                for (size_t i = 0; i < enum_def->variants[dv.tag].second.size(); ++i) {
                    if (i > 0)
                        tuple_type += ", ";
                    auto vt = is_generic ? resolve_type(enum_def->variants[dv.tag].second[i])
                                         : enum_def->variants[dv.tag].second[i];
                    tuple_type += llvm_type_for_drop(vt);
                }
                tuple_type += " }";
                field_ptr = fr();
                e("  " + field_ptr + " = getelementptr inbounds " + tuple_type + ", ptr " +
                  payload_ptr + ", i32 0, i32 " + std::to_string(field_idx));
            }

            if (is_str) {
                require_runtime_decl("tml_str_free");
                std::string str_val = fr();
                e("  " + str_val + " = load ptr, ptr " + field_ptr);
                e("  call void @tml_str_free(ptr " + str_val + ")");
            } else {
                bool has_drop_impl =
                    env_.type_implements(mangled, "Drop") ||
                    (!mangled.empty() && mangled.find("__") != std::string::npos &&
                     env_.type_implements(mangled.substr(0, mangled.find("__")), "Drop"));

                if (has_drop_impl) {
                    // For Heap[T] where T needs drop: drop inner before free
                    if (mangled.starts_with("Heap__")) {
                        std::string inner = mangled.substr(6);
                        bool inner_is_this_enum = (inner == enum_type_name);
                        bool inner_needs = env_.type_needs_drop(inner);
                        if (!inner_needs) {
                            auto ie = env_.lookup_enum(inner);
                            if (ie.has_value())
                                inner_needs = true;
                        }
                        if (inner_is_this_enum || inner_needs) {
                            // Load Heap.ptr (field 0)
                            std::string hp = fr();
                            e("  " + hp + " = getelementptr inbounds %struct." + mangled +
                              ", ptr " + field_ptr + ", i32 0, i32 0");
                            std::string rp = fr();
                            e("  " + rp + " = load ptr, ptr " + hp);
                            std::string nn = fr();
                            e("  " + nn + " = icmp ne ptr " + rp + ", null");
                            std::string idb = "edf_hi" + std::to_string(lc);
                            std::string aib = "edf_hd" + std::to_string(lc++);
                            e("  br i1 " + nn + ", label %" + idb + ", label %" + aib);
                            e(idb + ":");
                            // Recursive call to THIS function for self-ref enums
                            if (inner_is_this_enum) {
                                e("  call void @" + func_name + "(ptr " + rp + ")");
                            } else {
                                // Different inner enum: ensure its drop exists
                                ensure_enum_drop_function(inner);
                                auto inner_dk = inner + "_drop";
                                auto inner_it = functions_.find(inner_dk);
                                std::string inner_fn;
                                if (inner_it != functions_.end()) {
                                    inner_fn = inner_it->second.llvm_name;
                                } else {
                                    inner_fn = "@tml_" + prefix + inner + "_drop";
                                }
                                e("  call void " + inner_fn + "(ptr " + rp + ")");
                            }
                            e("  br label %" + aib);
                            e(aib + ":");
                        }
                    }

                    // Now call the Heap/type drop (e.g., mem_free)
                    std::string dfk = mangled + "_drop";
                    auto dit = functions_.find(dfk);
                    std::string dfn;
                    if (dit != functions_.end()) {
                        dfn = dit->second.llvm_name;
                    } else {
                        dfn = "@tml_" + mangled + "_drop";
                        functions_[dfk] = FuncInfo{dfn, "void (ptr)", "void", {"ptr"}};
                    }

                    std::string flln = "tml_" + mangled + "_drop";
                    if (generated_impl_methods_.find(flln) == generated_impl_methods_.end()) {
                        std::string bn = mangled;
                        auto sp = mangled.find("__");
                        if (sp != std::string::npos)
                            bn = mangled.substr(0, sp);
                        std::unordered_map<std::string, types::TypePtr> es;
                        pending_impl_method_instantiations_.push_back(
                            PendingImplMethod{mangled, "drop", es, bn, "",
                                              /*is_library_type=*/true});
                        generated_impl_methods_.insert(flln);
                    }

                    e("  call void " + dfn + "(ptr " + field_ptr + ")");
                } else {
                    // For non-Drop inner types, we need field-level drops.
                    // These need to go through the main output_ since emit_field_level_drops
                    // uses emit_line. Skip here — the Heap::drop (mem_free) handles cleanup.
                    // TODO: Support non-Drop field-level drops in enum drop functions
                }
            }
        }

        e("  br label %" + end_label);
    }

    e(default_label + ":");
    e("  br label %" + end_label);
    e(end_label + ":");
    e("  ret void");
    e("}");

    // Sync counters back
    temp_counter_ = lc;

    // Append to deferred output
    enum_drop_output_ << fn.str();
}

void LLVMIRGen::emit_scope_drops() {
    if (drop_scopes_.empty()) {
        return;
    }

    // Emit drops in reverse order (LIFO - last declared is dropped first)
    // Skip variables that have been consumed (moved into struct fields, etc.)
    // Also skip variables that have partial moves (some fields consumed)
    const auto& current_scope = drop_scopes_.back();
    for (auto it = current_scope.rbegin(); it != current_scope.rend(); ++it) {
        // Skip if the whole variable was consumed
        if (consumed_vars_.find(it->var_name) != consumed_vars_.end()) {
            continue;
        }
        // Skip if any field of this variable was consumed (partial move)
        // TODO: In the future, we could emit drop calls for individual non-moved fields
        if (has_consumed_fields(it->var_name)) {
            TML_DEBUG_LN("[DROP] Skipping drop for " << it->var_name << " due to partial move");
            continue;
        }
        emit_drop_call(*it);
    }
}

void LLVMIRGen::emit_all_drops() {
    // Emit drops for all scopes in reverse order (innermost to outermost)
    // Skip variables that have been consumed (moved into struct fields, etc.)
    for (auto scope_it = drop_scopes_.rbegin(); scope_it != drop_scopes_.rend(); ++scope_it) {
        // Within each scope, drop in reverse declaration order
        for (auto it = scope_it->rbegin(); it != scope_it->rend(); ++it) {
            // Skip if the whole variable was consumed
            if (consumed_vars_.find(it->var_name) != consumed_vars_.end()) {
                continue;
            }
            // Skip if any field of this variable was consumed (partial move)
            if (has_consumed_fields(it->var_name)) {
                TML_DEBUG_LN("[DROP] Skipping drop for "
                             << it->var_name << " in emit_all_drops due to partial move");
                continue;
            }
            emit_drop_call(*it);
        }
    }
}

std::string LLVMIRGen::register_temp_for_drop(const std::string& value,
                                              const std::string& type_name,
                                              const std::string& llvm_type,
                                              const std::string& existing_alloca) {
    // Use existing alloca if provided (from method dispatch spill), otherwise create new
    std::string temp_alloca;
    if (!existing_alloca.empty()) {
        temp_alloca = existing_alloca;
    } else {
        temp_alloca = fresh_reg();
        emit_line("  " + temp_alloca + " = alloca " + llvm_type);
        emit_line("  store " + llvm_type + " " + value + ", ptr " + temp_alloca);
    }

    DropInfo di;
    di.var_name = "__temp_" + std::to_string(temp_counter_);
    di.var_reg = temp_alloca;
    di.type_name = type_name;
    di.llvm_type = llvm_type;

    // Check if it needs field-level drops (same logic as register_for_drop)
    bool has_drop = env_.type_implements(type_name, "Drop");
    if (!has_drop) {
        auto sep_pos = type_name.find("__");
        if (sep_pos != std::string::npos) {
            std::string base_type = type_name.substr(0, sep_pos);
            has_drop = env_.type_implements(base_type, "Drop");
        }
    }

    if (!has_drop) {
        di.needs_field_drops = true;
    }

    temp_drops_.push_back(di);
    TML_DEBUG_LN("[DROP] Registered temp " << di.var_name << " for drop, type=" << type_name);

    // Also need to ensure drop function exists (same as register_for_drop)
    if (has_drop) {
        auto sep_pos = type_name.find("__");
        if (sep_pos != std::string::npos) {
            std::string base_type = type_name.substr(0, sep_pos);
            std::string drop_key = "tml_" + type_name + "_drop";
            if (generated_impl_methods_.find(drop_key) == generated_impl_methods_.end()) {
                // Pass empty type_subs — generic.cpp's recovery logic handles mapping
                std::unordered_map<std::string, types::TypePtr> empty_subs;
                pending_impl_method_instantiations_.push_back(
                    PendingImplMethod{type_name, "drop", empty_subs, base_type, "",
                                      /*is_library_type=*/true});
                generated_impl_methods_.insert(drop_key);
                std::string func_llvm_name = "tml_" + type_name + "_drop";
                functions_[type_name + "_drop"] =
                    FuncInfo{"@" + func_llvm_name, "void (ptr)", "void", {"ptr"}};
            }
        } else {
            std::string method_name = type_name + "_drop";
            if (functions_.find(method_name) == functions_.end()) {
                bool is_library = false;
                if (env_.module_registry()) {
                    const auto& all_modules = env_.module_registry()->get_all_modules();
                    for (const auto& [mod_name, mod] : all_modules) {
                        if (mod_name.starts_with("std::") || mod_name.starts_with("core::")) {
                            if (mod.structs.count(type_name) || mod.classes.count(type_name)) {
                                is_library = true;
                                break;
                            }
                        }
                    }
                }
                std::string prefix = is_library ? "" : get_suite_prefix();
                std::string func_llvm_name = "tml_" + prefix + type_name + "_drop";
                functions_[method_name] =
                    FuncInfo{"@" + func_llvm_name, "void (ptr)", "void", {"ptr"}};
                std::unordered_map<std::string, types::TypePtr> empty_subs;
                pending_impl_method_instantiations_.push_back(
                    PendingImplMethod{type_name, "drop", empty_subs, type_name, "",
                                      /*is_library_type=*/is_library});
                generated_impl_methods_.insert(func_llvm_name);
            }
        }
    }

    return temp_alloca;
}

void LLVMIRGen::emit_temp_drops() {
    if (temp_drops_.empty()) {
        return;
    }

    // Drop in reverse order (LIFO)
    for (auto it = temp_drops_.rbegin(); it != temp_drops_.rend(); ++it) {
        emit_drop_call(*it);
    }
    temp_drops_.clear();
}

void LLVMIRGen::flush_str_temps() {
    if (pending_str_temps_.empty()) {
        return;
    }
    // Don't emit frees after a terminator (ret/br/unreachable) — the block is already
    // terminated and emitting instructions would create invalid LLVM IR. The temps will
    // be cleaned up by the enclosing scope (if/when branch handler or function exit).
    if (block_terminated_) {
        return;
    }
    // Ensure tml_str_free is declared in the final IR
    require_runtime_decl("tml_str_free");
    // Free in reverse order (LIFO)
    for (auto it = pending_str_temps_.rbegin(); it != pending_str_temps_.rend(); ++it) {
        emit_line("  call void @tml_str_free(ptr " + *it + ")");
    }
    pending_str_temps_.clear();
}

void LLVMIRGen::consume_last_str_temp() {
    if (!pending_str_temps_.empty()) {
        pending_str_temps_.pop_back();
    }
}

void LLVMIRGen::consume_str_temp_if_arg(const std::string& reg) {
    // When a Str temp is passed as an argument to a function/method call,
    // the callee may take ownership (e.g., list.push(substring(...))).
    // Remove it from pending_str_temps_ to prevent use-after-free.
    for (auto it = pending_str_temps_.begin(); it != pending_str_temps_.end(); ++it) {
        if (*it == reg) {
            pending_str_temps_.erase(it);
            return;
        }
    }
}

} // namespace tml::codegen
