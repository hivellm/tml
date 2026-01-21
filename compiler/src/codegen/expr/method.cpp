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
    // Clear expected literal type context - it should only apply within explicit type annotations
    // (like "let x: F64 = 5") and not leak into method call arguments
    expected_literal_type_.clear();
    expected_literal_is_unsigned_ = false;

    const std::string& method = call.method;
    TML_DEBUG_LN("[METHOD] gen_method_call: " << method << " where_constraints.size="
                                              << current_where_constraints_.size());

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
        // Check for class static method call (ClassName.staticMethod())
        auto class_def = env_.lookup_class(type_name);
        if (class_def.has_value()) {
            // Look for static method
            for (const auto& m : class_def->methods) {
                if (m.sig.name == method && m.is_static) {
                    // Generate call to static method
                    std::string func_name = "@tml_" + get_suite_prefix() + type_name + "_" + method;
                    std::string ret_type = llvm_type_from_semantic(m.sig.return_type);

                    // Generate arguments
                    std::vector<std::string> args;
                    std::vector<std::string> arg_types;
                    for (const auto& arg : call.args) {
                        args.push_back(gen_expr(*arg));
                        arg_types.push_back(last_expr_type_);
                    }

                    // Generate call
                    std::string result = fresh_reg();
                    std::string call_str =
                        "  " + result + " = call " + ret_type + " " + func_name + "(";
                    for (size_t i = 0; i < args.size(); ++i) {
                        if (i > 0)
                            call_str += ", ";
                        call_str += arg_types[i] + " " + args[i];
                    }
                    call_str += ")";
                    emit_line(call_str);

                    last_expr_type_ = ret_type;
                    return result;
                }
            }
        }

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
                            PendingImplMethod{mangled_type_name, method, type_subs, type_name, ""});
                        generated_impl_methods_.insert(mangled_method_name);
                    }
                }

                // Generate the static method call
                // Look up in functions_ to get the correct LLVM name (handles suite prefix
                // correctly)
                std::string method_lookup_key = mangled_type_name + "_" + method;
                auto method_it = functions_.find(method_lookup_key);
                std::string fn_name;
                if (method_it != functions_.end()) {
                    fn_name = method_it->second.llvm_name;
                } else {
                    // Fallback: construct name with suite prefix for test-local functions
                    fn_name = "@tml_" + get_suite_prefix() + mangled_type_name + "_" + method;
                }

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

        // Also check for imported structs from module registry
        if (!is_type_name && env_.module_registry()) {
            const auto& all_modules = env_.module_registry()->get_all_modules();
            for (const auto& [mod_name, mod] : all_modules) {
                if (mod.structs.count(type_name) > 0) {
                    is_type_name = true;
                    break;
                }
            }
        }

        if (is_type_name && locals_.count(type_name) == 0) {
            auto result = gen_static_method_call(call, type_name);
            if (result) {
                return *result;
            }

            // Try looking up user-defined static methods in the environment/modules
            std::string qualified_name = type_name + "::" + method;
            auto func_sig = env_.lookup_func(qualified_name);

            // If not found locally, search all modules
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
                // Generate call to user-defined static method
                std::string fn_name = "@tml_" + get_suite_prefix() + type_name + "_" + method;

                // Look up in functions_ for the correct LLVM name
                std::string method_lookup_key = type_name + "_" + method;
                auto method_it = functions_.find(method_lookup_key);
                if (method_it != functions_.end()) {
                    fn_name = method_it->second.llvm_name;
                }

                std::vector<std::pair<std::string, std::string>> typed_args;
                for (size_t i = 0; i < call.args.size(); ++i) {
                    std::string val = gen_expr(*call.args[i]);
                    std::string arg_type = last_expr_type_;
                    std::string expected_type = arg_type;
                    if (i < func_sig->params.size()) {
                        expected_type = llvm_type_from_semantic(func_sig->params[i]);
                        // Type coercion if needed
                        if (arg_type != expected_type) {
                            bool is_int_actual = (arg_type[0] == 'i' && arg_type != "i1");
                            bool is_int_expected =
                                (expected_type[0] == 'i' && expected_type != "i1");
                            if (is_int_actual && is_int_expected) {
                                int actual_bits = std::stoi(arg_type.substr(1));
                                int expected_bits = std::stoi(expected_type.substr(1));
                                std::string coerced = fresh_reg();
                                if (expected_bits > actual_bits) {
                                    emit_line("  " + coerced + " = sext " + arg_type + " " + val +
                                              " to " + expected_type);
                                } else {
                                    emit_line("  " + coerced + " = trunc " + arg_type + " " + val +
                                              " to " + expected_type);
                                }
                                val = coerced;
                            }
                        }
                    }
                    typed_args.push_back({expected_type, val});
                }

                std::string args_str;
                for (size_t i = 0; i < typed_args.size(); ++i) {
                    if (i > 0)
                        args_str += ", ";
                    args_str += typed_args[i].first + " " + typed_args[i].second;
                }

                std::string ret_type =
                    func_sig->return_type ? llvm_type_from_semantic(func_sig->return_type) : "void";
                std::string result_reg = fresh_reg();
                if (ret_type == "void") {
                    emit_line("  call void " + fn_name + "(" + args_str + ")");
                    last_expr_type_ = "void";
                    return "void";
                } else {
                    emit_line("  " + result_reg + " = call " + ret_type + " " + fn_name + "(" +
                              args_str + ")");
                    last_expr_type_ = ret_type;
                    return result_reg;
                }
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
    // 4b. Handle method calls on bounded generics (e.g., C: Container[T])
    // =========================================================================
    // When the receiver is a type parameter with behavior bounds from where clauses,
    // we need to dispatch to the concrete impl method for the substituted type.
    TML_DEBUG_LN("[METHOD 4b] method=" << method
                                       << " where_constraints=" << current_where_constraints_.size()
                                       << " type_subs=" << current_type_subs_.size());
    if (!current_where_constraints_.empty() && !current_type_subs_.empty()) {
        // For bounded generics, the type checker has already validated that the method exists
        // We need to find the concrete type and dispatch to its impl method

        // Debug: dump current_type_subs_
        for (const auto& [key, val] : current_type_subs_) {
            TML_DEBUG_LN("[METHOD 4b] type_subs: " << key << " -> is_NamedType="
                                                   << val->is<types::NamedType>());
        }

        // Iterate through all where constraints to find one with a behavior that has this method
        for (const auto& constraint : current_where_constraints_) {
            TML_DEBUG_LN("[METHOD 4b] checking constraint for type_param="
                         << constraint.type_param
                         << " parameterized_bounds=" << constraint.parameterized_bounds.size());

            // Get the concrete type name from the type parameter substitution
            std::string concrete_type_name;
            auto sub_it = current_type_subs_.find(constraint.type_param);
            if (sub_it != current_type_subs_.end()) {
                auto sub_type = sub_it->second;
                TML_DEBUG_LN("[METHOD 4b] sub_type for "
                             << constraint.type_param
                             << " is_NamedType=" << sub_type->is<types::NamedType>());
                if (sub_type->is<types::NamedType>()) {
                    concrete_type_name = sub_type->as<types::NamedType>().name;
                }
            }
            TML_DEBUG_LN("[METHOD 4b] concrete_type_name=" << concrete_type_name);

            // Look through parameterized bounds for a behavior with this method
            for (const auto& bound : constraint.parameterized_bounds) {
                TML_DEBUG_LN("[METHOD 4b] checking bound.behavior_name=" << bound.behavior_name);
                auto behavior_def = env_.lookup_behavior(bound.behavior_name);
                if (behavior_def) {
                    TML_DEBUG_LN("[METHOD 4b] found behavior_def with "
                                 << behavior_def->methods.size() << " methods");
                    for (const auto& bmethod : behavior_def->methods) {
                        TML_DEBUG_LN("[METHOD 4b] checking bmethod.name="
                                     << bmethod.name << " vs method=" << method);
                        if (bmethod.name == method) {
                            // Found the method in the behavior!
                            // Now dispatch to the concrete impl for the substituted type
                            TML_DEBUG_LN("[METHOD 4b] FOUND method! concrete_type_name="
                                         << concrete_type_name);

                            // Build substitution map from behavior type params to bound's type args
                            std::unordered_map<std::string, types::TypePtr> behavior_subs;
                            if (!bound.type_args.empty() && !behavior_def->type_params.empty()) {
                                for (size_t i = 0; i < behavior_def->type_params.size() &&
                                                   i < bound.type_args.size();
                                     ++i) {
                                    behavior_subs[behavior_def->type_params[i]] =
                                        bound.type_args[i];
                                }
                            }

                            // Look up the impl method: ConcreteType::method
                            std::string qualified_name = concrete_type_name + "::" + method;
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
                                // Generate the call to the concrete impl method
                                std::string fn_name = "@tml_" + get_suite_prefix() +
                                                      concrete_type_name + "_" + method;

                                // Build arguments
                                std::vector<std::pair<std::string, std::string>> typed_args;

                                // First argument is 'this' (the receiver)
                                std::string this_type = "ptr";
                                std::string this_val = receiver;

                                // For structs, we need a pointer to the struct
                                // For ref types (ptr), we already have the pointer value
                                if (call.receiver->is<parser::IdentExpr>()) {
                                    const auto& ident = call.receiver->as<parser::IdentExpr>();
                                    auto it = locals_.find(ident.name);
                                    if (it != locals_.end()) {
                                        if (it->second.type == "ptr") {
                                            // Ref type: use loaded value (receiver) which is the
                                            // ptr
                                            this_val = receiver;
                                        } else {
                                            // Struct type: use the alloca which is a ptr to struct
                                            this_val = it->second.reg;
                                        }
                                    }
                                }

                                typed_args.push_back({this_type, this_val});

                                // Add remaining arguments with type substitution
                                for (size_t i = 0; i < call.args.size(); ++i) {
                                    std::string val = gen_expr(*call.args[i]);
                                    std::string arg_type = "i32";
                                    if (func_sig && i + 1 < func_sig->params.size()) {
                                        auto param_type = func_sig->params[i + 1];
                                        if (!behavior_subs.empty()) {
                                            param_type =
                                                types::substitute_type(param_type, behavior_subs);
                                        }
                                        arg_type = llvm_type_from_semantic(param_type);
                                    }
                                    typed_args.push_back({arg_type, val});
                                }

                                // Determine return type with substitution
                                auto return_type = bmethod.return_type;
                                if (!behavior_subs.empty()) {
                                    return_type =
                                        types::substitute_type(return_type, behavior_subs);
                                }
                                std::string ret_type = llvm_type_from_semantic(return_type);

                                // Build args string
                                std::string args_str;
                                for (size_t i = 0; i < typed_args.size(); ++i) {
                                    if (i > 0)
                                        args_str += ", ";
                                    args_str += typed_args[i].first + " " + typed_args[i].second;
                                }

                                // Generate the call
                                std::string result = fresh_reg();
                                if (ret_type == "void") {
                                    emit_line("  call void " + fn_name + "(" + args_str + ")");
                                    last_expr_type_ = "void";
                                    return "void";
                                } else {
                                    emit_line("  " + result + " = call " + ret_type + " " +
                                              fn_name + "(" + args_str + ")");
                                    last_expr_type_ = ret_type;
                                    return result;
                                }
                            }
                        }
                    }
                }
            }

            // Also check simple (non-parameterized) behavior bounds
            for (const auto& behavior_name : constraint.required_behaviors) {
                auto behavior_def = env_.lookup_behavior(behavior_name);
                if (behavior_def) {
                    for (const auto& bmethod : behavior_def->methods) {
                        if (bmethod.name == method) {
                            // Dispatch to ConcreteType::method
                            std::string qualified_name = concrete_type_name + "::" + method;
                            auto func_sig = env_.lookup_func(qualified_name);

                            if (func_sig) {
                                std::string fn_name = "@tml_" + get_suite_prefix() +
                                                      concrete_type_name + "_" + method;

                                std::vector<std::pair<std::string, std::string>> typed_args;
                                std::string this_val = receiver;
                                if (call.receiver->is<parser::IdentExpr>()) {
                                    const auto& ident = call.receiver->as<parser::IdentExpr>();
                                    auto it = locals_.find(ident.name);
                                    if (it != locals_.end()) {
                                        if (it->second.type == "ptr") {
                                            // Ref type: use loaded value (receiver)
                                            this_val = receiver;
                                        } else {
                                            // Struct type: use the alloca
                                            this_val = it->second.reg;
                                        }
                                    }
                                }
                                typed_args.push_back({"ptr", this_val});

                                for (size_t i = 0; i < call.args.size(); ++i) {
                                    std::string val = gen_expr(*call.args[i]);
                                    std::string arg_type = "i32";
                                    if (func_sig && i + 1 < func_sig->params.size()) {
                                        arg_type = llvm_type_from_semantic(func_sig->params[i + 1]);
                                    }
                                    typed_args.push_back({arg_type, val});
                                }

                                std::string ret_type = llvm_type_from_semantic(bmethod.return_type);

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
                                    emit_line("  " + result + " = call " + ret_type + " " +
                                              fn_name + "(" + args_str + ")");
                                    last_expr_type_ = ret_type;
                                    return result;
                                }
                            }
                        }
                    }
                }
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
            // Look up in functions_ to get the correct LLVM name
            std::string method_lookup_key = receiver_type_name + "_" + method;
            auto method_it = functions_.find(method_lookup_key);
            std::string fn_name;
            if (method_it != functions_.end()) {
                fn_name = method_it->second.llvm_name;
            } else {
                fn_name = "@tml_" + get_suite_prefix() + receiver_type_name + "_" + method;
            }
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

            // Use registered function's return type if available (handles value class by-value
            // returns)
            std::string ret_type = llvm_type_from_semantic(func_sig->return_type);
            if (method_it != functions_.end() && !method_it->second.ret_type.empty()) {
                ret_type = method_it->second.ret_type;
            }

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
                std::string method_type_suffix; // For method generic type args like cast[U8]

                // Handle method-level generic type arguments (e.g., cast[U8])
                // Use current_type_subs_ to resolve any type parameters (e.g., U -> U8)
                // Method-level type params come AFTER impl-level type params in
                // func_sig->type_params The impl-level params correspond to named.type_args (e.g.,
                // RawPtr[I64] -> T=I64) So we skip the first named.type_args.size() params when
                // mapping call.type_args
                if (!call.type_args.empty() && !func_sig->type_params.empty()) {
                    size_t impl_param_count = named.type_args.size();
                    for (size_t i = 0; i < call.type_args.size(); ++i) {
                        size_t param_idx = impl_param_count + i;
                        if (param_idx < func_sig->type_params.size()) {
                            // Convert parser type to semantic type, using current type subs
                            auto semantic_type = resolve_parser_type_with_subs(*call.type_args[i],
                                                                               current_type_subs_);
                            if (semantic_type) {
                                type_subs[func_sig->type_params[param_idx]] = semantic_type;
                                // Build method type suffix for mangling
                                if (!method_type_suffix.empty()) {
                                    method_type_suffix += "_";
                                }
                                method_type_suffix += mangle_type(semantic_type);
                            }
                        }
                    }
                }

                if (!named.type_args.empty()) {
                    mangled_type_name = mangle_struct_name(named.name, named.type_args);
                    // Build full method name including method-level type args
                    std::string method_for_key = method;
                    if (!method_type_suffix.empty()) {
                        method_for_key += "__" + method_type_suffix;
                    }
                    std::string mangled_method_name =
                        "tml_" + mangled_type_name + "_" + method_for_key;

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
                                    // Also add associated type mappings (e.g., I::Item -> I64)
                                    // Look up associated types for the concrete type argument
                                    if (named.type_args[i] &&
                                        named.type_args[i]->is<types::NamedType>()) {
                                        const auto& arg_named =
                                            named.type_args[i]->as<types::NamedType>();
                                        auto item_type =
                                            lookup_associated_type(arg_named.name, "Item");
                                        if (item_type) {
                                            // Add T::Item -> ConcreteType mapping
                                            std::string assoc_key =
                                                imported_type_params[i] + "::Item";
                                            type_subs[assoc_key] = item_type;
                                            // Also just "Item" for simpler lookups
                                            type_subs["Item"] = item_type;
                                        }
                                    }
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
                            pending_impl_method_instantiations_.push_back(
                                PendingImplMethod{mangled_type_name, method, type_subs, named.name,
                                                  method_type_suffix});
                            generated_impl_methods_.insert(mangled_method_name);
                        }
                    }
                }

                // Look up in functions_ to get the correct LLVM name
                // Include method type suffix for methods with their own generic params
                std::string full_method_name = method;
                if (!method_type_suffix.empty()) {
                    full_method_name += "__" + method_type_suffix;
                }
                std::string method_lookup_key = mangled_type_name + "_" + full_method_name;
                auto method_it = functions_.find(method_lookup_key);
                std::string fn_name;
                if (method_it != functions_.end()) {
                    fn_name = method_it->second.llvm_name;
                } else {
                    fn_name =
                        "@tml_" + get_suite_prefix() + mangled_type_name + "_" + full_method_name;
                }
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
                    std::string actual_type = last_expr_type_;
                    std::string expected_type = "i32";
                    if (func_sig && i + 1 < func_sig->params.size()) {
                        auto param_type = func_sig->params[i + 1];
                        if (!type_subs.empty()) {
                            param_type = types::substitute_type(param_type, type_subs);
                        }
                        expected_type = llvm_type_from_semantic(param_type);
                    }
                    // Add type coercion if needed
                    if (actual_type != expected_type) {
                        // Integer width coercion
                        bool is_int_actual = (actual_type[0] == 'i' && actual_type != "i1");
                        bool is_int_expected = (expected_type[0] == 'i' && expected_type != "i1");
                        if (is_int_actual && is_int_expected) {
                            // Sign-extend or truncate
                            int actual_bits = std::stoi(actual_type.substr(1));
                            int expected_bits = std::stoi(expected_type.substr(1));
                            std::string coerced = fresh_reg();
                            if (expected_bits > actual_bits) {
                                emit_line("  " + coerced + " = sext " + actual_type + " " + val +
                                          " to " + expected_type);
                            } else {
                                emit_line("  " + coerced + " = trunc " + actual_type + " " + val +
                                          " to " + expected_type);
                            }
                            val = coerced;
                        }
                    }
                    typed_args.push_back({expected_type, val});
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
                // Look up in functions_ to get the correct LLVM name
                std::string method_lookup_key = named2.name + "_" + method;
                auto method_it = functions_.find(method_lookup_key);
                std::string fn_name;
                if (method_it != functions_.end()) {
                    fn_name = method_it->second.llvm_name;
                } else {
                    fn_name = "@tml_" + get_suite_prefix() + named2.name + "_" + method;
                }
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

        // Check for dyn dispatch - handles both:
        // 1. Direct dyn type: LLVM type is %dyn.Error
        // 2. Reference to dyn: LLVM type is ptr, semantic type is ref dyn Error
        bool is_dyn_dispatch = false;
        std::string behavior_name;
        std::string dyn_type;
        std::string dyn_ptr;

        if (it != locals_.end()) {
            if (it->second.type.starts_with("%dyn.")) {
                // Direct dyn type: %dyn.Error
                is_dyn_dispatch = true;
                dyn_type = it->second.type;
                behavior_name = dyn_type.substr(5);
                dyn_ptr = it->second.reg;
                // Ensure the dyn type is defined before use
                emit_dyn_type(behavior_name);
            } else if (it->second.semantic_type) {
                // Check for ref dyn Error: semantic type is RefType with inner DynBehaviorType
                auto sem_type = it->second.semantic_type;
                if (sem_type->is<types::RefType>()) {
                    auto inner = sem_type->as<types::RefType>().inner;
                    if (inner && inner->is<types::DynBehaviorType>()) {
                        is_dyn_dispatch = true;
                        behavior_name = inner->as<types::DynBehaviorType>().behavior_name;
                        dyn_type = "%dyn." + behavior_name;
                        dyn_ptr = it->second.reg;
                        // Ensure the dyn type is defined before use
                        emit_dyn_type(behavior_name);
                    }
                }
            }
        }

        if (is_dyn_dispatch) {
            TML_DEBUG_LN("[DYN] Dyn dispatch detected for behavior: " << behavior_name
                                                                      << " method: " << method);
            auto behavior_methods_it = behavior_method_order_.find(behavior_name);

            // If behavior not registered yet, try to look it up and register dynamically
            // This handles behaviors defined in imported modules (like core::error::Error)
            if (behavior_methods_it == behavior_method_order_.end()) {
                auto behavior_def = env_.lookup_behavior(behavior_name);

                // If not found, search all modules in the registry
                if (!behavior_def && env_.module_registry()) {
                    const auto& all_modules = env_.module_registry()->get_all_modules();
                    for (const auto& [mod_name, mod] : all_modules) {
                        auto mod_behavior_it = mod.behaviors.find(behavior_name);
                        if (mod_behavior_it != mod.behaviors.end()) {
                            behavior_def = mod_behavior_it->second;
                            break;
                        }
                    }
                }

                if (behavior_def) {
                    std::vector<std::string> methods;
                    for (const auto& m : behavior_def->methods) {
                        methods.push_back(m.name);
                    }
                    behavior_method_order_[behavior_name] = methods;
                    behavior_methods_it = behavior_method_order_.find(behavior_name);
                }
            }

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

                    // If not found, search all modules in the registry
                    // This handles behaviors defined in imported modules (like core::error::Error)
                    if (!behavior_def && env_.module_registry()) {
                        const auto& all_modules = env_.module_registry()->get_all_modules();
                        for (const auto& [mod_name, mod] : all_modules) {
                            auto mod_behavior_it = mod.behaviors.find(behavior_name);
                            if (mod_behavior_it != mod.behaviors.end()) {
                                behavior_def = mod_behavior_it->second;
                                TML_DEBUG_LN("[DYN] Found behavior " << behavior_name
                                                                     << " in module " << mod_name);
                                break;
                            }
                        }
                    }

                    TML_DEBUG_LN("[DYN] Looking up behavior: " << behavior_name << " found: "
                                                               << (behavior_def ? "yes" : "no"));
                    if (behavior_def) {
                        // Build substitution map from behavior's type params to dyn's type args
                        std::unordered_map<std::string, types::TypePtr> type_subs;
                        if (it->second.semantic_type) {
                            // Handle both direct dyn and ref dyn
                            types::TypePtr dyn_sem_type = nullptr;
                            if (it->second.semantic_type->is<types::DynBehaviorType>()) {
                                dyn_sem_type = it->second.semantic_type;
                            } else if (it->second.semantic_type->is<types::RefType>()) {
                                auto inner = it->second.semantic_type->as<types::RefType>().inner;
                                if (inner && inner->is<types::DynBehaviorType>()) {
                                    dyn_sem_type = inner;
                                }
                            }
                            if (dyn_sem_type) {
                                const auto& dyn_sem = dyn_sem_type->as<types::DynBehaviorType>();
                                for (size_t i = 0; i < behavior_def->type_params.size() &&
                                                   i < dyn_sem.type_args.size();
                                     ++i) {
                                    type_subs[behavior_def->type_params[i]] = dyn_sem.type_args[i];
                                }
                            }
                        }

                        for (const auto& m : behavior_def->methods) {
                            if (m.name == method && m.return_type) {
                                // Substitute type parameters before converting to LLVM type
                                auto substituted_ret =
                                    types::substitute_type(m.return_type, type_subs);
                                return_llvm_type = llvm_type_from_semantic(substituted_ret);
                                TML_DEBUG_LN("[DYN] Method "
                                             << method << " return type: " << return_llvm_type);
                                break;
                            }
                        }
                        if (return_llvm_type == "i32") {
                            TML_DEBUG_LN("[DYN] WARNING: Method "
                                         << method << " in behavior " << behavior_name
                                         << " has fallback return type i32");
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
    // 13. Handle Fn trait method calls on closures and function types
    // =========================================================================
    // Closures and function pointers implement Fn, FnMut, FnOnce
    // call(), call_mut(), call_once() invoke the callable
    if (method == "call" || method == "call_mut" || method == "call_once") {
        if (receiver_type) {
            // Handle ClosureType
            if (receiver_type->is<types::ClosureType>()) {
                const auto& closure_type = receiver_type->as<types::ClosureType>();

                // Generate arguments for the closure call
                std::string args_str;
                for (size_t i = 0; i < call.args.size(); ++i) {
                    std::string arg_val = gen_expr(*call.args[i]);
                    std::string arg_type = "i32"; // default
                    if (i < closure_type.params.size()) {
                        arg_type = llvm_type_from_semantic(closure_type.params[i]);
                    }
                    if (!args_str.empty())
                        args_str += ", ";
                    args_str += arg_type + " " + arg_val;
                }

                // Determine return type
                std::string ret_type = "i32";
                if (closure_type.return_type) {
                    ret_type = llvm_type_from_semantic(closure_type.return_type);
                }

                // Call the closure (receiver is function pointer)
                std::string result = fresh_reg();
                if (ret_type == "void") {
                    emit_line("  call void " + receiver + "(" + args_str + ")");
                    last_expr_type_ = "void";
                    return "void";
                } else {
                    emit_line("  " + result + " = call " + ret_type + " " + receiver + "(" +
                              args_str + ")");
                    last_expr_type_ = ret_type;
                    return result;
                }
            }

            // Handle FuncType (function pointers)
            if (receiver_type->is<types::FuncType>()) {
                const auto& func_type = receiver_type->as<types::FuncType>();

                // Generate arguments for the function call
                std::string args_str;
                for (size_t i = 0; i < call.args.size(); ++i) {
                    std::string arg_val = gen_expr(*call.args[i]);
                    std::string arg_type = "i32"; // default
                    if (i < func_type.params.size()) {
                        arg_type = llvm_type_from_semantic(func_type.params[i]);
                    }
                    if (!args_str.empty())
                        args_str += ", ";
                    args_str += arg_type + " " + arg_val;
                }

                // Determine return type
                std::string ret_type = "void";
                if (func_type.return_type) {
                    ret_type = llvm_type_from_semantic(func_type.return_type);
                }

                // Call the function pointer
                std::string result = fresh_reg();
                if (ret_type == "void") {
                    emit_line("  call void " + receiver + "(" + args_str + ")");
                    last_expr_type_ = "void";
                    return "void";
                } else {
                    emit_line("  " + result + " = call " + ret_type + " " + receiver + "(" +
                              args_str + ")");
                    last_expr_type_ = ret_type;
                    return result;
                }
            }
        }
    }

    // =========================================================================
    // 14. Handle File instance methods
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

    // =========================================================================
    // 15. Handle class instance method calls
    // =========================================================================
    // Unwrap RefType if present (e.g., ref Counter -> Counter)
    types::TypePtr effective_receiver_type = receiver_type;
    if (receiver_type && receiver_type->is<types::RefType>()) {
        effective_receiver_type = receiver_type->as<types::RefType>().inner;
    }
    if (effective_receiver_type && effective_receiver_type->is<types::ClassType>()) {
        const auto& class_type = effective_receiver_type->as<types::ClassType>();
        auto class_def = env_.lookup_class(class_type.name);

        // Check if this is a generic class that needs mangling
        bool is_generic_class =
            !class_type.type_args.empty() ||
            pending_generic_classes_.find(class_type.name) != pending_generic_classes_.end();

        // For generic classes, compute the mangled name for function calls
        std::string mangled_class_name = class_type.name;
        if (!class_type.type_args.empty()) {
            mangled_class_name = mangle_struct_name(class_type.name, class_type.type_args);
        }

        if (class_def.has_value() || is_generic_class) {
            // Use base name for class definition lookup, mangled name for function calls
            std::string current_class = class_type.name;
            std::string current_mangled = mangled_class_name;
            while (!current_class.empty()) {
                // Try to find the class - either from pending generics or environment
                const parser::ClassDecl* parser_class = nullptr;
                std::optional<types::ClassDef> typed_class_opt;

                auto pending_it = pending_generic_classes_.find(current_class);
                if (pending_it != pending_generic_classes_.end()) {
                    parser_class = pending_it->second;
                } else {
                    typed_class_opt = env_.lookup_class(current_class);
                    if (!typed_class_opt.has_value())
                        break;
                }

                // Search for method in parser::ClassDecl (for generic classes)
                if (parser_class) {
                    for (const auto& m : parser_class->methods) {
                        if (m.name == method && !m.is_static) {
                            // Generate call to instance method using mangled name
                            std::string func_name =
                                "@tml_" + get_suite_prefix() + current_mangled + "_" + method;

                            // Resolve return type with type substitutions for generic params
                            std::string ret_type = "i32"; // Default fallback
                            if (m.return_type) {
                                // Build type substitution map from class type args
                                std::unordered_map<std::string, types::TypePtr> type_subs;
                                if (!class_type.type_args.empty() &&
                                    !parser_class->generics.empty()) {
                                    for (size_t i = 0; i < parser_class->generics.size() &&
                                                       i < class_type.type_args.size();
                                         ++i) {
                                        type_subs[parser_class->generics[i].name] =
                                            class_type.type_args[i];
                                    }
                                }
                                auto resolved_ret =
                                    resolve_parser_type_with_subs(**m.return_type, type_subs);
                                ret_type = llvm_type_from_semantic(resolved_ret);
                            }

                            // Use registered function's return type if available
                            std::string method_key = current_mangled + "_" + method;
                            auto method_it = functions_.find(method_key);
                            if (method_it != functions_.end() &&
                                !method_it->second.ret_type.empty()) {
                                ret_type = method_it->second.ret_type;
                            }

                            // Get receiver pointer (for 'this')
                            std::string this_ptr = receiver;

                            // For value classes, use the alloca pointer
                            if (call.receiver->is<parser::IdentExpr>()) {
                                const auto& ident_recv = call.receiver->as<parser::IdentExpr>();
                                auto it = locals_.find(ident_recv.name);
                                if (it != locals_.end()) {
                                    bool is_value_class_struct =
                                        it->second.type.starts_with("%class.") &&
                                        !it->second.type.ends_with("*");
                                    if (is_value_class_struct) {
                                        this_ptr = it->second.reg;
                                    }
                                }
                            }

                            // Handle ref types
                            if (receiver_type && receiver_type->is<types::RefType>()) {
                                std::string loaded_this = fresh_reg();
                                emit_line("  " + loaded_this + " = load ptr, ptr " + receiver);
                                this_ptr = loaded_this;
                            }

                            // Generate arguments: this pointer + regular args
                            std::string args_str = "ptr " + this_ptr;
                            for (const auto& arg : call.args) {
                                std::string arg_val = gen_expr(*arg);
                                args_str += ", " + last_expr_type_ + " " + arg_val;
                            }

                            // Generate call
                            if (ret_type == "void") {
                                emit_line("  call void " + func_name + "(" + args_str + ")");
                                last_expr_type_ = "void";
                                return "void";
                            } else {
                                std::string result = fresh_reg();
                                emit_line("  " + result + " = call " + ret_type + " " + func_name +
                                          "(" + args_str + ")");
                                last_expr_type_ = ret_type;
                                return result;
                            }
                        }
                    }
                    // Move to parent class
                    if (parser_class->extends) {
                        current_class = parser_class->extends->segments.back();
                        current_mangled = current_class;
                    } else {
                        current_class = "";
                    }
                    continue;
                }

                // Search for method in types::ClassDef (for regular classes)
                const auto& typed_class = typed_class_opt.value();
                for (const auto& m : typed_class.methods) {
                    if (m.sig.name == method && !m.is_static) {
                        // Generate call to instance method
                        std::string func_name =
                            "@tml_" + get_suite_prefix() + current_mangled + "_" + method;
                        std::string ret_type = llvm_type_from_semantic(m.sig.return_type);

                        // Use registered function's return type if available (handles value class
                        // by-value returns)
                        std::string method_key = current_mangled + "_" + method;
                        auto method_it = functions_.find(method_key);
                        if (method_it != functions_.end() && !method_it->second.ret_type.empty()) {
                            ret_type = method_it->second.ret_type;
                        }

                        // Get receiver pointer (for 'this')
                        std::string this_ptr = receiver;

                        // For value classes (stored as struct type), use the alloca pointer
                        // For regular classes, gen_ident already loads the pointer value
                        if (call.receiver->is<parser::IdentExpr>()) {
                            const auto& ident_recv = call.receiver->as<parser::IdentExpr>();
                            auto it = locals_.find(ident_recv.name);
                            if (it != locals_.end()) {
                                // Check if this is a value class (struct type, no asterisk)
                                // Value class: type is "%class.Name" -> use the alloca directly
                                // Regular class: type is "%class.Name*" -> receiver already has
                                // loaded ptr
                                bool is_value_class_struct =
                                    it->second.type.starts_with("%class.") &&
                                    !it->second.type.ends_with("*");
                                if (is_value_class_struct) {
                                    // Value class: use the alloca which is a ptr to struct
                                    this_ptr = it->second.reg;
                                }
                                // For regular classes, gen_ident already loads the pointer
                                // so 'receiver' is correct as-is
                            }
                        }
                        // Handle method chaining on value classes: when receiver is a method call
                        // that returns a struct by value, store it to a temp alloca
                        else if (last_expr_type_.starts_with("%class.") &&
                                 !last_expr_type_.ends_with("*")) {
                            // Receiver returned a value class struct - need to store to temp alloca
                            std::string temp_alloca = fresh_reg();
                            emit_line("  " + temp_alloca + " = alloca " + last_expr_type_);
                            emit_line("  store " + last_expr_type_ + " " + receiver + ", ptr " +
                                      temp_alloca);
                            this_ptr = temp_alloca;
                        }

                        // If receiver was 'ref ClassType', we have a pointer to the class variable
                        // which itself holds a pointer. Need to load to get the actual class ptr.
                        if (receiver_type && receiver_type->is<types::RefType>()) {
                            std::string loaded_this = fresh_reg();
                            emit_line("  " + loaded_this + " = load ptr, ptr " + receiver);
                            this_ptr = loaded_this;
                        }

                        // Generate arguments: this pointer + regular args
                        std::string args_str = "ptr " + this_ptr;
                        for (size_t arg_idx = 0; arg_idx < call.args.size(); ++arg_idx) {
                            const auto& arg = call.args[arg_idx];
                            std::string arg_val;
                            std::string arg_type;

                            // Get expected parameter type from method signature
                            std::string expected_param_type = "ptr"; // Default for class params
                            if (arg_idx + 1 < m.sig.params.size()) {
                                expected_param_type =
                                    llvm_type_from_semantic(m.sig.params[arg_idx + 1]);
                            }

                            // For value class arguments (IdentExpr) where method expects ptr,
                            // pass the alloca pointer instead of loading the value
                            if (expected_param_type == "ptr" && arg->is<parser::IdentExpr>()) {
                                const auto& ident_arg = arg->as<parser::IdentExpr>();
                                auto local_it = locals_.find(ident_arg.name);
                                if (local_it != locals_.end() &&
                                    local_it->second.type.starts_with("%class.") &&
                                    !local_it->second.type.ends_with("*")) {
                                    // Value class stored as struct: pass alloca pointer
                                    arg_val = local_it->second.reg;
                                    arg_type = "ptr";
                                } else {
                                    // Not a value class struct: use normal expression generation
                                    arg_val = gen_expr(*arg);
                                    arg_type = last_expr_type_;
                                }
                            } else {
                                arg_val = gen_expr(*arg);
                                arg_type = last_expr_type_;
                            }

                            args_str += ", " + arg_type + " " + arg_val;
                        }

                        // Generate call
                        if (ret_type == "void") {
                            emit_line("  call void " + func_name + "(" + args_str + ")");
                            last_expr_type_ = "void";
                            return "void";
                        } else {
                            std::string result = fresh_reg();
                            emit_line("  " + result + " = call " + ret_type + " " + func_name +
                                      "(" + args_str + ")");
                            last_expr_type_ = ret_type;
                            return result;
                        }
                    }
                }

                // Move to parent class (for types::ClassDef)
                current_class = typed_class.base_class.value_or("");
                current_mangled = current_class; // Parent is not generic in this context
            }
        }
    }

    // =========================================================================
    // 16. Handle NamedType that refers to a class (for method chaining on return values)
    // =========================================================================
    if (effective_receiver_type && effective_receiver_type->is<types::NamedType>()) {
        const auto& named_type = effective_receiver_type->as<types::NamedType>();
        auto class_def = env_.lookup_class(named_type.name);
        if (class_def.has_value()) {
            std::string current_class = named_type.name;
            while (!current_class.empty()) {
                auto current_def = env_.lookup_class(current_class);
                if (!current_def.has_value())
                    break;

                for (const auto& m : current_def->methods) {
                    if (m.sig.name == method && !m.is_static) {
                        // Generate call to instance method
                        std::string func_name =
                            "@tml_" + get_suite_prefix() + current_class + "_" + method;
                        std::string ret_type = llvm_type_from_semantic(m.sig.return_type);

                        // Use registered function's return type if available
                        std::string method_key = current_class + "_" + method;
                        auto method_it = functions_.find(method_key);
                        if (method_it != functions_.end() && !method_it->second.ret_type.empty()) {
                            ret_type = method_it->second.ret_type;
                        }

                        // For method chaining on value class returns, receiver is already a struct
                        // value Need to store it to a temp alloca to get a pointer for 'this'
                        std::string this_ptr = receiver;
                        if (last_expr_type_.starts_with("%class.") &&
                            !last_expr_type_.ends_with("*")) {
                            // Value class struct - store to temp alloca
                            std::string temp_alloca = fresh_reg();
                            emit_line("  " + temp_alloca + " = alloca " + last_expr_type_);
                            emit_line("  store " + last_expr_type_ + " " + receiver + ", ptr " +
                                      temp_alloca);
                            this_ptr = temp_alloca;
                        }

                        // Generate arguments: this pointer + regular args
                        std::string args_str = "ptr " + this_ptr;
                        for (size_t arg_idx = 0; arg_idx < call.args.size(); ++arg_idx) {
                            const auto& arg = call.args[arg_idx];
                            std::string arg_val;
                            std::string arg_type;

                            // Get expected parameter type from method signature
                            std::string expected_param_type = "ptr"; // Default for class params
                            if (arg_idx + 1 < m.sig.params.size()) {
                                expected_param_type =
                                    llvm_type_from_semantic(m.sig.params[arg_idx + 1]);
                            }

                            // For value class arguments (IdentExpr) where method expects ptr,
                            // pass the alloca pointer instead of loading the value
                            if (expected_param_type == "ptr" && arg->is<parser::IdentExpr>()) {
                                const auto& ident_arg = arg->as<parser::IdentExpr>();
                                auto local_it = locals_.find(ident_arg.name);
                                if (local_it != locals_.end() &&
                                    local_it->second.type.starts_with("%class.") &&
                                    !local_it->second.type.ends_with("*")) {
                                    // Value class stored as struct: pass alloca pointer
                                    arg_val = local_it->second.reg;
                                    arg_type = "ptr";
                                } else {
                                    // Not a value class struct: use normal expression generation
                                    arg_val = gen_expr(*arg);
                                    arg_type = last_expr_type_;
                                }
                            } else {
                                arg_val = gen_expr(*arg);
                                arg_type = last_expr_type_;
                            }

                            args_str += ", " + arg_type + " " + arg_val;
                        }

                        // Generate call
                        if (ret_type == "void") {
                            emit_line("  call void " + func_name + "(" + args_str + ")");
                            last_expr_type_ = "void";
                            return "void";
                        } else {
                            std::string result = fresh_reg();
                            emit_line("  " + result + " = call " + ret_type + " " + func_name +
                                      "(" + args_str + ")");
                            last_expr_type_ = ret_type;
                            return result;
                        }
                    }
                }

                // Move to parent class
                current_class = current_def->base_class.value_or("");
            }
        }
    }

    report_error("Unknown method: " + method, call.span);
    return "0";
}

} // namespace tml::codegen
