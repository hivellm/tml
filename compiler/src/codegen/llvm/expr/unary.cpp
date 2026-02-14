//! # LLVM IR Generator - Unary Expressions
//!
//! This file implements unary operator code generation.
//!
//! ## Operators
//!
//! | Operator | TML Syntax  | LLVM Instruction       |
//! |----------|-------------|------------------------|
//! | Negate   | `-x`        | `sub 0, x` or `fneg`   |
//! | Not      | `not x`     | `xor x, 1` (bool)      |
//! | BitNot   | `~x`        | `xor x, -1`            |
//! | Ref      | `ref x`     | Return alloca ptr      |
//! | RefMut   | `mut ref x` | Return alloca ptr      |
//! | Deref    | `*ptr`      | `load` from ptr        |
//!
//! ## Reference Operations
//!
//! `ref` and `mut ref` return the address of a variable without loading:
//! - For identifiers: return the alloca register directly
//! - For field access: return GEP to field
//!
//! ## Dereference
//!
//! `*ptr` emits a `load` instruction from the pointer.

#include "codegen/llvm/llvm_ir_gen.hpp"

namespace tml::codegen {

// Static helper to parse mangled type strings back to semantic types
static types::TypePtr parse_mangled_type_string(const std::string& s) {
    // Primitives
    if (s == "I64")
        return types::make_i64();
    if (s == "I32")
        return types::make_i32();
    if (s == "I8") {
        auto t = std::make_shared<types::Type>();
        t->kind = types::PrimitiveType{types::PrimitiveKind::I8};
        return t;
    }
    if (s == "I16") {
        auto t = std::make_shared<types::Type>();
        t->kind = types::PrimitiveType{types::PrimitiveKind::I16};
        return t;
    }
    if (s == "U8") {
        auto t = std::make_shared<types::Type>();
        t->kind = types::PrimitiveType{types::PrimitiveKind::U8};
        return t;
    }
    if (s == "U16") {
        auto t = std::make_shared<types::Type>();
        t->kind = types::PrimitiveType{types::PrimitiveKind::U16};
        return t;
    }
    if (s == "U32") {
        auto t = std::make_shared<types::Type>();
        t->kind = types::PrimitiveType{types::PrimitiveKind::U32};
        return t;
    }
    if (s == "U64") {
        auto t = std::make_shared<types::Type>();
        t->kind = types::PrimitiveType{types::PrimitiveKind::U64};
        return t;
    }
    if (s == "F32") {
        auto t = std::make_shared<types::Type>();
        t->kind = types::PrimitiveType{types::PrimitiveKind::F32};
        return t;
    }
    if (s == "F64")
        return types::make_f64();
    if (s == "Bool")
        return types::make_bool();
    if (s == "Str")
        return types::make_str();

    // Check for pointer prefix
    if (s.size() > 4 && s.substr(0, 4) == "ptr_") {
        std::string inner_str = s.substr(4);
        auto inner = parse_mangled_type_string(inner_str);
        if (inner) {
            auto t = std::make_shared<types::Type>();
            t->kind = types::PtrType{.is_mut = false, .inner = inner};
            return t;
        }
    }
    if (s.size() > 7 && s.substr(0, 7) == "mutptr_") {
        std::string inner_str = s.substr(7);
        auto inner = parse_mangled_type_string(inner_str);
        if (inner) {
            auto t = std::make_shared<types::Type>();
            t->kind = types::PtrType{.is_mut = true, .inner = inner};
            return t;
        }
    }

    // Check for nested generic (e.g., Mutex__I32)
    auto delim = s.find("__");
    if (delim != std::string::npos) {
        std::string base = s.substr(0, delim);
        std::string arg_str = s.substr(delim + 2);
        auto inner = parse_mangled_type_string(arg_str);
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

auto LLVMIRGen::gen_unary(const parser::UnaryExpr& unary) -> std::string {
    // Handle ref operations specially - we need the address, not the value
    if (unary.op == parser::UnaryOp::Ref || unary.op == parser::UnaryOp::RefMut) {
        // Handle ref *ptr - taking reference of dereferenced pointer returns the pointer
        if (unary.operand->is<parser::UnaryExpr>()) {
            const auto& inner_unary = unary.operand->as<parser::UnaryExpr>();
            if (inner_unary.op == parser::UnaryOp::Deref) {
                // ref *ptr == ptr
                std::string ptr = gen_expr(*inner_unary.operand);
                last_expr_type_ = "ptr";
                return ptr;
            }
        }

        // Get pointer to the operand
        if (unary.operand->is<parser::IdentExpr>()) {
            const auto& ident = unary.operand->as<parser::IdentExpr>();
            auto it = locals_.find(ident.name);
            if (it != locals_.end()) {
                // Return the alloca pointer directly (don't load)
                last_expr_type_ = "ptr";
                return it->second.reg;
            }
        }
        // Handle ref <literal> - allocate temp and return pointer
        if (unary.operand->is<parser::LiteralExpr>()) {
            std::string val = gen_expr(*unary.operand);
            std::string val_type = last_expr_type_;
            std::string tmp_alloca = fresh_reg();
            emit_line("  " + tmp_alloca + " = alloca " + val_type);
            emit_line("  store " + val_type + " " + val + ", ptr " + tmp_alloca);
            last_expr_type_ = "ptr";
            return tmp_alloca;
        }
        // Handle field access: ref this.field, ref x.field
        if (unary.operand->is<parser::FieldExpr>()) {
            const auto& field_expr = unary.operand->as<parser::FieldExpr>();

            // Get the struct pointer
            std::string struct_ptr;
            types::TypePtr base_type = infer_expr_type(*field_expr.object);

            if (field_expr.object->is<parser::IdentExpr>()) {
                const auto& ident = field_expr.object->as<parser::IdentExpr>();
                auto local_it = locals_.find(ident.name);
                if (local_it != locals_.end()) {
                    struct_ptr = local_it->second.reg;
                }
            } else if (field_expr.object->is<parser::UnaryExpr>()) {
                // Handle (*ptr).field - dereferenced pointer field access
                const auto& inner_unary = field_expr.object->as<parser::UnaryExpr>();
                if (inner_unary.op == parser::UnaryOp::Deref) {
                    // The struct pointer is the result of the dereference operand
                    struct_ptr = gen_expr(*inner_unary.operand);
                    // Infer the pointee type
                    types::TypePtr ptr_type = infer_expr_type(*inner_unary.operand);
                    if (ptr_type) {
                        if (ptr_type->is<types::PtrType>()) {
                            base_type = ptr_type->as<types::PtrType>().inner;
                        } else if (ptr_type->is<types::RefType>()) {
                            base_type = ptr_type->as<types::RefType>().inner;
                        } else if (ptr_type->is<types::NamedType>()) {
                            // Handle TML's Ptr[T] or RawPtr[T] wrapper types
                            const auto& named = ptr_type->as<types::NamedType>();
                            if ((named.name == "Ptr" || named.name == "RawPtr") &&
                                !named.type_args.empty()) {
                                base_type = named.type_args[0];
                            }
                        }
                        // Apply type substitutions for generic types
                        if (base_type && !current_type_subs_.empty()) {
                            base_type = apply_type_substitutions(base_type, current_type_subs_);
                        }
                    }
                }
            } else if (field_expr.object->is<parser::FieldExpr>()) {
                // Handle nested field access: ref this.inner.field
                // We need to generate GEPs for each level of field access
                const auto& nested_field = field_expr.object->as<parser::FieldExpr>();

                // Get the outermost struct
                if (nested_field.object->is<parser::IdentExpr>()) {
                    const auto& ident = nested_field.object->as<parser::IdentExpr>();
                    auto local_it = locals_.find(ident.name);
                    if (local_it != locals_.end()) {
                        std::string outer_ptr = local_it->second.reg;
                        std::string outer_type = local_it->second.type;

                        // Special handling for 'this' in impl methods
                        if (ident.name == "this" && !current_impl_type_.empty()) {
                            outer_type = "%struct." + current_impl_type_;
                        }

                        // Get outer struct type name
                        std::string outer_name = outer_type;
                        if (outer_name.starts_with("%struct.")) {
                            outer_name = outer_name.substr(8);
                        }

                        // Get field index and type for the intermediate field
                        int nested_idx = get_field_index(outer_name, nested_field.field);
                        std::string nested_field_type =
                            get_field_type(outer_name, nested_field.field);

                        if (nested_idx >= 0) {
                            // Generate GEP to get pointer to the nested struct field
                            std::string nested_ptr = fresh_reg();
                            emit_line("  " + nested_ptr + " = getelementptr " + outer_type +
                                      ", ptr " + outer_ptr + ", i32 0, i32 " +
                                      std::to_string(nested_idx));
                            struct_ptr = nested_ptr;

                            // If the nested field is a reference/pointer type, we need to load it
                            // to get the actual struct pointer
                            if (nested_field_type == "ptr") {
                                std::string loaded_ptr = fresh_reg();
                                emit_line("  " + loaded_ptr + " = load ptr, ptr " + struct_ptr);
                                struct_ptr = loaded_ptr;
                            }

                            // Get the semantic type of the nested field from struct definition
                            // For generic instantiations like MutexGuard__I32, extract base name
                            // MutexGuard
                            std::string base_struct_name = outer_name;
                            std::vector<types::TypePtr> outer_type_args;
                            auto sep_pos = outer_name.find("__");
                            if (sep_pos != std::string::npos) {
                                base_struct_name = outer_name.substr(0, sep_pos);
                                // Parse type args from mangled suffix (e.g., "BarrierState" from
                                // "MutexGuard__BarrierState")
                                std::string args_str = outer_name.substr(sep_pos + 2);
                                size_t pos = 0;
                                while (pos < args_str.size()) {
                                    auto next_sep = args_str.find("__", pos);
                                    std::string arg = (next_sep == std::string::npos)
                                                          ? args_str.substr(pos)
                                                          : args_str.substr(pos, next_sep - pos);
                                    types::TypePtr arg_type;
                                    if (arg == "I32")
                                        arg_type = types::make_i32();
                                    else if (arg == "I64")
                                        arg_type = types::make_i64();
                                    else if (arg == "U32")
                                        arg_type = types::make_primitive(types::PrimitiveKind::U32);
                                    else if (arg == "U64")
                                        arg_type = types::make_primitive(types::PrimitiveKind::U64);
                                    else if (arg == "Bool")
                                        arg_type = types::make_bool();
                                    else if (arg == "Str")
                                        arg_type = types::make_str();
                                    else if (arg == "F32")
                                        arg_type = types::make_primitive(types::PrimitiveKind::F32);
                                    else if (arg == "F64")
                                        arg_type = types::make_primitive(types::PrimitiveKind::F64);
                                    else {
                                        // Use parse_mangled_type_string for proper handling
                                        arg_type = parse_mangled_type_string(arg);
                                    }
                                    outer_type_args.push_back(arg_type);
                                    if (next_sep == std::string::npos)
                                        break;
                                    pos = next_sep + 2;
                                }
                            }

                            auto struct_def = env_.lookup_struct(base_struct_name);
                            if (struct_def &&
                                nested_idx < static_cast<int>(struct_def->fields.size())) {
                                base_type = struct_def->fields[nested_idx].type;

                                // Apply type substitution if we have type args from the outer
                                // struct This substitutes T -> BarrierState in Mutex[T] to get
                                // Mutex[BarrierState]
                                if (!outer_type_args.empty() && !struct_def->type_params.empty()) {
                                    std::unordered_map<std::string, types::TypePtr> substitutions;
                                    for (size_t i = 0; i < struct_def->type_params.size() &&
                                                       i < outer_type_args.size();
                                         ++i) {
                                        substitutions[struct_def->type_params[i]] =
                                            outer_type_args[i];
                                    }
                                    base_type = types::substitute_type(base_type, substitutions);
                                }

                                // If it's a reference type, unwrap to get the pointee type
                                if (base_type && base_type->is<types::RefType>()) {
                                    base_type = base_type->as<types::RefType>().inner;
                                }
                            } else {
                                // Fallback to infer_expr_type
                                base_type = infer_expr_type(*field_expr.object);
                            }
                        }
                    }
                }
            }

            // For generic structs, try to get base_type from pending_generic_structs_ if not
            // resolved
            if (!base_type && !current_impl_type_.empty() &&
                field_expr.object->is<parser::IdentExpr>()) {
                const auto& ident = field_expr.object->as<parser::IdentExpr>();
                if (ident.name == "this") {
                    // Parse impl type to get struct name and type args
                    std::string base_name = current_impl_type_;
                    std::vector<types::TypePtr> type_args_for_struct;

                    auto sep_pos = current_impl_type_.find("__");
                    if (sep_pos != std::string::npos) {
                        base_name = current_impl_type_.substr(0, sep_pos);
                        // Parse type args from mangled suffix
                        std::string args_str = current_impl_type_.substr(sep_pos + 2);
                        size_t pos = 0;
                        while (pos < args_str.size()) {
                            auto next_sep = args_str.find("__", pos);
                            std::string arg = (next_sep == std::string::npos)
                                                  ? args_str.substr(pos)
                                                  : args_str.substr(pos, next_sep - pos);
                            types::TypePtr arg_type;
                            if (arg == "I32")
                                arg_type = types::make_i32();
                            else if (arg == "I64")
                                arg_type = types::make_i64();
                            else if (arg == "U32")
                                arg_type = types::make_primitive(types::PrimitiveKind::U32);
                            else if (arg == "U64")
                                arg_type = types::make_primitive(types::PrimitiveKind::U64);
                            else if (arg == "Bool")
                                arg_type = types::make_bool();
                            else if (arg == "Str")
                                arg_type = types::make_str();
                            else {
                                // Use parse_mangled_type_string for proper handling
                                arg_type = parse_mangled_type_string(arg);
                            }
                            type_args_for_struct.push_back(arg_type);
                            if (next_sep == std::string::npos)
                                break;
                            pos = next_sep + 2;
                        }
                    }

                    // Create a named type for the struct
                    auto named_type = std::make_shared<types::Type>();
                    named_type->kind = types::NamedType{base_name, "", type_args_for_struct};
                    base_type = named_type;
                }
            }

            // Try to unwrap reference types
            if (base_type && base_type->is<types::RefType>()) {
                auto inner = base_type->as<types::RefType>().inner;
                if (inner) {
                    base_type = inner;
                }
            }

            if (!struct_ptr.empty() && base_type) {
                // Get the field index
                std::string type_name;
                std::vector<types::TypePtr> type_args;
                if (base_type->is<types::NamedType>()) {
                    type_name = base_type->as<types::NamedType>().name;
                    type_args = base_type->as<types::NamedType>().type_args;
                } else if (base_type->is<types::ClassType>()) {
                    type_name = base_type->as<types::ClassType>().name;
                    type_args = base_type->as<types::ClassType>().type_args;
                } else if (base_type->is<types::RefType>()) {
                    auto inner = base_type->as<types::RefType>().inner;
                    if (inner && inner->is<types::NamedType>()) {
                        type_name = inner->as<types::NamedType>().name;
                        type_args = inner->as<types::NamedType>().type_args;
                    } else if (inner && inner->is<types::ClassType>()) {
                        type_name = inner->as<types::ClassType>().name;
                        type_args = inner->as<types::ClassType>().type_args;
                    }
                }

                if (!type_name.empty()) {
                    int field_idx = -1;

                    // For generic instantiations like Mutex__I32, extract base name Mutex
                    std::string lookup_name = type_name;
                    auto sep_pos = type_name.find("__");
                    if (sep_pos != std::string::npos) {
                        lookup_name = type_name.substr(0, sep_pos);
                    }

                    // If we have type_args, ensure struct is instantiated first
                    // This registers fields in struct_fields_ so we can look them up
                    // Use the return value which handles UNRESOLVED cases properly
                    std::string struct_type_name_for_lookup = lookup_name;
                    if (!type_args.empty()) {
                        struct_type_name_for_lookup =
                            require_struct_instantiation(type_name, type_args);
                    }

                    // First check struct_fields_ directly for instantiated generic structs
                    auto sf_it = struct_fields_.find(struct_type_name_for_lookup);
                    if (sf_it != struct_fields_.end()) {
                        for (const auto& field : sf_it->second) {
                            if (field.name == field_expr.field) {
                                field_idx = field.index;
                                break;
                            }
                        }
                    }

                    // If not found, try struct lookup
                    if (field_idx < 0) {
                        auto struct_def = env_.lookup_struct(lookup_name);
                        if (struct_def) {
                            for (size_t i = 0; i < struct_def->fields.size(); ++i) {
                                if (struct_def->fields[i].name == field_expr.field) {
                                    field_idx = static_cast<int>(i);
                                    break;
                                }
                            }
                        }
                    }

                    // Try class lookup if struct lookup failed
                    if (field_idx < 0) {
                        auto class_def = env_.lookup_class(lookup_name);
                        if (class_def) {
                            for (size_t i = 0; i < class_def->fields.size(); ++i) {
                                if (class_def->fields[i].name == field_expr.field) {
                                    field_idx = static_cast<int>(i);
                                    break;
                                }
                            }
                        }
                    }

                    // Try pending_generic_structs_ for generic structs
                    if (field_idx < 0) {
                        auto generic_it = pending_generic_structs_.find(lookup_name);
                        if (generic_it != pending_generic_structs_.end()) {
                            const parser::StructDecl* decl = generic_it->second;
                            for (size_t i = 0; i < decl->fields.size(); ++i) {
                                if (decl->fields[i].name == field_expr.field) {
                                    field_idx = static_cast<int>(i);
                                    break;
                                }
                            }
                        }
                    }

                    // Try module registry for imported structs
                    if (field_idx < 0 && env_.module_registry()) {
                        const auto& all_modules = env_.module_registry()->get_all_modules();
                        for (const auto& [mod_name, mod] : all_modules) {
                            auto struct_it = mod.structs.find(lookup_name);
                            if (struct_it != mod.structs.end()) {
                                const auto& imported_struct = struct_it->second;
                                int idx = 0;
                                for (const auto& fld : imported_struct.fields) {
                                    if (fld.name == field_expr.field) {
                                        field_idx = idx;
                                        break;
                                    }
                                    idx++;
                                }
                                if (field_idx >= 0)
                                    break;
                            }
                        }
                    }

                    if (field_idx >= 0) {
                        std::string llvm_struct_type;
                        bool is_class = env_.lookup_class(lookup_name).has_value();
                        if (!type_args.empty()) {
                            // Use the result from require_struct_instantiation which was called
                            // above struct_type_name_for_lookup should already be set correctly
                            llvm_struct_type = "%struct." + struct_type_name_for_lookup;
                        } else if (is_class) {
                            // Classes use %class. prefix in LLVM type names
                            // Note: class fields are offset by 1 due to vtable pointer
                            llvm_struct_type = "%class." + type_name;
                            field_idx += 1; // Skip vtable pointer
                        } else {
                            llvm_struct_type = llvm_type_name(type_name);
                        }

                        // For classes, the local variable stores a pointer to the instance
                        // We need to load that pointer first
                        std::string actual_struct_ptr = struct_ptr;
                        if (is_class) {
                            std::string loaded_ptr = fresh_reg();
                            emit_line("  " + loaded_ptr + " = load ptr, ptr " + struct_ptr);
                            actual_struct_ptr = loaded_ptr;
                        }

                        std::string field_ptr = fresh_reg();
                        emit_line("  " + field_ptr + " = getelementptr " + llvm_struct_type +
                                  ", ptr " + actual_struct_ptr + ", i32 0, i32 " +
                                  std::to_string(field_idx));
                        last_expr_type_ = "ptr";
                        return field_ptr;
                    }
                }
            }
        }
        report_error("Can only take reference of variables", unary.span, "C003");
        last_expr_type_ = "ptr";
        return "null";
    }

    // Handle deref - load from pointer
    if (unary.op == parser::UnaryOp::Deref) {
        // Infer the inner type from the operand's type
        types::TypePtr operand_type = infer_expr_type(*unary.operand);
        std::string inner_llvm_type = "i32"; // default

        // Check for smart pointer types that implement Deref (like MutexGuard)
        // These need special handling - we need to access the inner data through fields
        if (operand_type && operand_type->is<types::NamedType>()) {
            const auto& named = operand_type->as<types::NamedType>();
            TML_DEBUG_LN("[DEREF] operand is NamedType: " << named.name);

            // Handle MutexGuard[T] - deref returns ref T via mutex.data
            if (named.name == "MutexGuard" && !named.type_args.empty()) {
                TML_DEBUG_LN("[DEREF] MutexGuard detected, type_arg="
                             << types::type_to_string(named.type_args[0]));

                // Get pointer to the MutexGuard (not the value)
                std::string guard_ptr;
                if (unary.operand->is<parser::IdentExpr>()) {
                    const auto& ident = unary.operand->as<parser::IdentExpr>();
                    auto it = locals_.find(ident.name);
                    if (it != locals_.end()) {
                        guard_ptr = it->second.reg;
                    }
                } else {
                    // For non-identifier operands (like *guard where guard is a ref),
                    // evaluate the expression and store to a temp alloca
                    std::string guard_value = gen_expr(*unary.operand);
                    types::TypePtr concrete_inner = named.type_args[0];
                    if (!current_type_subs_.empty()) {
                        concrete_inner =
                            apply_type_substitutions(concrete_inner, current_type_subs_);
                    }
                    // Use require_struct_instantiation to handle UNRESOLVED cases
                    std::string guard_mangled_temp = require_struct_instantiation(
                        "MutexGuard", std::vector<types::TypePtr>{concrete_inner});
                    std::string guard_type_temp = "%struct." + guard_mangled_temp;

                    guard_ptr = fresh_reg();
                    emit_line("  " + guard_ptr + " = alloca " + guard_type_temp);
                    emit_line("  store " + guard_type_temp + " " + guard_value + ", ptr " +
                              guard_ptr);
                    TML_DEBUG_LN("[DEREF] Created temp alloca for MutexGuard: " << guard_ptr);
                }

                if (!guard_ptr.empty()) {
                    // MutexGuard layout: { mutex: mut ref Mutex[T] }
                    // Mutex layout: { data: T, handle: ptr, is_locked: i1 }
                    // To get the data: guard.mutex.data

                    // Apply type substitutions to get concrete type args
                    types::TypePtr concrete_inner = named.type_args[0];
                    if (!current_type_subs_.empty()) {
                        concrete_inner =
                            apply_type_substitutions(concrete_inner, current_type_subs_);
                    }

                    // Get the mangled MutexGuard and Mutex type names
                    // Use require_struct_instantiation to handle UNRESOLVED cases
                    std::string guard_mangled = require_struct_instantiation(
                        "MutexGuard", std::vector<types::TypePtr>{concrete_inner});
                    std::string mutex_mangled = require_struct_instantiation(
                        "Mutex", std::vector<types::TypePtr>{concrete_inner});
                    std::string guard_type = "%struct." + guard_mangled;
                    std::string mutex_type = "%struct." + mutex_mangled;

                    // GEP to get mutex field (field 0) of MutexGuard
                    std::string mutex_field_ptr = fresh_reg();
                    emit_line("  " + mutex_field_ptr + " = getelementptr " + guard_type + ", ptr " +
                              guard_ptr + ", i32 0, i32 0");

                    // Load the mutex pointer (since mutex is a mut ref, it's stored as a ptr)
                    std::string mutex_ptr = fresh_reg();
                    emit_line("  " + mutex_ptr + " = load ptr, ptr " + mutex_field_ptr);

                    // GEP to get data field (field 0) of Mutex
                    std::string data_ptr = fresh_reg();
                    emit_line("  " + data_ptr + " = getelementptr " + mutex_type + ", ptr " +
                              mutex_ptr + ", i32 0, i32 0");

                    // Load the data value
                    inner_llvm_type = llvm_type_from_semantic(concrete_inner);
                    std::string result = fresh_reg();
                    emit_line("  " + result + " = load " + inner_llvm_type + ", ptr " + data_ptr);
                    last_expr_type_ = inner_llvm_type;
                    return result;
                }
            }

            // Handle RwLockReadGuard[T] / RwLockWriteGuard[T] - deref returns ref T via lock.data
            if ((named.name == "RwLockReadGuard" || named.name == "RwLockWriteGuard") &&
                !named.type_args.empty()) {
                TML_DEBUG_LN("[DEREF] " << named.name << " detected, type_arg="
                                        << types::type_to_string(named.type_args[0]));

                // Get pointer to the guard (not the value)
                std::string guard_ptr;
                if (unary.operand->is<parser::IdentExpr>()) {
                    const auto& ident = unary.operand->as<parser::IdentExpr>();
                    auto it = locals_.find(ident.name);
                    if (it != locals_.end()) {
                        guard_ptr = it->second.reg;
                    }
                } else {
                    std::string guard_value = gen_expr(*unary.operand);
                    types::TypePtr concrete_inner = named.type_args[0];
                    if (!current_type_subs_.empty()) {
                        concrete_inner =
                            apply_type_substitutions(concrete_inner, current_type_subs_);
                    }
                    std::string guard_mangled_temp = require_struct_instantiation(
                        named.name, std::vector<types::TypePtr>{concrete_inner});
                    std::string guard_type_temp = "%struct." + guard_mangled_temp;

                    guard_ptr = fresh_reg();
                    emit_line("  " + guard_ptr + " = alloca " + guard_type_temp);
                    emit_line("  store " + guard_type_temp + " " + guard_value + ", ptr " +
                              guard_ptr);
                }

                if (!guard_ptr.empty()) {
                    // RwLockReadGuard/RwLockWriteGuard layout: { lock: mut ref RwLock[T] }
                    // RwLock layout: { data: T, raw: RawRwLock, state: AtomicU32 }
                    // To get the data: guard.lock.data

                    types::TypePtr concrete_inner = named.type_args[0];
                    if (!current_type_subs_.empty()) {
                        concrete_inner =
                            apply_type_substitutions(concrete_inner, current_type_subs_);
                    }

                    std::string guard_mangled = require_struct_instantiation(
                        named.name, std::vector<types::TypePtr>{concrete_inner});
                    std::string rwlock_mangled = require_struct_instantiation(
                        "RwLock", std::vector<types::TypePtr>{concrete_inner});
                    std::string guard_type = "%struct." + guard_mangled;
                    std::string rwlock_type = "%struct." + rwlock_mangled;

                    // GEP to get lock field (field 0) of guard
                    std::string lock_field_ptr = fresh_reg();
                    emit_line("  " + lock_field_ptr + " = getelementptr " + guard_type + ", ptr " +
                              guard_ptr + ", i32 0, i32 0");

                    // Load the rwlock pointer (since lock is a mut ref, stored as ptr)
                    std::string rwlock_ptr = fresh_reg();
                    emit_line("  " + rwlock_ptr + " = load ptr, ptr " + lock_field_ptr);

                    // GEP to get data field (field 0) of RwLock
                    std::string data_ptr = fresh_reg();
                    emit_line("  " + data_ptr + " = getelementptr " + rwlock_type + ", ptr " +
                              rwlock_ptr + ", i32 0, i32 0");

                    // Load the data value
                    inner_llvm_type = llvm_type_from_semantic(concrete_inner);
                    std::string result = fresh_reg();
                    emit_line("  " + result + " = load " + inner_llvm_type + ", ptr " + data_ptr);
                    last_expr_type_ = inner_llvm_type;
                    return result;
                }
            }

            // Handle Arc[T] - deref returns T via ptr->data
            // Arc[T] { ptr: Ptr[ArcInner[T]] }
            // ArcInner[T] { strong, weak, data: T }
            if (named.name == "Arc" && !named.type_args.empty()) {
                TML_DEBUG_LN(
                    "[DEREF] Arc detected, type_arg=" << types::type_to_string(named.type_args[0]));

                // Get pointer to the Arc (not the value)
                std::string arc_ptr;
                if (unary.operand->is<parser::IdentExpr>()) {
                    const auto& ident = unary.operand->as<parser::IdentExpr>();
                    auto it = locals_.find(ident.name);
                    if (it != locals_.end()) {
                        arc_ptr = it->second.reg;
                    }
                } else {
                    // For non-identifier operands, evaluate and store to temp
                    std::string arc_value = gen_expr(*unary.operand);
                    types::TypePtr concrete_inner = named.type_args[0];
                    if (!current_type_subs_.empty()) {
                        concrete_inner =
                            apply_type_substitutions(concrete_inner, current_type_subs_);
                    }
                    // Use require_struct_instantiation to handle UNRESOLVED cases
                    std::string arc_mangled_temp = require_struct_instantiation(
                        "Arc", std::vector<types::TypePtr>{concrete_inner});
                    std::string arc_type_temp = "%struct." + arc_mangled_temp;

                    arc_ptr = fresh_reg();
                    emit_line("  " + arc_ptr + " = alloca " + arc_type_temp);
                    emit_line("  store " + arc_type_temp + " " + arc_value + ", ptr " + arc_ptr);
                }

                if (!arc_ptr.empty()) {
                    types::TypePtr concrete_inner = named.type_args[0];
                    if (!current_type_subs_.empty()) {
                        concrete_inner =
                            apply_type_substitutions(concrete_inner, current_type_subs_);
                    }

                    // Use require_struct_instantiation to handle UNRESOLVED cases
                    std::string arc_mangled = require_struct_instantiation(
                        "Arc", std::vector<types::TypePtr>{concrete_inner});
                    std::string inner_mangled = require_struct_instantiation(
                        "ArcInner", std::vector<types::TypePtr>{concrete_inner});
                    std::string arc_type = "%struct." + arc_mangled;
                    std::string inner_type = "%struct." + inner_mangled;

                    // GEP to get ptr field (field 0) of Arc
                    std::string ptr_field_ptr = fresh_reg();
                    emit_line("  " + ptr_field_ptr + " = getelementptr " + arc_type + ", ptr " +
                              arc_ptr + ", i32 0, i32 0");

                    // Load the ArcInner pointer
                    std::string inner_ptr = fresh_reg();
                    emit_line("  " + inner_ptr + " = load ptr, ptr " + ptr_field_ptr);

                    // GEP to get data field (field 2) of ArcInner
                    std::string data_ptr = fresh_reg();
                    emit_line("  " + data_ptr + " = getelementptr " + inner_type + ", ptr " +
                              inner_ptr + ", i32 0, i32 2");

                    // Load the data value
                    inner_llvm_type = llvm_type_from_semantic(concrete_inner);
                    std::string result = fresh_reg();
                    emit_line("  " + result + " = load " + inner_llvm_type + ", ptr " + data_ptr);
                    last_expr_type_ = inner_llvm_type;
                    return result;
                }
            }

            // Handle TML's Ptr[T] or RawPtr[T] type (NamedType wrapper around pointer)
            if ((named.name == "Ptr" || named.name == "RawPtr") && !named.type_args.empty()) {
                types::TypePtr inner = named.type_args[0];
                if (!current_type_subs_.empty()) {
                    inner = apply_type_substitutions(inner, current_type_subs_);
                }
                inner_llvm_type = llvm_type_from_semantic(inner);
                TML_DEBUG_LN("[DEREF] Ptr/RawPtr inner type: " << inner_llvm_type);
            }
        }

        if (operand_type) {
            if (operand_type->is<types::RefType>()) {
                const auto& ref_type = operand_type->as<types::RefType>();
                if (ref_type.inner) {
                    inner_llvm_type = llvm_type_from_semantic(ref_type.inner);
                }
            } else if (operand_type->is<types::PtrType>()) {
                const auto& ptr_type = operand_type->as<types::PtrType>();
                if (ptr_type.inner) {
                    inner_llvm_type = llvm_type_from_semantic(ptr_type.inner);
                }
            }
        }

        std::string ptr = gen_expr(*unary.operand);
        std::string result = fresh_reg();
        emit_line("  " + result + " = load " + inner_llvm_type + ", ptr " + ptr);
        last_expr_type_ = inner_llvm_type;
        return result;
    }

    // Handle postfix increment (i++)
    if (unary.op == parser::UnaryOp::Inc) {
        if (unary.operand->is<parser::IdentExpr>()) {
            const auto& ident = unary.operand->as<parser::IdentExpr>();
            auto it = locals_.find(ident.name);
            if (it != locals_.end()) {
                const VarInfo& var = it->second;
                // Load current value
                std::string old_val = fresh_reg();
                emit_line("  " + old_val + " = load " + var.type + ", ptr " + var.reg);
                // Add 1
                std::string new_val = fresh_reg();
                emit_line("  " + new_val + " = add " + var.type + " " + old_val + ", 1");
                // Store new value
                emit_line("  store " + var.type + " " + new_val + ", ptr " + var.reg);
                // Return old value (postfix semantics)
                return old_val;
            }
        }
        report_error("Can only increment variables", unary.span, "C003");
        return "0";
    }

    // Handle postfix decrement (i--)
    if (unary.op == parser::UnaryOp::Dec) {
        if (unary.operand->is<parser::IdentExpr>()) {
            const auto& ident = unary.operand->as<parser::IdentExpr>();
            auto it = locals_.find(ident.name);
            if (it != locals_.end()) {
                const VarInfo& var = it->second;
                // Load current value
                std::string old_val = fresh_reg();
                emit_line("  " + old_val + " = load " + var.type + ", ptr " + var.reg);
                // Subtract 1
                std::string new_val = fresh_reg();
                emit_line("  " + new_val + " = sub " + var.type + " " + old_val + ", 1");
                // Store new value
                emit_line("  store " + var.type + " " + new_val + ", ptr " + var.reg);
                // Return old value (postfix semantics)
                return old_val;
            }
        }
        report_error("Can only decrement variables", unary.span, "C003");
        return "0";
    }

    std::string operand = gen_expr(*unary.operand);
    std::string operand_type = last_expr_type_;
    std::string result = fresh_reg();

    switch (unary.op) {
    case parser::UnaryOp::Neg:
        if (operand_type == "double" || operand_type == "float") {
            emit_line("  " + result + " = fsub " + operand_type + " 0.0, " + operand);
            last_expr_type_ = operand_type;
        } else {
            emit_line("  " + result + " = sub " + operand_type + " 0, " + operand);
            last_expr_type_ = operand_type;
        }
        break;
    case parser::UnaryOp::Not:
        // Convert non-i1 integer operands to i1 first (e.g., C runtime functions returning i32)
        if (operand_type != "i1" &&
            (operand_type == "i8" || operand_type == "i16" || operand_type == "i32" ||
             operand_type == "i64" || operand_type == "i128")) {
            std::string bool_op = fresh_reg();
            emit_line("  " + bool_op + " = icmp ne " + operand_type + " " + operand + ", 0");
            emit_line("  " + result + " = xor i1 " + bool_op + ", 1");
        } else {
            emit_line("  " + result + " = xor i1 " + operand + ", 1");
        }
        last_expr_type_ = "i1";
        break;
    case parser::UnaryOp::BitNot:
        emit_line("  " + result + " = xor i32 " + operand + ", -1");
        last_expr_type_ = "i32";
        break;
    default:
        return operand;
    }

    return result;
}

} // namespace tml::codegen
