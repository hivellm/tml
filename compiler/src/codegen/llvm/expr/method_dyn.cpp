//! # LLVM IR Generator - Dynamic Dispatch
//!
//! This file implements dyn trait object method dispatch.
//! Extracted from method.cpp for maintainability.

#include "codegen/llvm/llvm_ir_gen.hpp"

namespace tml::codegen {

auto LLVMIRGen::try_gen_dyn_dispatch_call(const parser::MethodCallExpr& call,
                                          const std::string& /*receiver*/,
                                          types::TypePtr /*receiver_type*/)
    -> std::optional<std::string> {
    const std::string& method = call.method;

    if (!call.receiver->is<parser::IdentExpr>()) {
        return std::nullopt;
    }

    const auto& ident = call.receiver->as<parser::IdentExpr>();
    auto it = locals_.find(ident.name);
    if (it == locals_.end()) {
        return std::nullopt;
    }

    // Check for dyn dispatch - handles both:
    // 1. Direct dyn type: LLVM type is %dyn.Error
    // 2. Reference to dyn: LLVM type is ptr, semantic type is ref dyn Error
    bool is_dyn_dispatch = false;
    std::string behavior_name;
    std::string dyn_type;
    std::string dyn_ptr;

    if (it->second.type.starts_with("%dyn.")) {
        is_dyn_dispatch = true;
        dyn_type = it->second.type;
        behavior_name = dyn_type.substr(5);
        dyn_ptr = it->second.reg;
        emit_dyn_type(behavior_name);
    } else if (it->second.semantic_type) {
        auto sem_type = it->second.semantic_type;
        if (sem_type->is<types::RefType>()) {
            auto inner = sem_type->as<types::RefType>().inner;
            if (inner && inner->is<types::DynBehaviorType>()) {
                is_dyn_dispatch = true;
                behavior_name = inner->as<types::DynBehaviorType>().behavior_name;
                dyn_type = "%dyn." + behavior_name;
                dyn_ptr = it->second.reg;
                emit_dyn_type(behavior_name);
            }
        }
    }

    if (!is_dyn_dispatch) {
        return std::nullopt;
    }

    TML_DEBUG_LN("[DYN] Dyn dispatch detected for behavior: " << behavior_name
                                                              << " method: " << method);
    auto behavior_methods_it = behavior_method_order_.find(behavior_name);

    // If behavior not registered yet, try to look it up and register dynamically
    if (behavior_methods_it == behavior_method_order_.end()) {
        auto behavior_def = env_.lookup_behavior(behavior_name);

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

    if (behavior_methods_it == behavior_method_order_.end()) {
        return std::nullopt;
    }

    const auto& methods = behavior_methods_it->second;
    int method_idx = -1;
    for (size_t i = 0; i < methods.size(); ++i) {
        if (methods[i] == method) {
            method_idx = static_cast<int>(i);
            break;
        }
    }

    if (method_idx < 0) {
        return std::nullopt;
    }

    std::string data_field = fresh_reg();
    emit_line("  " + data_field + " = getelementptr " + dyn_type + ", ptr " + dyn_ptr +
              ", i32 0, i32 0");
    std::string data_ptr = fresh_reg();
    emit_line("  " + data_ptr + " = load ptr, ptr " + data_field);

    std::string vtable_field = fresh_reg();
    emit_line("  " + vtable_field + " = getelementptr " + dyn_type + ", ptr " + dyn_ptr +
              ", i32 0, i32 1");
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
    emit_line("  " + fn_ptr_loc + " = getelementptr " + vtable_type + ", ptr " + vtable_ptr +
              ", i32 0, i32 " + std::to_string(method_idx));
    std::string fn_ptr = fresh_reg();
    emit_line("  " + fn_ptr + " = load ptr, ptr " + fn_ptr_loc);

    // Get method signature from behavior definition
    std::string return_llvm_type = "i32";
    auto behavior_def = env_.lookup_behavior(behavior_name);

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
        std::unordered_map<std::string, types::TypePtr> type_subs;
        if (it->second.semantic_type) {
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
                for (size_t i = 0;
                     i < behavior_def->type_params.size() && i < dyn_sem.type_args.size(); ++i) {
                    type_subs[behavior_def->type_params[i]] = dyn_sem.type_args[i];
                }
            }
        }

        for (const auto& m : behavior_def->methods) {
            if (m.name == method && m.return_type) {
                auto substituted_ret = types::substitute_type(m.return_type, type_subs);
                return_llvm_type = llvm_type_from_semantic(substituted_ret);
                break;
            }
        }
    }

    // Generate method arguments
    std::string args_str = "ptr " + data_ptr;
    for (const auto& arg : call.args) {
        std::string arg_val = gen_expr(*arg);
        std::string arg_type = last_expr_type_;
        args_str += ", " + arg_type + " " + arg_val;
    }

    std::string result = fresh_reg();
    if (return_llvm_type == "void") {
        emit_line("  call void " + fn_ptr + "(" + args_str + ")");
        last_expr_type_ = "void";
        return "";
    } else {
        emit_line("  " + result + " = call " + return_llvm_type + " " + fn_ptr + "(" + args_str +
                  ")");
        last_expr_type_ = return_llvm_type;
        return result;
    }
}

} // namespace tml::codegen
