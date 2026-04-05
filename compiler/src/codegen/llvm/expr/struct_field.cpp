TML_MODULE("codegen_x86")

//! # LLVM IR Generator - Struct Field Access
//!
//! This file implements struct field access, field index/type lookup,
//! and auto-deref through smart pointer types.
//!
//! Split from struct.cpp which handles struct construction.
//!
//! ## Field Access
//!
//! `point.x` generates a GEP and load for the field at its index.
//!
//! ## Key Functions
//!
//! | Function                | Purpose                              |
//! |-------------------------|--------------------------------------|
//! | `gen_field`             | Access field on struct/class/union    |
//! | `get_field_index`       | Look up field position               |
//! | `get_field_type`        | Look up field LLVM type              |
//! | `get_field_semantic_type`| Look up field semantic type          |
//! | `get_class_field_info`  | Get full class field info             |

#include "codegen/llvm/llvm_ir_gen.hpp"

#include <cctype>
#include <iostream>
#include <unordered_set>

namespace tml::codegen {

// Helper to get field index for struct types - uses dynamic registry
auto LLVMIRGen::get_field_index(const std::string& struct_name, const std::string& field_name)
    -> int {
    // First check the dynamic struct_fields_ registry
    auto it = struct_fields_.find(struct_name);
    if (it != struct_fields_.end()) {
        for (const auto& field : it->second) {
            if (field.name == field_name) {
                return field.index;
            }
        }
    }

    // Check class_fields_ registry for class types
    auto class_it = class_fields_.find(struct_name);
    if (class_it != class_fields_.end()) {
        for (const auto& field : class_it->second) {
            if (field.name == field_name) {
                return field.index;
            }
        }
    }

    // Fallback for hardcoded types (legacy support)
    if (struct_name == "Point") {
        if (field_name == "x")
            return 0;
        if (field_name == "y")
            return 1;
    }
    if (struct_name == "Rectangle") {
        if (field_name == "origin")
            return 0;
        if (field_name == "width")
            return 1;
        if (field_name == "height")
            return 2;
    }
    return 0;
}

// Helper to get field type for struct types - uses dynamic registry
auto LLVMIRGen::get_field_type(const std::string& struct_name, const std::string& field_name)
    -> std::string {
    // First check the dynamic struct_fields_ registry
    auto it = struct_fields_.find(struct_name);
    if (it != struct_fields_.end()) {
        for (const auto& field : it->second) {
            if (field.name == field_name) {
                return field.llvm_type;
            }
        }
    }

    // Check class_fields_ registry for class types
    auto class_it = class_fields_.find(struct_name);
    if (class_it != class_fields_.end()) {
        for (const auto& field : class_it->second) {
            if (field.name == field_name) {
                return field.llvm_type;
            }
        }
    }

    // Fallback for hardcoded types (legacy support)
    if (struct_name == "Rectangle" && field_name == "origin") {
        return "%struct.Point";
    }
    return "i32";
}

// Helper to get field semantic type for struct types - uses dynamic registry
auto LLVMIRGen::get_field_semantic_type(const std::string& struct_name,
                                        const std::string& field_name) -> types::TypePtr {
    // Check the dynamic struct_fields_ registry
    auto it = struct_fields_.find(struct_name);
    if (it != struct_fields_.end()) {
        for (const auto& field : it->second) {
            if (field.name == field_name) {
                return field.semantic_type;
            }
        }
    }

    // Note: class_fields_ uses a different struct (ClassFieldInfo) without semantic_type
    // For class fields, we'd need to look up the type from the class definition
    return nullptr;
}

// Helper to get full class field info (including inheritance details)
auto LLVMIRGen::get_class_field_info(const std::string& class_name, const std::string& field_name)
    -> std::optional<ClassFieldInfo> {
    auto class_it = class_fields_.find(class_name);
    if (class_it != class_fields_.end()) {
        for (const auto& field : class_it->second) {
            if (field.name == field_name) {
                return field;
            }
        }
    }
    return std::nullopt;
}

auto LLVMIRGen::gen_field(const parser::FieldExpr& field) -> std::string {
    // Handle static field access (ClassName.field)
    if (field.object->is<parser::IdentExpr>()) {
        const auto& ident = field.object->as<parser::IdentExpr>();

        // Check if it's a class name for static field access
        std::string static_key = ident.name + "." + field.field;
        auto static_it = static_fields_.find(static_key);
        if (static_it != static_fields_.end()) {
            // Load from global static field
            std::string result = fresh_reg();
            emit_line("  " + result + " = load " + static_it->second.type + ", ptr " +
                      static_it->second.global_name);
            last_expr_type_ = static_it->second.type;
            return result;
        }
    }

    // =========================================================================
    // Optional chaining: expr?.field
    //   Desugars to: when expr { Just(v) => Just(v.field), Nothing => Nothing }
    //   The receiver must be Maybe[T]. We generate the Maybe check, branch on
    //   Just/Nothing, and for the Just branch, access the field on the unwrapped value.
    // =========================================================================
    if (field.optional_chain) {
        // Infer the receiver's semantic type — must be Maybe[T]
        auto receiver_sem_type = infer_expr_type(*field.object);
        types::TypePtr inner_type;
        if (receiver_sem_type && receiver_sem_type->is<types::NamedType>()) {
            auto& named = receiver_sem_type->as<types::NamedType>();
            if (named.name == "Maybe" && !named.type_args.empty()) {
                inner_type = named.type_args[0];
            }
        }
        if (!inner_type) {
            report_error("Optional chaining `?.` requires Maybe[T] receiver", field.span, "C100");
            return "0";
        }

        // Generate the receiver expression (produces the Maybe value)
        std::string maybe_val = gen_expr(*field.object);
        std::string maybe_llvm_type = llvm_type_from_semantic(receiver_sem_type, true);
        std::string inner_llvm_type = llvm_type_from_semantic(inner_type, true);

        // Create labels for the branches
        std::string just_label = fresh_label("optfield_just");
        std::string nothing_label = fresh_label("optfield_nothing");
        std::string end_label = fresh_label("optfield_end");

        bool is_nullable = (maybe_llvm_type == "ptr");

        if (is_nullable) {
            // Nullable pointer optimization: null = Nothing, non-null = Just
            std::string is_null = fresh_reg();
            emit_line("  " + is_null + " = icmp eq ptr " + maybe_val + ", null");
            emit_line("  br i1 " + is_null + ", label %" + nothing_label + ", label %" +
                      just_label);
        } else {
            // Struct-based Maybe: extract tag from field 0
            std::string loaded_maybe = maybe_val;
            if (last_expr_type_ == "ptr") {
                loaded_maybe = fresh_reg();
                emit_line("  " + loaded_maybe + " = load " + maybe_llvm_type + ", ptr " +
                          maybe_val);
                maybe_val = loaded_maybe;
            }
            std::string tag = fresh_reg();
            emit_line("  " + tag + " = extractvalue " + maybe_llvm_type + " " + maybe_val + ", 0");
            std::string is_nothing = fresh_reg();
            emit_line("  " + is_nothing + " = icmp ne i32 " + tag + ", 0");
            emit_line("  br i1 " + is_nothing + ", label %" + nothing_label + ", label %" +
                      just_label);
        }

        // === Just branch: unwrap value and access the field ===
        emit_line(just_label + ":");
        current_block_ = just_label;

        std::string unwrapped_val;
        if (is_nullable) {
            unwrapped_val = maybe_val;
        } else {
            // Extract payload via alloca + GEP (works for both struct and scalar payloads)
            std::string maybe_alloca = fresh_reg();
            emit_line("  " + maybe_alloca + " = alloca " + maybe_llvm_type);
            emit_line("  store " + maybe_llvm_type + " " + maybe_val + ", ptr " + maybe_alloca);
            std::string payload_ptr = fresh_reg();
            emit_line("  " + payload_ptr + " = getelementptr inbounds " + maybe_llvm_type +
                      ", ptr " + maybe_alloca + ", i32 0, i32 1");
            unwrapped_val = fresh_reg();
            emit_line("  " + unwrapped_val + " = load " + inner_llvm_type + ", ptr " + payload_ptr);
        }

        // Store the unwrapped value to a temp alloca so we can GEP into it for the field
        std::string temp_inner_alloca = fresh_reg();
        emit_line("  " + temp_inner_alloca + " = alloca " + inner_llvm_type);
        emit_line("  store " + inner_llvm_type + " " + unwrapped_val + ", ptr " +
                  temp_inner_alloca);

        // Generate field access on the unwrapped value by constructing a synthetic
        // FieldExpr with optional_chain=false pointing to a temp local.
        std::string temp_name = "__opt_field_" + std::to_string(label_counter_++);
        VarInfo temp_local;
        temp_local.reg = temp_inner_alloca;
        temp_local.type = inner_llvm_type;
        temp_local.semantic_type = inner_type;
        temp_local.is_ptr_to_value = true;
        locals_[temp_name] = temp_local;

        // Create a synthetic FieldExpr with the unwrapped receiver
        auto& mutable_field = const_cast<parser::FieldExpr&>(field);
        auto orig_object = std::move(mutable_field.object);
        bool orig_optional_chain = mutable_field.optional_chain;

        auto temp_ident = std::make_unique<parser::Expr>();
        temp_ident->kind = parser::IdentExpr{temp_name, field.span};
        temp_ident->span = field.span;
        mutable_field.object = std::move(temp_ident);
        mutable_field.optional_chain = false;

        std::string field_result = gen_field(mutable_field);
        std::string result_type = last_expr_type_;

        // Restore original object and flag
        mutable_field.object = std::move(orig_object);
        mutable_field.optional_chain = orig_optional_chain;

        // Remove temporary local
        locals_.erase(temp_name);

        std::string just_result_val = field_result;
        std::string just_result_block = current_block_;

        // Check if the field type is already Maybe (avoid Maybe[Maybe[V]])
        bool result_is_already_maybe = result_type.find("Maybe") != std::string::npos ||
                                       result_type.find("maybe") != std::string::npos;

        std::string final_result_type;
        std::string just_wrapped;

        if (result_is_already_maybe) {
            // Field already Maybe — flatten (no extra wrapping)
            final_result_type = result_type;
            just_wrapped = just_result_val;
        } else if (result_type == "ptr") {
            // Nullable Maybe (result is a pointer type like Str)
            final_result_type = "ptr";
            just_wrapped = just_result_val;
        } else {
            // Wrap in Maybe: construct { i32 0 (Just tag), <result> }
            // Build the Maybe semantic type for the field result
            types::TypePtr field_sem_type = get_field_semantic_type(
                inner_type->is<types::NamedType>() ? inner_type->as<types::NamedType>().name : "",
                field.field);
            if (!field_sem_type) {
                // Fallback: create a dummy type from the LLVM type
                field_sem_type =
                    std::make_shared<types::Type>(types::Type{types::NamedType{result_type, ""}});
            }
            auto result_maybe_sem = std::make_shared<types::Type>(
                types::Type{types::NamedType{"Maybe", "", {field_sem_type}}});
            final_result_type = llvm_type_from_semantic(result_maybe_sem, true);

            // Build the Maybe struct via alloca + store
            std::string temp_maybe_alloca = fresh_reg();
            emit_line("  " + temp_maybe_alloca + " = alloca " + final_result_type);
            // Zero-initialize first, then set tag=0 (Just)
            emit_line("  store " + final_result_type + " zeroinitializer, ptr " +
                      temp_maybe_alloca);
            // Store tag = 0 (Just)
            std::string tag_ptr = fresh_reg();
            emit_line("  " + tag_ptr + " = getelementptr inbounds " + final_result_type + ", ptr " +
                      temp_maybe_alloca + ", i32 0, i32 0");
            emit_line("  store i32 0, ptr " + tag_ptr);
            // Store payload
            std::string payload_ptr = fresh_reg();
            emit_line("  " + payload_ptr + " = getelementptr inbounds " + final_result_type +
                      ", ptr " + temp_maybe_alloca + ", i32 0, i32 1");
            emit_line("  store " + result_type + " " + just_result_val + ", ptr " + payload_ptr);
            just_wrapped = fresh_reg();
            emit_line("  " + just_wrapped + " = load " + final_result_type + ", ptr " +
                      temp_maybe_alloca);
        }

        emit_line("  br label %" + end_label);
        just_result_block = current_block_;

        // === Nothing branch: produce Nothing value ===
        emit_line(nothing_label + ":");
        current_block_ = nothing_label;

        std::string nothing_val;
        if (final_result_type == "ptr") {
            nothing_val = "null";
        } else {
            // Nothing = tag 1, zeroed payload
            std::string nothing_with_tag = fresh_reg();
            emit_line("  " + nothing_with_tag + " = insertvalue " + final_result_type +
                      " zeroinitializer, i32 1, 0");
            nothing_val = nothing_with_tag;
        }

        emit_line("  br label %" + end_label);
        std::string nothing_block = current_block_;

        // === End: phi to merge the result ===
        emit_line(end_label + ":");
        current_block_ = end_label;

        std::string phi_result = fresh_reg();
        emit_line("  " + phi_result + " = phi " + final_result_type + " [ " + just_wrapped + ", %" +
                  just_result_block + " ], [ " + nothing_val + ", %" + nothing_block + " ]");

        last_expr_type_ = final_result_type;
        return phi_result;
    }

    // Handle field access on struct
    std::string struct_type;
    std::string struct_ptr;
    bool is_ssa_struct_value = false; // True if struct_ptr is an SSA value (not a pointer)

    // If the object is an identifier, look up its type
    if (field.object->is<parser::IdentExpr>()) {
        const auto& ident = field.object->as<parser::IdentExpr>();
        auto it = locals_.find(ident.name);
        if (it != locals_.end()) {
            struct_type = it->second.type;
            struct_ptr = it->second.reg;
            // Only SSA struct values (not pointer params like 'this') can use extractvalue
            is_ssa_struct_value =
                it->second.is_direct_param && it->second.type.find("%struct.") == 0;

            // Handle ref/ptr parameters: resolve the actual struct type from semantic type
            // and load the pointer from the alloca (e.g., other: ref TypeId -> load + GEP)
            if (struct_type == "ptr" && ident.name != "this" && ident.name != "self" &&
                it->second.semantic_type) {
                types::TypePtr sem_type = it->second.semantic_type;
                if (sem_type->is<types::RefType>()) {
                    const auto& ref = sem_type->as<types::RefType>();
                    types::TypePtr resolved_inner = ref.inner;
                    if (!current_type_subs_.empty()) {
                        resolved_inner = apply_type_substitutions(ref.inner, current_type_subs_);
                    }
                    struct_type = llvm_type_from_semantic(resolved_inner);
                    std::string loaded_ptr = fresh_reg();
                    emit_line("  " + loaded_ptr + " = load ptr, ptr " + struct_ptr);
                    struct_ptr = loaded_ptr;
                } else if (sem_type->is<types::PtrType>()) {
                    const auto& ptr = sem_type->as<types::PtrType>();
                    types::TypePtr resolved_inner = ptr.inner;
                    if (!current_type_subs_.empty()) {
                        resolved_inner = apply_type_substitutions(ptr.inner, current_type_subs_);
                    }
                    struct_type = llvm_type_from_semantic(resolved_inner);
                    std::string loaded_ptr = fresh_reg();
                    emit_line("  " + loaded_ptr + " = load ptr, ptr " + struct_ptr);
                    struct_ptr = loaded_ptr;
                }
            }

            // Special handling for 'this' in impl methods
            if (ident.name == "this" && !current_impl_type_.empty()) {
                // 'this' is a pointer to the impl type
                struct_type = "%struct." + current_impl_type_;

                // Ensure the generic struct is instantiated so its fields are registered
                // Parse the mangled name to get base_name and type_args
                // e.g., "Arc__I32" -> base="Arc", type_args=[I32]
                auto sep_pos = current_impl_type_.find("__");
                if (sep_pos != std::string::npos) {
                    std::string base_name = current_impl_type_.substr(0, sep_pos);
                    // Use semantic type from locals_ if available for proper type args
                    if (it->second.semantic_type &&
                        it->second.semantic_type->is<types::NamedType>()) {
                        const auto& named = it->second.semantic_type->as<types::NamedType>();
                        if (!named.type_args.empty()) {
                            // Apply current type substitutions to get concrete types
                            // e.g., Arc[T] with T=I32 becomes Arc[I32]
                            std::vector<types::TypePtr> concrete_args;
                            for (const auto& arg : named.type_args) {
                                if (!current_type_subs_.empty()) {
                                    concrete_args.push_back(
                                        apply_type_substitutions(arg, current_type_subs_));
                                } else {
                                    concrete_args.push_back(arg);
                                }
                            }
                            require_struct_instantiation(named.name, concrete_args);
                        }
                    }
                }
            }
        }
    } else if (field.object->is<parser::FieldExpr>()) {
        // Chained field access (e.g., this.inner.receiver_alive)
        // Generate the intermediate field access recursively
        const auto& nested_field = field.object->as<parser::FieldExpr>();

        // Get the outermost struct
        if (nested_field.object->is<parser::IdentExpr>()) {
            const auto& ident = nested_field.object->as<parser::IdentExpr>();
            std::string outer_type;
            std::string outer_ptr;

            // Special handling for 'this' in impl methods
            if (ident.name == "this" && !current_impl_type_.empty()) {
                outer_type = "%struct." + current_impl_type_;
                outer_ptr = "%this";
            } else {
                auto it = locals_.find(ident.name);
                if (it != locals_.end()) {
                    outer_type = it->second.type;
                    outer_ptr = it->second.reg;

                    // Handle ref types - resolve the actual struct type from semantic type
                    // This fixes chained field access on ref parameters (e.g.,
                    // ref_param.field1.field2)
                    if (outer_type == "ptr" && it->second.semantic_type) {
                        types::TypePtr sem_type = it->second.semantic_type;
                        if (sem_type->is<types::RefType>()) {
                            const auto& ref = sem_type->as<types::RefType>();
                            types::TypePtr resolved_inner = ref.inner;
                            if (!current_type_subs_.empty()) {
                                resolved_inner =
                                    apply_type_substitutions(ref.inner, current_type_subs_);
                            }
                            outer_type = llvm_type_from_semantic(resolved_inner);
                            // Load the pointer from the alloca
                            std::string loaded_ptr = fresh_reg();
                            emit_line("  " + loaded_ptr + " = load ptr, ptr " + outer_ptr);
                            outer_ptr = loaded_ptr;
                        } else if (sem_type->is<types::PtrType>()) {
                            const auto& ptr = sem_type->as<types::PtrType>();
                            types::TypePtr resolved_inner = ptr.inner;
                            if (!current_type_subs_.empty()) {
                                resolved_inner =
                                    apply_type_substitutions(ptr.inner, current_type_subs_);
                            }
                            outer_type = llvm_type_from_semantic(resolved_inner);
                            // Load the pointer from the alloca
                            std::string loaded_ptr = fresh_reg();
                            emit_line("  " + loaded_ptr + " = load ptr, ptr " + outer_ptr);
                            outer_ptr = loaded_ptr;
                        }
                    }
                }
            }

            if (!outer_type.empty() && !outer_ptr.empty()) {
                // Get outer struct type name
                std::string outer_name = outer_type;
                if (outer_name.starts_with("%struct.")) {
                    outer_name = outer_name.substr(8);
                }

                // Check for auto-deref on the outer type (e.g., Arc[ChannelInner[T]])
                types::TypePtr outer_sem_type = infer_expr_type(*nested_field.object);
                types::TypePtr deref_target = get_deref_target_type(outer_sem_type);
                if (deref_target && !struct_has_field(outer_name, nested_field.field)) {
                    // Need to auto-deref to access the field
                    auto sep_pos = outer_name.find("__");
                    std::string base_type_name =
                        (sep_pos != std::string::npos) ? outer_name.substr(0, sep_pos) : outer_name;

                    if (base_type_name == "Arc" || base_type_name == "Shared" ||
                        base_type_name == "Rc") {
                        // Arc layout: { ptr: Ptr[ArcInner[T]] }
                        std::string arc_ptr_field = fresh_reg();
                        emit_line("  " + arc_ptr_field + " = getelementptr inbounds " + outer_type +
                                  ", ptr " + outer_ptr + ", i32 0, i32 0");
                        std::string inner_ptr = fresh_reg();
                        emit_line("  " + inner_ptr + " = load ptr, ptr " + arc_ptr_field);

                        // Get ArcInner type
                        std::string arc_inner_mangled = mangle_struct_name(
                            "ArcInner", std::vector<types::TypePtr>{deref_target});
                        std::string arc_inner_type = "%struct." + arc_inner_mangled;

                        // GEP to data field (index 2)
                        std::string data_ptr = fresh_reg();
                        emit_line("  " + data_ptr + " = getelementptr inbounds " + arc_inner_type +
                                  ", ptr " + inner_ptr + ", i32 0, i32 2");

                        // Update outer_ptr and outer_type to point to inner struct
                        outer_ptr = data_ptr;
                        if (deref_target->is<types::NamedType>()) {
                            const auto& inner_named = deref_target->as<types::NamedType>();
                            if (!inner_named.type_args.empty()) {
                                // Use return value to handle UNRESOLVED cases
                                outer_name = require_struct_instantiation(inner_named.name,
                                                                          inner_named.type_args);
                            } else {
                                outer_name = inner_named.name;
                            }
                            outer_type = "%struct." + outer_name;
                        }
                    } else if (base_type_name == "Box" || base_type_name == "Heap") {
                        // Box layout: { ptr: Ptr[T] }
                        std::string box_ptr_field = fresh_reg();
                        emit_line("  " + box_ptr_field + " = getelementptr inbounds " + outer_type +
                                  ", ptr " + outer_ptr + ", i32 0, i32 0");
                        std::string inner_ptr = fresh_reg();
                        emit_line("  " + inner_ptr + " = load ptr, ptr " + box_ptr_field);

                        outer_ptr = inner_ptr;
                        if (deref_target->is<types::NamedType>()) {
                            const auto& inner_named = deref_target->as<types::NamedType>();
                            if (!inner_named.type_args.empty()) {
                                // Use return value to handle UNRESOLVED cases
                                outer_name = require_struct_instantiation(inner_named.name,
                                                                          inner_named.type_args);
                            } else {
                                outer_name = inner_named.name;
                            }
                            outer_type = "%struct." + outer_name;
                        }
                    }
                }

                // Get field index for nested field
                int nested_idx = get_field_index(outer_name, nested_field.field);
                std::string nested_type = get_field_type(outer_name, nested_field.field);

                // Get pointer to nested field
                std::string nested_ptr = fresh_reg();
                emit_line("  " + nested_ptr + " = getelementptr inbounds " + outer_type + ", ptr " +
                          outer_ptr + ", i32 0, i32 " + std::to_string(nested_idx));

                struct_type = nested_type;
                struct_ptr = nested_ptr;

                // If nested_type is "ptr", get the semantic type for correct type inference
                // This is crucial for generic struct fields like `mutex: mut ref Mutex[T]`
                if (nested_type == "ptr") {
                    types::TypePtr field_sem_type =
                        get_field_semantic_type(outer_name, nested_field.field);
                    if (field_sem_type && !current_type_subs_.empty()) {
                        field_sem_type =
                            apply_type_substitutions(field_sem_type, current_type_subs_);
                    }
                    // Store the semantic type for later use in the struct_type == "ptr" path
                    if (field_sem_type) {
                        // Extract the inner type from Ref/Ptr
                        types::TypePtr inner_type = field_sem_type;
                        if (field_sem_type->is<types::RefType>()) {
                            inner_type = field_sem_type->as<types::RefType>().inner;
                        } else if (field_sem_type->is<types::PtrType>()) {
                            inner_type = field_sem_type->as<types::PtrType>().inner;
                        }
                        if (inner_type && inner_type->is<types::NamedType>()) {
                            const auto& named = inner_type->as<types::NamedType>();
                            if (!named.type_args.empty()) {
                                // Use return value to handle UNRESOLVED cases
                                std::string mangled =
                                    require_struct_instantiation(named.name, named.type_args);
                                struct_type = "%struct." + mangled;
                            } else {
                                struct_type = "%struct." + named.name;
                            }
                        }
                    }
                }
            }
        } else {
            // Handle deeper nesting: recursively generate the intermediate field access
            std::string nested_val = gen_expr(*field.object);
            types::TypePtr nested_sem_type = infer_expr_type(*field.object);

            // For struct types, gen_expr returns a loaded value
            // We need to store to a temp alloca if it's a struct value
            if (last_expr_type_.starts_with("%struct.")) {
                std::string temp_ptr = fresh_reg();
                emit_line("  " + temp_ptr + " = alloca " + last_expr_type_);
                emit_line("  store " + last_expr_type_ + " " + nested_val + ", ptr " + temp_ptr);
                struct_ptr = temp_ptr;
                struct_type = last_expr_type_;
            } else if (last_expr_type_ == "ptr") {
                // Pointer type - use directly
                struct_ptr = nested_val;

                // Infer the struct type from the semantic type
                // Apply type substitutions for generic contexts
                types::TypePtr resolved_sem_type = nested_sem_type;
                if (resolved_sem_type && !current_type_subs_.empty()) {
                    resolved_sem_type =
                        apply_type_substitutions(resolved_sem_type, current_type_subs_);
                }

                if (resolved_sem_type && resolved_sem_type->is<types::NamedType>()) {
                    const auto& named = resolved_sem_type->as<types::NamedType>();
                    if (!named.type_args.empty()) {
                        // Use return value to handle UNRESOLVED cases
                        std::string mangled =
                            require_struct_instantiation(named.name, named.type_args);
                        struct_type = "%struct." + mangled;
                    } else {
                        struct_type = "%struct." + named.name;
                    }
                }
            }
        }
    } else if (field.object->is<parser::UnaryExpr>()) {
        // Handle dereferenced pointer field access (e.g., (*ptr).field)
        const auto& unary = field.object->as<parser::UnaryExpr>();
        if (unary.op == parser::UnaryOp::Deref) {
            // Generate the pointer value
            struct_ptr = gen_expr(*unary.operand);

            // Infer the pointee type
            types::TypePtr ptr_type = infer_expr_type(*unary.operand);
            if (ptr_type) {
                types::TypePtr inner_type;
                if (ptr_type->is<types::PtrType>()) {
                    inner_type = ptr_type->as<types::PtrType>().inner;
                } else if (ptr_type->is<types::RefType>()) {
                    inner_type = ptr_type->as<types::RefType>().inner;
                } else if (ptr_type->is<types::NamedType>()) {
                    // Handle TML's Ptr[T] type (NamedType with name="Ptr" or "RawPtr")
                    const auto& named = ptr_type->as<types::NamedType>();
                    if ((named.name == "Ptr" || named.name == "RawPtr") &&
                        !named.type_args.empty()) {
                        inner_type = named.type_args[0];
                    }
                }

                // Apply type substitutions for generic types
                // E.g., Ptr[Node[T]] with T -> I32 becomes Node[I32]
                if (inner_type && !current_type_subs_.empty()) {
                    inner_type = apply_type_substitutions(inner_type, current_type_subs_);
                }

                if (inner_type) {
                    std::string type_name;
                    if (inner_type->is<types::NamedType>()) {
                        type_name = inner_type->as<types::NamedType>().name;
                        // Check if it's a generic type and mangle accordingly
                        const auto& named = inner_type->as<types::NamedType>();
                        if (!named.type_args.empty()) {
                            // Ensure generic struct is instantiated so fields are registered
                            // Use return value to handle UNRESOLVED cases
                            std::string mangled =
                                require_struct_instantiation(type_name, named.type_args);
                            struct_type = "%struct." + mangled;
                        } else {
                            struct_type = "%struct." + type_name;
                        }
                    } else if (inner_type->is<types::ClassType>()) {
                        type_name = inner_type->as<types::ClassType>().name;
                        struct_type = "%class." + type_name;
                    }
                }
            }
        }
    } else if (field.object->is<parser::CallExpr>() || field.object->is<parser::MethodCallExpr>()) {
        // Handle field access on function/method call return value (e.g., func().field)
        // Generate the call, which returns a struct value
        std::string call_result = gen_expr(*field.object);
        types::TypePtr call_type = infer_expr_type(*field.object);

        // Apply current type substitutions to resolve generic types
        if (call_type && !current_type_subs_.empty()) {
            call_type = apply_type_substitutions(call_type, current_type_subs_);
        }

        TML_DEBUG_LN("[GEN_FIELD] CallExpr/MethodCallExpr - field="
                     << field.field << " last_expr_type_=" << last_expr_type_
                     << " call_type=" << (call_type ? types::type_to_string(call_type) : "null"));

        // For struct return values, we need to store to a temp alloca
        if (last_expr_type_.starts_with("%struct.")) {
            std::string temp_ptr = fresh_reg();
            emit_line("  " + temp_ptr + " = alloca " + last_expr_type_);
            emit_line("  store " + last_expr_type_ + " " + call_result + ", ptr " + temp_ptr);
            struct_ptr = temp_ptr;
            struct_type = last_expr_type_;
        } else if ((last_expr_type_ == "ptr" || last_expr_type_ == "i64") && call_type &&
                   call_type->is<types::NamedType>()) {
            // Check if the semantic type is a struct - if so, treat i64 as ptr
            // This handles cases like List::get returning an i64 that is actually a struct pointer
            const auto& named = call_type->as<types::NamedType>();

            // Look up if this is a struct type
            bool is_struct_type = false;
            auto struct_def = env_.lookup_struct(named.name);
            if (struct_def) {
                is_struct_type = true;
            } else if (env_.module_registry()) {
                const auto& all_modules = env_.module_registry()->get_all_modules();
                for (const auto& [mod_name, mod] : all_modules) {
                    if (mod.structs.find(named.name) != mod.structs.end() ||
                        mod.internal_structs.find(named.name) != mod.internal_structs.end()) {
                        is_struct_type = true;
                        break;
                    }
                }
            }

            if (is_struct_type) {
                // Convert i64 to ptr if needed
                if (last_expr_type_ == "i64") {
                    std::string ptr_val = fresh_reg();
                    emit_line("  " + ptr_val + " = inttoptr i64 " + call_result + " to ptr");
                    struct_ptr = ptr_val;
                } else {
                    struct_ptr = call_result;
                }

                // Resolve the struct type
                if (!named.type_args.empty()) {
                    std::string mangled = require_struct_instantiation(named.name, named.type_args);
                    struct_type = "%struct." + mangled;
                } else {
                    struct_type = "%struct." + named.name;
                }
            }
        } else if (last_expr_type_ == "ptr" && call_type) {
            // Pointer type - the return value is a pointer to the struct
            struct_ptr = call_result;

            // Resolve the struct type from semantic type
            types::TypePtr resolved_type = call_type;
            if (!current_type_subs_.empty()) {
                resolved_type = apply_type_substitutions(call_type, current_type_subs_);
            }

            if (resolved_type && resolved_type->is<types::NamedType>()) {
                const auto& named = resolved_type->as<types::NamedType>();
                if (!named.type_args.empty()) {
                    std::string mangled = require_struct_instantiation(named.name, named.type_args);
                    struct_type = "%struct." + mangled;
                } else {
                    struct_type = "%struct." + named.name;
                }
            }
        }
    }

    if (struct_type.empty() || struct_ptr.empty()) {
        report_error("Cannot resolve field access object", field.span, "C027");
        return "0";
    }

    // If struct_type is ptr, infer the actual struct type from the expression
    if (struct_type == "ptr") {
        types::TypePtr semantic_type = infer_expr_type(*field.object);
        TML_DEBUG_LN("[GEN_FIELD] struct_type is ptr, field="
                     << field.field << ", semantic_type="
                     << (semantic_type ? types::type_to_string(semantic_type) : "null"));
        if (semantic_type) {
            // If the semantic type is a reference or pointer, get the inner type
            // and load the pointer from the alloca first
            if (semantic_type->is<types::RefType>()) {
                const auto& ref = semantic_type->as<types::RefType>();
                // Apply type substitutions for generic impl methods
                // e.g., if inner is Mutex[T] and current_type_subs_ = {T: I32},
                // we need Mutex[I32] not Mutex[T]
                types::TypePtr resolved_inner = ref.inner;
                TML_DEBUG_LN("[GEN_FIELD] RefType inner=" << types::type_to_string(ref.inner)
                                                          << ", current_type_subs_.size="
                                                          << current_type_subs_.size());
                if (!current_type_subs_.empty()) {
                    resolved_inner = apply_type_substitutions(ref.inner, current_type_subs_);
                    TML_DEBUG_LN("[GEN_FIELD] After substitution: "
                                 << types::type_to_string(resolved_inner));
                }
                struct_type = llvm_type_from_semantic(resolved_inner);
                TML_DEBUG_LN("[GEN_FIELD] struct_type set to: " << struct_type);
                // struct_ptr points to an alloca containing a pointer to the struct
                // We need to load the pointer first
                std::string loaded_ptr = fresh_reg();
                emit_line("  " + loaded_ptr + " = load ptr, ptr " + struct_ptr);
                struct_ptr = loaded_ptr;
            } else if (semantic_type->is<types::PtrType>()) {
                const auto& ptr = semantic_type->as<types::PtrType>();
                // Apply type substitutions for generic impl methods
                types::TypePtr resolved_inner = ptr.inner;
                if (!current_type_subs_.empty()) {
                    resolved_inner = apply_type_substitutions(ptr.inner, current_type_subs_);
                }
                struct_type = llvm_type_from_semantic(resolved_inner);
                // Same - load the pointer from the alloca
                std::string loaded_ptr = fresh_reg();
                emit_line("  " + loaded_ptr + " = load ptr, ptr " + struct_ptr);
                struct_ptr = loaded_ptr;
            } else if (semantic_type->is<types::ClassType>()) {
                // Class types are heap-allocated pointers
                // Use %class.ClassName as the struct type
                const auto& cls = semantic_type->as<types::ClassType>();
                struct_type = "%class." + cls.name;
                // For local variables, the alloca stores a pointer to the class instance
                // We need to load the pointer first (unless it's a direct parameter)
                if (field.object->is<parser::IdentExpr>()) {
                    const auto& ident = field.object->as<parser::IdentExpr>();
                    auto it = locals_.find(ident.name);
                    bool is_direct_param = (it != locals_.end() && it->second.is_direct_param);
                    if (!is_direct_param) {
                        // Local variable - load the pointer from the alloca
                        std::string loaded_ptr = fresh_reg();
                        emit_line("  " + loaded_ptr + " = load ptr, ptr " + struct_ptr);
                        struct_ptr = loaded_ptr;
                    }
                    // Direct parameters (this, other method params) are already pointers
                }
            } else {
                struct_type = llvm_type_from_semantic(semantic_type);
            }
        }
    }

    // Check if this is tuple element access (field name is a number like "0", "1", "2")
    bool is_tuple_access = !field.field.empty() && std::isdigit(field.field[0]);

    if (is_tuple_access) {
        // Tuple element access: tuple.0, tuple.1, etc.
        types::TypePtr obj_type = infer_expr_type(*field.object);
        if (obj_type && obj_type->is<types::TupleType>()) {
            const auto& tuple_type = obj_type->as<types::TupleType>();
            size_t idx = std::stoul(field.field);

            if (idx >= tuple_type.elements.size()) {
                report_error("Tuple index out of bounds: " + field.field, field.span, "C027");
                return "0";
            }

            // Get the element type
            types::TypePtr elem_type = tuple_type.elements[idx];
            std::string elem_llvm_type = llvm_type_from_semantic(elem_type);

            // Generate tuple type string for getelementptr
            std::string tuple_llvm_type = "{ ";
            for (size_t i = 0; i < tuple_type.elements.size(); ++i) {
                if (i > 0)
                    tuple_llvm_type += ", ";
                tuple_llvm_type += llvm_type_from_semantic(tuple_type.elements[i]);
            }
            tuple_llvm_type += " }";

            // Use getelementptr to access element, then load
            std::string elem_ptr = fresh_reg();
            emit_line("  " + elem_ptr + " = getelementptr inbounds " + tuple_llvm_type + ", ptr " +
                      struct_ptr + ", i32 0, i32 " + std::to_string(idx));

            std::string result = fresh_reg();
            emit_line("  " + result + " = load " + elem_llvm_type + ", ptr " + elem_ptr);
            last_expr_type_ = elem_llvm_type;

            // Mark the tuple variable as consumed when extracting elements (move semantics)
            // This prevents double-free when tuple elements are moved to new bindings
            if (field.object->is<parser::IdentExpr>()) {
                const auto& ident = field.object->as<parser::IdentExpr>();
                mark_var_consumed(ident.name);
            }

            return result;
        }
    }

    // Get struct/class/union type name
    std::string type_name = struct_type;
    bool is_union_type = false;
    if (type_name.starts_with("%struct.")) {
        type_name = type_name.substr(8);
    } else if (type_name.starts_with("%class.")) {
        type_name = type_name.substr(7);
    } else if (type_name.starts_with("%union.")) {
        type_name = type_name.substr(7);
        is_union_type = true;
    }

    // Check if this is a union type (also check registry in case type was set differently)
    if (!is_union_type && union_types_.find(type_name) != union_types_.end()) {
        is_union_type = true;
    }

    // Check for auto-deref on smart pointer types (Arc, Box, etc.)
    // If the field is not found on the smart pointer type, dereference to the inner type
    types::TypePtr obj_type = infer_expr_type(*field.object);
    types::TypePtr deref_target = get_deref_target_type(obj_type);
    if (deref_target && !struct_has_field(type_name, field.field)) {
        TML_DEBUG_LN("[GEN_FIELD] Auto-deref: " << type_name << " -> "
                                                << types::type_to_string(deref_target));

        // Generate deref code for Arc[T]:
        // 1. Load arc.ptr (field 0) to get Ptr[ArcInner[T]]
        // 2. GEP to get (*ptr).data (field 2) which is T
        // 3. Then access field.field on T

        std::string ptr_type = type_name;
        // Extract base type name from mangled name (e.g., Arc__ChannelInner__I32 -> Arc)
        auto sep_pos = ptr_type.find("__");
        std::string base_type_name =
            (sep_pos != std::string::npos) ? ptr_type.substr(0, sep_pos) : ptr_type;

        if (base_type_name == "Arc" || base_type_name == "Shared" || base_type_name == "Rc") {
            // Arc layout: { ptr: Ptr[ArcInner[T]] }
            // ArcInner layout: { strong: AtomicUsize, weak: AtomicUsize, data: T }

            // Load the inner ptr from Arc struct (field 0)
            std::string arc_ptr_field = fresh_reg();
            emit_line("  " + arc_ptr_field + " = getelementptr inbounds " + struct_type + ", ptr " +
                      struct_ptr + ", i32 0, i32 0");
            std::string inner_ptr = fresh_reg();
            emit_line("  " + inner_ptr + " = load ptr, ptr " + arc_ptr_field);

            // Get the ArcInner type - need to figure out its mangled name
            // For Arc[ChannelInner[I32]], inner is ChannelInner[I32]
            // ArcInner[ChannelInner[I32]] is the actual inner struct
            std::string arc_inner_mangled = "ArcInner";
            if (deref_target->is<types::NamedType>()) {
                arc_inner_mangled =
                    mangle_struct_name("ArcInner", std::vector<types::TypePtr>{deref_target});
            }

            // GEP to get data field of ArcInner (field index 2: strong=0, weak=1, data=2)
            std::string arc_inner_type = "%struct." + arc_inner_mangled;
            std::string data_ptr = fresh_reg();
            emit_line("  " + data_ptr + " = getelementptr inbounds " + arc_inner_type + ", ptr " +
                      inner_ptr + ", i32 0, i32 2");

            // Now update struct_ptr to point to the data and struct_type to the inner type
            struct_ptr = data_ptr;
            if (deref_target->is<types::NamedType>()) {
                const auto& inner_named = deref_target->as<types::NamedType>();
                if (!inner_named.type_args.empty()) {
                    // Use return value to handle UNRESOLVED cases
                    std::string mangled =
                        require_struct_instantiation(inner_named.name, inner_named.type_args);
                    struct_type = "%struct." + mangled;
                    type_name = mangled;
                } else {
                    struct_type = "%struct." + inner_named.name;
                    type_name = inner_named.name;
                }
            }
            TML_DEBUG_LN("[GEN_FIELD] After auto-deref: struct_type="
                         << struct_type << " type_name=" << type_name);
        } else if (base_type_name == "Box" || base_type_name == "Heap") {
            // Box/Heap layout: { ptr: Ptr[T] }
            // Simply load the ptr and access the field on T

            // Load the inner ptr from Box struct (field 0)
            std::string box_ptr_field = fresh_reg();
            emit_line("  " + box_ptr_field + " = getelementptr inbounds " + struct_type + ", ptr " +
                      struct_ptr + ", i32 0, i32 0");
            std::string inner_ptr = fresh_reg();
            emit_line("  " + inner_ptr + " = load ptr, ptr " + box_ptr_field);

            // Update struct_ptr and struct_type
            struct_ptr = inner_ptr;
            if (deref_target->is<types::NamedType>()) {
                const auto& inner_named = deref_target->as<types::NamedType>();
                if (!inner_named.type_args.empty()) {
                    // Use return value to handle UNRESOLVED cases
                    std::string mangled =
                        require_struct_instantiation(inner_named.name, inner_named.type_args);
                    struct_type = "%struct." + mangled;
                    type_name = mangled;
                } else {
                    struct_type = "%struct." + inner_named.name;
                    type_name = inner_named.name;
                }
            }
        } else if (base_type_name == "Ptr" || base_type_name == "RawPtr") {
            // Ptr[T] is already a pointer to T
            // struct_ptr is the pointer value, just update the type info
            if (deref_target->is<types::NamedType>()) {
                const auto& inner_named = deref_target->as<types::NamedType>();
                if (!inner_named.type_args.empty()) {
                    // Use return value to handle UNRESOLVED cases
                    std::string mangled =
                        require_struct_instantiation(inner_named.name, inner_named.type_args);
                    struct_type = "%struct." + mangled;
                    type_name = mangled;
                } else {
                    struct_type = "%struct." + inner_named.name;
                    type_name = inner_named.name;
                }
            }
            TML_DEBUG_LN("[GEN_FIELD] Ptr auto-deref: struct_type=" << struct_type
                                                                    << " type_name=" << type_name);
        }
    }

    // Check if this is a class property access (getter call)
    std::string prop_key = type_name + "." + field.field;
    auto prop_it = class_properties_.find(prop_key);
    if (prop_it != class_properties_.end() && prop_it->second.has_getter) {
        // Property access - call getter method instead of direct field access
        const auto& prop_info = prop_it->second;
        std::string getter_name = "@" + mangle_impl_method(type_name, "get_" + prop_info.name);

        std::string result = fresh_reg();
        if (prop_info.is_static) {
            // Static property getter - no 'this' parameter
            emit_line("  " + result + " = call " + prop_info.llvm_type + " " + getter_name + "()");
        } else {
            // Instance property getter - pass 'this' pointer
            emit_line("  " + result + " = call " + prop_info.llvm_type + " " + getter_name +
                      "(ptr " + struct_ptr + ")");
        }
        last_expr_type_ = prop_info.llvm_type;
        return result;
    }

    // Get field index and type
    int field_idx = get_field_index(type_name, field.field);
    std::string field_type = get_field_type(type_name, field.field);

    // Union field access - load directly from union pointer (all fields at offset 0)
    if (is_union_type) {
        std::string result = fresh_reg();
        emit_line("  " + result + " = load " + field_type + ", ptr " + struct_ptr);
        last_expr_type_ = field_type;
        return result;
    }

    // SIMD vector field access — use extractelement instead of GEP+load
    if (is_simd_type(type_name)) {
        const auto& info = simd_types_.at(type_name);
        std::string vec_type = simd_vec_type_str(info);
        std::string vec_val;
        if (is_ssa_struct_value) {
            // SSA value (direct param passed by value): the SIMD struct type IS the
            // vector type itself (e.g. %struct.F32x4 = type <4 x float>), so the
            // SSA value is already the vector — use it directly.
            vec_val = struct_ptr;
        } else {
            // Pointer: load the full vector from memory
            vec_val = fresh_reg();
            emit_line("  " + vec_val + " = load " + vec_type + ", ptr " + struct_ptr);
        }
        // Extract the element
        std::string result = fresh_reg();
        emit_line("  " + result + " = extractelement " + vec_type + " " + vec_val + ", i32 " +
                  std::to_string(field_idx));
        last_expr_type_ = info.element_llvm_type;
        return result;
    }

    // For SSA struct values (direct params), use extractvalue instead of GEP+load.
    // This produces: %result = extractvalue %struct.Point %p, 0
    // Instead of:    %ptr = getelementptr %struct.Point, ptr %alloca, 0, 0
    //               %result = load i32, ptr %ptr
    if (is_ssa_struct_value && field_idx >= 0) {
        std::string result = fresh_reg();
        emit_line("  " + result + " = extractvalue " + struct_type + " " + struct_ptr + ", " +
                  std::to_string(field_idx));
        last_expr_type_ = field_type;
        return result;
    }

    std::string field_ptr;

    // Check if this is an inherited field (for class types)
    auto field_info = get_class_field_info(type_name, field.field);
    if (field_info && field_info->is_inherited && !field_info->inheritance_path.empty()) {
        // Generate chained GEPs through inheritance path
        std::string current_ptr = struct_ptr;
        std::string current_type = struct_type;

        for (const auto& step : field_info->inheritance_path) {
            std::string next_ptr = fresh_reg();
            emit_line("  " + next_ptr + " = getelementptr inbounds " + current_type + ", ptr " +
                      current_ptr + ", i32 0, i32 " + std::to_string(step.index));
            current_ptr = next_ptr;
            current_type = "%class." + step.class_name;
        }
        field_ptr = current_ptr;
    } else {
        // Direct field access
        field_ptr = fresh_reg();
        emit_line("  " + field_ptr + " = getelementptr inbounds " + struct_type + ", ptr " +
                  struct_ptr + ", i32 0, i32 " + std::to_string(field_idx));
    }

    std::string result = fresh_reg();
    emit_line("  " + result + " = load " + field_type + ", ptr " + field_ptr);
    last_expr_type_ = field_type;
    return result;
}

} // namespace tml::codegen
