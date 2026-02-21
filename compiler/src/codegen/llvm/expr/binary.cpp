//! # LLVM IR Generator - Binary Expressions
//!
//! This file implements binary operator code generation.
//!
//! ## Operator Categories
//!
//! | Category    | Operators                    | LLVM Instructions   |
//! |-------------|------------------------------|---------------------|
//! | Arithmetic  | `+` `-` `*` `/` `%`          | add, sub, mul, div  |
//! | Comparison  | `==` `!=` `<` `>` `<=` `>=`  | icmp, fcmp          |
//! | Logical     | `and` `or`                   | and, or (short-circuit)|
//! | Bitwise     | `&` `\|` `^` `<<` `>>`        | and, or, xor, shl, shr|
//! | Assignment  | `=`                          | store               |
//!
//! ## Type Handling
//!
//! - Integer operations use `add`, `sub`, `mul`, `sdiv`/`udiv`
//! - Float operations use `fadd`, `fsub`, `fmul`, `fdiv`
//! - Comparisons use `icmp`/`fcmp` with appropriate predicates
//!
//! ## Assignment
//!
//! Assignment to identifiers uses `store` instruction.
//! Compound assignments (+=, -=, etc.) are lowered to load-op-store.

#include "codegen/llvm/llvm_ir_gen.hpp"
#include "lexer/lexer.hpp"

#include <algorithm>
#include <functional>
#include <unordered_set>

namespace tml::codegen {

auto LLVMIRGen::gen_binary(const parser::BinaryExpr& bin) -> std::string {
    // Handle assignment specially - don't evaluate left for deref assignments
    if (bin.op == parser::BinaryOp::Assign) {
        // For field assignments, we need to set expected types BEFORE evaluating RHS
        // This is needed for generic enum unit variants like 'Nothing' to get the correct type
        // and for integer literals to get the correct size (e.g., -1 should be i64 not i32)
        std::string saved_expected_enum_type = expected_enum_type_;
        std::string saved_expected_literal_type = expected_literal_type_;
        bool saved_expected_literal_is_unsigned = expected_literal_is_unsigned_;

        if (bin.left->is<parser::FieldExpr>()) {
            // Get the field type to use as expected enum type for RHS
            types::TypePtr lhs_type = infer_expr_type(*bin.left);
            if (lhs_type) {
                std::string llvm_type = llvm_type_from_semantic(lhs_type);
                if (llvm_type.starts_with("%struct.")) {
                    expected_enum_type_ = llvm_type;
                }
                // Set expected_literal_type_ for integer fields
                if (llvm_type == "i8") {
                    expected_literal_type_ = "i8";
                    expected_literal_is_unsigned_ = false;
                } else if (llvm_type == "i16") {
                    expected_literal_type_ = "i16";
                    expected_literal_is_unsigned_ = false;
                } else if (llvm_type == "i64") {
                    expected_literal_type_ = "i64";
                    expected_literal_is_unsigned_ = false;
                }
            }
        } else if (bin.left->is<parser::IdentExpr>()) {
            // For variable assignments, also set expected type
            auto it = locals_.find(bin.left->as<parser::IdentExpr>().name);
            if (it != locals_.end()) {
                const std::string& target_type = it->second.type;
                if (target_type == "i8") {
                    expected_literal_type_ = "i8";
                    expected_literal_is_unsigned_ = false;
                } else if (target_type == "i16") {
                    expected_literal_type_ = "i16";
                    expected_literal_is_unsigned_ = false;
                } else if (target_type == "i64") {
                    expected_literal_type_ = "i64";
                    expected_literal_is_unsigned_ = false;
                }
            }
        }

        std::string right = gen_expr(*bin.right);

        // Restore expected types
        expected_enum_type_ = saved_expected_enum_type;
        expected_literal_type_ = saved_expected_literal_type;
        expected_literal_is_unsigned_ = saved_expected_literal_is_unsigned;

        if (bin.left->is<parser::IdentExpr>()) {
            auto it = locals_.find(bin.left->as<parser::IdentExpr>().name);
            if (it != locals_.end()) {
                // Check if this is a mutable reference - if so, we need to store THROUGH the
                // reference
                bool is_mut_ref = false;
                std::string inner_llvm_type = it->second.type;

                if (it->second.semantic_type && it->second.semantic_type->is<types::RefType>() &&
                    it->second.semantic_type->as<types::RefType>().is_mut) {
                    is_mut_ref = true;
                    // Get the inner type for store
                    const auto& ref_type = it->second.semantic_type->as<types::RefType>();
                    if (ref_type.inner) {
                        inner_llvm_type = llvm_type_from_semantic(ref_type.inner);
                    }
                }

                if (is_mut_ref) {
                    // Assignment through mutable reference:
                    // 1. Load the pointer from the alloca
                    // 2. Store the value through that pointer
                    std::string ptr_reg = fresh_reg();
                    emit_line("  " + ptr_reg + " = load ptr, ptr " + it->second.reg);
                    emit_line("  store " + inner_llvm_type + " " + right + ", ptr " + ptr_reg);
                } else {
                    // Handle integer truncation if needed (e.g., i32 result to i8 variable)
                    std::string value_to_store = right;
                    std::string right_type = last_expr_type_;
                    std::string target_type = it->second.type;

                    // Helper to get integer size
                    auto get_int_size = [](const std::string& t) -> int {
                        if (t == "i8")
                            return 8;
                        if (t == "i16")
                            return 16;
                        if (t == "i32")
                            return 32;
                        if (t == "i64")
                            return 64;
                        if (t == "i128")
                            return 128;
                        return 0;
                    };

                    if (right_type != target_type) {
                        int right_size = get_int_size(right_type);
                        int target_size = get_int_size(target_type);

                        if (right_size > 0 && target_size > 0 && right_size > target_size) {
                            // Truncate to smaller type
                            std::string trunc_reg = fresh_reg();
                            emit_line("  " + trunc_reg + " = trunc " + right_type + " " + right +
                                      " to " + target_type);
                            value_to_store = trunc_reg;
                        }
                    }

                    emit_line("  store " + target_type + " " + value_to_store + ", ptr " +
                              it->second.reg);
                }
            }
        } else if (bin.left->is<parser::UnaryExpr>()) {
            const auto& unary = bin.left->as<parser::UnaryExpr>();
            if (unary.op == parser::UnaryOp::Deref) {
                // Dereferenced pointer assignment: *ptr = value
                // IMPORTANT: Don't use last_semantic_type_ here - it's from the RHS!
                // We need to infer the type of the LHS operand specifically.
                types::TypePtr operand_type = infer_expr_type(*unary.operand);
                std::string inner_llvm_type = "i32"; // default
                TML_DEBUG_LN("[DEREF_ASSIGN] operand_type="
                             << (operand_type ? types::type_to_string(operand_type) : "null"));

                // Check for smart pointer types that implement DerefMut (like MutexGuard)
                if (operand_type && operand_type->is<types::NamedType>()) {
                    const auto& named = operand_type->as<types::NamedType>();
                    TML_DEBUG_LN("[DEREF_ASSIGN] operand is NamedType: " << named.name);

                    // Handle MutexGuard[T] - deref_mut writes through mutex.data
                    if (named.name == "MutexGuard" && !named.type_args.empty()) {
                        TML_DEBUG_LN("[DEREF_ASSIGN] MutexGuard detected");

                        // Get pointer to the MutexGuard (not the value)
                        std::string guard_ptr;
                        if (unary.operand->is<parser::IdentExpr>()) {
                            const auto& ident = unary.operand->as<parser::IdentExpr>();
                            auto it = locals_.find(ident.name);
                            if (it != locals_.end()) {
                                guard_ptr = it->second.reg;
                            }
                        }

                        if (!guard_ptr.empty()) {
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
                            emit_line("  " + mutex_field_ptr + " = getelementptr " + guard_type +
                                      ", ptr " + guard_ptr + ", i32 0, i32 0");

                            // Load the mutex pointer
                            std::string mutex_ptr = fresh_reg();
                            emit_line("  " + mutex_ptr + " = load ptr, ptr " + mutex_field_ptr);

                            // GEP to get data field (field 0) of Mutex
                            std::string data_ptr = fresh_reg();
                            emit_line("  " + data_ptr + " = getelementptr " + mutex_type +
                                      ", ptr " + mutex_ptr + ", i32 0, i32 0");

                            // Store the value
                            inner_llvm_type = llvm_type_from_semantic(concrete_inner);
                            emit_line("  store " + inner_llvm_type + " " + right + ", ptr " +
                                      data_ptr);
                            last_expr_type_ = inner_llvm_type;
                            return right;
                        }
                    }
                }

                // Get the pointer (not the dereferenced value!)
                std::string ptr = gen_expr(*unary.operand);

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
                    } else if (operand_type->is<types::NamedType>()) {
                        // Handle TML's Ptr[T] type
                        const auto& named = operand_type->as<types::NamedType>();
                        if ((named.name == "Ptr" || named.name == "RawPtr") &&
                            !named.type_args.empty()) {
                            inner_llvm_type = llvm_type_from_semantic(named.type_args[0]);
                        }
                    }
                }

                emit_line("  store " + inner_llvm_type + " " + right + ", ptr " + ptr);
                last_expr_type_ = inner_llvm_type; // Assignment returns the assigned value's type
            }
        } else if (bin.left->is<parser::FieldExpr>()) {
            // Field assignment: obj.field = value or this.field = value or ClassName.static = value
            const auto& field = bin.left->as<parser::FieldExpr>();

            // Check for static field assignment first
            if (field.object->is<parser::IdentExpr>()) {
                const auto& ident = field.object->as<parser::IdentExpr>();
                std::string static_key = ident.name + "." + field.field;
                auto static_it = static_fields_.find(static_key);
                if (static_it != static_fields_.end()) {
                    // Store to global static field
                    emit_line("  store " + static_it->second.type + " " + right + ", ptr " +
                              static_it->second.global_name);
                    return right; // Return the assigned value
                }
            }

            std::string struct_type;
            std::string struct_ptr;

            if (field.object->is<parser::IdentExpr>()) {
                const auto& ident = field.object->as<parser::IdentExpr>();
                auto it = locals_.find(ident.name);
                if (it != locals_.end()) {
                    struct_type = it->second.type;
                    struct_ptr = it->second.reg;

                    // Special handling for 'this' in impl methods
                    if (ident.name == "this" && !current_impl_type_.empty()) {
                        struct_type = "%struct." + current_impl_type_;
                    }

                    // Handle ref types - resolve the actual struct type from semantic type
                    // This fixes chained field assignment on ref parameters
                    if (struct_type == "ptr" && it->second.semantic_type) {
                        types::TypePtr sem_type = it->second.semantic_type;
                        if (sem_type->is<types::RefType>()) {
                            const auto& ref = sem_type->as<types::RefType>();
                            types::TypePtr resolved_inner = ref.inner;
                            if (!current_type_subs_.empty()) {
                                resolved_inner =
                                    apply_type_substitutions(ref.inner, current_type_subs_);
                            }
                            struct_type = llvm_type_from_semantic(resolved_inner);
                            // Load the pointer from the alloca
                            std::string loaded_ptr = fresh_reg();
                            emit_line("  " + loaded_ptr + " = load ptr, ptr " + struct_ptr);
                            struct_ptr = loaded_ptr;
                        } else if (sem_type->is<types::PtrType>()) {
                            const auto& ptr = sem_type->as<types::PtrType>();
                            types::TypePtr resolved_inner = ptr.inner;
                            if (!current_type_subs_.empty()) {
                                resolved_inner =
                                    apply_type_substitutions(ptr.inner, current_type_subs_);
                            }
                            struct_type = llvm_type_from_semantic(resolved_inner);
                            // Load the pointer from the alloca
                            std::string loaded_ptr = fresh_reg();
                            emit_line("  " + loaded_ptr + " = load ptr, ptr " + struct_ptr);
                            struct_ptr = loaded_ptr;
                        }
                    }
                }
            } else if (field.object->is<parser::FieldExpr>()) {
                // Chained field assignment: outer.inner.value = x
                // or deeper: app.settings.config.count = x
                // Collect all field access steps, then traverse from root

                // Collect the chain of field accesses (excluding the final field which is handled
                // separately) Start with the intermediate fields from field.object
                std::vector<std::string> field_chain;
                const parser::Expr* current = field.object.get();

                while (current->is<parser::FieldExpr>()) {
                    const auto& fe = current->as<parser::FieldExpr>();
                    field_chain.push_back(fe.field);
                    current = fe.object.get();
                }

                // Now 'current' should be an IdentExpr (the root variable)
                // and field_chain contains intermediate fields in reverse order
                std::reverse(field_chain.begin(), field_chain.end());

                if (current->is<parser::IdentExpr>()) {
                    const auto& ident = current->as<parser::IdentExpr>();
                    std::string current_type;
                    std::string current_ptr;

                    // Special handling for 'this' in impl methods
                    if (ident.name == "this" && !current_impl_type_.empty()) {
                        current_type = "%struct." + current_impl_type_;
                        current_ptr = "%this";
                    } else {
                        auto it = locals_.find(ident.name);
                        if (it != locals_.end()) {
                            current_type = it->second.type;
                            current_ptr = it->second.reg;

                            // Handle ref types - resolve the actual struct type
                            if (current_type == "ptr" && it->second.semantic_type) {
                                types::TypePtr sem_type = it->second.semantic_type;
                                if (sem_type->is<types::RefType>()) {
                                    const auto& ref = sem_type->as<types::RefType>();
                                    types::TypePtr resolved_inner = ref.inner;
                                    if (!current_type_subs_.empty()) {
                                        resolved_inner =
                                            apply_type_substitutions(ref.inner, current_type_subs_);
                                    }
                                    current_type = llvm_type_from_semantic(resolved_inner);
                                    std::string loaded_ptr = fresh_reg();
                                    emit_line("  " + loaded_ptr + " = load ptr, ptr " +
                                              current_ptr);
                                    current_ptr = loaded_ptr;
                                } else if (sem_type->is<types::PtrType>()) {
                                    const auto& ptr = sem_type->as<types::PtrType>();
                                    types::TypePtr resolved_inner = ptr.inner;
                                    if (!current_type_subs_.empty()) {
                                        resolved_inner =
                                            apply_type_substitutions(ptr.inner, current_type_subs_);
                                    }
                                    current_type = llvm_type_from_semantic(resolved_inner);
                                    std::string loaded_ptr = fresh_reg();
                                    emit_line("  " + loaded_ptr + " = load ptr, ptr " +
                                              current_ptr);
                                    current_ptr = loaded_ptr;
                                }
                            }
                        }
                    }

                    // Traverse the field chain with GEPs
                    // All fields in field_chain are intermediate - the final field is field.field
                    if (!current_type.empty() && !current_ptr.empty()) {
                        for (size_t i = 0; i < field_chain.size(); ++i) {
                            const std::string& fname = field_chain[i];
                            std::string type_name = current_type;
                            if (type_name.starts_with("%struct.")) {
                                type_name = type_name.substr(8);
                            }

                            int field_idx = get_field_index(type_name, fname);
                            std::string field_type = get_field_type(type_name, fname);

                            std::string next_ptr = fresh_reg();
                            emit_line("  " + next_ptr + " = getelementptr " + current_type +
                                      ", ptr " + current_ptr + ", i32 0, i32 " +
                                      std::to_string(field_idx));

                            current_ptr = next_ptr;
                            current_type = field_type;
                        }

                        struct_type = current_type;
                        struct_ptr = current_ptr;
                    }
                }
            } else if (field.object->is<parser::UnaryExpr>()) {
                // Handle (*ptr).field = value pattern
                const auto& unary = field.object->as<parser::UnaryExpr>();
                if (unary.op == parser::UnaryOp::Deref) {
                    // Get the pointer being dereferenced
                    struct_ptr = gen_expr(*unary.operand);

                    // Get the pointee (struct) type from the operand's type
                    // IMPORTANT: Don't use last_semantic_type_ here - it's from the RHS!
                    // We need to infer the type of the operand specifically.
                    types::TypePtr operand_type = infer_expr_type(*unary.operand);

                    if (operand_type) {
                        // Extract the inner (pointee) type from pointer/ref types
                        types::TypePtr inner_type;
                        if (operand_type->is<types::PtrType>()) {
                            inner_type = operand_type->as<types::PtrType>().inner;
                        } else if (operand_type->is<types::RefType>()) {
                            inner_type = operand_type->as<types::RefType>().inner;
                        }

                        // Apply type substitutions for generic contexts (e.g., T -> I32)
                        // This is critical for imported generic types like Shared[T]
                        if (inner_type && !current_type_subs_.empty()) {
                            inner_type = apply_type_substitutions(inner_type, current_type_subs_);
                        }

                        if (inner_type) {
                            if (inner_type->is<types::NamedType>()) {
                                const auto& named_inner = inner_type->as<types::NamedType>();
                                if (!named_inner.type_args.empty()) {
                                    // Generic struct: ensure instantiation and field registration
                                    std::string mangled = require_struct_instantiation(
                                        named_inner.name, named_inner.type_args);
                                    struct_type = "%struct." + mangled;
                                } else {
                                    struct_type = "%struct." + named_inner.name;
                                }
                            } else if (inner_type->is<types::ClassType>()) {
                                struct_type = "%class." + inner_type->as<types::ClassType>().name;
                            } else {
                                struct_type = llvm_type_from_semantic(inner_type);
                            }
                        }

                        if (struct_type.empty() && operand_type->is<types::NamedType>()) {
                            // Handle Ptr[T] as NamedType with name "Ptr" and type_args
                            const auto& named = operand_type->as<types::NamedType>();
                            if (named.name == "Ptr" && !named.type_args.empty()) {
                                // Get the inner type (first type arg)
                                struct_type = llvm_type_from_semantic(named.type_args[0]);
                            }
                            // Handle smart pointer types that implement DerefMut
                            // For (*guard).field = value where guard is MutexGuard[T]
                            static const std::unordered_set<std::string> deref_mut_types = {
                                "Arc",
                                "Box",
                                "Heap",
                                "Rc",
                                "Shared",
                                "Weak",
                                "MutexGuard",
                                "RwLockReadGuard",
                                "RwLockWriteGuard",
                                "Ref",
                                "RefMut",
                            };
                            if (deref_mut_types.count(named.name) > 0 && !named.type_args.empty()) {
                                // Get the inner type (first type arg)
                                types::TypePtr deref_inner_type = named.type_args[0];
                                if (!current_type_subs_.empty()) {
                                    deref_inner_type = apply_type_substitutions(deref_inner_type,
                                                                                current_type_subs_);
                                }
                                struct_type = llvm_type_from_semantic(deref_inner_type);
                                TML_DEBUG_LN("[FIELD_ASSIGN] Smart pointer "
                                             << named.name << " deref to " << struct_type);

                                // For MutexGuard specifically, navigate to mutex->data
                                if (named.name == "MutexGuard") {
                                    // struct_ptr is currently the MutexGuard VALUE (not a pointer!)
                                    // We need to store it to a temp alloca and navigate through it

                                    // Get mangled type names
                                    // Use require_struct_instantiation to handle UNRESOLVED cases
                                    std::string guard_mangled = require_struct_instantiation(
                                        "MutexGuard",
                                        std::vector<types::TypePtr>{deref_inner_type});
                                    std::string mutex_mangled = require_struct_instantiation(
                                        "Mutex", std::vector<types::TypePtr>{deref_inner_type});
                                    std::string guard_llvm_type = "%struct." + guard_mangled;
                                    std::string mutex_llvm_type = "%struct." + mutex_mangled;

                                    // Store MutexGuard value to temp alloca
                                    std::string temp_alloca = fresh_reg();
                                    emit_line("  " + temp_alloca + " = alloca " + guard_llvm_type);
                                    emit_line("  store " + guard_llvm_type + " " + struct_ptr +
                                              ", ptr " + temp_alloca);

                                    // GEP to mutex field (field 0) of MutexGuard
                                    std::string mutex_field_ptr = fresh_reg();
                                    emit_line("  " + mutex_field_ptr + " = getelementptr " +
                                              guard_llvm_type + ", ptr " + temp_alloca +
                                              ", i32 0, i32 0");

                                    // Load the mutex pointer
                                    std::string mutex_ptr = fresh_reg();
                                    emit_line("  " + mutex_ptr + " = load ptr, ptr " +
                                              mutex_field_ptr);

                                    // GEP to data field (field 0) of Mutex
                                    std::string data_ptr = fresh_reg();
                                    emit_line("  " + data_ptr + " = getelementptr " +
                                              mutex_llvm_type + ", ptr " + mutex_ptr +
                                              ", i32 0, i32 0");

                                    // For field access, we need the pointer to the data
                                    // If deref_inner_type is Ptr[T], load the pointer value
                                    // Otherwise, data_ptr points directly to the struct
                                    if (deref_inner_type->is<types::NamedType>()) {
                                        const auto& inner_named =
                                            deref_inner_type->as<types::NamedType>();
                                        if (inner_named.name == "Ptr" &&
                                            !inner_named.type_args.empty()) {
                                            // Load the Ptr value
                                            std::string ptr_val = fresh_reg();
                                            emit_line("  " + ptr_val + " = load ptr, ptr " +
                                                      data_ptr);
                                            struct_ptr = ptr_val;
                                            // struct_type is already set to inner of Ptr
                                            struct_type =
                                                llvm_type_from_semantic(inner_named.type_args[0]);
                                            TML_DEBUG_LN("[FIELD_ASSIGN] MutexGuard[Ptr[T]] "
                                                         "navigated to data ptr: "
                                                         << struct_ptr);
                                        } else {
                                            struct_ptr = data_ptr;
                                        }
                                    } else {
                                        struct_ptr = data_ptr;
                                    }
                                }
                                // For Arc specifically, navigate to ptr->data
                                // Arc[T] { ptr: Ptr[ArcInner[T]] }
                                // ArcInner[T] { strong, weak, data: T }
                                else if (named.name == "Arc") {
                                    // Get mangled type names
                                    // Use require_struct_instantiation to handle UNRESOLVED cases
                                    std::string arc_mangled = require_struct_instantiation(
                                        "Arc", std::vector<types::TypePtr>{deref_inner_type});
                                    std::string inner_mangled = require_struct_instantiation(
                                        "ArcInner", std::vector<types::TypePtr>{deref_inner_type});
                                    std::string arc_llvm_type = "%struct." + arc_mangled;
                                    std::string inner_llvm_type = "%struct." + inner_mangled;

                                    // Store Arc value to temp alloca
                                    std::string temp_alloca = fresh_reg();
                                    emit_line("  " + temp_alloca + " = alloca " + arc_llvm_type);
                                    emit_line("  store " + arc_llvm_type + " " + struct_ptr +
                                              ", ptr " + temp_alloca);

                                    // GEP to ptr field (field 0) of Arc
                                    std::string ptr_field_ptr = fresh_reg();
                                    emit_line("  " + ptr_field_ptr + " = getelementptr " +
                                              arc_llvm_type + ", ptr " + temp_alloca +
                                              ", i32 0, i32 0");

                                    // Load the ArcInner pointer
                                    std::string inner_ptr = fresh_reg();
                                    emit_line("  " + inner_ptr + " = load ptr, ptr " +
                                              ptr_field_ptr);

                                    // GEP to data field (field 2) of ArcInner
                                    std::string data_ptr = fresh_reg();
                                    emit_line("  " + data_ptr + " = getelementptr " +
                                              inner_llvm_type + ", ptr " + inner_ptr +
                                              ", i32 0, i32 2");

                                    // For field access, we need the pointer to the data
                                    if (deref_inner_type->is<types::NamedType>()) {
                                        const auto& inner_named =
                                            deref_inner_type->as<types::NamedType>();
                                        if (inner_named.name == "Ptr" &&
                                            !inner_named.type_args.empty()) {
                                            // Load the Ptr value
                                            std::string ptr_val = fresh_reg();
                                            emit_line("  " + ptr_val + " = load ptr, ptr " +
                                                      data_ptr);
                                            struct_ptr = ptr_val;
                                            struct_type =
                                                llvm_type_from_semantic(inner_named.type_args[0]);
                                            TML_DEBUG_LN("[FIELD_ASSIGN] Arc[Ptr[T]] "
                                                         "navigated to data ptr: "
                                                         << struct_ptr);
                                        } else {
                                            struct_ptr = data_ptr;
                                        }
                                    } else {
                                        struct_ptr = data_ptr;
                                    }
                                }
                            }
                        }
                    }
                }
            }

            if (!struct_type.empty() && !struct_ptr.empty()) {
                // If struct_type is ptr (e.g., for mut ref parameters), resolve the inner type
                // and load the pointer from the alloca first
                TML_DEBUG_LN("[FIELD_ASSIGN] struct_type=" << struct_type
                                                           << " struct_ptr=" << struct_ptr
                                                           << " field=" << field.field);
                if (struct_type == "ptr") {
                    types::TypePtr semantic_type = infer_expr_type(*field.object);
                    TML_DEBUG_LN(
                        "[FIELD_ASSIGN] semantic_type="
                        << (semantic_type ? types::type_to_string(semantic_type) : "null"));
                    if (semantic_type) {
                        if (semantic_type->is<types::RefType>()) {
                            const auto& ref = semantic_type->as<types::RefType>();
                            struct_type = llvm_type_from_semantic(ref.inner);
                            // struct_ptr points to an alloca containing a pointer to the struct
                            // We need to load the pointer first
                            std::string loaded_ptr = fresh_reg();
                            emit_line("  " + loaded_ptr + " = load ptr, ptr " + struct_ptr);
                            struct_ptr = loaded_ptr;
                        } else if (semantic_type->is<types::PtrType>()) {
                            const auto& ptr = semantic_type->as<types::PtrType>();
                            struct_type = llvm_type_from_semantic(ptr.inner);
                            // Same - load the pointer from the alloca
                            std::string loaded_ptr = fresh_reg();
                            emit_line("  " + loaded_ptr + " = load ptr, ptr " + struct_ptr);
                            struct_ptr = loaded_ptr;
                        } else if (semantic_type->is<types::ClassType>()) {
                            const auto& cls = semantic_type->as<types::ClassType>();
                            struct_type = "%class." + cls.name;
                            // For class types, 'this' is already a direct pointer - no load needed
                        } else if (semantic_type->is<types::NamedType>()) {
                            // Handle Ptr[T] - struct_ptr is the pointer value, get inner type
                            const auto& named = semantic_type->as<types::NamedType>();
                            if (named.name == "Ptr" && !named.type_args.empty()) {
                                struct_type = llvm_type_from_semantic(named.type_args[0]);
                                TML_DEBUG_LN("[FIELD_ASSIGN] Ptr[T] inner type: " << struct_type);
                                // struct_ptr is already the pointer value - no load needed
                            } else {
                                struct_type = llvm_type_from_semantic(semantic_type);
                            }
                        } else {
                            struct_type = llvm_type_from_semantic(semantic_type);
                        }
                    }
                }

                // Get the struct type name for field lookup
                std::string type_name = struct_type;
                // Strip pointer suffix if present
                if (type_name.ends_with("*")) {
                    type_name = type_name.substr(0, type_name.length() - 1);
                }
                // Strip %struct. or %class. prefix
                if (type_name.starts_with("%struct.")) {
                    type_name = type_name.substr(8);
                } else if (type_name.starts_with("%class.")) {
                    type_name = type_name.substr(7);
                }

                // Check if this is a property setter call
                std::string prop_key = type_name + "." + field.field;
                auto prop_it = class_properties_.find(prop_key);
                if (prop_it != class_properties_.end() && prop_it->second.has_setter) {
                    // Property assignment - call setter method instead of direct field store
                    const auto& prop_info = prop_it->second;
                    std::string setter_name =
                        "@tml_" + get_suite_prefix() + type_name + "_set_" + prop_info.name;

                    if (prop_info.is_static) {
                        // Static property setter - no 'this' parameter
                        emit_line("  call void " + setter_name + "(" + prop_info.llvm_type + " " +
                                  right + ")");
                    } else {
                        // Instance property setter - pass 'this' pointer and value
                        emit_line("  call void " + setter_name + "(ptr " + struct_ptr + ", " +
                                  prop_info.llvm_type + " " + right + ")");
                    }
                    return right;
                }

                // For class fields, use the class type without pointer suffix
                std::string gep_type = struct_type;
                if (gep_type.ends_with("*")) {
                    gep_type = gep_type.substr(0, gep_type.length() - 1);
                }

                // Get field pointer
                int field_idx = get_field_index(type_name, field.field);
                std::string field_type = get_field_type(type_name, field.field);

                // SIMD vector field assignment — load+insertelement+store
                if (is_simd_type(type_name)) {
                    const auto& info = simd_types_.at(type_name);
                    std::string vec_type = simd_vec_type_str(info);
                    // Load current vector
                    std::string old_vec = fresh_reg();
                    emit_line("  " + old_vec + " = load " + vec_type + ", ptr " + struct_ptr);
                    // Insert new element
                    std::string new_vec = fresh_reg();
                    emit_line("  " + new_vec + " = insertelement " + vec_type + " " + old_vec +
                              ", " + info.element_llvm_type + " " + right + ", i32 " +
                              std::to_string(field_idx));
                    // Store updated vector
                    emit_line("  store " + vec_type + " " + new_vec + ", ptr " + struct_ptr);
                } else {
                    std::string field_ptr = fresh_reg();
                    emit_line("  " + field_ptr + " = getelementptr " + gep_type + ", ptr " +
                              struct_ptr + ", i32 0, i32 " + std::to_string(field_idx));

                    // Store value to field
                    emit_line("  store " + field_type + " " + right + ", ptr " + field_ptr);
                }
            }
        } else if (bin.left->is<parser::PathExpr>()) {
            // PathExpr assignment: Counter::count = value (static field via :: syntax)
            const auto& path = bin.left->as<parser::PathExpr>();
            if (path.path.segments.size() == 2) {
                std::string class_name = path.path.segments[0];
                std::string field_name = path.path.segments[1];
                std::string static_key = class_name + "." + field_name;
                auto static_it = static_fields_.find(static_key);
                if (static_it != static_fields_.end()) {
                    // Store to global static field
                    emit_line("  store " + static_it->second.type + " " + right + ", ptr " +
                              static_it->second.global_name);
                    return right;
                }
            }
        } else if (bin.left->is<parser::IndexExpr>()) {
            // Array index assignment: arr[i] = value
            const auto& idx_expr = bin.left->as<parser::IndexExpr>();

            // Get the array pointer
            std::string arr_ptr;
            std::string arr_type;
            if (idx_expr.object->is<parser::IdentExpr>()) {
                const auto& ident = idx_expr.object->as<parser::IdentExpr>();
                auto it = locals_.find(ident.name);
                if (it != locals_.end()) {
                    arr_ptr = it->second.reg;
                    arr_type = it->second.type;
                }
            }

            if (!arr_ptr.empty()) {
                // Generate index
                std::string idx = gen_expr(*idx_expr.index);
                std::string idx_i64 = fresh_reg();
                if (last_expr_type_ == "i64") {
                    idx_i64 = idx;
                } else {
                    emit_line("  " + idx_i64 + " = sext " + last_expr_type_ + " " + idx +
                              " to i64");
                }

                // Get element type from array type
                // Array type is like "[5 x i32]", we need "i32"
                std::string elem_type = "i32"; // default
                types::TypePtr semantic_type = infer_expr_type(*idx_expr.object);
                if (semantic_type && semantic_type->is<types::ArrayType>()) {
                    const auto& arr = semantic_type->as<types::ArrayType>();
                    elem_type = llvm_type_from_semantic(arr.element);
                }

                // Get element pointer
                std::string elem_ptr = fresh_reg();
                emit_line("  " + elem_ptr + " = getelementptr " + arr_type + ", ptr " + arr_ptr +
                          ", i64 0, i64 " + idx_i64);

                // Store value to element
                emit_line("  store " + elem_type + " " + right + ", ptr " + elem_ptr);
            }
        }
        return right;
    }

    // Handle compound assignment operators (+=, -=, *=, /=, %=, etc.)
    if (bin.op >= parser::BinaryOp::AddAssign && bin.op <= parser::BinaryOp::ShrAssign) {
        if (bin.left->is<parser::IdentExpr>()) {
            const auto& ident = bin.left->as<parser::IdentExpr>();
            auto it = locals_.find(ident.name);
            if (it != locals_.end()) {
                const VarInfo& var = it->second;

                // Load current value
                std::string current = fresh_reg();
                emit_line("  " + current + " = load " + var.type + ", ptr " + var.reg);

                // Generate right operand
                std::string right = gen_expr(*bin.right);

                // Perform the operation
                std::string result = fresh_reg();
                std::string op_type = var.type;
                bool is_float = (op_type == "double" || op_type == "float");

                switch (bin.op) {
                case parser::BinaryOp::AddAssign:
                    if (is_float)
                        emit_line("  " + result + " = fadd " + op_type + " " + current + ", " +
                                  right);
                    else
                        emit_line("  " + result + " = add nsw " + op_type + " " + current + ", " +
                                  right);
                    break;
                case parser::BinaryOp::SubAssign:
                    if (is_float)
                        emit_line("  " + result + " = fsub " + op_type + " " + current + ", " +
                                  right);
                    else
                        emit_line("  " + result + " = sub nsw " + op_type + " " + current + ", " +
                                  right);
                    break;
                case parser::BinaryOp::MulAssign:
                    if (is_float)
                        emit_line("  " + result + " = fmul " + op_type + " " + current + ", " +
                                  right);
                    else
                        emit_line("  " + result + " = mul nsw " + op_type + " " + current + ", " +
                                  right);
                    break;
                case parser::BinaryOp::DivAssign:
                    if (is_float)
                        emit_line("  " + result + " = fdiv " + op_type + " " + current + ", " +
                                  right);
                    else
                        emit_line("  " + result + " = sdiv " + op_type + " " + current + ", " +
                                  right);
                    break;
                case parser::BinaryOp::ModAssign:
                    emit_line("  " + result + " = srem " + op_type + " " + current + ", " + right);
                    break;
                case parser::BinaryOp::BitAndAssign:
                    emit_line("  " + result + " = and " + op_type + " " + current + ", " + right);
                    break;
                case parser::BinaryOp::BitOrAssign:
                    emit_line("  " + result + " = or " + op_type + " " + current + ", " + right);
                    break;
                case parser::BinaryOp::BitXorAssign:
                    emit_line("  " + result + " = xor " + op_type + " " + current + ", " + right);
                    break;
                case parser::BinaryOp::ShlAssign:
                    emit_line("  " + result + " = shl " + op_type + " " + current + ", " + right);
                    break;
                case parser::BinaryOp::ShrAssign:
                    emit_line("  " + result + " = ashr " + op_type + " " + current + ", " + right);
                    break;
                default:
                    result = current;
                    break;
                }

                // Store result back
                emit_line("  store " + var.type + " " + result + ", ptr " + var.reg);
                last_expr_type_ = var.type;
                return result;
            }
        }
        report_error("Compound assignment requires a variable on the left side", bin.span, "C003");
        return "0";
    }

    // =========================================================================
    // String Concat Chain Optimization
    // =========================================================================
    // Detect patterns like "a" + "b" + "c" and optimize:
    // 1. If ALL are literals -> concatenate at compile time (zero runtime cost!)
    // 2. Otherwise -> fuse into single allocation (one call instead of N-1)
    if (bin.op == parser::BinaryOp::Add) {
        // Check if operands are strings (we infer types from left/right operands)
        types::TypePtr left_type_check = infer_expr_type(*bin.left);
        bool is_string_add =
            left_type_check && left_type_check->is<types::PrimitiveType>() &&
            left_type_check->as<types::PrimitiveType>().kind == types::PrimitiveKind::Str;

        if (is_string_add) {
            // Collect all strings in the concat chain using a helper lambda
            std::vector<const parser::Expr*> strings;

            // Helper function to collect strings recursively
            std::function<void(const parser::Expr&)> collect_strings = [&](const parser::Expr& e) {
                if (e.is<parser::BinaryExpr>()) {
                    const auto& b = e.as<parser::BinaryExpr>();
                    if (b.op == parser::BinaryOp::Add) {
                        // Check if this binary op is also a string concat
                        types::TypePtr t = infer_expr_type(e);
                        if (t && t->is<types::PrimitiveType>() &&
                            t->as<types::PrimitiveType>().kind == types::PrimitiveKind::Str) {
                            // This is also a string concat - recurse
                            collect_strings(*b.left);
                            collect_strings(*b.right);
                            return;
                        }
                    }
                }
                // Not a concat - this is a leaf string
                strings.push_back(&e);
            };

            // Start collection from left and right operands
            collect_strings(*bin.left);
            collect_strings(*bin.right);

            // =========================================================================
            // COMPILE-TIME STRING LITERAL CONCATENATION
            // =========================================================================
            // If ALL strings are literals, concatenate at compile time!
            // This makes "Hello" + " " + "World" + "!" essentially free.
            bool all_literals = true;
            std::string concatenated;
            for (const auto* s : strings) {
                if (s->is<parser::LiteralExpr>()) {
                    const auto& lit = s->as<parser::LiteralExpr>();
                    if (lit.token.kind == lexer::TokenKind::StringLiteral) {
                        concatenated += std::string(lit.token.string_value().value);
                        continue;
                    }
                }
                all_literals = false;
                break;
            }

            if (all_literals && strings.size() >= 2) {
                // Emit the concatenated string as a constant - ZERO RUNTIME COST!
                std::string const_name = add_string_literal(concatenated);
                last_expr_type_ = "ptr";
                return const_name;
            }
            // =========================================================================

            // =========================================================================
            // INLINE STRING CONCAT CODEGEN
            // =========================================================================
            // Generate inline LLVM IR for string concatenation to avoid FFI overhead.
            // For each string:
            //   - If literal: use known length (compile-time constant)
            //   - If runtime: call strlen
            // Then: malloc(total_len + 1), memcpy each string, null terminate
            //
            // This saves ~5-10ns per concat by avoiding the function call overhead.
            if (strings.size() >= 2 && strings.size() <= 4) {
                // Collect string values and their lengths
                struct StringInfo {
                    std::string value;  // LLVM register or constant
                    std::string len;    // Length (constant or register)
                    bool is_literal;    // True if compile-time known
                    size_t literal_len; // Length if literal
                };
                std::vector<StringInfo> infos;
                infos.reserve(strings.size());

                size_t total_literal_len = 0;
                bool has_runtime_strings = false;

                for (const auto* s : strings) {
                    StringInfo info;
                    if (s->is<parser::LiteralExpr>()) {
                        const auto& lit = s->as<parser::LiteralExpr>();
                        if (lit.token.kind == lexer::TokenKind::StringLiteral) {
                            std::string str_val(lit.token.string_value().value);
                            info.value = add_string_literal(str_val);
                            info.literal_len = str_val.size();
                            info.len = std::to_string(str_val.size());
                            info.is_literal = true;
                            total_literal_len += str_val.size();
                            infos.push_back(info);
                            continue;
                        }
                    }
                    // Runtime string - need to call strlen
                    info.value = gen_expr(*s);
                    info.is_literal = false;
                    info.literal_len = 0;
                    has_runtime_strings = true;
                    infos.push_back(info);
                }

                // Calculate lengths for runtime strings
                std::string total_len_reg;
                if (has_runtime_strings) {
                    // Start with known literal length
                    total_len_reg = fresh_reg();
                    emit_line("  " + total_len_reg + " = add i64 0, " +
                              std::to_string(total_literal_len));

                    for (auto& info : infos) {
                        if (!info.is_literal) {
                            // Call strlen for runtime strings
                            std::string len_reg = fresh_reg();
                            emit_line("  " + len_reg + " = call i64 @strlen(ptr " + info.value +
                                      ")");
                            info.len = len_reg;
                            // Add to total
                            std::string new_total = fresh_reg();
                            emit_line("  " + new_total + " = add i64 " + total_len_reg + ", " +
                                      len_reg);
                            total_len_reg = new_total;
                        }
                    }
                } else {
                    // All literals - total is known at compile time
                    total_len_reg = std::to_string(total_literal_len);
                }

                // Allocate buffer: malloc(total_len + 1) for null terminator
                std::string alloc_size = fresh_reg();
                emit_line("  " + alloc_size + " = add i64 " + total_len_reg + ", 1");
                std::string result_ptr = fresh_reg();
                emit_line("  " + result_ptr + " = call ptr @malloc(i64 " + alloc_size + ")");

                // Copy each string using memcpy
                std::string offset = "0";
                bool offset_is_const = true; // Track if offset is a compile-time constant
                size_t const_offset = 0;     // Numeric value if constant

                for (size_t i = 0; i < infos.size(); ++i) {
                    const auto& info = infos[i];
                    // Calculate destination pointer
                    std::string dest_ptr;
                    if (offset == "0") {
                        dest_ptr = result_ptr;
                    } else {
                        dest_ptr = fresh_reg();
                        emit_line("  " + dest_ptr + " = getelementptr i8, ptr " + result_ptr +
                                  ", i64 " + offset);
                    }

                    // Copy the string (memcpy intrinsic)
                    emit_line("  call void @llvm.memcpy.p0.p0.i64(ptr " + dest_ptr + ", ptr " +
                              info.value + ", i64 " + info.len + ", i1 false)");

                    // Update offset for next string
                    if (i < infos.size() - 1) {
                        if (offset_is_const && info.is_literal) {
                            // Both offset and current length are constants - keep as constant
                            const_offset += info.literal_len;
                            offset = std::to_string(const_offset);
                        } else {
                            // Need runtime addition
                            std::string new_offset = fresh_reg();
                            emit_line("  " + new_offset + " = add i64 " + offset + ", " + info.len);
                            offset = new_offset;
                            offset_is_const = false;
                        }
                    }
                }

                // Null terminate
                std::string end_ptr = fresh_reg();
                emit_line("  " + end_ptr + " = getelementptr i8, ptr " + result_ptr + ", i64 " +
                          total_len_reg);
                emit_line("  store i8 0, ptr " + end_ptr);

                last_expr_type_ = "ptr";
                return result_ptr;
            }
            // =========================================================================

            // For 5+ strings, fall through to default two-operand concatenation
        }
    }
    // =========================================================================

    // Delegate to gen_binary_ops for operand evaluation, type coercion,
    // tuple/enum comparisons, and the operator switch (arithmetic, comparison,
    // logical, bitwise). See binary_ops.cpp.
    return gen_binary_ops(bin);
}

} // namespace tml::codegen
