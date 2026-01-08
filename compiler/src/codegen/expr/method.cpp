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

#include "codegen/llvm_ir_gen.hpp"
#include "types/module.hpp"

#include <iostream>

namespace tml::codegen {

auto LLVMIRGen::gen_method_call(const parser::MethodCallExpr& call) -> std::string {
    const std::string& method = call.method;

    // =========================================================================
    // 1. Check for static method calls (Type::method)
    // =========================================================================
    std::string type_name;
    bool has_type_name = false;

    if (call.receiver->is<parser::IdentExpr>()) {
        type_name = call.receiver->as<parser::IdentExpr>().name;
        has_type_name = true;
    } else if (call.receiver->is<parser::PathExpr>()) {
        const auto& path_expr = call.receiver->as<parser::PathExpr>();
        if (path_expr.path.segments.size() == 1) {
            type_name = path_expr.path.segments[0];
            has_type_name = true;
        }
    }

    if (has_type_name) {
        // Check if this is a generic struct from:
        // 1. Local pending_generic_structs_ or pending_generic_impls_
        // 2. Imported structs from module registry with type_params
        bool is_generic_struct = pending_generic_structs_.count(type_name) > 0 ||
                                 pending_generic_impls_.count(type_name) > 0;

        // Also check for imported generic structs
        std::vector<std::string> imported_type_params;
        if (!is_generic_struct && env_.module_registry()) {
            const auto& all_modules = env_.module_registry()->get_all_modules();
            for (const auto& [mod_name, mod] : all_modules) {
                auto struct_it = mod.structs.find(type_name);
                if (struct_it != mod.structs.end() && !struct_it->second.type_params.empty()) {
                    is_generic_struct = true;
                    imported_type_params = struct_it->second.type_params;
                    break;
                }
            }
        }

        // For generic struct static methods (like Range::new), use expected_enum_type_ for type
        // args
        if (is_generic_struct && locals_.count(type_name) == 0) {
            // Look up the impl method and generate the monomorphized call
            std::string qualified_name = type_name + "::" + method;
            auto func_sig = env_.lookup_func(qualified_name);

            // If not found locally, search modules
            if (!func_sig && env_.module_registry()) {
                const auto& all_modules = env_.module_registry()->get_all_modules();
                for (const auto& [mod_name, mod] : all_modules) {
                    auto func_it = mod.functions.find(qualified_name);
                    if (func_it != mod.functions.end()) {
                        func_sig = func_it->second;
                        break;
                    }
                }
            }

            if (func_sig) {
                // Determine the type arguments from expected_enum_type_
                std::string mangled_type_name = type_name;
                std::unordered_map<std::string, types::TypePtr> type_subs;

                if (!expected_enum_type_.empty() &&
                    expected_enum_type_.find("%struct." + type_name + "__") == 0) {
                    // Extract type args from expected_enum_type_ like "%struct.Range__I64"
                    mangled_type_name = expected_enum_type_.substr(8); // Remove "%struct."

                    // Build type substitutions - try local impls first, then imported type params
                    std::vector<std::string> generic_names;
                    auto impl_it = pending_generic_impls_.find(type_name);
                    if (impl_it != pending_generic_impls_.end()) {
                        for (const auto& g : impl_it->second->generics) {
                            generic_names.push_back(g.name);
                        }
                    } else if (!imported_type_params.empty()) {
                        generic_names = imported_type_params;
                    }

                    // For simple cases like Range__I64, extract the type arg
                    std::string suffix = mangled_type_name.substr(type_name.length());
                    if (suffix.starts_with("__") && generic_names.size() == 1) {
                        std::string type_arg_str = suffix.substr(2);
                        types::TypePtr type_arg = nullptr;

                        // Helper to create primitive types
                        auto make_prim = [](types::PrimitiveKind kind) -> types::TypePtr {
                            auto t = std::make_shared<types::Type>();
                            t->kind = types::PrimitiveType{kind};
                            return t;
                        };

                        if (type_arg_str == "I64")
                            type_arg = types::make_i64();
                        else if (type_arg_str == "I32")
                            type_arg = types::make_i32();
                        else if (type_arg_str == "I8")
                            type_arg = make_prim(types::PrimitiveKind::I8);
                        else if (type_arg_str == "I16")
                            type_arg = make_prim(types::PrimitiveKind::I16);
                        else if (type_arg_str == "U8")
                            type_arg = make_prim(types::PrimitiveKind::U8);
                        else if (type_arg_str == "U16")
                            type_arg = make_prim(types::PrimitiveKind::U16);
                        else if (type_arg_str == "U32")
                            type_arg = make_prim(types::PrimitiveKind::U32);
                        else if (type_arg_str == "U64")
                            type_arg = make_prim(types::PrimitiveKind::U64);
                        else if (type_arg_str == "F32")
                            type_arg = make_prim(types::PrimitiveKind::F32);
                        else if (type_arg_str == "F64")
                            type_arg = types::make_f64();
                        else if (type_arg_str == "Bool")
                            type_arg = types::make_bool();
                        else if (type_arg_str == "Str")
                            type_arg = types::make_str();
                        else
                            type_arg = types::make_i64(); // Default fallback

                        if (type_arg && !generic_names.empty()) {
                            type_subs[generic_names[0]] = type_arg;
                        }
                    }
                }

                // Request impl method instantiation if needed
                std::string mangled_method_name = "tml_" + mangled_type_name + "_" + method;
                if (generated_impl_methods_.find(mangled_method_name) ==
                    generated_impl_methods_.end()) {
                    // For both local and imported generic impls, request instantiation
                    auto impl_it = pending_generic_impls_.find(type_name);
                    if (impl_it != pending_generic_impls_.end() || !imported_type_params.empty()) {
                        pending_impl_method_instantiations_.push_back(
                            PendingImplMethod{mangled_type_name, method, type_subs, type_name});
                        generated_impl_methods_.insert(mangled_method_name);
                    }
                }

                // Generate the static method call
                std::string fn_name = "@tml_" + mangled_type_name + "_" + method;

                // Generate arguments (no receiver for static methods)
                std::vector<std::pair<std::string, std::string>> typed_args;
                for (size_t i = 0; i < call.args.size(); ++i) {
                    std::string val = gen_expr(*call.args[i]);
                    std::string arg_type = last_expr_type_;
                    if (func_sig && i < func_sig->params.size()) {
                        auto param_type = func_sig->params[i];
                        if (!type_subs.empty()) {
                            param_type = types::substitute_type(param_type, type_subs);
                        }
                        arg_type = llvm_type_from_semantic(param_type);
                    }
                    typed_args.push_back({arg_type, val});
                }

                auto return_type = func_sig->return_type;
                if (!type_subs.empty()) {
                    return_type = types::substitute_type(return_type, type_subs);
                }
                std::string ret_type = llvm_type_from_semantic(return_type);

                std::string args_str;
                for (size_t i = 0; i < typed_args.size(); ++i) {
                    if (i > 0)
                        args_str += ", ";
                    args_str += typed_args[i].first + " " + typed_args[i].second;
                }

                std::string result = fresh_reg();
                if (ret_type == "void") {
                    emit_line("  call void " + fn_name + "(" + args_str + ")");
                    last_expr_type_ = "void";
                    return "void";
                } else {
                    emit_line("  " + result + " = call " + ret_type + " " + fn_name + "(" +
                              args_str + ")");
                    last_expr_type_ = ret_type;
                    return result;
                }
            }
        }

        bool is_type_name =
            struct_types_.count(type_name) > 0 || type_name == "List" || type_name == "HashMap" ||
            type_name == "Buffer" || type_name == "File" || type_name == "Path" ||
            type_name == "I8" || type_name == "I16" || type_name == "I32" || type_name == "I64" ||
            type_name == "I128" || type_name == "U8" || type_name == "U16" || type_name == "U32" ||
            type_name == "U64" || type_name == "U128" || type_name == "F32" || type_name == "F64" ||
            type_name == "Bool" || type_name == "Str";

        if (is_type_name && locals_.count(type_name) == 0) {
            auto result = gen_static_method_call(call, type_name);
            if (result) {
                return *result;
            }
            report_error("Unknown static method: " + type_name + "." + method, call.span);
            return "0";
        }
    }

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
        if (field_expr.object->is<parser::IdentExpr>()) {
            const auto& ident = field_expr.object->as<parser::IdentExpr>();
            if (ident.name == "this") {
                base_ptr = "%this";
            } else {
                auto it = locals_.find(ident.name);
                if (it != locals_.end()) {
                    base_ptr = it->second.reg;
                }
            }
        }

        if (!base_ptr.empty()) {
            // Infer the base object type
            types::TypePtr base_type = infer_expr_type(*field_expr.object);
            TML_DEBUG_LN("[FIELD_MUTATION] base_type exists: " << (base_type ? "yes" : "no"));
            if (base_type) {
                TML_DEBUG_LN("[FIELD_MUTATION] base_type is NamedType: "
                             << (base_type->is<types::NamedType>() ? "yes" : "no"));
            }
            if (base_type && base_type->is<types::NamedType>()) {
                const auto& base_named = base_type->as<types::NamedType>();
                std::string base_type_name = base_named.name;

                // Get the mangled struct type name if it has type args
                std::string struct_type_name = base_type_name;
                if (!base_named.type_args.empty()) {
                    struct_type_name = mangle_struct_name(base_type_name, base_named.type_args);
                }
                std::string llvm_struct_type = "%struct." + struct_type_name;

                // Get field index
                int field_idx = get_field_index(struct_type_name, field_expr.field);
                if (field_idx >= 0) {
                    std::string field_type = get_field_type(struct_type_name, field_expr.field);

                    // Generate getelementptr to get pointer to the field
                    std::string field_ptr = fresh_reg();
                    emit_line("  " + field_ptr + " = getelementptr " + llvm_struct_type + ", ptr " +
                              base_ptr + ", i32 0, i32 " + std::to_string(field_idx));

                    // For primitive types, load the value; for structs, keep the pointer
                    receiver_ptr = field_ptr;
                    if (field_type == "i8" || field_type == "i16" || field_type == "i32" ||
                        field_type == "i64" || field_type == "i128" || field_type == "i1" ||
                        field_type == "float" || field_type == "double") {
                        // Load primitive value for method calls like to_string()
                        std::string loaded = fresh_reg();
                        emit_line("  " + loaded + " = load " + field_type + ", ptr " + field_ptr);
                        receiver = loaded;
                    } else {
                        receiver = field_ptr;
                    }
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
            }
        }
    }

    // =========================================================================
    // 4. Get receiver type info
    // =========================================================================
    types::TypePtr receiver_type = infer_expr_type(*call.receiver);

    // If receiver type is a type parameter, substitute with actual type from current_type_subs_
    if (receiver_type && receiver_type->is<types::NamedType>() && !current_type_subs_.empty()) {
        const auto& named = receiver_type->as<types::NamedType>();
        auto sub_it = current_type_subs_.find(named.name);
        if (sub_it != current_type_subs_.end()) {
            receiver_type = sub_it->second;
        }
    }

    std::string receiver_type_name;
    if (receiver_type) {
        if (receiver_type->is<types::NamedType>()) {
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
                report_error("Ptr.write() requires a value argument", call.span);
                return "void";
            }
            std::string val = gen_expr(*call.args[0]);
            emit_line("  store " + inner_llvm_type + " " + val + ", ptr " + receiver);
            return "void";
        }
        if (method == "offset") {
            if (call.args.empty()) {
                report_error("Ptr.offset() requires an offset argument", call.span);
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
    // 6b. Handle primitive type behavior methods (partial_cmp, cmp, debug_string, etc.)
    // =========================================================================
    if (receiver_type && receiver_type->is<types::PrimitiveType>() && !receiver_type_name.empty()) {
        // Look for impl methods on primitive types (e.g., impl PartialOrd for I64)
        std::string qualified_name = receiver_type_name + "::" + method;
        types::FuncSig func_sig_value;
        types::FuncSig* func_sig = nullptr;

        // Search module registry for the method
        if (env_.module_registry()) {
            const auto& all_modules = env_.module_registry()->get_all_modules();
            for (const auto& [mod_name, mod] : all_modules) {
                auto func_it = mod.functions.find(qualified_name);
                if (func_it != mod.functions.end()) {
                    func_sig_value = func_it->second;
                    func_sig = &func_sig_value;
                    break;
                }
            }
        }

        // Also try local lookup
        if (!func_sig) {
            auto local_sig = env_.lookup_func(qualified_name);
            if (local_sig) {
                func_sig_value = *local_sig;
                func_sig = &func_sig_value;
            }
        }

        if (func_sig) {
            std::string fn_name = "@tml_" + receiver_type_name + "_" + method;
            std::string llvm_ty = llvm_type_from_semantic(receiver_type);

            // Build arguments - this (by value for primitives), then args
            std::vector<std::pair<std::string, std::string>> typed_args;
            typed_args.push_back({llvm_ty, receiver});

            for (size_t i = 0; i < call.args.size(); ++i) {
                std::string val = gen_expr(*call.args[i]);
                std::string arg_type = "i32";
                if (i + 1 < func_sig->params.size()) {
                    arg_type = llvm_type_from_semantic(func_sig->params[i + 1]);
                }
                typed_args.push_back({arg_type, val});
            }

            std::string ret_type = llvm_type_from_semantic(func_sig->return_type);
            std::string args_str;
            for (size_t i = 0; i < typed_args.size(); ++i) {
                if (i > 0)
                    args_str += ", ";
                args_str += typed_args[i].first + " " + typed_args[i].second;
            }

            std::string result = fresh_reg();
            if (ret_type == "void") {
                emit_line("  call void " + fn_name + "(" + args_str + ")");
                last_expr_type_ = "void";
                return "void";
            } else {
                emit_line("  " + result + " = call " + ret_type + " " + fn_name + "(" + args_str +
                          ")");
                last_expr_type_ = ret_type;
                return result;
            }
        }
    }

    // =========================================================================
    // 7. Handle Ordering enum methods
    // =========================================================================
    if (receiver_type && receiver_type->is<types::NamedType>()) {
        const auto& named = receiver_type->as<types::NamedType>();
        if (named.name == "Ordering") {
            std::string tag_val = fresh_reg();
            emit_line("  " + tag_val + " = extractvalue %struct.Ordering " + receiver + ", 0");

            if (method == "is_less") {
                std::string result = fresh_reg();
                emit_line("  " + result + " = icmp eq i32 " + tag_val + ", 0");
                last_expr_type_ = "i1";
                return result;
            }
            if (method == "is_equal") {
                std::string result = fresh_reg();
                emit_line("  " + result + " = icmp eq i32 " + tag_val + ", 1");
                last_expr_type_ = "i1";
                return result;
            }
            if (method == "is_greater") {
                std::string result = fresh_reg();
                emit_line("  " + result + " = icmp eq i32 " + tag_val + ", 2");
                last_expr_type_ = "i1";
                return result;
            }
            if (method == "reverse") {
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
                if (call.args.empty()) {
                    report_error("then_cmp() requires an argument", call.span);
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
            std::string tag_val = fresh_reg();
            emit_line("  " + tag_val + " = extractvalue " + enum_type_name + " " + receiver +
                      ", 0");
            auto result = gen_maybe_method(call, receiver, enum_type_name, tag_val, named);
            if (result) {
                return *result;
            }
        }

        // Handle Outcome[T, E] methods
        if (named.name == "Outcome" && named.type_args.size() >= 2) {
            std::string enum_type_name = llvm_type_from_semantic(receiver_type, true);
            std::string tag_val = fresh_reg();
            emit_line("  " + tag_val + " = extractvalue " + enum_type_name + " " + receiver +
                      ", 0");
            auto result = gen_outcome_method(call, receiver, enum_type_name, tag_val, named);
            if (result) {
                return *result;
            }
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
    // 10. Check for user-defined impl methods
    // =========================================================================
    if (receiver_type && receiver_type->is<types::NamedType>()) {
        const auto& named = receiver_type->as<types::NamedType>();
        bool is_builtin_type =
            (named.name == "List" || named.name == "HashMap" || named.name == "Buffer" ||
             named.name == "File" || named.name == "Path");
        bool is_slice_inlined = (named.name == "Slice" || named.name == "MutSlice") &&
                                (method == "len" || method == "is_empty");

        if (!is_builtin_type && !is_slice_inlined) {
            std::string qualified_name = named.name + "::" + method;
            auto func_sig = env_.lookup_func(qualified_name);
            if (func_sig) {
                std::string mangled_type_name = named.name;
                std::unordered_map<std::string, types::TypePtr> type_subs;

                if (!named.type_args.empty()) {
                    mangled_type_name = mangle_struct_name(named.name, named.type_args);
                    std::string mangled_method_name = "tml_" + mangled_type_name + "_" + method;

                    // Check locally defined impls first
                    auto impl_it = pending_generic_impls_.find(named.name);
                    if (impl_it != pending_generic_impls_.end()) {
                        const auto& impl = *impl_it->second;
                        for (size_t i = 0; i < impl.generics.size() && i < named.type_args.size();
                             ++i) {
                            type_subs[impl.generics[i].name] = named.type_args[i];
                        }
                    }

                    // Also check imported structs for type params
                    std::vector<std::string> imported_type_params;
                    if (impl_it == pending_generic_impls_.end() && env_.module_registry()) {
                        const auto& all_modules = env_.module_registry()->get_all_modules();
                        for (const auto& [mod_name, mod] : all_modules) {
                            auto struct_it = mod.structs.find(named.name);
                            if (struct_it != mod.structs.end() &&
                                !struct_it->second.type_params.empty()) {
                                imported_type_params = struct_it->second.type_params;
                                for (size_t i = 0;
                                     i < imported_type_params.size() && i < named.type_args.size();
                                     ++i) {
                                    type_subs[imported_type_params[i]] = named.type_args[i];
                                }
                                break;
                            }
                        }
                    }

                    if (generated_impl_methods_.find(mangled_method_name) ==
                        generated_impl_methods_.end()) {
                        // Request instantiation for both local and imported generic impls
                        if (impl_it != pending_generic_impls_.end() ||
                            !imported_type_params.empty()) {
                            pending_impl_method_instantiations_.push_back(PendingImplMethod{
                                mangled_type_name, method, type_subs, named.name});
                            generated_impl_methods_.insert(mangled_method_name);
                        }
                    }
                }

                std::string fn_name = "@tml_" + mangled_type_name + "_" + method;
                std::string impl_receiver_val;

                // Determine the LLVM type for the receiver based on the impl type
                std::string impl_llvm_type = llvm_type_name(named.name);
                bool is_primitive_impl = (impl_llvm_type[0] != '%');

                if (call.receiver->is<parser::IdentExpr>()) {
                    const auto& ident = call.receiver->as<parser::IdentExpr>();
                    auto it = locals_.find(ident.name);
                    if (it != locals_.end()) {
                        if (is_primitive_impl) {
                            // For primitives, pass the value directly
                            impl_receiver_val = receiver;
                        } else {
                            // For structs, pass the pointer
                            impl_receiver_val =
                                (it->second.type == "ptr") ? receiver : it->second.reg;
                        }
                    } else {
                        impl_receiver_val = receiver;
                    }
                } else if (call.receiver->is<parser::FieldExpr>() && !receiver_ptr.empty()) {
                    // For field expressions, use the field pointer directly
                    // This ensures mutations happen in place (section 10)
                    impl_receiver_val = receiver_ptr;
                } else if (last_expr_type_.starts_with("%struct.")) {
                    std::string tmp = fresh_reg();
                    emit_line("  " + tmp + " = alloca " + last_expr_type_);
                    emit_line("  store " + last_expr_type_ + " " + receiver + ", ptr " + tmp);
                    impl_receiver_val = tmp;
                } else {
                    impl_receiver_val = receiver;
                }

                std::vector<std::pair<std::string, std::string>> typed_args;
                // For primitive types, pass the value with the correct type
                // For structs/enums, pass as pointer
                std::string this_arg_type = is_primitive_impl ? impl_llvm_type : "ptr";
                typed_args.push_back({this_arg_type, impl_receiver_val});

                for (size_t i = 0; i < call.args.size(); ++i) {
                    std::string val = gen_expr(*call.args[i]);
                    std::string arg_type = "i32";
                    if (func_sig && i + 1 < func_sig->params.size()) {
                        auto param_type = func_sig->params[i + 1];
                        if (!type_subs.empty()) {
                            param_type = types::substitute_type(param_type, type_subs);
                        }
                        arg_type = llvm_type_from_semantic(param_type);
                    }
                    typed_args.push_back({arg_type, val});
                }

                auto return_type = func_sig->return_type;
                if (!type_subs.empty()) {
                    return_type = types::substitute_type(return_type, type_subs);
                }
                std::string ret_type = llvm_type_from_semantic(return_type);

                std::string args_str;
                for (size_t i = 0; i < typed_args.size(); ++i) {
                    if (i > 0)
                        args_str += ", ";
                    args_str += typed_args[i].first + " " + typed_args[i].second;
                }

                std::string result = fresh_reg();
                if (ret_type == "void") {
                    emit_line("  call void " + fn_name + "(" + args_str + ")");
                    last_expr_type_ = "void";
                    return "void";
                } else {
                    emit_line("  " + result + " = call " + ret_type + " " + fn_name + "(" +
                              args_str + ")");
                    last_expr_type_ = ret_type;
                    return result;
                }
            }
        }
    }

    // =========================================================================
    // 11. Try module lookup for impl methods
    // =========================================================================
    if (receiver_type && receiver_type->is<types::NamedType>()) {
        const auto& named2 = receiver_type->as<types::NamedType>();
        bool is_builtin_type2 =
            (named2.name == "List" || named2.name == "HashMap" || named2.name == "Buffer" ||
             named2.name == "File" || named2.name == "Path");
        if (!is_builtin_type2) {
            std::string qualified_name = named2.name + "::" + method;
            TML_DEBUG_LN("[METHOD] Looking for impl method: " << qualified_name);
            auto func_sig = env_.lookup_func(qualified_name);

            if (!func_sig) {
                std::string module_path = named2.module_path;
                if (module_path.empty()) {
                    auto import_path = env_.resolve_imported_symbol(named2.name);
                    if (import_path) {
                        auto pos = import_path->rfind("::");
                        if (pos != std::string::npos) {
                            module_path = import_path->substr(0, pos);
                        }
                    }
                }
                if (!module_path.empty()) {
                    auto module = env_.get_module(module_path);
                    if (module) {
                        auto func_it = module->functions.find(qualified_name);
                        if (func_it != module->functions.end()) {
                            func_sig = func_it->second;
                        }
                    }
                }
                if (!func_sig && env_.module_registry()) {
                    const auto& all_modules = env_.module_registry()->get_all_modules();
                    for (const auto& [mod_name, mod] : all_modules) {
                        auto func_it = mod.functions.find(qualified_name);
                        if (func_it != mod.functions.end()) {
                            func_sig = func_it->second;
                            break;
                        }
                    }
                }
            }
            if (func_sig) {
                std::string fn_name = "@tml_" + named2.name + "_" + method;
                std::string impl_receiver_val;

                // Determine the LLVM type for the receiver based on the impl type
                std::string impl_llvm_type = llvm_type_name(named2.name);
                bool is_primitive_impl = (impl_llvm_type[0] != '%');

                if (call.receiver->is<parser::IdentExpr>()) {
                    const auto& ident = call.receiver->as<parser::IdentExpr>();
                    auto it = locals_.find(ident.name);
                    if (it != locals_.end()) {
                        if (is_primitive_impl) {
                            // For primitives, pass the value directly
                            impl_receiver_val = receiver;
                        } else {
                            // For structs, pass the pointer
                            impl_receiver_val =
                                (it->second.type == "ptr") ? receiver : it->second.reg;
                        }
                    } else {
                        impl_receiver_val = receiver;
                    }
                } else if (call.receiver->is<parser::FieldExpr>() && !receiver_ptr.empty()) {
                    // For field expressions, use the field pointer directly
                    // This ensures mutations happen in place (section 11)
                    impl_receiver_val = receiver_ptr;
                } else if (last_expr_type_.starts_with("%struct.")) {
                    std::string tmp = fresh_reg();
                    emit_line("  " + tmp + " = alloca " + last_expr_type_);
                    emit_line("  store " + last_expr_type_ + " " + receiver + ", ptr " + tmp);
                    impl_receiver_val = tmp;
                } else {
                    impl_receiver_val = receiver;
                }

                std::vector<std::pair<std::string, std::string>> typed_args;
                // For primitive types, pass the value with the correct type
                // For structs/enums, pass as pointer
                std::string this_arg_type = is_primitive_impl ? impl_llvm_type : "ptr";
                typed_args.push_back({this_arg_type, impl_receiver_val});

                for (size_t i = 0; i < call.args.size(); ++i) {
                    std::string val = gen_expr(*call.args[i]);
                    std::string arg_type = "i32";
                    if (func_sig && i + 1 < func_sig->params.size()) {
                        arg_type = llvm_type_from_semantic(func_sig->params[i + 1]);
                    }
                    typed_args.push_back({arg_type, val});
                }

                std::string ret_type = llvm_type_from_semantic(func_sig->return_type);
                std::string args_str;
                for (size_t i = 0; i < typed_args.size(); ++i) {
                    if (i > 0)
                        args_str += ", ";
                    args_str += typed_args[i].first + " " + typed_args[i].second;
                }

                std::string result = fresh_reg();
                if (ret_type == "void") {
                    emit_line("  call void " + fn_name + "(" + args_str + ")");
                    last_expr_type_ = "void";
                    return "void";
                } else {
                    emit_line("  " + result + " = call " + ret_type + " " + fn_name + "(" +
                              args_str + ")");
                    last_expr_type_ = ret_type;
                    return result;
                }
            }
        }
    }

    // =========================================================================
    // 12. Handle dyn dispatch
    // =========================================================================
    if (call.receiver->is<parser::IdentExpr>()) {
        const auto& ident = call.receiver->as<parser::IdentExpr>();
        auto it = locals_.find(ident.name);
        if (it != locals_.end() && it->second.type.starts_with("%dyn.")) {
            std::string dyn_type = it->second.type;
            std::string behavior_name = dyn_type.substr(5);
            std::string dyn_ptr = it->second.reg;

            auto behavior_methods_it = behavior_method_order_.find(behavior_name);
            if (behavior_methods_it != behavior_method_order_.end()) {
                const auto& methods = behavior_methods_it->second;
                int method_idx = -1;
                for (size_t i = 0; i < methods.size(); ++i) {
                    if (methods[i] == method) {
                        method_idx = static_cast<int>(i);
                        break;
                    }
                }

                if (method_idx >= 0) {
                    std::string data_field = fresh_reg();
                    emit_line("  " + data_field + " = getelementptr " + dyn_type + ", ptr " +
                              dyn_ptr + ", i32 0, i32 0");
                    std::string data_ptr = fresh_reg();
                    emit_line("  " + data_ptr + " = load ptr, ptr " + data_field);

                    std::string vtable_field = fresh_reg();
                    emit_line("  " + vtable_field + " = getelementptr " + dyn_type + ", ptr " +
                              dyn_ptr + ", i32 0, i32 1");
                    std::string vtable_ptr = fresh_reg();
                    emit_line("  " + vtable_ptr + " = load ptr, ptr " + vtable_field);

                    // Build vtable struct type based on number of methods
                    std::string vtable_type = "{ ";
                    for (size_t i = 0; i < methods.size(); ++i) {
                        if (i > 0)
                            vtable_type += ", ";
                        vtable_type += "ptr";
                    }
                    vtable_type += " }";

                    std::string fn_ptr_loc = fresh_reg();
                    emit_line("  " + fn_ptr_loc + " = getelementptr " + vtable_type + ", ptr " +
                              vtable_ptr + ", i32 0, i32 " + std::to_string(method_idx));
                    std::string fn_ptr = fresh_reg();
                    emit_line("  " + fn_ptr + " = load ptr, ptr " + fn_ptr_loc);

                    // Get method signature from behavior definition
                    std::string return_llvm_type = "i32"; // default fallback
                    auto behavior_def = env_.lookup_behavior(behavior_name);
                    if (behavior_def) {
                        // Build substitution map from behavior's type params to dyn's type args
                        std::unordered_map<std::string, types::TypePtr> type_subs;
                        if (it->second.semantic_type &&
                            it->second.semantic_type->is<types::DynBehaviorType>()) {
                            const auto& dyn_sem =
                                it->second.semantic_type->as<types::DynBehaviorType>();
                            for (size_t i = 0; i < behavior_def->type_params.size() &&
                                               i < dyn_sem.type_args.size();
                                 ++i) {
                                type_subs[behavior_def->type_params[i]] = dyn_sem.type_args[i];
                            }
                        }

                        for (const auto& m : behavior_def->methods) {
                            if (m.name == method && m.return_type) {
                                // Substitute type parameters before converting to LLVM type
                                auto substituted_ret =
                                    types::substitute_type(m.return_type, type_subs);
                                return_llvm_type = llvm_type_from_semantic(substituted_ret);
                                break;
                            }
                        }
                    }

                    // Generate method arguments
                    std::string args_str = "ptr " + data_ptr;
                    std::string args_types = "ptr";
                    for (const auto& arg : call.args) {
                        std::string arg_val = gen_expr(*arg);
                        std::string arg_type = last_expr_type_;
                        args_str += ", " + arg_type + " " + arg_val;
                        args_types += ", " + arg_type;
                    }

                    std::string result = fresh_reg();
                    if (return_llvm_type == "void") {
                        emit_line("  call void " + fn_ptr + "(" + args_str + ")");
                        last_expr_type_ = "void";
                        return "";
                    } else {
                        emit_line("  " + result + " = call " + return_llvm_type + " " + fn_ptr +
                                  "(" + args_str + ")");
                        last_expr_type_ = return_llvm_type;
                        return result;
                    }
                }
            }
        }
    }

    // =========================================================================
    // 13. Handle File instance methods
    // =========================================================================
    if (method == "is_open" || method == "read_line" || method == "write_str" || method == "size" ||
        method == "close") {
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
                report_error("write_str requires a content argument", call.span);
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
    }

    report_error("Unknown method: " + method, call.span);
    return "0";
}

} // namespace tml::codegen
