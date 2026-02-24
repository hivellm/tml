TML_MODULE("codegen_x86")

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

#include "codegen/llvm/llvm_ir_gen.hpp"
#include "lexer/lexer.hpp"
#include "parser/parser.hpp"
#include "types/module.hpp"

namespace tml::codegen {

// Helper: Recursively match a parser type pattern against a concrete semantic type
// to extract type parameter bindings. Handles nested generics like Maybe[T] -> Maybe[I32].
static void match_where_pattern(const parser::Type& pattern, const types::TypePtr& concrete,
                                std::unordered_map<std::string, types::TypePtr>& type_subs) {
    if (!concrete)
        return;
    if (!pattern.is<parser::NamedType>())
        return;
    const auto& named = pattern.as<parser::NamedType>();
    if (named.path.segments.empty())
        return;
    const std::string& name = named.path.segments.back();
    bool has_type_args = named.generics.has_value() && !named.generics->args.empty();
    if (!has_type_args) {
        // Simple name like "T" — add mapping if not already present
        if (type_subs.find(name) == type_subs.end()) {
            type_subs[name] = concrete;
        }
    } else if (concrete->is<types::NamedType>()) {
        // Generic type like Maybe[T] — recurse into type args
        const auto& concrete_named = concrete->as<types::NamedType>();
        if (concrete_named.name == name) {
            const auto& pattern_args = named.generics->args;
            size_t min_args = std::min(pattern_args.size(), concrete_named.type_args.size());
            for (size_t i = 0; i < min_args; ++i) {
                if (pattern_args[i].is_type() && concrete_named.type_args[i]) {
                    const auto& pt = pattern_args[i].as_type();
                    if (pt) {
                        match_where_pattern(*pt, concrete_named.type_args[i], type_subs);
                    }
                }
            }
        }
    }
}

// Helper: Resolve where clause type equalities from an impl's where clause.
// Matches concrete types in type_subs against patterns to derive additional bindings.
static void resolve_impl_where_clause(const parser::WhereClause& where_clause,
                                      std::unordered_map<std::string, types::TypePtr>& type_subs) {
    for (const auto& [lhs, rhs] : where_clause.type_equalities) {
        if (!lhs || !rhs || !lhs->is<parser::NamedType>())
            continue;
        const auto& lhs_name = lhs->as<parser::NamedType>().path.segments.back();
        auto sub_it = type_subs.find(lhs_name);
        if (sub_it == type_subs.end() || !sub_it->second)
            continue;
        const auto& concrete = sub_it->second;
        if (rhs->is<parser::FuncType>() && concrete->is<types::FuncType>()) {
            const auto& pat = rhs->as<parser::FuncType>();
            const auto& con = concrete->as<types::FuncType>();
            if (pat.return_type && con.return_type) {
                match_where_pattern(*pat.return_type, con.return_type, type_subs);
            }
            for (size_t pi = 0; pi < pat.params.size() && pi < con.params.size(); ++pi) {
                if (pat.params[pi] && con.params[pi]) {
                    match_where_pattern(*pat.params[pi], con.params[pi], type_subs);
                }
            }
        }
    }
}

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
    // File/Path now use normal dispatch via @extern FFI
    bool is_slice_inlined = (named.name == "Slice" || named.name == "MutSlice") &&
                            (method == "len" || method == "is_empty");

    if (is_slice_inlined) {
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
                                // Only infer if we haven't already inferred this type param
                                if (type_subs.find(type_param) == type_subs.end()) {
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
                // Handle FuncType parameters: func(E) -> F where F is the type param
                else if (param_type && param_type->is<types::FuncType>()) {
                    const auto& func_type = param_type->as<types::FuncType>();
                    // Check if the return type is the type parameter we're looking for
                    if (func_type.return_type && func_type.return_type->is<types::NamedType>()) {
                        const auto& ret_named = func_type.return_type->as<types::NamedType>();
                        if (ret_named.name == type_param && ret_named.type_args.empty()) {
                            // Only infer if we haven't already inferred this type param
                            // (multiple function params may share the same return type param)
                            if (type_subs.find(type_param) == type_subs.end()) {
                                // Infer from the argument's return type
                                auto arg_type = infer_expr_type(*call.args[p_idx - 1]);
                                if (arg_type && arg_type->is<types::FuncType>()) {
                                    const auto& arg_func = arg_type->as<types::FuncType>();
                                    if (arg_func.return_type) {
                                        type_subs[type_param] = arg_func.return_type;
                                        if (!method_type_suffix.empty()) {
                                            method_type_suffix += "_";
                                        }
                                        method_type_suffix += mangle_type(arg_func.return_type);
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

        // If the library already emitted methods using the unmangled base name
        // (e.g., tml_BTreeMap_insert from gen_impl_method), use the base name
        // so user code calls the existing function instead of a non-existent mangled one.
        std::string base_fn_check = "@tml_" + named.name + "_" + method;
        if (mangled_type_name != named.name && generated_functions_.count(base_fn_check) > 0) {
            mangled_type_name = named.name;
        }

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
                // Also resolve associated types for concrete type arguments
                // e.g., for I: Iterator where I = Counter, resolve I::Item = Counter::Item = I32
                if (named.type_args[i] && named.type_args[i]->is<types::NamedType>()) {
                    const auto& arg_named = named.type_args[i]->as<types::NamedType>();
                    auto item_type = lookup_associated_type(arg_named.name, "Item");
                    if (item_type) {
                        std::string assoc_key = impl.generics[i].name + "::Item";
                        type_subs[assoc_key] = item_type;
                        type_subs["Item"] = item_type;
                    }
                }
            }
            // Check if this type is from an imported module (even though impl is in
            // pending_generic_impls_, it may have been registered from an imported module)
            if (env_.module_registry()) {
                const auto& all_modules = env_.module_registry()->get_all_modules();
                for (const auto& [mod_name, mod] : all_modules) {
                    if (mod.structs.find(named.name) != mod.structs.end() ||
                        mod.enums.find(named.name) != mod.enums.end()) {
                        is_imported = true;
                        break;
                    }
                }
            }
            // Also check builtin enums (Outcome, Maybe, ControlFlow, etc.)
            // These are registered in the type environment, not in module registry
            if (!is_imported) {
                auto builtin_enum = env_.lookup_enum(named.name);
                if (builtin_enum) {
                    is_imported = true;
                }
            }
        }

        // Check imported structs and enums for type params
        std::vector<std::string> imported_type_params;
        if (impl_it == pending_generic_impls_.end()) {
            // First check builtin enums via env_.lookup_enum
            auto builtin_enum = env_.lookup_enum(named.name);
            if (builtin_enum && !builtin_enum->type_params.empty()) {
                imported_type_params = builtin_enum->type_params;
                for (size_t i = 0; i < imported_type_params.size() && i < named.type_args.size();
                     ++i) {
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
            }
            // Also check module registry for imported structs and enums
            else if (env_.module_registry()) {
                const auto& all_modules = env_.module_registry()->get_all_modules();
                for (const auto& [mod_name, mod] : all_modules) {
                    // Check structs
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
                    // Check enums
                    auto enum_it = mod.enums.find(named.name);
                    if (enum_it != mod.enums.end() && !enum_it->second.type_params.empty()) {
                        imported_type_params = enum_it->second.type_params;
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
        }

        // Only update is_imported from imported_type_params if it wasn't already
        // set to true from the pending_generic_impls_ module registry check above.
        if (!is_imported) {
            is_imported = !imported_type_params.empty();
        }

        // Resolve where clause type equalities to derive additional type substitutions.
        // For example: `impl[F, T] Iterator for OnceWith[F] where F = func() -> T`
        // With F already mapped to func() -> I32, this derives T -> I32.
        // Also handles nested patterns like `where F = func() -> Maybe[T]`.
        // Check local impls first, then imported modules.
        {
            auto local_impl_it = pending_generic_impls_.find(named.name);
            if (local_impl_it != pending_generic_impls_.end()) {
                const auto& local_impl = *local_impl_it->second;
                if (local_impl.where_clause) {
                    resolve_impl_where_clause(*local_impl.where_clause, type_subs);
                }
            }
            // For imported types, search module source for impl with where clause
            else if (env_.module_registry()) {
                const auto& all_modules = env_.module_registry()->get_all_modules();
                for (const auto& [mod_name, mod] : all_modules) {
                    auto struct_it2 = mod.structs.find(named.name);
                    if (struct_it2 == mod.structs.end() || mod.source_code.empty())
                        continue;
                    // Get parsed AST from cache or parse
                    const parser::Module* parsed_mod_ptr = nullptr;
                    parser::Module local_parsed_mod;
                    if (GlobalASTCache::should_cache(mod_name)) {
                        parsed_mod_ptr = GlobalASTCache::instance().get(mod_name);
                    }
                    if (!parsed_mod_ptr) {
                        auto source = lexer::Source::from_string(mod.source_code, mod.file_path);
                        lexer::Lexer lex(source);
                        auto tokens = lex.tokenize();
                        if (lex.has_errors())
                            continue;
                        parser::Parser mod_parser(std::move(tokens));
                        auto module_name_stem = mod_name;
                        if (auto pos = module_name_stem.rfind("::"); pos != std::string::npos) {
                            module_name_stem = module_name_stem.substr(pos + 2);
                        }
                        auto parse_result = mod_parser.parse_module(module_name_stem);
                        if (!std::holds_alternative<parser::Module>(parse_result))
                            continue;
                        local_parsed_mod = std::get<parser::Module>(std::move(parse_result));
                        if (GlobalASTCache::should_cache(mod_name)) {
                            GlobalASTCache::instance().put(mod_name, std::move(local_parsed_mod));
                            parsed_mod_ptr = GlobalASTCache::instance().get(mod_name);
                        } else {
                            parsed_mod_ptr = &local_parsed_mod;
                        }
                    }
                    if (!parsed_mod_ptr)
                        continue;
                    // Find impl for our type with where clause
                    for (const auto& decl : parsed_mod_ptr->decls) {
                        if (!decl->is<parser::ImplDecl>())
                            continue;
                        const auto& imp = decl->as<parser::ImplDecl>();
                        if (!imp.self_type || !imp.self_type->is<parser::NamedType>())
                            continue;
                        const auto& target = imp.self_type->as<parser::NamedType>();
                        if (target.path.segments.empty() ||
                            target.path.segments.back() != named.name)
                            continue;
                        if (!imp.where_clause)
                            continue;
                        resolve_impl_where_clause(*imp.where_clause, type_subs);
                    }
                    break;
                }
            }
        }

        TML_DEBUG_LN("[IMPL_METHOD]   generic path: mangled="
                     << mangled_type_name << " is_imported=" << is_imported
                     << " imported_type_params=" << imported_type_params.size()
                     << " is_local=" << (impl_it != pending_generic_impls_.end())
                     << " mangled_method=" << mangled_method_name);

        if (generated_impl_methods_.find(mangled_method_name) == generated_impl_methods_.end()) {
            bool is_local = impl_it != pending_generic_impls_.end();
            if (is_local || is_imported) {
                TML_DEBUG_LN("[IMPL_METHOD]   QUEUING PendingImplMethod: " << mangled_method_name);
                pending_impl_method_instantiations_.push_back(
                    PendingImplMethod{mangled_type_name, method, type_subs, named.name,
                                      method_type_suffix, /*is_library_type=*/is_imported});
                generated_impl_methods_.insert(mangled_method_name);
            } else {
                TML_DEBUG_LN("[IMPL_METHOD]   NOT queuing: is_local=" << is_local << " is_imported="
                                                                      << is_imported);
            }
        } else {
            TML_DEBUG_LN("[IMPL_METHOD]   already generated: " << mangled_method_name);
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
        // Primitive types always have impl methods from library modules
        bool is_primitive_type =
            (named.name == "Str" || named.name == "I8" || named.name == "I16" ||
             named.name == "I32" || named.name == "I64" || named.name == "U8" ||
             named.name == "U16" || named.name == "U32" || named.name == "U64" ||
             named.name == "F32" || named.name == "F64" || named.name == "Bool" ||
             named.name == "Char");
        if (is_primitive_type) {
            is_imported = true;
        }
        // Check if this is an imported type (struct, enum, or primitive with impl methods)
        if (!is_imported && env_.module_registry()) {
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
                // Also check if the method is registered as a function (handles
                // other types with impl blocks in library modules)
                auto func_it = mod.functions.find(named.name + "::" + method);
                if (func_it != mod.functions.end()) {
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
        // Also try without suite prefix in case it's defined in a library module
        if (!get_suite_prefix().empty()) {
            method_it = functions_.find(mangled_type_name + "_" + full_method_name);
            if (method_it != functions_.end()) {
                fn_name = method_it->second.llvm_name;
            }
        }
        if (fn_name.empty()) {
            // Primitive types (Str, I32, etc.) always have impl methods from library
            // modules, never from local test code, so never add suite prefix.
            bool is_primitive_type =
                (named.name == "Str" || named.name == "I8" || named.name == "I16" ||
                 named.name == "I32" || named.name == "I64" || named.name == "U8" ||
                 named.name == "U16" || named.name == "U32" || named.name == "U64" ||
                 named.name == "F32" || named.name == "F64" || named.name == "Bool" ||
                 named.name == "Char");
            std::string prefix = (is_imported || is_primitive_type) ? "" : get_suite_prefix();
            fn_name = "@tml_" + prefix + mangled_type_name + "_" + full_method_name;
        }
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
            } else if (it->second.is_direct_param && it->second.type.find("%struct.") == 0) {
                // Direct SSA param — spill to stack for method call
                std::string tmp = fresh_reg();
                emit_line("  " + tmp + " = alloca " + it->second.type);
                emit_line("  store " + it->second.type + " " + receiver + ", ptr " + tmp);
                impl_receiver_val = tmp;
            } else {
                impl_receiver_val = (it->second.type == "ptr") ? receiver : it->second.reg;
            }
        } else {
            impl_receiver_val = receiver;
        }
    } else if (call.receiver->is<parser::FieldExpr>()) {
        // For field expressions:
        // - For ptr types: use loaded pointer value
        // - For struct fields: use field pointer directly (mutations in place)
        // - Otherwise: spill struct to stack for method call
        if (last_expr_type_ == "ptr") {
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
    std::string this_arg_type = is_primitive_impl ? impl_llvm_type : "ptr";
    typed_args.push_back({this_arg_type, impl_receiver_val});

    for (size_t i = 0; i < call.args.size(); ++i) {
        std::string val = gen_expr(*call.args[i]);
        // Function/closure parameters now use fat pointer { ptr, ptr } — no coercion needed
        // The fat pointer preserves the env_ptr for capturing closures
        std::string actual_type = last_expr_type_;
        std::string expected_type = "i32";
        types::TypePtr param_type_resolved;
        if (func_sig && i + 1 < func_sig->params.size()) {
            param_type_resolved = func_sig->params[i + 1];
            if (!type_subs.empty()) {
                param_type_resolved = types::substitute_type(param_type_resolved, type_subs);
            }
            expected_type = llvm_type_from_semantic(param_type_resolved);
            // Function-typed parameters use fat pointer { ptr, ptr }
            if (param_type_resolved->is<types::FuncType>()) {
                expected_type = "{ ptr, ptr }";
            }
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
            // ptr -> { ptr, ptr } conversion: wrap bare function pointer in fat pointer
            else if (actual_type == "ptr" && expected_type == "{ ptr, ptr }") {
                std::string fat1 = fresh_reg();
                std::string fat2 = fresh_reg();
                emit_line("  " + fat1 + " = insertvalue { ptr, ptr } undef, ptr " + val + ", 0");
                emit_line("  " + fat2 + " = insertvalue { ptr, ptr } " + fat1 + ", ptr null, 1");
                val = fat2;
            }
        }
        // Array-to-slice coercion: when parameter expects ref [T] (slice) but argument
        // is a ref to a fixed-size array [T; N], create a fat pointer { ptr, i64 }
        // containing the array data pointer and the array length.
        if (actual_type == "ptr" && expected_type == "ptr" && param_type_resolved &&
            param_type_resolved->is<types::RefType>()) {
            const auto& ref_type = param_type_resolved->as<types::RefType>();
            if (ref_type.inner && ref_type.inner->is<types::SliceType>()) {
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

    // Coverage instrumentation at call site for library methods
    // This tracks usage of library functions even if they get inlined
    emit_coverage(qualified_name);

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
        // - For ptr types: use loaded pointer value
        // - For struct fields: use field pointer directly (mutations in place)
        // - Otherwise: spill struct to stack for method call
        if (last_expr_type_ == "ptr") {
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
    // For structs/enums, pass as pointer
    std::string this_arg_type = is_primitive_impl ? impl_llvm_type : "ptr";
    typed_args.push_back({this_arg_type, impl_receiver_val});

    for (size_t i = 0; i < call.args.size(); ++i) {
        std::string val = gen_expr(*call.args[i]);
        std::string actual_type = last_expr_type_;
        std::string arg_type = "i32";
        types::TypePtr param_type_ptr;
        if (func_sig && i + 1 < func_sig->params.size()) {
            param_type_ptr = func_sig->params[i + 1];
            arg_type = llvm_type_from_semantic(param_type_ptr);
            // Function-typed parameters use fat pointer to support closures
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
                resolve_impl_where_clause(*imp.where_clause, type_subs);
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
