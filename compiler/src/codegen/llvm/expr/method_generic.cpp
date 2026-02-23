TML_MODULE("codegen_x86")

//! # LLVM IR Generator - Generic & Closure Method Dispatch
//!
//! This file handles two sections from gen_method_call:
//!
//! ## Section 4b: Bounded Generic Method Dispatch
//! Handles method calls on bounded generics (e.g., C: Container[T]).
//! When the receiver is a type parameter with behavior bounds from where clauses,
//! dispatches to the concrete impl method for the substituted type.
//!
//! ## Section 13: Fn Trait Method Calls
//! Handles call(), call_mut(), call_once() on closures and function pointers
//! that implement the Fn, FnMut, FnOnce behaviors.
//!
//! Extracted from method.cpp to reduce file size.

#include "codegen/llvm/llvm_ir_gen.hpp"
#include "types/module.hpp"

#include <iostream>
#include <unordered_set>

namespace tml::codegen {

auto LLVMIRGen::gen_method_bounded_generic_dispatch(
    const parser::MethodCallExpr& call, const std::string& method, const std::string& receiver,
    const std::string& receiver_ptr, const types::TypePtr& receiver_type,
    const std::string& receiver_type_name, bool receiver_was_ref) -> std::optional<std::string> {
    (void)receiver_type;
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
                             << " is_NamedType=" << sub_type->is<types::NamedType>()
                             << " is_PrimitiveType=" << sub_type->is<types::PrimitiveType>());
                if (sub_type->is<types::NamedType>()) {
                    concrete_type_name = sub_type->as<types::NamedType>().name;
                } else if (sub_type->is<types::PrimitiveType>()) {
                    // Handle primitive types (I8, I16, I32, I64, etc.)
                    const auto& prim = sub_type->as<types::PrimitiveType>();
                    concrete_type_name = types::primitive_kind_to_string(prim.kind);
                }
            }
            TML_DEBUG_LN("[METHOD 4b] concrete_type_name=" << concrete_type_name);

            // Skip this constraint if receiver's actual type doesn't match the
            // constraint's concrete type. This prevents matching Y: Debug when the
            // receiver is actually of type R (both bounded by Debug).
            if (!concrete_type_name.empty() && !receiver_type_name.empty() &&
                concrete_type_name != receiver_type_name) {
                TML_DEBUG_LN("[METHOD 4b] SKIP: receiver_type_name=" << receiver_type_name
                                                                     << " != concrete_type_name="
                                                                     << concrete_type_name);
                continue;
            }

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

                            // For primitive types with intrinsic methods (duplicate, to_owned,
                            // etc.), delegate to gen_primitive_method instead of generating a
                            // function call
                            if (sub_it != current_type_subs_.end() &&
                                sub_it->second->is<types::PrimitiveType>()) {
                                // Check if this is an intrinsic primitive method
                                static const std::unordered_set<std::string> primitive_intrinsics =
                                    {"duplicate",   "to_owned",     "borrow", "borrow_mut",
                                     "to_string",   "debug_string", "hash",   "cmp",
                                     "partial_cmp", "add",          "sub",    "mul",
                                     "div",         "rem",          "neg",    "abs",
                                     "eq",          "ne",           "lt",     "le",
                                     "gt",          "ge",           "min",    "max",
                                     "clamp",       "is_zero",      "is_one"};
                                if (primitive_intrinsics.count(method)) {
                                    TML_DEBUG_LN("[METHOD 4b] Delegating primitive method to "
                                                 "gen_primitive_method");
                                    // If receiver was originally a ref T, we need to dereference
                                    // to get the primitive value for methods like to_owned,
                                    // duplicate
                                    std::string actual_receiver = receiver;
                                    if (receiver_was_ref) {
                                        std::string prim_ty =
                                            llvm_type_from_semantic(sub_it->second);
                                        actual_receiver = fresh_reg();
                                        emit_line("  " + actual_receiver + " = load " + prim_ty +
                                                  ", ptr " + receiver);
                                    }
                                    auto prim_result = gen_primitive_method(
                                        call, actual_receiver, receiver_ptr, sub_it->second);
                                    if (prim_result) {
                                        return *prim_result;
                                    }
                                    // If gen_primitive_method didn't handle it, fall through
                                }
                            }

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
                            // Also substitute Self with the concrete type
                            if (sub_it != current_type_subs_.end()) {
                                behavior_subs["Self"] = sub_it->second;
                            }

                            // Look up the impl method: ConcreteType::method
                            std::string qualified_name = concrete_type_name + "::" + method;
                            auto func_sig = env_.lookup_func(qualified_name);
                            bool is_lib_type = this->is_library_method(concrete_type_name, method);

                            // If not found locally, search modules
                            if (!func_sig && env_.module_registry()) {
                                const auto& all_modules = env_.module_registry()->get_all_modules();
                                for (const auto& [mod_name, mod] : all_modules) {
                                    auto func_it = mod.functions.find(qualified_name);
                                    if (func_it != mod.functions.end()) {
                                        func_sig = func_it->second;
                                        is_lib_type = true;
                                        break;
                                    }
                                }
                            }

                            if (func_sig) {
                                // Generate the call to the concrete impl method
                                // Only use suite prefix for test-local methods, not library methods
                                std::string prefix = is_lib_type ? "" : get_suite_prefix();
                                std::string fn_name =
                                    "@tml_" + prefix + concrete_type_name + "_" + method;

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
                                } else if (call.receiver->is<parser::FieldExpr>()) {
                                    // For field expressions:
                                    // - If receiver_ptr is set, use it (pointer to field)
                                    // - Otherwise, spill the struct to stack
                                    if (last_expr_type_ == "ptr") {
                                        this_val = receiver;
                                    } else if (!receiver_ptr.empty()) {
                                        this_val = receiver_ptr;
                                    } else if (last_expr_type_.starts_with("%struct.")) {
                                        // Spill struct to stack for method call
                                        std::string tmp = fresh_reg();
                                        emit_line("  " + tmp + " = alloca " + last_expr_type_);
                                        emit_line("  store " + last_expr_type_ + " " + receiver +
                                                  ", ptr " + tmp);
                                        this_val = tmp;
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
            TML_DEBUG_LN("[METHOD 4b] checking required_behaviors.size="
                         << constraint.required_behaviors.size());
            for (const auto& behavior_name : constraint.required_behaviors) {
                TML_DEBUG_LN("[METHOD 4b] checking required_behavior=" << behavior_name
                                                                       << " for method=" << method);

                // For primitive types with intrinsic methods, delegate to gen_primitive_method
                if (sub_it != current_type_subs_.end() &&
                    sub_it->second->is<types::PrimitiveType>()) {
                    static const std::unordered_set<std::string> primitive_intrinsics = {
                        "duplicate",    "to_owned", "borrow", "borrow_mut",  "to_string",
                        "debug_string", "hash",     "cmp",    "partial_cmp", "add",
                        "sub",          "mul",      "div",    "rem",         "neg",
                        "abs",          "eq",       "ne",     "lt",          "le",
                        "gt",           "ge",       "min",    "max",         "clamp",
                        "is_zero",      "is_one"};
                    if (primitive_intrinsics.count(method)) {
                        TML_DEBUG_LN("[METHOD 4b] Delegating primitive method to "
                                     "gen_primitive_method (required_behaviors)");
                        // If receiver was originally a ref T, we need to dereference
                        // to get the primitive value for methods like to_owned, duplicate
                        std::string actual_receiver = receiver;
                        if (receiver_was_ref) {
                            std::string prim_ty = llvm_type_from_semantic(sub_it->second);
                            actual_receiver = fresh_reg();
                            emit_line("  " + actual_receiver + " = load " + prim_ty + ", ptr " +
                                      receiver);
                        }
                        auto prim_result = gen_primitive_method(call, actual_receiver, receiver_ptr,
                                                                sub_it->second);
                        if (prim_result) {
                            return *prim_result;
                        }
                    }
                }

                // First, try to directly dispatch to ConcreteType::method
                // This handles cases where the behavior definition isn't loaded but the impl exists
                std::string qualified_name = concrete_type_name + "::" + method;
                TML_DEBUG_LN("[METHOD 4b] trying direct qualified_name=" << qualified_name);
                auto func_sig = env_.lookup_func(qualified_name);
                // Check if type is from a library module (works for impl methods too)
                bool is_from_library = this->is_library_method(concrete_type_name, method);

                // If not found locally, search modules
                if (!func_sig && env_.module_registry()) {
                    const auto& all_modules = env_.module_registry()->get_all_modules();
                    for (const auto& [mod_name, mod] : all_modules) {
                        auto func_it = mod.functions.find(qualified_name);
                        if (func_it != mod.functions.end()) {
                            func_sig = func_it->second;
                            is_from_library = true;
                            break;
                        }
                    }
                }

                TML_DEBUG_LN("[METHOD 4b] func_sig found: " << (func_sig ? "yes" : "no"));

                if (func_sig) {
                    // Only use suite prefix for test-local functions, not library methods
                    std::string prefix = is_from_library ? "" : get_suite_prefix();
                    std::string fn_name = "@tml_" + prefix + concrete_type_name + "_" + method;

                    std::vector<std::pair<std::string, std::string>> typed_args;
                    std::string this_val = receiver;
                    if (call.receiver->is<parser::IdentExpr>()) {
                        const auto& ident = call.receiver->as<parser::IdentExpr>();
                        auto it = locals_.find(ident.name);
                        if (it != locals_.end()) {
                            if (it->second.type == "ptr") {
                                this_val = receiver;
                            } else {
                                this_val = it->second.reg;
                            }
                        }
                    } else if (call.receiver->is<parser::FieldExpr>()) {
                        // For field expressions:
                        // - If receiver_ptr is set, use it (pointer to field)
                        // - Otherwise, spill the struct to stack
                        if (last_expr_type_ == "ptr") {
                            this_val = receiver;
                        } else if (!receiver_ptr.empty()) {
                            this_val = receiver_ptr;
                        } else if (last_expr_type_.starts_with("%struct.")) {
                            // Spill struct to stack for method call
                            std::string tmp = fresh_reg();
                            emit_line("  " + tmp + " = alloca " + last_expr_type_);
                            emit_line("  store " + last_expr_type_ + " " + receiver + ", ptr " +
                                      tmp);
                            this_val = tmp;
                        }
                    }

                    // Determine 'this' type based on func_sig first param
                    // For instance methods (self/this), struct types are always passed as ptr
                    // Only primitives are passed by value
                    std::string this_type = "ptr";
                    if (!func_sig->params.empty()) {
                        auto first_param_type = func_sig->params[0];
                        std::string llvm_first = llvm_type_from_semantic(first_param_type);
                        // Primitives (i8, i16, i32, etc.) are passed by value
                        // Structs/classes (%struct.X, %class.X) are passed by ptr
                        if (llvm_first[0] != '%') {
                            this_type = llvm_first; // primitive
                        }
                        // else keep as "ptr" for structs
                    }
                    typed_args.push_back({this_type, this_val});

                    for (size_t i = 0; i < call.args.size(); ++i) {
                        std::string val = gen_expr(*call.args[i]);
                        std::string arg_type = "i32";
                        if (i + 1 < func_sig->params.size()) {
                            arg_type = llvm_type_from_semantic(func_sig->params[i + 1]);
                        }
                        typed_args.push_back({arg_type, val});
                    }

                    // Get return type with Self substitution
                    std::string ret_type = "void";
                    if (func_sig->return_type) {
                        auto return_type = func_sig->return_type;
                        // Substitute Self with the concrete type
                        if (sub_it != current_type_subs_.end()) {
                            std::unordered_map<std::string, types::TypePtr> self_subs;
                            self_subs["Self"] = sub_it->second;
                            return_type = types::substitute_type(return_type, self_subs);
                        }
                        ret_type = llvm_type_from_semantic(return_type);
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
                        emit_line("  " + result + " = call " + ret_type + " " + fn_name + "(" +
                                  args_str + ")");
                        last_expr_type_ = ret_type;
                        return result;
                    }
                }
            }
        }
    }

    return std::nullopt;
}

auto LLVMIRGen::gen_method_fn_trait_call(const parser::MethodCallExpr& call,
                                         const std::string& method, const std::string& receiver,
                                         const types::TypePtr& receiver_type)
    -> std::optional<std::string> {
    // =========================================================================
    // 13. Handle Fn trait method calls on closures and function types
    // =========================================================================
    // Closures and function pointers implement Fn, FnMut, FnOnce
    // call(), call_mut(), call_once() invoke the callable
    if (method == "call" || method == "call_mut" || method == "call_once") {
        if (receiver_type) {
            // Handle ClosureType — receiver is a fat pointer { fn_ptr, env_ptr }
            if (receiver_type->is<types::ClosureType>()) {
                const auto& closure_type = receiver_type->as<types::ClosureType>();
                bool is_capturing = !closure_type.captures.empty();

                // Extract fn_ptr from fat pointer receiver
                std::string fn_ptr = fresh_reg();
                emit_line("  " + fn_ptr + " = extractvalue { ptr, ptr } " + receiver + ", 0");

                // Build argument list
                std::string args_str;
                std::vector<std::string> arg_types;

                if (is_capturing) {
                    // Capturing closure: prepend env_ptr
                    std::string env_ptr = fresh_reg();
                    emit_line("  " + env_ptr + " = extractvalue { ptr, ptr } " + receiver + ", 1");
                    args_str = "ptr " + env_ptr;
                    arg_types.push_back("ptr");
                }

                for (size_t i = 0; i < call.args.size(); ++i) {
                    std::string arg_val = gen_expr(*call.args[i]);
                    std::string arg_type = "i32"; // default
                    if (i < closure_type.params.size()) {
                        arg_type = llvm_type_from_semantic(closure_type.params[i]);
                    }
                    if (!args_str.empty())
                        args_str += ", ";
                    args_str += arg_type + " " + arg_val;
                    arg_types.push_back(arg_type);
                }

                // Determine return type
                std::string ret_type = "i32";
                if (closure_type.return_type) {
                    ret_type = llvm_type_from_semantic(closure_type.return_type);
                }

                // Build function type signature for indirect call
                std::string func_type_sig = ret_type + " (";
                for (size_t i = 0; i < arg_types.size(); ++i) {
                    if (i > 0)
                        func_type_sig += ", ";
                    func_type_sig += arg_types[i];
                }
                func_type_sig += ")";

                // Call through fn_ptr with env as first arg
                std::string result = fresh_reg();
                if (ret_type == "void") {
                    emit_line("  call " + func_type_sig + " " + fn_ptr + "(" + args_str + ")");
                    last_expr_type_ = "void";
                    return "void";
                } else {
                    emit_line("  " + result + " = call " + func_type_sig + " " + fn_ptr + "(" +
                              args_str + ")");
                    last_expr_type_ = ret_type;
                    return result;
                }
            }

            // Handle FuncType (function pointers)
            if (receiver_type->is<types::FuncType>()) {
                const auto& func_type = receiver_type->as<types::FuncType>();

                // Check if receiver is actually a fat pointer (closure stored as func type)
                bool is_fat_ptr = (last_expr_type_ == "{ ptr, ptr }");

                // Check if it's a capturing closure via receiver VarInfo
                bool is_capturing = false;
                if (is_fat_ptr && call.receiver->is<parser::IdentExpr>()) {
                    const auto& ident = call.receiver->as<parser::IdentExpr>();
                    auto var_it = locals_.find(ident.name);
                    if (var_it != locals_.end()) {
                        is_capturing = var_it->second.is_capturing_closure;
                    }
                }

                std::string call_target = receiver;
                std::string args_str;
                std::vector<std::string> arg_types;

                if (is_fat_ptr) {
                    // Fat pointer — extract fn_ptr
                    std::string fn_ptr = fresh_reg();
                    emit_line("  " + fn_ptr + " = extractvalue { ptr, ptr } " + receiver + ", 0");
                    call_target = fn_ptr;

                    if (is_capturing) {
                        // Capturing closure: also extract and prepend env_ptr
                        std::string env_ptr = fresh_reg();
                        emit_line("  " + env_ptr + " = extractvalue { ptr, ptr } " + receiver +
                                  ", 1");
                        args_str = "ptr " + env_ptr;
                        arg_types.push_back("ptr");
                    }
                }

                // Generate user arguments
                for (size_t i = 0; i < call.args.size(); ++i) {
                    std::string arg_val = gen_expr(*call.args[i]);
                    std::string arg_type = "i32"; // default
                    if (i < func_type.params.size()) {
                        arg_type = llvm_type_from_semantic(func_type.params[i]);
                    }
                    if (!args_str.empty())
                        args_str += ", ";
                    args_str += arg_type + " " + arg_val;
                    arg_types.push_back(arg_type);
                }

                // Determine return type
                std::string ret_type = "void";
                if (func_type.return_type) {
                    ret_type = llvm_type_from_semantic(func_type.return_type);
                }

                // Build function type signature for indirect call
                std::string func_type_sig = ret_type + " (";
                for (size_t i = 0; i < arg_types.size(); ++i) {
                    if (i > 0)
                        func_type_sig += ", ";
                    func_type_sig += arg_types[i];
                }
                func_type_sig += ")";

                // Call the function pointer
                std::string result = fresh_reg();
                if (ret_type == "void") {
                    emit_line("  call " + func_type_sig + " " + call_target + "(" + args_str + ")");
                    last_expr_type_ = "void";
                    return "void";
                } else {
                    emit_line("  " + result + " = call " + func_type_sig + " " + call_target + "(" +
                              args_str + ")");
                    last_expr_type_ = ret_type;
                    return result;
                }
            }
        }

        // Fallback: if no semantic type but last_expr_type_ is fat pointer
        if (last_expr_type_ == "{ ptr, ptr }") {
            // Check if capturing via VarInfo
            bool is_capturing = false;
            if (call.receiver->is<parser::IdentExpr>()) {
                const auto& ident = call.receiver->as<parser::IdentExpr>();
                auto var_it = locals_.find(ident.name);
                if (var_it != locals_.end()) {
                    is_capturing = var_it->second.is_capturing_closure;
                }
            }

            std::string fn_ptr = fresh_reg();
            emit_line("  " + fn_ptr + " = extractvalue { ptr, ptr } " + receiver + ", 0");

            std::string args_str;
            std::vector<std::string> arg_types;
            if (is_capturing) {
                std::string env_ptr = fresh_reg();
                emit_line("  " + env_ptr + " = extractvalue { ptr, ptr } " + receiver + ", 1");
                args_str = "ptr " + env_ptr;
                arg_types.push_back("ptr");
            }
            for (size_t i = 0; i < call.args.size(); ++i) {
                std::string arg_val = gen_expr(*call.args[i]);
                std::string arg_type = last_expr_type_.empty() ? "i32" : last_expr_type_;
                if (!args_str.empty())
                    args_str += ", ";
                args_str += arg_type + " " + arg_val;
                arg_types.push_back(arg_type);
            }

            std::string ret_type = "i32"; // default
            std::string func_type_sig = ret_type + " (";
            for (size_t i = 0; i < arg_types.size(); ++i) {
                if (i > 0)
                    func_type_sig += ", ";
                func_type_sig += arg_types[i];
            }
            func_type_sig += ")";

            std::string result = fresh_reg();
            emit_line("  " + result + " = call " + func_type_sig + " " + fn_ptr + "(" + args_str +
                      ")");
            last_expr_type_ = ret_type;
            return result;
        }
    }

    return std::nullopt;
}

} // namespace tml::codegen
