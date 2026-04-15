TML_MODULE("codegen_x86")

//! # LLVM IR Generator - Module Impl Method Calls
//!
//! This file implements cross-module impl method resolution and codegen.
//! Extracted from method_impl.cpp for maintainability.
//!
//! ## Coverage
//!
//! - Imported module impl methods (try_gen_module_impl_method_call)
//! - Generic type instantiation via module registry
//! - Where clause resolution for module types

#include "codegen/llvm/llvm_ir_gen.hpp"
#include "lexer/lexer.hpp"
#include "parser/parser.hpp"
#include "types/module.hpp"

namespace tml::codegen {

// Helper: Substitute inner type params in an associated type returned by lookup_associated_type.
// Duplicated from method_impl.cpp (static — no ODR violation).
static types::TypePtr substitute_inner_assoc_type_mod(types::TypePtr item_type,
                                                      const types::NamedType& arg_named,
                                                      const types::TypeEnv& env) {
    if (!item_type || arg_named.type_args.empty()) {
        return item_type;
    }
    std::vector<std::string> inner_params;
    auto inner_struct = env.lookup_struct(arg_named.name);
    if (inner_struct.has_value()) {
        inner_params = inner_struct->type_params;
    } else if (env.module_registry()) {
        for (const auto& [mn, m] : env.module_registry()->get_all_modules()) {
            auto sit = m.structs.find(arg_named.name);
            if (sit != m.structs.end()) {
                inner_params = sit->second.type_params;
                break;
            }
        }
    }
    if (inner_params.empty()) {
        return item_type;
    }
    std::unordered_map<std::string, types::TypePtr> inner_subs;
    for (size_t j = 0; j < inner_params.size() && j < arg_named.type_args.size(); ++j) {
        inner_subs[inner_params[j]] = arg_named.type_args[j];
    }
    if (!inner_subs.empty()) {
        item_type = types::substitute_type(item_type, inner_subs);
    }
    return item_type;
}

// Helper: Recursively match a parser type pattern against a concrete semantic type.
// Duplicated from method_impl.cpp (static — no ODR violation).
static void match_where_pattern_mod(const parser::Type& pattern, const types::TypePtr& concrete,
                                    std::unordered_map<std::string, types::TypePtr>& type_subs) {
    if (!concrete) {
        return;
    }
    if (pattern.is<parser::RefType>()) {
        const auto& ref_pattern = pattern.as<parser::RefType>();
        if (ref_pattern.inner && concrete->is<types::RefType>()) {
            const auto& concrete_ref = concrete->as<types::RefType>();
            if (concrete_ref.inner) {
                match_where_pattern_mod(*ref_pattern.inner, concrete_ref.inner, type_subs);
            }
        }
        return;
    }
    if (!pattern.is<parser::NamedType>()) {
        return;
    }
    const auto& named = pattern.as<parser::NamedType>();
    if (named.path.segments.empty()) {
        return;
    }
    const std::string& name = named.path.segments.back();
    bool has_type_args = named.generics.has_value() && !named.generics->args.empty();
    if (!has_type_args) {
        auto existing = type_subs.find(name);
        bool is_placeholder = false;
        if (existing != type_subs.end() && existing->second &&
            existing->second->is<types::NamedType>()) {
            const auto& existing_named = existing->second->as<types::NamedType>();
            if (existing_named.name == name && existing_named.type_args.empty()) {
                is_placeholder = true;
            }
        }
        if (existing == type_subs.end() || is_placeholder) {
            type_subs[name] = concrete;
        }
    } else if (concrete->is<types::NamedType>()) {
        const auto& concrete_named = concrete->as<types::NamedType>();
        if (concrete_named.name == name) {
            const auto& pattern_args = named.generics->args;
            size_t min_args = std::min(pattern_args.size(), concrete_named.type_args.size());
            for (size_t i = 0; i < min_args; ++i) {
                if (pattern_args[i].is_type() && concrete_named.type_args[i]) {
                    const auto& pt = pattern_args[i].as_type();
                    if (pt) {
                        match_where_pattern_mod(*pt, concrete_named.type_args[i], type_subs);
                    }
                }
            }
        }
    }
}

// Helper: Extract return type and params from a function-like semantic type.
// Duplicated from method_impl.cpp (static — no ODR violation).
static bool extract_func_signature_mod(const types::TypePtr& type, types::TypePtr& ret,
                                       std::vector<types::TypePtr>& params) {
    if (type->is<types::FuncType>()) {
        const auto& func = type->as<types::FuncType>();
        ret = func.return_type;
        params = func.params;
        return true;
    }
    if (type->is<types::ClosureType>()) {
        const auto& clos = type->as<types::ClosureType>();
        ret = clos.return_type;
        params = clos.params;
        return true;
    }
    return false;
}

// Helper: Resolve where clause type equalities from an impl's where clause.
// Duplicated from method_impl.cpp (static — no ODR violation).
static void
resolve_impl_where_clause_mod(const parser::WhereClause& where_clause,
                              std::unordered_map<std::string, types::TypePtr>& type_subs) {
    for (const auto& [lhs, rhs] : where_clause.type_equalities) {
        if (!lhs || !rhs || !lhs->is<parser::NamedType>()) {
            continue;
        }
        const auto& lhs_named = lhs->as<parser::NamedType>();
        const auto& segments = lhs_named.path.segments;
        if (segments.empty()) {
            continue;
        }
        if (segments.size() >= 2) {
            const std::string& type_param = segments[0];
            const std::string& assoc_name = segments[1];
            auto param_it = type_subs.find(type_param);
            if (param_it == type_subs.end() || !param_it->second) {
                continue;
            }
            std::string assoc_key = type_param + "::" + assoc_name;
            auto assoc_it = type_subs.find(assoc_key);
            if (assoc_it != type_subs.end() && assoc_it->second) {
                match_where_pattern_mod(*rhs, assoc_it->second, type_subs);
            }
            continue;
        }
        const std::string& lhs_name = segments.back();
        auto sub_it = type_subs.find(lhs_name);
        if (sub_it == type_subs.end() || !sub_it->second) {
            continue;
        }
        const auto& concrete = sub_it->second;
        if (rhs->is<parser::FuncType>()) {
            types::TypePtr con_ret;
            std::vector<types::TypePtr> con_params;
            if (extract_func_signature_mod(concrete, con_ret, con_params)) {
                const auto& pat = rhs->as<parser::FuncType>();
                if (pat.return_type && con_ret) {
                    match_where_pattern_mod(*pat.return_type, con_ret, type_subs);
                }
                for (size_t pi = 0; pi < pat.params.size() && pi < con_params.size(); ++pi) {
                    if (pat.params[pi] && con_params[pi]) {
                        if (pat.params[pi]->is<parser::NamedType>()) {
                            const auto& param_named = pat.params[pi]->as<parser::NamedType>();
                            if (param_named.path.segments.size() >= 2) {
                                continue;
                            }
                        }
                        match_where_pattern_mod(*pat.params[pi], con_params[pi], type_subs);
                    }
                }
            }
        } else {
            match_where_pattern_mod(*rhs, concrete, type_subs);
        }
    }
}

auto LLVMIRGen::try_gen_module_impl_method_call(const parser::MethodCallExpr& call,
                                                const std::string& receiver,
                                                const std::string& receiver_ptr,
                                                types::TypePtr receiver_type)
    -> std::optional<std::string> {
    const std::string& method = call.method;

    // Convert ArrayType to synthetic NamedType("Array") for dispatch,
    // same as in try_gen_impl_method_call. This enables dispatch of
    // specialized Array methods like impl[const N: I64] Array[U8, N]::is_ascii().
    types::TypePtr effective_receiver = receiver_type;
    if (receiver_type && receiver_type->is<types::ArrayType>()) {
        const auto& arr = receiver_type->as<types::ArrayType>();
        std::vector<types::TypePtr> type_args;
        if (arr.element) {
            type_args.push_back(arr.element);
        }
        auto synth = std::make_shared<types::Type>();
        synth->kind = types::NamedType{"Array", "", type_args};
        effective_receiver = synth;
    }

    if (!(effective_receiver && effective_receiver->is<types::NamedType>())) {
        return std::nullopt;
    }

    const auto& named2 = effective_receiver->as<types::NamedType>();
    bool is_builtin_type2 = (named2.name == "File" || named2.name == "Path");
    if (is_builtin_type2) {
        return std::nullopt;
    }

    std::string qualified_name = named2.name + "::" + method;
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
        // Also search GlobalModuleCache for modules not loaded into registry
        // (e.g., behavior impls in sibling submodules like std::collections::behaviors)
        if (!func_sig) {
            for (const auto& [mod_path, mod] : types::GlobalModuleCache::instance().get_all()) {
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
    // For generic types with type_args (e.g., Repeat[I32]), use the mangled name
    std::string type_name_for_call = named2.name;
    if (!named2.type_args.empty()) {
        type_name_for_call = mangle_struct_name(named2.name, named2.type_args);
    }
    std::string method_lookup_key = type_name_for_call + "_" + method;
    auto method_it = functions_.find(method_lookup_key);
    std::string fn_name;
    if (method_it != functions_.end()) {
        fn_name = method_it->second.llvm_name;
    } else {
        // Also try base name lookup for non-generic methods
        method_it = functions_.find(named2.name + "_" + method);
        if (method_it != functions_.end()) {
            fn_name = method_it->second.llvm_name;
        } else {
            fn_name = "@" + mangle_impl_method(type_name_for_call, method);
        }
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
            } else if (it->second.is_direct_param && it->second.type.find("%struct.") == 0) {
                // Direct SSA param — spill to stack for method call
                std::string tmp = fresh_reg();
                emit_line("  " + tmp + " = alloca " + it->second.type);
                emit_line("  store " + it->second.type + " " + receiver + ", ptr " + tmp);
                impl_receiver_val = tmp;
            } else {
                // For structs, pass the pointer
                impl_receiver_val = (it->second.type == "ptr") ? receiver : it->second.reg;
            }
        } else {
            impl_receiver_val = receiver;
        }
    } else if (call.receiver->is<parser::FieldExpr>()) {
        // For field expressions:
        // - For primitive types: pass the loaded value (not the field pointer)
        // - For ptr types: use loaded pointer value
        // - For struct fields: use field pointer directly (mutations in place)
        // - Otherwise: spill struct to stack for method call
        if (is_primitive_impl || last_expr_type_ == "ptr") {
            // Primitive methods / ptr types — use loaded value
            impl_receiver_val = receiver;
        } else if (!receiver_ptr.empty()) {
            impl_receiver_val = receiver_ptr;
        } else if (last_expr_type_.starts_with("%struct.")) {
            // Field expression but no receiver_ptr - need to spill struct to stack
            std::string tmp = fresh_reg();
            emit_line("  " + tmp + " = alloca " + last_expr_type_);
            emit_line("  store " + last_expr_type_ + " " + receiver + ", ptr " + tmp);
            impl_receiver_val = tmp;
        } else {
            impl_receiver_val = receiver;
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
    // For structs/enums, pass as pointer (unless first param is by-value)
    std::string this_arg_type = is_primitive_impl ? impl_llvm_type : "ptr";

    // NOTE: Struct/enum this/self is ALWAYS passed by pointer (ptr), regardless of
    // whether the semantic type is RefType (mut this) or NamedType (immutable this).
    // The function definition always emits `ptr %this` for struct types.
    // Do NOT load the struct value and pass by value — that creates a type mismatch.

    // Skip 'this' argument for Unit type — Unit methods have no 'this' parameter
    bool is_unit_type2 = (impl_llvm_type == "void" || impl_llvm_type == "{}");
    if (!is_unit_type2) {
        typed_args.push_back({this_arg_type, impl_receiver_val});
    }

    for (size_t i = 0; i < call.args.size(); ++i) {
        std::string val = gen_expr(*call.args[i]);
        std::string actual_type = last_expr_type_;
        std::string arg_type = actual_type;
        types::TypePtr param_type_ptr;
        // func_sig usually has 'this' at index 0 (semantic params), so offset is 1.
        // However, default behavior methods from module binary cache may omit 'this'.
        if (func_sig && i + 1 < func_sig->params.size()) {
            param_type_ptr = func_sig->params[i + 1];
            arg_type = llvm_type_from_semantic(param_type_ptr);
            if (param_type_ptr->is<types::FuncType>()) {
                arg_type = "{ ptr, ptr }";
            }
        }
        // Fallback for func_sig without 'this': direct indexing
        else if (func_sig && i < func_sig->params.size()) {
            param_type_ptr = func_sig->params[i];
            arg_type = llvm_type_from_semantic(param_type_ptr);
            if (param_type_ptr->is<types::FuncType>()) {
                arg_type = "{ ptr, ptr }";
            }
        }
        // ptr -> { ptr, ptr } conversion: wrap bare function pointer in fat pointer
        if (actual_type == "ptr" && arg_type == "{ ptr, ptr }") {
            std::string fat1 = fresh_reg();
            std::string fat2 = fresh_reg();
            emit_line("  " + fat1 + " = insertvalue { ptr, ptr } undef, ptr " + val + ", 0");
            emit_line("  " + fat2 + " = insertvalue { ptr, ptr } " + fat1 + ", ptr null, 1");
            val = fat2;
        }
        // Array-to-slice coercion: when parameter expects ref [T] (slice) but argument
        // is a ref to a fixed-size array [T; N], create a fat pointer { ptr, i64 }
        // containing the array data pointer and the array length.
        if (actual_type == "ptr" && arg_type == "ptr" && param_type_ptr &&
            param_type_ptr->is<types::RefType>()) {
            const auto& ref_type = param_type_ptr->as<types::RefType>();
            if (ref_type.inner && ref_type.inner->is<types::SliceType>()) {
                // Parameter expects ref [T] — check if argument is an array
                auto arg_semantic = infer_expr_type(*call.args[i]);
                size_t array_size = 0;
                if (arg_semantic && arg_semantic->is<types::ArrayType>()) {
                    array_size = arg_semantic->as<types::ArrayType>().size;
                } else if (arg_semantic && arg_semantic->is<types::RefType>()) {
                    const auto& arg_ref = arg_semantic->as<types::RefType>();
                    if (arg_ref.inner && arg_ref.inner->is<types::ArrayType>()) {
                        array_size = arg_ref.inner->as<types::ArrayType>().size;
                    }
                }
                if (array_size > 0) {
                    // Create fat pointer { ptr, i64 } on stack
                    std::string fat_alloca = fresh_reg();
                    emit_line("  " + fat_alloca + " = alloca { ptr, i64 }");
                    std::string data_field = fresh_reg();
                    emit_line("  " + data_field + " = getelementptr inbounds { ptr, i64 }, ptr " +
                              fat_alloca + ", i32 0, i32 0");
                    emit_line("  store ptr " + val + ", ptr " + data_field);
                    std::string len_field = fresh_reg();
                    emit_line("  " + len_field + " = getelementptr inbounds { ptr, i64 }, ptr " +
                              fat_alloca + ", i32 0, i32 1");
                    emit_line("  store i64 " + std::to_string(array_size) + ", ptr " + len_field);
                    val = fat_alloca;
                }
            }
        }
        // struct/enum → ptr ABI fix (see impl.cpp:282)
        // i+1 because typed_args[0] is 'this'
        if (method_it != functions_.end() && (i + 1) < method_it->second.param_types.size()) {
            const auto& expected_def = method_it->second.param_types[i + 1];
            if (expected_def == "ptr" &&
                (arg_type.find("%struct.") == 0 || arg_type.find("%enum.") == 0)) {
                std::string temp = fresh_reg();
                emit_line("  " + temp + " = alloca " + arg_type);
                emit_line("  store " + arg_type + " " + val + ", ptr " + temp);
                val = temp;
                arg_type = "ptr";
            }
        }
        typed_args.push_back({arg_type, val});
    }

    // Build type substitutions for generic types
    std::unordered_map<std::string, types::TypePtr> type_subs;
    if (!named2.type_args.empty() && env_.module_registry()) {
        const auto& all_modules = env_.module_registry()->get_all_modules();
        for (const auto& [mod_name, mod] : all_modules) {
            auto enum_it = mod.enums.find(named2.name);
            if (enum_it != mod.enums.end() && !enum_it->second.type_params.empty()) {
                for (size_t i = 0;
                     i < enum_it->second.type_params.size() && i < named2.type_args.size(); ++i) {
                    type_subs[enum_it->second.type_params[i]] = named2.type_args[i];
                }
                break;
            }
            auto struct_it = mod.structs.find(named2.name);
            if (struct_it != mod.structs.end() && !struct_it->second.type_params.empty()) {
                for (size_t i = 0;
                     i < struct_it->second.type_params.size() && i < named2.type_args.size(); ++i) {
                    type_subs[struct_it->second.type_params[i]] = named2.type_args[i];
                }
                break;
            }
        }
    }

    // Resolve where clause type equalities for module impl methods
    if (!type_subs.empty() && env_.module_registry()) {
        const auto& all_modules2 = env_.module_registry()->get_all_modules();
        for (const auto& [mod_name2, mod2] : all_modules2) {
            auto struct_it2 = mod2.structs.find(named2.name);
            if (struct_it2 == mod2.structs.end() || mod2.source_code.empty())
                continue;
            const parser::Module* parsed_mod_ptr = nullptr;
            parser::Module local_parsed_mod;
            if (GlobalASTCache::should_cache(mod_name2)) {
                parsed_mod_ptr = GlobalASTCache::instance().get(mod_name2);
            }
            if (!parsed_mod_ptr) {
                auto source = lexer::Source::from_string(mod2.source_code, mod2.file_path);
                lexer::Lexer lex(source);
                auto tokens = lex.tokenize();
                if (!lex.has_errors()) {
                    parser::Parser mod_parser(std::move(tokens));
                    auto stem = mod_name2;
                    if (auto pos = stem.rfind("::"); pos != std::string::npos) {
                        stem = stem.substr(pos + 2);
                    }
                    auto parse_result = mod_parser.parse_module(stem);
                    if (std::holds_alternative<parser::Module>(parse_result)) {
                        local_parsed_mod = std::get<parser::Module>(std::move(parse_result));
                        if (GlobalASTCache::should_cache(mod_name2)) {
                            GlobalASTCache::instance().put(mod_name2, std::move(local_parsed_mod));
                            parsed_mod_ptr = GlobalASTCache::instance().get(mod_name2);
                        } else {
                            parsed_mod_ptr = &local_parsed_mod;
                        }
                    }
                }
            }
            if (!parsed_mod_ptr)
                continue;
            for (const auto& decl : parsed_mod_ptr->decls) {
                if (!decl->is<parser::ImplDecl>())
                    continue;
                const auto& imp = decl->as<parser::ImplDecl>();
                if (!imp.self_type || !imp.self_type->is<parser::NamedType>())
                    continue;
                const auto& target = imp.self_type->as<parser::NamedType>();
                if (target.path.segments.empty() || target.path.segments.back() != named2.name)
                    continue;
                if (!imp.where_clause)
                    continue;
                resolve_impl_where_clause_mod(*imp.where_clause, type_subs);
            }
            break;
        }
    }

    // Apply type substitutions to return type for generic types
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

    // Coverage instrumentation at call site for library methods
    emit_coverage(qualified_name);

    std::string result = fresh_reg();
    if (ret_type == "void") {
        emit_line("  call void " + fn_name + "(" + args_str + ")");
        last_expr_type_ = "void";
        last_semantic_type_ = nullptr;
        return std::string("void");
    } else {
        emit_line("  " + result + " = call " + ret_type + " " + fn_name + "(" + args_str + ")");
        last_expr_type_ = ret_type;
        last_semantic_type_ = return_type; // Track substituted semantic type for type inference
        return result;
    }
}

} // namespace tml::codegen
