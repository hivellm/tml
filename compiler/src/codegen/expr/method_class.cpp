//! # LLVM IR Generator - Class Instance Method Calls
//!
//! This file handles method calls on class instances.
//! Extracted from method.cpp for maintainability.

#include "codegen/llvm_ir_gen.hpp"

namespace tml::codegen {

auto LLVMIRGen::try_gen_class_instance_call(const parser::MethodCallExpr& call,
                                            const std::string& receiver,
                                            const std::string& /*receiver_ptr*/,
                                            types::TypePtr receiver_type)
    -> std::optional<std::string> {
    const std::string& method = call.method;

    // Unwrap RefType if present (e.g., ref Counter -> Counter)
    types::TypePtr effective_receiver_type = receiver_type;
    if (receiver_type && receiver_type->is<types::RefType>()) {
        effective_receiver_type = receiver_type->as<types::RefType>().inner;
    }

    // =========================================================================
    // Handle ClassType receivers
    // =========================================================================
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
                            // Only use suite prefix for test-local methods
                            std::string prefix = is_library_method(current_mangled, method)
                                                     ? ""
                                                     : get_suite_prefix();
                            std::string func_name =
                                "@tml_" + prefix + current_mangled + "_" + method;

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
                                return std::string("void");
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
                        // Only use suite prefix for test-local methods
                        std::string prefix =
                            is_library_method(current_mangled, method) ? "" : get_suite_prefix();
                        std::string func_name = "@tml_" + prefix + current_mangled + "_" + method;
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
                            return std::string("void");
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
    // Handle NamedType that refers to a class (for method chaining on return values)
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
                        // Only use suite prefix for test-local methods
                        std::string prefix =
                            is_library_method(current_class, method) ? "" : get_suite_prefix();
                        std::string func_name = "@tml_" + prefix + current_class + "_" + method;
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
                            return std::string("void");
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

    return std::nullopt;
}

} // namespace tml::codegen
