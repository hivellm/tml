TML_MODULE("codegen_x86")

//! # LLVM IR Generator - Method Call Dispatcher
//!
//! This file is the main entry point for method call code generation.
//! It delegates to specialized handlers based on receiver type.
//!
//! ## Dispatch Order
//!
//! 1. Static methods: `Type::method()` → method_static.cpp
//! 2. Primitive methods: `.to_string()`, `.abs()` → method_primitive.cpp
//! 3. Collection methods: `.push()`, `.get()` → method_collection.cpp
//! 4. Slice methods: `.len()`, `.get()` → method_slice.cpp
//! 5. Maybe methods: `.unwrap()`, `.map()` → method_maybe.cpp
//! 6. Outcome methods: `.unwrap()`, `.ok()` → method_outcome.cpp
//! 7. Array methods: `.len()`, `.get()` → method_array.cpp
//! 8. User-defined methods: Look up in impl blocks
//!
//! ## Specialized Files
//!
//! | File                    | Handles                        |
//! |-------------------------|--------------------------------|
//! | method_static.cpp       | `Type::method()` static calls  |
//! | method_primitive.cpp    | Integer, Float, Bool methods   |
//! | method_collection.cpp   | List, HashMap, Buffer methods  |
//! | method_slice.cpp        | Slice, MutSlice methods        |
//! | method_maybe.cpp        | Maybe[T] methods               |
//! | method_outcome.cpp      | Outcome[T,E] methods           |
//! | method_array.cpp        | Array[T; N] methods            |

#include "codegen/llvm/llvm_ir_gen.hpp"
#include "types/module.hpp"

#include <iostream>
#include <unordered_set>

namespace tml::codegen {

// Static helper to parse mangled type strings like "Mutex__I32" into proper TypePtr
// This is used for nested generic type inference and avoids expensive std::function lambdas
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
    if (s == "Usize") {
        auto t = std::make_shared<types::Type>();
        t->kind = types::PrimitiveType{types::PrimitiveKind::U64};
        return t;
    }
    if (s == "Isize") {
        auto t = std::make_shared<types::Type>();
        t->kind = types::PrimitiveType{types::PrimitiveKind::I64};
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

    // Check for pointer prefix (e.g., ptr_ChannelNode__I32 -> Ptr[ChannelNode[I32]])
    // Must be checked BEFORE the __ delim check to properly handle nested types
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

auto LLVMIRGen::gen_method_call(const parser::MethodCallExpr& call) -> std::string {
    // Clear expected literal type context - it should only apply within explicit type annotations
    // (like "let x: F64 = 5") and not leak into method call arguments
    expected_literal_type_.clear();
    expected_literal_is_unsigned_ = false;

    const std::string& method = call.method;
    TML_DEBUG_LN("[METHOD] gen_method_call: " << method << " where_constraints.size="
                                              << current_where_constraints_.size());

    // =========================================================================
    // 1. Static method dispatch (delegated to method_static_dispatch.cpp)
    // =========================================================================
    if (auto r = gen_method_static_dispatch(call, method))
        return *r;

    // =========================================================================
    // 2. Check for array methods first (before generating receiver)
    // =========================================================================
    auto array_result = gen_array_method(call, method);
    if (array_result.has_value()) {
        return *array_result;
    }

    // =========================================================================
    // 2b. Check for SliceType [T] methods (before generating receiver)
    // =========================================================================
    auto slice_type_result = gen_slice_type_method(call, method);
    if (slice_type_result.has_value()) {
        return *slice_type_result;
    }

    // =========================================================================
    // 3. Generate receiver and get receiver pointer
    // =========================================================================
    std::string receiver;
    std::string receiver_ptr;

    // Special handling for FieldExpr receiver: we need the pointer to the field,
    // not a loaded copy, so that mutations inside the method are persisted.
    TML_DEBUG_LN("[METHOD_CALL] receiver is FieldExpr: "
                 << (call.receiver->is<parser::FieldExpr>() ? "yes" : "no"));
    if (call.receiver->is<parser::FieldExpr>()) {
        const auto& field_expr = call.receiver->as<parser::FieldExpr>();

        // Generate the base object expression
        std::string base_ptr;
        types::TypePtr base_type;

        if (field_expr.object->is<parser::IdentExpr>()) {
            const auto& ident = field_expr.object->as<parser::IdentExpr>();
            if (ident.name == "this") {
                base_ptr = "%this";
                // For 'this' in impl blocks, use current_impl_type_ if infer fails
                base_type = infer_expr_type(*field_expr.object);
                if (!base_type && !current_impl_type_.empty()) {
                    // Create a NamedType for current_impl_type_
                    auto result = std::make_shared<types::Type>();
                    result->kind = types::NamedType{current_impl_type_, "", {}};
                    base_type = result;
                }
            } else {
                auto it = locals_.find(ident.name);
                if (it != locals_.end()) {
                    base_ptr = it->second.reg;
                    base_type = it->second.semantic_type;
                }
                if (!base_type) {
                    base_type = infer_expr_type(*field_expr.object);
                }
            }
        } else if (field_expr.object->is<parser::FieldExpr>()) {
            // Handle nested field access: this.inner.field
            // Generate the nested field access - gen_expr will return the loaded value
            // but we need the semantic type to determine the struct type
            std::string nested_val = gen_expr(*field_expr.object);
            base_type = infer_expr_type(*field_expr.object);

            // For struct types, gen_expr returns a loaded value but we need a pointer
            // The field is stored in memory, so we can use the value as if it were a pointer
            // Actually, for struct fields that are themselves structs, gen_field returns
            // a loaded value. We need the pointer to access sub-fields.
            // Re-generate using gen_field_ptr if available, or allocate temp storage
            if (last_expr_type_.starts_with("%struct.") || last_expr_type_ == "ptr") {
                // For pointer types, the loaded value IS the pointer
                if (last_expr_type_ == "ptr") {
                    base_ptr = nested_val;
                } else {
                    // For struct values, we need to store to a temp alloca
                    std::string temp_ptr = fresh_reg();
                    emit_line("  " + temp_ptr + " = alloca " + last_expr_type_);
                    emit_line("  store " + last_expr_type_ + " " + nested_val + ", ptr " +
                              temp_ptr);
                    base_ptr = temp_ptr;
                }
            } else {
                // Primitive field - use value directly but we may need its address for method calls
                base_ptr = nested_val;
            }
        } else if (field_expr.object->is<parser::UnaryExpr>()) {
            // Handle dereferenced pointer field access: (*ptr).field
            const auto& unary = field_expr.object->as<parser::UnaryExpr>();
            if (unary.op == parser::UnaryOp::Deref) {
                // Generate the pointer value - this becomes our base_ptr
                base_ptr = gen_expr(*unary.operand);

                // Infer the pointee type
                types::TypePtr ptr_type = infer_expr_type(*unary.operand);
                if (ptr_type) {
                    if (ptr_type->is<types::PtrType>()) {
                        base_type = ptr_type->as<types::PtrType>().inner;
                    } else if (ptr_type->is<types::RefType>()) {
                        base_type = ptr_type->as<types::RefType>().inner;
                    } else if (ptr_type->is<types::NamedType>()) {
                        // Handle TML's Ptr[T] type (NamedType wrapper)
                        const auto& named = ptr_type->as<types::NamedType>();
                        if ((named.name == "Ptr" || named.name == "RawPtr") &&
                            !named.type_args.empty()) {
                            base_type = named.type_args[0];
                            TML_DEBUG_LN("[FIELD_MUTATION] NamedType Ptr inner: "
                                         << types::type_to_string(base_type));
                        }
                    }

                    // Apply type substitutions for generic types
                    if (base_type && !current_type_subs_.empty()) {
                        base_type = apply_type_substitutions(base_type, current_type_subs_);
                    }
                }
            }
        }

        if (!base_ptr.empty() && base_type) {
            TML_DEBUG_LN("[FIELD_MUTATION] base_type exists: " << (base_type ? "yes" : "no"));
            if (base_type) {
                TML_DEBUG_LN("[FIELD_MUTATION] base_type is NamedType: "
                             << (base_type->is<types::NamedType>() ? "yes" : "no"));
            }
            if (base_type->is<types::NamedType>()) {
                const auto& base_named = base_type->as<types::NamedType>();
                std::string base_type_name = base_named.name;

                // Get the mangled struct type name if it has type args
                std::string struct_type_name = base_type_name;
                if (!base_named.type_args.empty()) {
                    // Ensure generic struct is instantiated so fields are registered
                    // Use the return value which handles UNRESOLVED cases properly
                    struct_type_name =
                        require_struct_instantiation(base_type_name, base_named.type_args);
                }
                std::string llvm_struct_type = "%struct." + struct_type_name;

                // Check for auto-deref: if field not found on base type but base type implements
                // Deref
                types::TypePtr deref_target = get_deref_target_type(base_type);
                if (deref_target && !struct_has_field(struct_type_name, field_expr.field)) {
                    TML_DEBUG_LN("[METHOD_CALL] Auto-deref for FieldExpr: "
                                 << base_type_name << " -> "
                                 << types::type_to_string(deref_target));

                    // Generate deref code for Arc[T] or Box[T]
                    auto sep_pos = base_type_name.find("__");
                    std::string smart_ptr_name = (sep_pos != std::string::npos)
                                                     ? base_type_name.substr(0, sep_pos)
                                                     : base_type_name;

                    if (smart_ptr_name == "Arc" || smart_ptr_name == "Shared" ||
                        smart_ptr_name == "Rc") {
                        // Arc layout: { ptr: Ptr[ArcInner[T]] }
                        // ArcInner layout: { strong, weak, data }
                        std::string arc_ptr_field = fresh_reg();
                        emit_line("  " + arc_ptr_field + " = getelementptr " + llvm_struct_type +
                                  ", ptr " + base_ptr + ", i32 0, i32 0");
                        std::string inner_ptr = fresh_reg();
                        emit_line("  " + inner_ptr + " = load ptr, ptr " + arc_ptr_field);

                        // Get ArcInner type
                        std::string arc_inner_mangled = mangle_struct_name(
                            "ArcInner", std::vector<types::TypePtr>{deref_target});
                        std::string arc_inner_type = "%struct." + arc_inner_mangled;

                        // GEP to data field (index 2)
                        std::string data_ptr = fresh_reg();
                        emit_line("  " + data_ptr + " = getelementptr " + arc_inner_type +
                                  ", ptr " + inner_ptr + ", i32 0, i32 2");

                        // Update base_ptr and types to point to inner struct
                        base_ptr = data_ptr;
                        if (deref_target->is<types::NamedType>()) {
                            const auto& inner_named = deref_target->as<types::NamedType>();
                            if (!inner_named.type_args.empty()) {
                                // Use return value to handle UNRESOLVED cases
                                struct_type_name = require_struct_instantiation(
                                    inner_named.name, inner_named.type_args);
                            } else {
                                struct_type_name = inner_named.name;
                            }
                            llvm_struct_type = "%struct." + struct_type_name;
                        }
                    } else if (smart_ptr_name == "Box" || smart_ptr_name == "Heap") {
                        // Box layout: { ptr: Ptr[T] }
                        std::string box_ptr_field = fresh_reg();
                        emit_line("  " + box_ptr_field + " = getelementptr " + llvm_struct_type +
                                  ", ptr " + base_ptr + ", i32 0, i32 0");
                        std::string inner_ptr = fresh_reg();
                        emit_line("  " + inner_ptr + " = load ptr, ptr " + box_ptr_field);

                        // Update base_ptr and types
                        base_ptr = inner_ptr;
                        if (deref_target->is<types::NamedType>()) {
                            const auto& inner_named = deref_target->as<types::NamedType>();
                            if (!inner_named.type_args.empty()) {
                                // Use return value to handle UNRESOLVED cases
                                struct_type_name = require_struct_instantiation(
                                    inner_named.name, inner_named.type_args);
                            } else {
                                struct_type_name = inner_named.name;
                            }
                            llvm_struct_type = "%struct." + struct_type_name;
                        }
                    }
                }

                // Get field index
                int field_idx = get_field_index(struct_type_name, field_expr.field);
                if (field_idx >= 0) {
                    std::string field_type = get_field_type(struct_type_name, field_expr.field);

                    // Generate getelementptr to get pointer to the field
                    std::string field_ptr = fresh_reg();
                    emit_line("  " + field_ptr + " = getelementptr " + llvm_struct_type + ", ptr " +
                              base_ptr + ", i32 0, i32 " + std::to_string(field_idx));

                    // Load the field value for method calls - structs, primitives, and pointers
                    // all need to be loaded from the field pointer before use.
                    // The receiver_ptr is kept for methods that mutate the receiver.
                    receiver_ptr = field_ptr;
                    std::string loaded = fresh_reg();
                    emit_line("  " + loaded + " = load " + field_type + ", ptr " + field_ptr);
                    receiver = loaded;
                    last_expr_type_ = field_type;
                }
            }
        }

        // Fallback if we couldn't get the field pointer
        if (receiver.empty()) {
            receiver = gen_expr(*call.receiver);
        }
    } else {
        receiver = gen_expr(*call.receiver);
        if (call.receiver->is<parser::IdentExpr>()) {
            const auto& ident = call.receiver->as<parser::IdentExpr>();
            auto it = locals_.find(ident.name);
            if (it != locals_.end()) {
                receiver_ptr = it->second.reg;
            } else if (ident.name == "this") {
                // 'this' is an implicit parameter, not in locals_ map
                receiver_ptr = "%this";
            }
        }
    }

    // =========================================================================
    // 4. Get receiver type info
    // =========================================================================
    types::TypePtr receiver_type = infer_expr_type(*call.receiver);

    // For FieldExpr receivers in generic impl blocks, try to get field type from
    // pending_generic_structs_ This handles cases where infer_expr_type returns an incorrect
    // fallback type
    if (call.receiver->is<parser::FieldExpr>()) {
        const auto& field_expr = call.receiver->as<parser::FieldExpr>();
        if (field_expr.object->is<parser::IdentExpr>()) {
            const auto& ident = field_expr.object->as<parser::IdentExpr>();
            if (ident.name == "this" && !current_impl_type_.empty()) {
                // Parse the impl type to get base name and type args
                std::string base_name = current_impl_type_;
                std::vector<types::TypePtr> type_args;

                auto sep_pos = current_impl_type_.find("__");
                if (sep_pos != std::string::npos) {
                    base_name = current_impl_type_.substr(0, sep_pos);

                    // Parse type args from mangled suffix using recursive parser
                    // This properly handles nested types like ptr_ChannelNode__I32 as
                    // Ptr[ChannelNode[I32]] For most generic types (Mutex, MutexGuard, Arc, etc.)
                    // which have a single type parameter, this is the complete type arg.
                    std::string args_str = current_impl_type_.substr(sep_pos + 2);
                    auto parsed_type = parse_mangled_type_string(args_str);
                    if (parsed_type) {
                        type_args.push_back(parsed_type);
                    }
                }

                // Look up the generic struct definition
                auto generic_it = pending_generic_structs_.find(base_name);
                if (generic_it != pending_generic_structs_.end()) {
                    const parser::StructDecl* decl = generic_it->second;

                    // Build type substitution map
                    std::unordered_map<std::string, types::TypePtr> subs;
                    for (size_t i = 0; i < decl->generics.size() && i < type_args.size(); ++i) {
                        subs[decl->generics[i].name] = type_args[i];
                    }

                    // Find the field
                    for (const auto& decl_field : decl->fields) {
                        if (decl_field.name == field_expr.field && decl_field.type) {
                            receiver_type = resolve_parser_type_with_subs(*decl_field.type, subs);
                            break;
                        }
                    }
                }

                // Also try module registry for imported structs
                // Check if receiver_type is valid (not a primitive fallback type like Str)
                bool needs_registry_lookup =
                    !receiver_type ||
                    (receiver_type->is<types::PrimitiveType>() &&
                     receiver_type->as<types::PrimitiveType>().kind == types::PrimitiveKind::Str);
                if (needs_registry_lookup && env_.module_registry()) {
                    const auto& all_modules = env_.module_registry()->get_all_modules();
                    for (const auto& [mod_name, mod] : all_modules) {
                        auto struct_it = mod.structs.find(base_name);
                        if (struct_it != mod.structs.end()) {
                            const auto& struct_def = struct_it->second;

                            // Build type substitution map
                            std::unordered_map<std::string, types::TypePtr> subs;
                            for (size_t i = 0;
                                 i < struct_def.type_params.size() && i < type_args.size(); ++i) {
                                subs[struct_def.type_params[i]] = type_args[i];
                            }

                            // Find the field
                            for (const auto& field : struct_def.fields) {
                                if (field.name == field_expr.field && field.type) {
                                    if (!subs.empty()) {
                                        receiver_type = types::substitute_type(field.type, subs);
                                    } else {
                                        receiver_type = field.type;
                                    }
                                    break;
                                }
                            }
                            if (receiver_type)
                                break;
                        }
                    }
                }
            }
        }
    }

    // Apply type substitutions to the receiver type.
    // This handles both simple type parameters (T -> I32) AND nested generic types
    // like AtomicPtr[Node[T]] -> AtomicPtr[Node[I32]]
    if (receiver_type && !current_type_subs_.empty()) {
        receiver_type = apply_type_substitutions(receiver_type, current_type_subs_);
    }

    // If receiver type is a reference, unwrap it for method dispatch
    // Methods are dispatched on the inner type, not the reference type
    // Track whether we unwrapped a reference, as the receiver value will be a pointer
    // Note: Type substitutions were already applied above, so inner is already concrete
    bool receiver_was_ref = false;
    if (receiver_type && receiver_type->is<types::RefType>()) {
        const auto& ref = receiver_type->as<types::RefType>();
        if (ref.inner) {
            receiver_type = ref.inner;
            receiver_was_ref = true;
        }
    }

    std::string receiver_type_name;
    if (receiver_type) {
        if (receiver_type->is<types::ClassType>()) {
            receiver_type_name = receiver_type->as<types::ClassType>().name;
        } else if (receiver_type->is<types::NamedType>()) {
            receiver_type_name = receiver_type->as<types::NamedType>().name;
        } else if (receiver_type->is<types::PrimitiveType>()) {
            // Convert primitive type to name for method dispatch
            const auto& prim = receiver_type->as<types::PrimitiveType>();
            switch (prim.kind) {
            case types::PrimitiveKind::I8:
                receiver_type_name = "I8";
                break;
            case types::PrimitiveKind::I16:
                receiver_type_name = "I16";
                break;
            case types::PrimitiveKind::I32:
                receiver_type_name = "I32";
                break;
            case types::PrimitiveKind::I64:
                receiver_type_name = "I64";
                break;
            case types::PrimitiveKind::I128:
                receiver_type_name = "I128";
                break;
            case types::PrimitiveKind::U8:
                receiver_type_name = "U8";
                break;
            case types::PrimitiveKind::U16:
                receiver_type_name = "U16";
                break;
            case types::PrimitiveKind::U32:
                receiver_type_name = "U32";
                break;
            case types::PrimitiveKind::U64:
                receiver_type_name = "U64";
                break;
            case types::PrimitiveKind::U128:
                receiver_type_name = "U128";
                break;
            case types::PrimitiveKind::F32:
                receiver_type_name = "F32";
                break;
            case types::PrimitiveKind::F64:
                receiver_type_name = "F64";
                break;
            case types::PrimitiveKind::Bool:
                receiver_type_name = "Bool";
                break;
            case types::PrimitiveKind::Char:
                receiver_type_name = "Char";
                break;
            case types::PrimitiveKind::Str:
                receiver_type_name = "Str";
                break;
            default:
                break;
            }
        }
    }

    // =========================================================================
    // 4a. Inline codegen for comparison methods on primitives (eq/ne/lt/le/gt/ge)
    // =========================================================================
    // These methods from PartialEq/PartialOrd must be handled before any other dispatch
    // because default behavior methods (ne, le, ge) may not have generated LLVM functions,
    // and module registry lookups can produce incorrect parameter types.
    if (receiver_type && receiver_type->is<types::PrimitiveType>() &&
        (method == "eq" || method == "ne" || method == "lt" || method == "le" || method == "gt" ||
         method == "ge") &&
        call.args.size() == 1) {
        const auto& prim = receiver_type->as<types::PrimitiveType>();
        std::string llvm_ty = llvm_type_from_semantic(receiver_type);
        bool is_signed =
            (prim.kind == types::PrimitiveKind::I8 || prim.kind == types::PrimitiveKind::I16 ||
             prim.kind == types::PrimitiveKind::I32 || prim.kind == types::PrimitiveKind::I64 ||
             prim.kind == types::PrimitiveKind::I128);
        bool is_unsigned =
            (prim.kind == types::PrimitiveKind::U8 || prim.kind == types::PrimitiveKind::U16 ||
             prim.kind == types::PrimitiveKind::U32 || prim.kind == types::PrimitiveKind::U64 ||
             prim.kind == types::PrimitiveKind::U128);
        bool is_float =
            (prim.kind == types::PrimitiveKind::F32 || prim.kind == types::PrimitiveKind::F64);
        bool is_bool = (prim.kind == types::PrimitiveKind::Bool);

        if (is_signed || is_unsigned || is_float || is_bool) {
            // Emit type-specific coverage
            std::string prim_name = types::primitive_kind_to_string(prim.kind);
            if (method == "eq" || method == "ne") {
                emit_coverage(prim_name + "::" + method);
            } else {
                // lt, le, gt, ge are PartialOrd defaults
                emit_coverage("PartialOrd::" + method);
            }
            // Load other value from ref parameter
            std::string other_ref = gen_expr(*call.args[0]);
            std::string other = fresh_reg();
            emit_line("  " + other + " = load " + llvm_ty + ", ptr " + other_ref);

            // Get receiver value (may need to load if it was a reference)
            std::string receiver_val = receiver;
            if (receiver_was_ref) {
                receiver_val = fresh_reg();
                emit_line("  " + receiver_val + " = load " + llvm_ty + ", ptr " + receiver);
            }

            std::string result = fresh_reg();
            if (is_float) {
                std::string op;
                if (method == "eq")
                    op = "oeq";
                else if (method == "ne")
                    op = "une";
                else if (method == "lt")
                    op = "olt";
                else if (method == "le")
                    op = "ole";
                else if (method == "gt")
                    op = "ogt";
                else
                    op = "oge";
                emit_line("  " + result + " = fcmp " + op + " " + llvm_ty + " " + receiver_val +
                          ", " + other);
            } else {
                std::string op;
                if (method == "eq")
                    op = "eq";
                else if (method == "ne")
                    op = "ne";
                else if (method == "lt")
                    op = is_signed ? "slt" : "ult";
                else if (method == "le")
                    op = is_signed ? "sle" : "ule";
                else if (method == "gt")
                    op = is_signed ? "sgt" : "ugt";
                else
                    op = is_signed ? "sge" : "uge";
                emit_line("  " + result + " = icmp " + op + " " + llvm_ty + " " + receiver_val +
                          ", " + other);
            }

            last_expr_type_ = "i1";
            return result;
        }
    }

    // =========================================================================
    // 4b. Bounded generic dispatch (delegated to method_generic.cpp)
    // =========================================================================
    if (auto r =
            gen_method_bounded_generic_dispatch(call, method, receiver, receiver_ptr, receiver_type,
                                                receiver_type_name, receiver_was_ref)) {
        return *r;
    }

    // =========================================================================
    // 5. Handle Ptr[T] methods
    // =========================================================================
    if (receiver_type && receiver_type->is<types::PtrType>()) {
        const auto& ptr_type = receiver_type->as<types::PtrType>();
        std::string inner_llvm_type = llvm_type_from_semantic(ptr_type.inner);

        if (method == "read") {
            std::string result = fresh_reg();
            emit_line("  " + result + " = load " + inner_llvm_type + ", ptr " + receiver);
            last_expr_type_ = inner_llvm_type;
            return result;
        }
        if (method == "write") {
            if (call.args.empty()) {
                report_error("Ptr.write() requires a value argument", call.span, "C019");
                return "void";
            }
            std::string val = gen_expr(*call.args[0]);
            emit_line("  store " + inner_llvm_type + " " + val + ", ptr " + receiver);
            return "void";
        }
        if (method == "offset") {
            if (call.args.empty()) {
                report_error("Ptr.offset() requires an offset argument", call.span, "C019");
                return receiver;
            }
            std::string offset = gen_expr(*call.args[0]);
            std::string offset_i64 = fresh_reg();
            emit_line("  " + offset_i64 + " = sext i32 " + offset + " to i64");
            std::string result = fresh_reg();
            emit_line("  " + result + " = getelementptr " + inner_llvm_type + ", ptr " + receiver +
                      ", i64 " + offset_i64);
            last_expr_type_ = "ptr";
            return result;
        }
        if (method == "is_null") {
            std::string result = fresh_reg();
            emit_line("  " + result + " = icmp eq ptr " + receiver + ", null");
            last_expr_type_ = "i1";
            return result;
        }
    }

    // =========================================================================
    // 6. Handle primitive type methods
    // =========================================================================
    auto prim_result = gen_primitive_method(call, receiver, receiver_ptr, receiver_type);
    if (prim_result) {
        return *prim_result;
    }

    // =========================================================================
    // 6b. Handle primitive type behavior methods (see method_prim_behavior.cpp)
    // =========================================================================
    if (auto prim_behavior_result = try_gen_primitive_behavior_method(
            call, receiver, receiver_type, receiver_type_name, receiver_was_ref)) {
        return *prim_behavior_result;
    }

    // =========================================================================
    // 6b. Expand type aliases before method dispatch
    // =========================================================================
    if (receiver_type && receiver_type->is<types::NamedType>()) {
        const auto& pre_named = receiver_type->as<types::NamedType>();
        auto alias_base = env_.lookup_type_alias(pre_named.name);
        std::optional<std::vector<std::string>> alias_generics;
        if (!alias_base && env_.module_registry()) {
            for (const auto& [mod_path, mod] : env_.module_registry()->get_all_modules()) {
                auto it = mod.type_aliases.find(pre_named.name);
                if (it != mod.type_aliases.end()) {
                    alias_base = it->second;
                    auto gen_it = mod.type_alias_generics.find(pre_named.name);
                    if (gen_it != mod.type_alias_generics.end()) {
                        alias_generics = gen_it->second;
                    }
                    break;
                }
            }
        } else if (alias_base) {
            alias_generics = env_.lookup_type_alias_generics(pre_named.name);
        }
        if (alias_base) {
            if (alias_generics && !alias_generics->empty() && !pre_named.type_args.empty()) {
                std::unordered_map<std::string, types::TypePtr> subs;
                for (size_t i = 0; i < alias_generics->size() && i < pre_named.type_args.size();
                     ++i) {
                    subs[(*alias_generics)[i]] = pre_named.type_args[i];
                }
                receiver_type = types::substitute_type(*alias_base, subs);
            } else {
                receiver_type = *alias_base;
            }
        }
    }

    // =========================================================================
    // 7. Handle Ordering enum methods
    // =========================================================================
    if (receiver_type && receiver_type->is<types::NamedType>()) {
        const auto& named = receiver_type->as<types::NamedType>();
        if (named.name == "Ordering") {
            // When receiver is a pointer (e.g., in default method bodies where this is ptr),
            // load the struct value first before extractvalue
            std::string ordering_val = receiver;
            if (last_expr_type_ == "ptr" || receiver.find("%this") != std::string::npos) {
                // Check if receiver is actually a pointer by looking at the local type
                auto it = locals_.find("this");
                if (it != locals_.end() && it->second.type == "ptr" && receiver == "%this") {
                    ordering_val = fresh_reg();
                    emit_line("  " + ordering_val + " = load %struct.Ordering, ptr " + receiver);
                }
            }
            std::string tag_val = fresh_reg();
            emit_line("  " + tag_val + " = extractvalue %struct.Ordering " + ordering_val + ", 0");

            if (method == "is_less") {
                emit_coverage("Ordering::is_less");
                std::string result = fresh_reg();
                emit_line("  " + result + " = icmp eq i32 " + tag_val + ", 0");
                last_expr_type_ = "i1";
                return result;
            }
            if (method == "is_equal") {
                emit_coverage("Ordering::is_equal");
                std::string result = fresh_reg();
                emit_line("  " + result + " = icmp eq i32 " + tag_val + ", 1");
                last_expr_type_ = "i1";
                return result;
            }
            if (method == "is_greater") {
                emit_coverage("Ordering::is_greater");
                std::string result = fresh_reg();
                emit_line("  " + result + " = icmp eq i32 " + tag_val + ", 2");
                last_expr_type_ = "i1";
                return result;
            }
            if (method == "reverse") {
                emit_coverage("Ordering::reverse");
                std::string is_less = fresh_reg();
                emit_line("  " + is_less + " = icmp eq i32 " + tag_val + ", 0");
                std::string is_greater = fresh_reg();
                emit_line("  " + is_greater + " = icmp eq i32 " + tag_val + ", 2");
                std::string sel1 = fresh_reg();
                emit_line("  " + sel1 + " = select i1 " + is_greater + ", i32 0, i32 1");
                std::string new_tag = fresh_reg();
                emit_line("  " + new_tag + " = select i1 " + is_less + ", i32 2, i32 " + sel1);
                std::string result = fresh_reg();
                emit_line("  " + result + " = insertvalue %struct.Ordering undef, i32 " + new_tag +
                          ", 0");
                last_expr_type_ = "%struct.Ordering";
                return result;
            }
            if (method == "then_cmp") {
                emit_coverage("Ordering::then_cmp");
                if (call.args.empty()) {
                    report_error("then_cmp() requires an argument", call.span, "C015");
                    return "0";
                }
                std::string other = gen_expr(*call.args[0]);
                std::string other_tag = fresh_reg();
                emit_line("  " + other_tag + " = extractvalue %struct.Ordering " + other + ", 0");
                std::string is_equal = fresh_reg();
                emit_line("  " + is_equal + " = icmp eq i32 " + tag_val + ", 1");
                std::string new_tag = fresh_reg();
                emit_line("  " + new_tag + " = select i1 " + is_equal + ", i32 " + other_tag +
                          ", i32 " + tag_val);
                std::string result = fresh_reg();
                emit_line("  " + result + " = insertvalue %struct.Ordering undef, i32 " + new_tag +
                          ", 0");
                last_expr_type_ = "%struct.Ordering";
                return result;
            }
            if (method == "to_string") {
                emit_coverage("Ordering::to_string");
                std::string less_str = add_string_literal("Less");
                std::string equal_str = add_string_literal("Equal");
                std::string greater_str = add_string_literal("Greater");
                std::string is_less = fresh_reg();
                emit_line("  " + is_less + " = icmp eq i32 " + tag_val + ", 0");
                std::string is_equal = fresh_reg();
                emit_line("  " + is_equal + " = icmp eq i32 " + tag_val + ", 1");
                std::string sel1 = fresh_reg();
                emit_line("  " + sel1 + " = select i1 " + is_equal + ", ptr " + equal_str +
                          ", ptr " + greater_str);
                std::string result = fresh_reg();
                emit_line("  " + result + " = select i1 " + is_less + ", ptr " + less_str +
                          ", ptr " + sel1);
                last_expr_type_ = "ptr";
                return result;
            }
            if (method == "debug_string") {
                emit_coverage("Ordering::debug_string");
                std::string less_str = add_string_literal("Ordering::Less");
                std::string equal_str = add_string_literal("Ordering::Equal");
                std::string greater_str = add_string_literal("Ordering::Greater");
                std::string is_less = fresh_reg();
                emit_line("  " + is_less + " = icmp eq i32 " + tag_val + ", 0");
                std::string is_equal = fresh_reg();
                emit_line("  " + is_equal + " = icmp eq i32 " + tag_val + ", 1");
                std::string sel1 = fresh_reg();
                emit_line("  " + sel1 + " = select i1 " + is_equal + ", ptr " + equal_str +
                          ", ptr " + greater_str);
                std::string result = fresh_reg();
                emit_line("  " + result + " = select i1 " + is_less + ", ptr " + less_str +
                          ", ptr " + sel1);
                last_expr_type_ = "ptr";
                return result;
            }
        }

        // Handle Maybe[T] methods
        if (named.name == "Maybe") {
            std::string enum_type_name = llvm_type_from_semantic(receiver_type, true);

            // If receiver is from field access, it's a pointer - need to load first
            std::string maybe_val = receiver;
            if (call.receiver->is<parser::FieldExpr>() && enum_type_name.starts_with("%struct.")) {
                std::string loaded = fresh_reg();
                emit_line("  " + loaded + " = load " + enum_type_name + ", ptr " + receiver);
                maybe_val = loaded;
            }

            std::string tag_val = fresh_reg();
            emit_line("  " + tag_val + " = extractvalue " + enum_type_name + " " + maybe_val +
                      ", 0");
            auto result = gen_maybe_method(call, maybe_val, enum_type_name, tag_val, named);
            if (result) {
                return *result;
            }
        }

        // Handle Outcome[T, E] methods
        if (named.name == "Outcome" && named.type_args.size() >= 2) {
            std::string enum_type_name = llvm_type_from_semantic(receiver_type, true);

            // If receiver is from field access, it's a pointer - need to load first
            std::string outcome_val = receiver;
            if (call.receiver->is<parser::FieldExpr>() && enum_type_name.starts_with("%struct.")) {
                std::string loaded = fresh_reg();
                emit_line("  " + loaded + " = load " + enum_type_name + ", ptr " + receiver);
                outcome_val = loaded;
            }

            std::string tag_val = fresh_reg();
            emit_line("  " + tag_val + " = extractvalue " + enum_type_name + " " + outcome_val +
                      ", 0");
            auto result = gen_outcome_method(call, outcome_val, enum_type_name, tag_val, named);
            if (result) {
                return *result;
            }
        }
    }

    // Special case: handle is_ok/is_err on compare_exchange results when type inference failed
    // The receiver might be I32 due to fallback, but if the receiver is a compare_exchange call,
    // we know it returns Outcome and should handle is_ok/is_err accordingly
    if ((method == "is_ok" || method == "is_err") && call.receiver->is<parser::MethodCallExpr>()) {
        const auto& inner_call = call.receiver->as<parser::MethodCallExpr>();
        if (inner_call.method == "compare_exchange" ||
            inner_call.method == "compare_exchange_weak") {
            // The receiver is a compare_exchange call - assume Outcome type
            // For is_ok/is_err, we just need to check the tag field
            // Outcome is represented as { i32 tag, inner_type value }
            // tag 0 = Ok, tag 1 = Err
            std::string tag_val = fresh_reg();
            emit_line("  " + tag_val + " = extractvalue { i32, i32 } " + receiver + ", 0");
            std::string result = fresh_reg();
            if (method == "is_ok") {
                emit_line("  " + result + " = icmp eq i32 " + tag_val + ", 0");
            } else { // is_err
                emit_line("  " + result + " = icmp ne i32 " + tag_val + ", 0");
            }
            last_expr_type_ = "i1";
            return result;
        }
    }

    // =========================================================================
    // 8. Handle Slice/MutSlice methods
    // =========================================================================
    auto slice_result = gen_slice_method(call, receiver, receiver_type_name, receiver_type);
    if (slice_result) {
        return *slice_result;
    }

    // =========================================================================
    // 9. Handle collection methods (List, HashMap, Buffer)
    // =========================================================================
    auto coll_result = gen_collection_method(call, receiver, receiver_type_name, receiver_type);
    if (coll_result) {
        return *coll_result;
    }

    // =========================================================================
    // 10. Check for user-defined impl methods (see method_impl.cpp)
    // =========================================================================
    if (auto impl_result = try_gen_impl_method_call(call, receiver, receiver_ptr, receiver_type)) {
        return *impl_result;
    }

    // =========================================================================
    // 11. Try module lookup for impl methods (see method_impl.cpp)
    // =========================================================================
    if (auto module_impl_result =
            try_gen_module_impl_method_call(call, receiver, receiver_ptr, receiver_type)) {
        return *module_impl_result;
    }

    // =========================================================================
    // 12. Handle dyn dispatch (see method_dyn.cpp)
    // =========================================================================
    if (auto dyn_result = try_gen_dyn_dispatch_call(call, receiver, receiver_type)) {
        return *dyn_result;
    }

    // =========================================================================
    // 13. Fn trait method calls (delegated to method_generic.cpp)
    // =========================================================================
    if (auto r = gen_method_fn_trait_call(call, method, receiver, receiver_type)) {
        return *r;
    }

    // =========================================================================
    // 14. Handle File instance methods
    // =========================================================================
    if (method == "is_open" || method == "read_line" || method == "write_str" || method == "size" ||
        method == "close" || method == "flush") {
        std::string file_ptr = receiver_ptr;
        if (file_ptr.empty()) {
            file_ptr = fresh_reg();
            emit_line("  " + file_ptr + " = alloca %struct.File");
            emit_line("  store %struct.File " + receiver + ", ptr " + file_ptr);
        }

        std::string handle_field_ptr = fresh_reg();
        emit_line("  " + handle_field_ptr + " = getelementptr %struct.File, ptr " + file_ptr +
                  ", i32 0, i32 0");
        std::string handle = fresh_reg();
        emit_line("  " + handle + " = load ptr, ptr " + handle_field_ptr);

        if (method == "is_open") {
            std::string result = fresh_reg();
            emit_line("  " + result + " = call i1 @file_is_open(ptr " + handle + ")");
            last_expr_type_ = "i1";
            return result;
        }
        if (method == "read_line") {
            std::string result = fresh_reg();
            emit_line("  " + result + " = call ptr @file_read_line(ptr " + handle + ")");
            last_expr_type_ = "ptr";
            return result;
        }
        if (method == "write_str") {
            if (call.args.empty()) {
                report_error("write_str requires a content argument", call.span, "C015");
                return "0";
            }
            std::string content_arg = gen_expr(*call.args[0]);
            std::string result = fresh_reg();
            emit_line("  " + result + " = call i1 @file_write_str(ptr " + handle + ", ptr " +
                      content_arg + ")");
            last_expr_type_ = "i1";
            return result;
        }
        if (method == "size") {
            std::string result = fresh_reg();
            emit_line("  " + result + " = call i64 @file_size(ptr " + handle + ")");
            last_expr_type_ = "i64";
            return result;
        }
        if (method == "close") {
            emit_line("  call void @file_close(ptr " + handle + ")");
            return "void";
        }
        if (method == "flush") {
            std::string result = fresh_reg();
            emit_line("  " + result + " = call i1 @file_flush(ptr " + handle + ")");
            last_expr_type_ = "i1";
            return result;
        }
    }

    // =========================================================================
    // 15-16. Handle class instance method calls (see method_class.cpp)
    // =========================================================================
    if (auto class_result =
            try_gen_class_instance_call(call, receiver, receiver_ptr, receiver_type)) {
        return *class_result;
    }

    // =========================================================================
    // 17. Handle function pointer field calls (e.g., vtable.call_fn(args))
    // =========================================================================
    {
        // Get the receiver type name - use receiver_type_name that was computed earlier
        std::string fn_field_type_name = receiver_type_name;

        // For 'this' inside generic impl methods, use current_impl_type_ for the
        // LLVM struct name (e.g., "FromFn__Fn") since the struct is monomorphized
        std::string llvm_struct_name = fn_field_type_name;
        if (receiver_ptr == "%this" && !current_impl_type_.empty()) {
            llvm_struct_name = current_impl_type_;
        }

        // Look up the struct definition
        auto struct_def = env_.lookup_struct(fn_field_type_name);
        if (struct_def) {
            // Look for a field with the method name
            int field_idx = 0;
            for (const auto& fld : struct_def->fields) {
                if (fld.name == method) {
                    // Resolve field type — may be a generic type parameter
                    types::TypePtr resolved_field_type = fld.type;
                    if (resolved_field_type && resolved_field_type->is<types::NamedType>() &&
                        !current_type_subs_.empty()) {
                        const auto& named = resolved_field_type->as<types::NamedType>();
                        auto sub_it = current_type_subs_.find(named.name);
                        if (sub_it != current_type_subs_.end() && sub_it->second) {
                            resolved_field_type = sub_it->second;
                        }
                    }

                    // Check if the field is a function type
                    if (resolved_field_type && resolved_field_type->is<types::FuncType>()) {
                        const auto& func = resolved_field_type->as<types::FuncType>();

                        // Get pointer to the field (stored as fat pointer { fn_ptr, env_ptr })
                        std::string field_ptr = fresh_reg();
                        emit_line("  " + field_ptr + " = getelementptr inbounds %struct." +
                                  llvm_struct_name + ", ptr " + receiver_ptr + ", i32 0, i32 " +
                                  std::to_string(field_idx));

                        // Load the fat pointer { fn_ptr, env_ptr }
                        std::string fat_val = fresh_reg();
                        emit_line("  " + fat_val + " = load { ptr, ptr }, ptr " + field_ptr);

                        // Extract fn_ptr and env_ptr
                        std::string fn_ptr = fresh_reg();
                        emit_line("  " + fn_ptr + " = extractvalue { ptr, ptr } " + fat_val +
                                  ", 0");
                        std::string env_ptr = fresh_reg();
                        emit_line("  " + env_ptr + " = extractvalue { ptr, ptr } " + fat_val +
                                  ", 1");

                        // Generate arguments
                        std::vector<std::string> arg_values;
                        std::vector<std::string> arg_types;
                        for (size_t i = 0; i < call.args.size(); ++i) {
                            std::string arg = gen_expr(*call.args[i]);
                            arg_values.push_back(arg);
                            if (i < func.params.size()) {
                                arg_types.push_back(llvm_type_from_semantic(func.params[i]));
                            } else {
                                arg_types.push_back(last_expr_type_);
                            }
                        }

                        // Determine return type
                        std::string ret_type = llvm_type_from_semantic(func.return_type);

                        // Check if env is null to determine calling convention
                        std::string is_null = fresh_reg();
                        emit_line("  " + is_null + " = icmp eq ptr " + env_ptr + ", null");

                        std::string label_thin = "fp_thin" + std::to_string(label_counter_);
                        std::string label_fat = "fp_fat" + std::to_string(label_counter_);
                        std::string label_merge = "fp_merge" + std::to_string(label_counter_);
                        label_counter_++;

                        emit_line("  br i1 " + is_null + ", label %" + label_thin + ", label %" +
                                  label_fat);

                        // Thin call path (no env — plain function pointer)
                        emit_line(label_thin + ":");
                        std::string args_str_thin;
                        for (size_t i = 0; i < arg_values.size(); ++i) {
                            if (i > 0)
                                args_str_thin += ", ";
                            args_str_thin += arg_types[i] + " " + arg_values[i];
                        }
                        std::string thin_result;
                        if (ret_type != "void") {
                            thin_result = fresh_reg();
                            emit_line("  " + thin_result + " = call " + ret_type + " " + fn_ptr +
                                      "(" + args_str_thin + ")");
                        } else {
                            emit_line("  call void " + fn_ptr + "(" + args_str_thin + ")");
                        }
                        emit_line("  br label %" + label_merge);

                        // Fat call path (with env as first arg — capturing closure)
                        emit_line(label_fat + ":");
                        std::string args_str_fat = "ptr " + env_ptr;
                        for (size_t i = 0; i < arg_values.size(); ++i) {
                            args_str_fat += ", ";
                            args_str_fat += arg_types[i] + " " + arg_values[i];
                        }
                        std::string fat_result;
                        if (ret_type != "void") {
                            fat_result = fresh_reg();
                            emit_line("  " + fat_result + " = call " + ret_type + " " + fn_ptr +
                                      "(" + args_str_fat + ")");
                        } else {
                            emit_line("  call void " + fn_ptr + "(" + args_str_fat + ")");
                        }
                        emit_line("  br label %" + label_merge);

                        // Merge
                        emit_line(label_merge + ":");
                        if (ret_type != "void") {
                            std::string phi_result = fresh_reg();
                            emit_line("  " + phi_result + " = phi " + ret_type + " [ " +
                                      thin_result + ", %" + label_thin + " ], [ " + fat_result +
                                      ", %" + label_fat + " ]");
                            last_expr_type_ = ret_type;
                            return phi_result;
                        }

                        last_expr_type_ = ret_type;
                        return ret_type == "void" ? "void" : "";
                    }
                    break;
                }
                field_idx++;
            }
        }
    }

    report_error("Unknown method: " + method, call.span, "C006");
    return "0";
}

} // namespace tml::codegen
