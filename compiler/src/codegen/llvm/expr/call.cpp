TML_MODULE("codegen_x86")

//! # LLVM IR Generator - Function Call Dispatcher
//!
//! This file implements the main function call dispatch logic.
//!
//! ## Call Resolution Order
//!
//! `gen_call()` resolves calls in this priority:
//!
//! 1. **Primitive static methods**: `I32::default()`, `Bool::default()`
//! 2. **Enum constructors**: `Maybe::Just(x)`, `Outcome::Ok(v)`
//! 3. **Builtin functions**: print, panic, assert, math, etc.
//! 4. **Generic functions**: Instantiate and call monomorphized version
//! 5. **User-defined functions**: Direct call to defined function
//! 6. **Indirect calls**: Call through function pointer
//!
//! ## Path Expressions
//!
//! Path expressions like `Type::method` or `Module::func` are resolved
//! by joining segments with `::` and looking up the mangled name.
//!
//! ## Generic Instantiation
//!
//! Generic calls trigger monomorphization - a specialized version of
//! the function is generated for the concrete type arguments.

#include "codegen/llvm/llvm_ir_gen.hpp"

namespace tml::codegen {

auto LLVMIRGen::gen_call(const parser::CallExpr& call) -> std::string {
    // Clear expected literal type context - it should only apply within explicit type annotations
    // (like "let x: F64 = 5") and not leak into function call arguments
    expected_literal_type_.clear();
    expected_literal_is_unsigned_ = false;

    // Get function name
    std::string fn_name;
    if (call.callee->is<parser::IdentExpr>()) {
        fn_name = call.callee->as<parser::IdentExpr>().name;
    } else if (call.callee->is<parser::PathExpr>()) {
        // Handle path expressions like Instant::now, Duration::as_millis_f64
        const auto& path = call.callee->as<parser::PathExpr>().path;
        // Join segments with ::
        for (size_t i = 0; i < path.segments.size(); ++i) {
            if (i > 0)
                fn_name += "::";
            fn_name += path.segments[i];
        }
    }
    if (!call.callee->is<parser::IdentExpr>() && !call.callee->is<parser::PathExpr>() &&
        !call.callee->is<parser::FieldExpr>()) {
        report_error("Complex callee not supported", call.span, "C002");
        return "0";
    }

    if (call.callee->is<parser::FieldExpr>()) {
        // Handle calling function pointers stored in struct fields: cb.action(21)
        // Function pointer fields are stored as fat pointers { fn_ptr, env_ptr }
        // to support both plain function pointers (env=null) and capturing closures
        std::string fat_ptr_val = gen_expr(*call.callee);
        std::string callee_type = last_expr_type_;

        // Infer the function type from the field
        types::TypePtr func_type = infer_expr_type(*call.callee);
        if (func_type && func_type->is<types::FuncType>()) {
            const auto& ft = func_type->as<types::FuncType>();

            // Extract function pointer and environment pointer from fat pointer
            std::string fn_ptr, env_ptr;
            if (callee_type == "{ ptr, ptr }") {
                fn_ptr = fresh_reg();
                emit_line("  " + fn_ptr + " = extractvalue { ptr, ptr } " + fat_ptr_val + ", 0");
                env_ptr = fresh_reg();
                emit_line("  " + env_ptr + " = extractvalue { ptr, ptr } " + fat_ptr_val + ", 1");
            } else {
                // Fallback: treat as thin pointer (backwards compat)
                fn_ptr = fat_ptr_val;
                env_ptr = "";
            }

            // Build user argument list, coercing to declared param types
            std::vector<std::string> arg_vals;
            std::vector<std::string> arg_types;
            for (size_t i = 0; i < call.args.size(); ++i) {
                std::string val = gen_expr(*call.args[i]);
                std::string arg_type = last_expr_type_;
                // Coerce integer arguments to match declared parameter type
                if (i < ft.params.size()) {
                    std::string decl_type = llvm_type_from_semantic(ft.params[i]);
                    auto int_bits = [](const std::string& t) -> int {
                        if (t.size() > 1 && t[0] == 'i' &&
                            std::isdigit(static_cast<unsigned char>(t[1])))
                            return std::stoi(t.substr(1));
                        return -1;
                    };
                    int src_bits = int_bits(arg_type);
                    int dst_bits = int_bits(decl_type);
                    if (src_bits > 0 && dst_bits > src_bits) {
                        std::string coerced = fresh_reg();
                        emit_line("  " + coerced + " = sext " + arg_type + " " + val + " to " +
                                  decl_type);
                        val = coerced;
                        arg_type = decl_type;
                    }
                }
                arg_vals.push_back(val);
                arg_types.push_back(arg_type);
            }

            // Build call signature
            std::string ret_type =
                ft.return_type ? llvm_type_from_semantic(ft.return_type) : "void";

            if (!env_ptr.empty()) {
                // Fat pointer call: check if env is null to determine calling convention
                // Non-null env -> capturing closure: call fn(env, args...)
                // Null env -> plain function: call fn(args...)
                std::string is_null = fresh_reg();
                emit_line("  " + is_null + " = icmp eq ptr " + env_ptr + ", null");

                std::string label_thin = "fp_thin" + std::to_string(label_counter_);
                std::string label_fat = "fp_fat" + std::to_string(label_counter_);
                std::string label_merge = "fp_merge" + std::to_string(label_counter_);
                label_counter_++;

                emit_line("  br i1 " + is_null + ", label %" + label_thin + ", label %" +
                          label_fat);

                // Thin call (no env)
                emit_line(label_thin + ":");
                std::string args_str_thin;
                for (size_t i = 0; i < arg_vals.size(); ++i) {
                    if (i > 0)
                        args_str_thin += ", ";
                    args_str_thin += arg_types[i] + " " + arg_vals[i];
                }
                std::string thin_result;
                if (ret_type == "void") {
                    emit_line("  call void " + fn_ptr + "(" + args_str_thin + ")");
                } else {
                    thin_result = fresh_reg();
                    emit_line("  " + thin_result + " = call " + ret_type + " " + fn_ptr + "(" +
                              args_str_thin + ")");
                }
                emit_line("  br label %" + label_merge);

                // Fat call (with env as first arg)
                emit_line(label_fat + ":");
                std::string args_str_fat = "ptr " + env_ptr;
                for (size_t i = 0; i < arg_vals.size(); ++i) {
                    args_str_fat += ", ";
                    args_str_fat += arg_types[i] + " " + arg_vals[i];
                }
                std::string fat_result;
                if (ret_type == "void") {
                    emit_line("  call void " + fn_ptr + "(" + args_str_fat + ")");
                } else {
                    fat_result = fresh_reg();
                    emit_line("  " + fat_result + " = call " + ret_type + " " + fn_ptr + "(" +
                              args_str_fat + ")");
                }
                emit_line("  br label %" + label_merge);

                // Merge
                emit_line(label_merge + ":");
                if (ret_type == "void") {
                    last_expr_type_ = "void";
                    return "void";
                } else {
                    std::string phi_result = fresh_reg();
                    emit_line("  " + phi_result + " = phi " + ret_type + " [ " + thin_result +
                              ", %" + label_thin + " ], [ " + fat_result + ", %" + label_fat +
                              " ]");
                    last_expr_type_ = ret_type;
                    return phi_result;
                }
            } else {
                // Thin pointer fallback
                std::string args_str;
                for (size_t i = 0; i < arg_vals.size(); ++i) {
                    if (i > 0)
                        args_str += ", ";
                    args_str += arg_types[i] + " " + arg_vals[i];
                }
                if (ret_type == "void") {
                    emit_line("  call void " + fn_ptr + "(" + args_str + ")");
                    last_expr_type_ = "void";
                    return "void";
                } else {
                    std::string result = fresh_reg();
                    emit_line("  " + result + " = call " + ret_type + " " + fn_ptr + "(" +
                              args_str + ")");
                    last_expr_type_ = ret_type;
                    return result;
                }
            }
        }

        report_error("Cannot call non-function field", call.span, "C024");
        return "0";
    }

    // ============ PRIMITIVE TYPES & INTRINSICS ============
    // Delegated to call_primitive.cpp
    if (auto result = gen_call_primitive_or_intrinsic(call, fn_name)) {
        return *result;
    }

    // ============ BUILTIN HANDLERS ============
    // Try each category of builtins. If any handler returns a value, use it.

    // Try intrinsics first (unreachable, assume, etc.)
    if (auto result = try_gen_intrinsic(fn_name, call)) {
        return *result;
    }

    if (auto result = try_gen_builtin_io(fn_name, call)) {
        return *result;
    }

    if (auto result = try_gen_builtin_mem(fn_name, call)) {
        return *result;
    }

    if (auto result = try_gen_builtin_atomic(fn_name, call)) {
        return *result;
    }

    if (auto result = try_gen_builtin_sync(fn_name, call)) {
        return *result;
    }

    // Phase 25: time builtins removed — time_ns/sleep_ms now @extern in std::time

    if (auto result = try_gen_builtin_math(fn_name, call)) {
        return *result;
    }

    if (auto result = try_gen_builtin_string(fn_name, call)) {
        return *result;
    }

    if (auto result = try_gen_builtin_assert(fn_name, call)) {
        return *result;
    }

    if (auto result = try_gen_builtin_async(fn_name, call)) {
        return *result;
    }

    // ============ ENUM CONSTRUCTORS ============
    // Delegated to call_enum.cpp
    if (auto result = gen_call_enum_constructor(call, fn_name)) {
        return *result;
    }

    // ============ INDIRECT FUNCTION POINTER CALLS ============
    // Delegated to call_indirect.cpp
    if (auto result = gen_call_indirect(call, fn_name)) {
        return *result;
    }

    // ============ GENERIC FUNCTION CALLS ============
    // Delegated to call_generic_func.cpp
    if (auto result = gen_call_generic_func(call, fn_name)) {
        return *result;
    }

    // ============ CLASS CONSTRUCTOR CALLS ============
    // Delegated to call_class.cpp
    if (auto result = gen_call_class_constructor(call, fn_name)) {
        return *result;
    }

    // ============ GENERIC CLASS STATIC METHODS ============
    // Handle calls like Utils::identity[I32](42) where identity is a generic static method
    if (call.callee->is<parser::PathExpr>()) {
        const auto& path_expr = call.callee->as<parser::PathExpr>();
        const auto& path = path_expr.path;
        if (path.segments.size() == 2 && path_expr.generics.has_value()) {
            const std::string& class_name = path.segments[0];
            const std::string& method_name = path.segments[1];
            const auto& gen_args = path_expr.generics->args;

            // Check if this is a generic class static method
            std::string method_key = class_name + "::" + method_name;
            auto pending_it = pending_generic_class_methods_.find(method_key);
            if (pending_it != pending_generic_class_methods_.end()) {
                const auto& pending = pending_it->second;
                const auto& method = pending.class_decl->methods[pending.method_index];

                // Build type substitutions from generic arguments
                // IMPORTANT: Use current_type_subs_ to resolve type parameters like T -> I32
                std::unordered_map<std::string, types::TypePtr> type_subs;
                for (size_t i = 0; i < method.generics.size() && i < gen_args.size(); ++i) {
                    if (!method.generics[i].is_const && gen_args[i].is_type()) {
                        type_subs[method.generics[i].name] = resolve_parser_type_with_subs(
                            *gen_args[i].as_type(), current_type_subs_);
                    }
                }

                // Build mangled name suffix (e.g., "_I32" for identity[I32])
                std::vector<types::TypePtr> method_type_args;
                for (const auto& arg : gen_args) {
                    if (arg.is_type()) {
                        auto sem_type =
                            resolve_parser_type_with_subs(*arg.as_type(), current_type_subs_);
                        method_type_args.push_back(sem_type);
                    }
                }
                std::string type_suffix =
                    method_type_args.empty() ? "" : "__" + mangle_type_args(method_type_args);

                // Generate mangled function name
                std::string mangled_func =
                    "@" + mangle_impl_method(class_name, method_name + type_suffix);

                // Queue the instantiation for later (after current function)
                if (generated_functions_.find(mangled_func) == generated_functions_.end()) {
                    pending_generic_class_method_insts_.push_back(PendingGenericClassMethodInst{
                        pending.class_decl, &method, type_suffix, type_subs});
                    generated_functions_.insert(mangled_func);
                }

                // Generate arguments
                std::vector<std::string> args;
                std::vector<std::string> arg_types;
                for (const auto& arg : call.args) {
                    args.push_back(gen_expr(*arg));
                    arg_types.push_back(last_expr_type_);
                }

                // Determine return type with substitution
                std::string ret_type = "void";
                if (method.return_type) {
                    auto sem_ret =
                        resolve_parser_type_with_subs(*method.return_type.value(), type_subs);
                    ret_type = llvm_type_from_semantic(sem_ret);
                }

                // Generate call
                std::string result = fresh_reg();
                std::string call_str =
                    "  " + result + " = call " + ret_type + " " + mangled_func + "(";
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

    // ============ GENERIC STRUCT STATIC METHODS ============
    // Delegated to call_generic_struct.cpp
    if (auto result = gen_call_generic_struct_method(call, fn_name)) {
        return *result;
    }

    // ============ USER-DEFINED FUNCTIONS ============
    // Delegated to call_user.cpp
    return gen_call_user_function(call, fn_name);
}

} // namespace tml::codegen
