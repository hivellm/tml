TML_MODULE("codegen_x86")

//! # LLVM IR Generator - Indirect Function Pointer Calls
//!
//! This file handles indirect calls through function pointer variables.
//! Two calling conventions are supported:
//!
//! - **Fat pointer** (`{ ptr, ptr }`): closure with fn_ptr + env_ptr.
//!   A runtime null-check on env_ptr determines thin vs fat dispatch.
//! - **Thin pointer** (`ptr`): plain function pointer, no environment.
//!
//! Both paths coerce integer arguments to match declared parameter types
//! from the semantic FuncType/ClosureType attached to the local variable.

#include "codegen/llvm/llvm_ir_gen.hpp"

#include <cctype>

namespace tml::codegen {

auto LLVMIRGen::gen_call_indirect(const parser::CallExpr& call, const std::string& fn_name)
    -> std::optional<std::string> {
    // Check if this is an indirect call through a function pointer variable
    auto local_it = locals_.find(fn_name);

    // Fat pointer closure call: variable type is "{ ptr, ptr }" (fn_ptr + env_ptr)
    if (local_it != locals_.end() && local_it->second.type == "{ ptr, ptr }") {
        bool is_capturing = local_it->second.is_capturing_closure;

        // Load the fat pointer from the alloca
        std::string fat_ptr = fresh_reg();
        emit_line("  " + fat_ptr + " = load { ptr, ptr }, ptr " + local_it->second.reg);

        // Extract fn_ptr and env_ptr
        std::string fn_ptr = fresh_reg();
        emit_line("  " + fn_ptr + " = extractvalue { ptr, ptr } " + fat_ptr + ", 0");
        std::string env_ptr = fresh_reg();
        emit_line("  " + env_ptr + " = extractvalue { ptr, ptr } " + fat_ptr + ", 1");

        // Extract declared parameter types and return type from semantic type
        std::vector<types::TypePtr> declared_params;
        std::string ret_type = "void";
        if (local_it->second.semantic_type) {
            if (local_it->second.semantic_type->is<types::FuncType>()) {
                const auto& func_type = local_it->second.semantic_type->as<types::FuncType>();
                ret_type = llvm_type_from_semantic(func_type.return_type);
                declared_params = func_type.params;
            } else if (local_it->second.semantic_type->is<types::ClosureType>()) {
                const auto& closure_type = local_it->second.semantic_type->as<types::ClosureType>();
                ret_type = llvm_type_from_semantic(closure_type.return_type);
                declared_params = closure_type.params;
            }
        }

        // Generate user arguments, coercing to declared param types
        std::vector<std::pair<std::string, std::string>> user_args;
        for (size_t i = 0; i < call.args.size(); ++i) {
            std::string val = gen_expr(*call.args[i]);
            std::string arg_type = last_expr_type_;
            // Coerce integer arguments to match declared parameter type
            if (i < declared_params.size()) {
                std::string decl_type = llvm_type_from_semantic(declared_params[i]);
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
            user_args.push_back({val, arg_type});
        }

        if (is_capturing) {
            // Known capturing closure: call fn(env, args...)
            std::vector<std::pair<std::string, std::string>> arg_vals;
            arg_vals.push_back({env_ptr, "ptr"});
            arg_vals.insert(arg_vals.end(), user_args.begin(), user_args.end());

            std::string func_type_sig = ret_type + " (";
            for (size_t i = 0; i < arg_vals.size(); ++i) {
                if (i > 0)
                    func_type_sig += ", ";
                func_type_sig += arg_vals[i].second;
            }
            func_type_sig += ")";

            if (ret_type == "void") {
                emit("  call " + func_type_sig + " " + fn_ptr + "(");
                for (size_t i = 0; i < arg_vals.size(); ++i) {
                    if (i > 0)
                        emit(", ");
                    emit(arg_vals[i].second + " " + arg_vals[i].first);
                }
                emit_line(")");
                last_expr_type_ = "void";
                return "0";
            }

            std::string result = fresh_reg();
            emit("  " + result + " = call " + func_type_sig + " " + fn_ptr + "(");
            for (size_t i = 0; i < arg_vals.size(); ++i) {
                if (i > 0)
                    emit(", ");
                emit(arg_vals[i].second + " " + arg_vals[i].first);
            }
            emit_line(")");
            last_expr_type_ = ret_type;
            return result;
        } else {
            // Unknown or non-capturing: runtime null-check on env_ptr
            // Non-null env -> capturing closure: call fn(env, args...)
            // Null env -> plain function: call fn(args...)
            std::string is_null = fresh_reg();
            emit_line("  " + is_null + " = icmp eq ptr " + env_ptr + ", null");

            std::string label_thin = "fp_thin" + std::to_string(label_counter_);
            std::string label_fat = "fp_fat" + std::to_string(label_counter_);
            std::string label_merge = "fp_merge" + std::to_string(label_counter_);
            label_counter_++;

            emit_line("  br i1 " + is_null + ", label %" + label_thin + ", label %" + label_fat);

            // Thin call (no env)
            emit_line(label_thin + ":");
            std::string args_str_thin;
            for (size_t i = 0; i < user_args.size(); ++i) {
                if (i > 0)
                    args_str_thin += ", ";
                args_str_thin += user_args[i].second + " " + user_args[i].first;
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
            for (size_t i = 0; i < user_args.size(); ++i) {
                args_str_fat += ", ";
                args_str_fat += user_args[i].second + " " + user_args[i].first;
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
                return "0";
            } else {
                std::string phi_result = fresh_reg();
                emit_line("  " + phi_result + " = phi " + ret_type + " [ " + thin_result + ", %" +
                          label_thin + " ], [ " + fat_result + ", %" + label_fat + " ]");
                last_expr_type_ = ret_type;
                return phi_result;
            }
        }
    }

    // Thin function pointer call: variable type is "ptr" (plain func pointer, no env)
    if (local_it != locals_.end() && local_it->second.type == "ptr") {
        // This is a plain function pointer variable - generate indirect call
        std::string fn_ptr;
        if (local_it->second.reg[0] == '@') {
            fn_ptr = local_it->second.reg;
        } else {
            fn_ptr = fresh_reg();
            emit_line("  " + fn_ptr + " = load ptr, ptr " + local_it->second.reg);
        }

        // Generate arguments (no env pointer for thin function pointers)
        std::vector<std::pair<std::string, std::string>> arg_vals;

        // Legacy: prepend captured variables if present (backward compat)
        if (local_it->second.closure_captures.has_value()) {
            const auto& captures = local_it->second.closure_captures.value();
            for (size_t i = 0; i < captures.captured_names.size(); ++i) {
                const std::string& cap_name = captures.captured_names[i];
                const std::string& cap_type = captures.captured_types[i];
                auto cap_it = locals_.find(cap_name);
                if (cap_it != locals_.end()) {
                    std::string cap_val = fresh_reg();
                    emit_line("  " + cap_val + " = load " + cap_type + ", ptr " +
                              cap_it->second.reg);
                    arg_vals.push_back({cap_val, cap_type});
                } else {
                    arg_vals.push_back({"0", cap_type});
                }
            }
        }

        // Extract declared parameter types and return type from semantic type
        std::vector<types::TypePtr> thin_declared_params;
        std::string ret_type = "void";
        if (local_it->second.semantic_type) {
            if (local_it->second.semantic_type->is<types::FuncType>()) {
                const auto& func_type = local_it->second.semantic_type->as<types::FuncType>();
                ret_type = llvm_type_from_semantic(func_type.return_type);
                thin_declared_params = func_type.params;
            } else if (local_it->second.semantic_type->is<types::ClosureType>()) {
                const auto& closure_type = local_it->second.semantic_type->as<types::ClosureType>();
                ret_type = llvm_type_from_semantic(closure_type.return_type);
                thin_declared_params = closure_type.params;
            }
        }

        for (size_t i = 0; i < call.args.size(); ++i) {
            std::string val = gen_expr(*call.args[i]);
            std::string arg_type = last_expr_type_;
            // Coerce integer arguments to match declared parameter type
            if (i < thin_declared_params.size()) {
                std::string decl_type = llvm_type_from_semantic(thin_declared_params[i]);
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
            arg_vals.push_back({val, arg_type});
        }

        std::string func_type_sig = ret_type + " (";
        for (size_t i = 0; i < arg_vals.size(); ++i) {
            if (i > 0)
                func_type_sig += ", ";
            func_type_sig += arg_vals[i].second;
        }
        func_type_sig += ")";

        if (ret_type == "void") {
            emit("  call " + func_type_sig + " " + fn_ptr + "(");
            for (size_t i = 0; i < arg_vals.size(); ++i) {
                if (i > 0)
                    emit(", ");
                emit(arg_vals[i].second + " " + arg_vals[i].first);
            }
            emit_line(")");
            last_expr_type_ = "void";
            return "0";
        }

        std::string result = fresh_reg();
        emit("  " + result + " = call " + func_type_sig + " " + fn_ptr + "(");
        for (size_t i = 0; i < arg_vals.size(); ++i) {
            if (i > 0)
                emit(", ");
            emit(arg_vals[i].second + " " + arg_vals[i].first);
        }
        emit_line(")");
        last_expr_type_ = ret_type;
        return result;
    }

    return std::nullopt;
}

} // namespace tml::codegen
