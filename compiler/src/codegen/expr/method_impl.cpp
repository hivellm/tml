//! # LLVM IR Generator - Impl Method Calls
//!
//! This file implements user-defined impl method resolution and codegen.
//! Extracted from method.cpp for maintainability.
//!
//! ## Coverage
//!
//! - Local impl methods (pending_generic_impls_)
//! - Imported module impl methods
//! - Generic type instantiation
//! - Method-level type arguments

#include "codegen/llvm_ir_gen.hpp"
#include "types/module.hpp"

namespace tml::codegen {

auto LLVMIRGen::try_gen_impl_method_call(const parser::MethodCallExpr& call,
                                         const std::string& receiver,
                                         const std::string& receiver_ptr,
                                         types::TypePtr receiver_type)
    -> std::optional<std::string> {
    const std::string& method = call.method;

    // Only handle NamedType receivers
    if (!receiver_type || !receiver_type->is<types::NamedType>()) {
        return std::nullopt;
    }

    const auto& named = receiver_type->as<types::NamedType>();
    bool is_builtin_type = (named.name == "List" || named.name == "HashMap" ||
                            named.name == "Buffer" || named.name == "File" || named.name == "Path");
    bool is_slice_inlined = (named.name == "Slice" || named.name == "MutSlice") &&
                            (method == "len" || method == "is_empty");

    if (is_builtin_type || is_slice_inlined) {
        return std::nullopt;
    }

    std::string qualified_name = named.name + "::" + method;
    auto func_sig = env_.lookup_func(qualified_name);

    if (!func_sig) {
        // Try module lookup
        if (env_.module_registry()) {
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

    if (!func_sig) {
        return std::nullopt;
    }

    std::string mangled_type_name = named.name;
    std::unordered_map<std::string, types::TypePtr> type_subs;
    std::string method_type_suffix;
    bool is_imported = false;

    // Handle method-level generic type arguments
    if (!call.type_args.empty() && !func_sig->type_params.empty()) {
        size_t impl_param_count = named.type_args.size();
        for (size_t i = 0; i < call.type_args.size(); ++i) {
            size_t param_idx = impl_param_count + i;
            if (param_idx < func_sig->type_params.size()) {
                auto semantic_type =
                    resolve_parser_type_with_subs(*call.type_args[i], current_type_subs_);
                if (semantic_type) {
                    type_subs[func_sig->type_params[param_idx]] = semantic_type;
                    if (!method_type_suffix.empty()) {
                        method_type_suffix += "_";
                    }
                    method_type_suffix += mangle_type(semantic_type);
                }
            }
        }
    }
    // Infer method-level type parameters from argument types
    else if (call.type_args.empty() && !func_sig->type_params.empty()) {
        size_t impl_param_count = named.type_args.size();
        for (size_t tp_idx = impl_param_count; tp_idx < func_sig->type_params.size(); ++tp_idx) {
            const std::string& type_param = func_sig->type_params[tp_idx];
            for (size_t p_idx = 1; p_idx < func_sig->params.size() && p_idx - 1 < call.args.size();
                 ++p_idx) {
                const auto& param_type = func_sig->params[p_idx];
                if (param_type && param_type->is<types::NamedType>()) {
                    const auto& param_named = param_type->as<types::NamedType>();
                    for (size_t ta_idx = 0; ta_idx < param_named.type_args.size(); ++ta_idx) {
                        const auto& ta = param_named.type_args[ta_idx];
                        if (ta && ta->is<types::NamedType>()) {
                            const auto& ta_named = ta->as<types::NamedType>();
                            if (ta_named.name == type_param) {
                                auto arg_type = infer_expr_type(*call.args[p_idx - 1]);
                                if (arg_type && arg_type->is<types::NamedType>()) {
                                    const auto& arg_named = arg_type->as<types::NamedType>();
                                    if (ta_idx < arg_named.type_args.size()) {
                                        auto inferred = arg_named.type_args[ta_idx];
                                        if (inferred) {
                                            type_subs[type_param] = inferred;
                                            if (!method_type_suffix.empty()) {
                                                method_type_suffix += "_";
                                            }
                                            method_type_suffix += mangle_type(inferred);
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    // Handle generic type arguments
    if (!named.type_args.empty()) {
        mangled_type_name = mangle_struct_name(named.name, named.type_args);
        std::string method_for_key = method;
        if (!method_type_suffix.empty()) {
            method_for_key += "__" + method_type_suffix;
        }
        std::string mangled_method_name = "tml_" + mangled_type_name + "_" + method_for_key;

        // Check locally defined impls first
        auto impl_it = pending_generic_impls_.find(named.name);
        if (impl_it != pending_generic_impls_.end()) {
            const auto& impl = *impl_it->second;
            for (size_t i = 0; i < impl.generics.size() && i < named.type_args.size(); ++i) {
                type_subs[impl.generics[i].name] = named.type_args[i];
            }
        }

        // Check imported structs for type params
        std::vector<std::string> imported_type_params;
        if (impl_it == pending_generic_impls_.end() && env_.module_registry()) {
            const auto& all_modules = env_.module_registry()->get_all_modules();
            for (const auto& [mod_name, mod] : all_modules) {
                auto struct_it = mod.structs.find(named.name);
                if (struct_it != mod.structs.end() && !struct_it->second.type_params.empty()) {
                    imported_type_params = struct_it->second.type_params;
                    for (size_t i = 0;
                         i < imported_type_params.size() && i < named.type_args.size(); ++i) {
                        type_subs[imported_type_params[i]] = named.type_args[i];
                        if (named.type_args[i] && named.type_args[i]->is<types::NamedType>()) {
                            const auto& arg_named = named.type_args[i]->as<types::NamedType>();
                            auto item_type = lookup_associated_type(arg_named.name, "Item");
                            if (item_type) {
                                std::string assoc_key = imported_type_params[i] + "::Item";
                                type_subs[assoc_key] = item_type;
                                type_subs["Item"] = item_type;
                            }
                        }
                    }
                    break;
                }
            }
        }

        is_imported = !imported_type_params.empty();

        if (generated_impl_methods_.find(mangled_method_name) == generated_impl_methods_.end()) {
            bool is_local = impl_it != pending_generic_impls_.end();
            if (is_local || is_imported) {
                pending_impl_method_instantiations_.push_back(
                    PendingImplMethod{mangled_type_name, method, type_subs, named.name,
                                      method_type_suffix, /*is_library_type=*/is_imported});
                generated_impl_methods_.insert(mangled_method_name);
            }
        }
    }
    // Handle method-level generics on non-generic types
    else if (!method_type_suffix.empty()) {
        if (env_.module_registry()) {
            const auto& all_modules = env_.module_registry()->get_all_modules();
            for (const auto& [mod_name, mod] : all_modules) {
                auto struct_it = mod.structs.find(named.name);
                if (struct_it != mod.structs.end()) {
                    is_imported = true;
                    break;
                }
            }
        }

        std::string full_method_for_key = method + "__" + method_type_suffix;
        std::string mangled_method_name = "tml_" + mangled_type_name + "_" + full_method_for_key;

        if (generated_impl_methods_.find(mangled_method_name) == generated_impl_methods_.end()) {
            pending_impl_method_instantiations_.push_back(
                PendingImplMethod{mangled_type_name, method, type_subs, named.name,
                                  method_type_suffix, /*is_library_type=*/is_imported});
            generated_impl_methods_.insert(mangled_method_name);
        }
    }
    // Handle non-generic imported types with non-generic methods (e.g., Text::as_str)
    else {
        // Check if this is an imported type (struct or enum)
        if (env_.module_registry()) {
            const auto& all_modules = env_.module_registry()->get_all_modules();
            for (const auto& [mod_name, mod] : all_modules) {
                auto struct_it = mod.structs.find(named.name);
                if (struct_it != mod.structs.end()) {
                    is_imported = true;
                    break;
                }
                auto enum_it = mod.enums.find(named.name);
                if (enum_it != mod.enums.end()) {
                    is_imported = true;
                    break;
                }
            }
        }

        if (is_imported) {
            std::string mangled_method_name = "tml_" + mangled_type_name + "_" + method;
            if (generated_impl_methods_.find(mangled_method_name) ==
                generated_impl_methods_.end()) {
                pending_impl_method_instantiations_.push_back(
                    PendingImplMethod{mangled_type_name, method, type_subs, named.name,
                                      /*method_type_suffix=*/"", /*is_library_type=*/true});
                generated_impl_methods_.insert(mangled_method_name);
            }
        }
    }

    // Look up in functions_ to get the correct LLVM name
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
        std::string prefix = is_imported ? "" : get_suite_prefix();
        fn_name = "@tml_" + prefix + mangled_type_name + "_" + full_method_name;
    }

    std::string impl_receiver_val;
    std::string impl_llvm_type = llvm_type_name(named.name);
    bool is_primitive_impl = (impl_llvm_type[0] != '%');

    if (call.receiver->is<parser::IdentExpr>()) {
        const auto& ident = call.receiver->as<parser::IdentExpr>();
        auto it = locals_.find(ident.name);
        if (it != locals_.end()) {
            if (is_primitive_impl) {
                impl_receiver_val = receiver;
            } else {
                impl_receiver_val = (it->second.type == "ptr") ? receiver : it->second.reg;
            }
        } else {
            impl_receiver_val = receiver;
        }
    } else if (call.receiver->is<parser::FieldExpr>() && !receiver_ptr.empty()) {
        if (last_expr_type_ == "ptr") {
            impl_receiver_val = receiver;
        } else {
            impl_receiver_val = receiver_ptr;
        }
    } else if (last_expr_type_.starts_with("%struct.")) {
        std::string tmp = fresh_reg();
        emit_line("  " + tmp + " = alloca " + last_expr_type_);
        emit_line("  store " + last_expr_type_ + " " + receiver + ", ptr " + tmp);
        impl_receiver_val = tmp;
    } else {
        impl_receiver_val = receiver;
    }

    std::vector<std::pair<std::string, std::string>> typed_args;
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
        if (actual_type != expected_type) {
            bool is_int_actual = (actual_type[0] == 'i' && actual_type != "i1");
            bool is_int_expected = (expected_type[0] == 'i' && expected_type != "i1");
            if (is_int_actual && is_int_expected) {
                int actual_bits = std::stoi(actual_type.substr(1));
                int expected_bits = std::stoi(expected_type.substr(1));
                std::string coerced = fresh_reg();
                if (expected_bits > actual_bits) {
                    emit_line("  " + coerced + " = sext " + actual_type + " " + val + " to " +
                              expected_type);
                } else {
                    emit_line("  " + coerced + " = trunc " + actual_type + " " + val + " to " +
                              expected_type);
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
        last_semantic_type_ = nullptr;
        return "void";
    } else {
        emit_line("  " + result + " = call " + ret_type + " " + fn_name + "(" + args_str + ")");
        last_expr_type_ = ret_type;
        last_semantic_type_ = return_type;
        return result;
    }
}

auto LLVMIRGen::try_gen_module_impl_method_call(const parser::MethodCallExpr& call,
                                                const std::string& receiver,
                                                const std::string& receiver_ptr,
                                                types::TypePtr receiver_type)
    -> std::optional<std::string> {
    const std::string& method = call.method;

    if (!(receiver_type && receiver_type->is<types::NamedType>())) {
        return std::nullopt;
    }

    const auto& named2 = receiver_type->as<types::NamedType>();
    bool is_builtin_type2 =
        (named2.name == "List" || named2.name == "HashMap" || named2.name == "Buffer" ||
         named2.name == "File" || named2.name == "Path");
    if (is_builtin_type2) {
        return std::nullopt;
    }

    std::string qualified_name = named2.name + "::" + method;
    TML_DEBUG_LN("[METHOD] Looking for impl method: " << qualified_name);
    auto func_sig = env_.lookup_func(qualified_name);
    bool is_from_library = false;

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
                    is_from_library = true;
                }
            }
        }
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
    }

    if (!func_sig) {
        return std::nullopt;
    }

    // Look up in functions_ to get the correct LLVM name
    std::string method_lookup_key = named2.name + "_" + method;
    auto method_it = functions_.find(method_lookup_key);
    std::string fn_name;
    if (method_it != functions_.end()) {
        fn_name = method_it->second.llvm_name;
    } else {
        // Only use suite prefix for test-local functions, not library methods
        std::string prefix = is_from_library ? "" : get_suite_prefix();
        fn_name = "@tml_" + prefix + named2.name + "_" + method;
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
                impl_receiver_val = (it->second.type == "ptr") ? receiver : it->second.reg;
            }
        } else {
            impl_receiver_val = receiver;
        }
    } else if (call.receiver->is<parser::FieldExpr>() && !receiver_ptr.empty()) {
        // For field expressions:
        // - For struct fields: use field pointer directly (mutations in place)
        // - For pointer/ref fields: use loaded pointer value (method expects ptr to target)
        if (last_expr_type_ == "ptr") {
            impl_receiver_val = receiver; // Use loaded pointer value
        } else {
            impl_receiver_val = receiver_ptr; // Use field pointer for struct fields
        }
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
        last_semantic_type_ = nullptr;
        return std::string("void");
    } else {
        emit_line("  " + result + " = call " + ret_type + " " + fn_name + "(" + args_str + ")");
        last_expr_type_ = ret_type;
        last_semantic_type_ = func_sig->return_type; // Track semantic type for deref assignments
        return result;
    }
}

} // namespace tml::codegen
