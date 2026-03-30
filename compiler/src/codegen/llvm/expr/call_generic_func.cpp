TML_MODULE("codegen_x86")

//! # LLVM IR Generator - Generic Function Call Instantiation
//!
//! This file handles calls to generic (polymorphic) functions. When a call
//! like `forget[I32](x)` or `cloned(iter)` is encountered, it:
//!
//! 1. Looks up the pending generic function definition
//! 2. Infers type arguments via unification of call arguments with parameter types
//! 3. Resolves where-clause type equalities for additional bindings
//! 4. Registers a monomorphized instantiation via `require_func_instantiation`
//! 5. Emits the call to the mangled instantiation name

#include "codegen/llvm/llvm_ir_gen.hpp"

#include <cctype>

namespace tml::codegen {

// Static helper: match a parser type pattern against a concrete semantic type
// to extract type parameter bindings. Used for where-clause type equality resolution.
// e.g., pattern `ref T` matched against concrete `ref I32` derives T = I32.
static void match_where_pattern_call(const parser::Type& pattern, const types::TypePtr& concrete,
                                     std::unordered_map<std::string, types::TypePtr>& type_subs) {
    if (!concrete)
        return;
    if (pattern.is<parser::RefType>()) {
        const auto& ref_pattern = pattern.as<parser::RefType>();
        if (ref_pattern.inner && concrete->is<types::RefType>()) {
            const auto& concrete_ref = concrete->as<types::RefType>();
            if (concrete_ref.inner) {
                match_where_pattern_call(*ref_pattern.inner, concrete_ref.inner, type_subs);
            }
        }
        return;
    }
    // Handle FuncType patterns: e.g., func() -> Maybe[T]
    if (pattern.is<parser::FuncType>()) {
        const auto& func_pattern = pattern.as<parser::FuncType>();
        if (concrete->is<types::FuncType>()) {
            const auto& concrete_func = concrete->as<types::FuncType>();
            // Match return types
            if (func_pattern.return_type && concrete_func.return_type) {
                match_where_pattern_call(*func_pattern.return_type, concrete_func.return_type,
                                         type_subs);
            }
            // Match parameter types
            for (size_t pi = 0; pi < func_pattern.params.size() && pi < concrete_func.params.size();
                 ++pi) {
                if (func_pattern.params[pi] && concrete_func.params[pi]) {
                    match_where_pattern_call(*func_pattern.params[pi], concrete_func.params[pi],
                                             type_subs);
                }
            }
        } else if (concrete->is<types::ClosureType>()) {
            // Also handle ClosureType (closures are FuncType-like)
            const auto& concrete_closure = concrete->as<types::ClosureType>();
            if (func_pattern.return_type && concrete_closure.return_type) {
                match_where_pattern_call(*func_pattern.return_type, concrete_closure.return_type,
                                         type_subs);
            }
            for (size_t pi = 0;
                 pi < func_pattern.params.size() && pi < concrete_closure.params.size(); ++pi) {
                if (func_pattern.params[pi] && concrete_closure.params[pi]) {
                    match_where_pattern_call(*func_pattern.params[pi], concrete_closure.params[pi],
                                             type_subs);
                }
            }
        }
        return;
    }
    if (!pattern.is<parser::NamedType>())
        return;
    const auto& named = pattern.as<parser::NamedType>();
    if (named.path.segments.empty())
        return;
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
                        match_where_pattern_call(*pt, concrete_named.type_args[i], type_subs);
                    }
                }
            }
        }
    }
}

// Static helper to parse mangled type strings like "Mutex__I32" into proper TypePtr
// This is used for nested generic type inference and avoids expensive std::function lambdas
static types::TypePtr parse_mangled_type_string(const std::string& s) {
    // Primitives
    if (s == "I64")
        return types::make_i64();
    if (s == "I32")
        return types::make_i32();
    if (s == "I8") {
        auto t = std::make_shared<types::Type>();
        t->kind = types::PrimitiveType{types::PrimitiveKind::I8};
        return t;
    }
    if (s == "I16") {
        auto t = std::make_shared<types::Type>();
        t->kind = types::PrimitiveType{types::PrimitiveKind::I16};
        return t;
    }
    if (s == "U8") {
        auto t = std::make_shared<types::Type>();
        t->kind = types::PrimitiveType{types::PrimitiveKind::U8};
        return t;
    }
    if (s == "U16") {
        auto t = std::make_shared<types::Type>();
        t->kind = types::PrimitiveType{types::PrimitiveKind::U16};
        return t;
    }
    if (s == "U32") {
        auto t = std::make_shared<types::Type>();
        t->kind = types::PrimitiveType{types::PrimitiveKind::U32};
        return t;
    }
    if (s == "U64") {
        auto t = std::make_shared<types::Type>();
        t->kind = types::PrimitiveType{types::PrimitiveKind::U64};
        return t;
    }
    if (s == "Usize") {
        auto t = std::make_shared<types::Type>();
        t->kind = types::PrimitiveType{types::PrimitiveKind::U64};
        return t;
    }
    if (s == "Isize") {
        auto t = std::make_shared<types::Type>();
        t->kind = types::PrimitiveType{types::PrimitiveKind::I64};
        return t;
    }
    if (s == "F32") {
        auto t = std::make_shared<types::Type>();
        t->kind = types::PrimitiveType{types::PrimitiveKind::F32};
        return t;
    }
    if (s == "F64")
        return types::make_f64();
    if (s == "Bool")
        return types::make_bool();
    if (s == "Str")
        return types::make_str();

    // Check for pointer prefix (e.g., ptr_ChannelNode__I32 -> Ptr[ChannelNode[I32]])
    if (s.substr(0, 4) == "ptr_") {
        std::string inner_str = s.substr(4);
        auto inner = parse_mangled_type_string(inner_str);
        if (inner) {
            auto t = std::make_shared<types::Type>();
            t->kind = types::PtrType{.inner = inner};
            return t;
        }
    }

    // Check for nested generic (e.g., Mutex__I32)
    auto delim = s.find("__");
    if (delim != std::string::npos) {
        std::string base = s.substr(0, delim);
        std::string arg_str = s.substr(delim + 2);
        auto inner = parse_mangled_type_string(arg_str);
        if (inner) {
            auto t = std::make_shared<types::Type>();
            t->kind = types::NamedType{base, "", {inner}};
            return t;
        }
    }

    // Simple struct type
    auto t = std::make_shared<types::Type>();
    t->kind = types::NamedType{s, "", {}};
    return t;
}

auto LLVMIRGen::gen_call_generic_func(const parser::CallExpr& call, const std::string& fn_name)
    -> std::optional<std::string> {
    // Check if this is a generic function call
    auto pending_func_it = pending_generic_funcs_.find(fn_name);
    // Arity check on direct match: if the generic function's param count doesn't match
    // the call's arg count, this is a name collision (e.g., bare "replace" matching
    // mem::replace[T](2 params) when the caller wants str::replace(3 params)).
    if (pending_func_it != pending_generic_funcs_.end()) {
        size_t gen_param_count = pending_func_it->second->params.size();
        if (call.args.size() != gen_param_count) {
            pending_func_it = pending_generic_funcs_.end();
        }
    }
    // Arity-based disambiguation: when the primary map entry has wrong arity,
    // search the _all_ map for an alternative generic function with the same bare
    // name and the correct param count. This handles cases like bare "take" where
    // mem::take[T](1 param) was registered first but iter::take[I](2 params) is wanted.
    if (pending_func_it == pending_generic_funcs_.end()) {
        // Determine the bare name to search for
        std::string search_name = fn_name;
        size_t last_sep = fn_name.rfind("::");
        if (last_sep != std::string::npos) {
            search_name = fn_name.substr(last_sep + 2);
        }
        auto all_it = pending_generic_funcs_all_.find(search_name);
        if (all_it != pending_generic_funcs_all_.end()) {
            for (const auto& [fdecl, mod_name] : all_it->second) {
                if (fdecl->params.size() == call.args.size()) {
                    // Found a match with correct arity. Register under an arity-qualified
                    // key so it doesn't conflict with the existing bare-name entry.
                    // generate_pending_instantiations uses base_name to look up the
                    // FuncDecl, so we must store it in pending_generic_funcs_ under this key.
                    std::string arity_key = search_name + "___a" + std::to_string(call.args.size());
                    pending_generic_funcs_[arity_key] = fdecl;
                    generic_func_modules_[arity_key] = mod_name;
                    pending_func_it = pending_generic_funcs_.find(arity_key);
                    break;
                }
            }
        }
    }
    // For module-qualified calls like "mem::forget", also try the bare name "forget"
    // since module functions are registered by bare name during gen_func_decl
    // BUT: skip this for Type::method patterns (e.g., RawMutPtr::from_addr)
    // where the first segment is a type name (starts with uppercase).
    // Those are struct static methods, not module-qualified standalone functions.
    if (pending_func_it == pending_generic_funcs_.end() &&
        fn_name.find("::") != std::string::npos) {
        size_t last_sep = fn_name.rfind("::");
        std::string prefix = fn_name.substr(0, last_sep);
        std::string bare_name = fn_name.substr(last_sep + 2);
        // Only do bare name fallback for module-qualified calls (lowercase prefix)
        // Skip for Type::method patterns where prefix is a type name (uppercase)
        bool is_type_static_method =
            !prefix.empty() && std::isupper(prefix[0]) && prefix.find("::") == std::string::npos;
        if (!is_type_static_method) {
            auto candidate = pending_generic_funcs_.find(bare_name);
            if (candidate != pending_generic_funcs_.end()) {
                // Arity check: the generic function's param count must match the call's arg count.
                // This prevents e.g. str::replace(s, pat, rep) [3 args] from matching
                // mem::replace[T](dest, src) [2 params] just because the bare name is "replace".
                size_t gen_param_count = candidate->second->params.size();
                // Account for possible 'this' receiver in first param position
                size_t effective_params = gen_param_count;
                if (!candidate->second->params.empty() && candidate->second->params[0].pattern &&
                    candidate->second->params[0].pattern->is<parser::IdentPattern>() &&
                    candidate->second->params[0].pattern->as<parser::IdentPattern>().name ==
                        "this") {
                    effective_params = gen_param_count - 1;
                }
                if (call.args.size() == effective_params || call.args.size() == gen_param_count) {
                    pending_func_it = candidate;
                }
            }
        }
    }
    if (pending_func_it == pending_generic_funcs_.end()) {
        return std::nullopt;
    }

    const auto& gen_func = *pending_func_it->second;

    // Build set of generic parameter names for unification
    std::unordered_set<std::string> generic_names;
    for (const auto& g : gen_func.generics) {
        generic_names.insert(g.name);
    }

    // First, check for explicit type arguments in the callee
    // e.g., get_from_container[IntBox](ref box, 0) has explicit type arg IntBox
    std::unordered_map<std::string, types::TypePtr> bindings;
    if (call.callee->is<parser::PathExpr>()) {
        const auto& path_expr = call.callee->as<parser::PathExpr>();
        if (path_expr.generics.has_value() && !path_expr.generics->args.empty()) {
            // Map explicit type args to generic parameters
            for (size_t i = 0; i < path_expr.generics->args.size() && i < gen_func.generics.size();
                 ++i) {
                const auto& arg = path_expr.generics->args[i];
                if (arg.is_type()) {
                    // Convert parser type to semantic type
                    std::unordered_map<std::string, types::TypePtr> empty_subs;
                    types::TypePtr explicit_type =
                        resolve_parser_type_with_subs(*arg.as_type(), empty_subs);
                    bindings[gen_func.generics[i].name] = explicit_type;
                    TML_DEBUG_LN(
                        "[GENERIC CALL] explicit type arg: "
                        << gen_func.generics[i].name << " -> "
                        << (explicit_type->is<types::NamedType>() ? "NamedType" : "other"));
                }
            }
        }
    }

    // Infer any remaining type arguments using unification
    // For each argument, unify the parameter type pattern with the argument type
    // Also save arg types for where-clause resolution (unify_types may store
    // a different representation than infer_expr_type returns).
    std::vector<types::TypePtr> arg_types_for_where;
    for (size_t i = 0; i < call.args.size() && i < gen_func.params.size(); ++i) {
        types::TypePtr arg_type = infer_expr_type(*call.args[i]);
        arg_types_for_where.push_back(arg_type);
        unify_types(*gen_func.params[i].type, arg_type, generic_names, bindings);
    }
    // After unification, check if any bindings lost type_args.
    // unify_types may store a clone without type_args (e.g., SliceIter instead of
    // SliceIter[I32]) if the TypePtr is shared and later modified. Re-apply from
    // the saved arg types to ensure full type information is preserved.
    for (size_t i = 0; i < gen_func.params.size() && i < arg_types_for_where.size(); ++i) {
        if (!gen_func.params[i].type->is<parser::NamedType>())
            continue;
        const auto& pt = gen_func.params[i].type->as<parser::NamedType>();
        if (pt.path.segments.empty())
            continue;
        const std::string& param_name = pt.path.segments.back();
        if (generic_names.count(param_name) == 0)
            continue;
        auto& bound = bindings[param_name];
        if (bound && arg_types_for_where[i] && arg_types_for_where[i]->is<types::NamedType>() &&
            bound->is<types::NamedType>()) {
            const auto& arg_named = arg_types_for_where[i]->as<types::NamedType>();
            const auto& bound_named = bound->as<types::NamedType>();
            if (bound_named.name == arg_named.name && bound_named.type_args.empty() &&
                !arg_named.type_args.empty()) {
                // Binding lost type_args — restore from the saved arg type
                bound = arg_types_for_where[i];
            }
        }
    }

    // If some generic parameters couldn't be inferred from arguments,
    // try to infer from the return type using the expected type annotation context.
    // For example: `let e: Empty[I32] = empty()` — T can't be inferred from args (none),
    // but can be inferred from the return type Empty[T] matched against Empty[I32].
    {
        bool has_unbound = false;
        for (const auto& g : gen_func.generics) {
            if (bindings.find(g.name) == bindings.end()) {
                has_unbound = true;
                break;
            }
        }
        if (has_unbound && !expected_enum_type_.empty() && gen_func.return_type.has_value()) {
            // Parse expected_enum_type_ to semantic type for unification.
            // expected_enum_type_ is like "%struct.Empty__I32" — extract the mangled name
            // and convert to a semantic type via semantic_type_from_llvm.
            types::TypePtr expected_ret = semantic_type_from_llvm(expected_enum_type_);
            if (expected_ret) {
                unify_types(**gen_func.return_type, expected_ret, generic_names, bindings);
            }
        }
    }

    // Resolve where-clause type equalities to derive additional bindings.
    // For example: `func cloned[I: Iterator, T: Duplicate](iter: I) where I::Item = ref T`
    // After inferring I = SliceIter[I32] from args, resolve I::Item = ref I32,
    // then match pattern `ref T` to derive T = I32.
    if (gen_func.where_clause && !gen_func.where_clause->type_equalities.empty()) {
        for (const auto& [lhs, rhs] : gen_func.where_clause->type_equalities) {
            if (!lhs || !rhs)
                continue;
            if (!lhs->is<parser::NamedType>())
                continue;
            const auto& lhs_named = lhs->as<parser::NamedType>();
            if (lhs_named.path.segments.size() >= 2) {
                // Associated type equality: e.g., I::Item = ref T
                const std::string& param_name = lhs_named.path.segments[0];
                const std::string& assoc_name = lhs_named.path.segments.back();
                // Find the concrete type for this param. Prefer the saved arg type
                // (from infer_expr_type) over the binding, because unify_types may
                // store a type without full type_args.
                types::TypePtr concrete_param;
                for (size_t ai = 0; ai < gen_func.params.size(); ++ai) {
                    if (gen_func.params[ai].type->is<parser::NamedType>()) {
                        const auto& pt = gen_func.params[ai].type->as<parser::NamedType>();
                        if (!pt.path.segments.empty() && pt.path.segments.back() == param_name &&
                            ai < arg_types_for_where.size()) {
                            concrete_param = arg_types_for_where[ai];
                            break;
                        }
                    }
                }
                if (!concrete_param) {
                    auto param_it = bindings.find(param_name);
                    if (param_it != bindings.end())
                        concrete_param = param_it->second;
                }
                if (!concrete_param)
                    continue;
                types::TypePtr concrete_assoc =
                    resolve_assoc_type_for_concrete(concrete_param, assoc_name);
                if (concrete_assoc) {
                    // Match RHS pattern against concrete to extract type params
                    match_where_pattern_call(*rhs, concrete_assoc, bindings);
                }
            } else if (lhs_named.path.segments.size() == 1) {
                // Simple equality: e.g., F = func() -> T
                const std::string& param_name = lhs_named.path.segments[0];
                auto param_it = bindings.find(param_name);
                if (param_it == bindings.end() || !param_it->second)
                    continue;
                match_where_pattern_call(*rhs, param_it->second, bindings);
            }
        }
    }

    // Extract inferred type args in the order of generic parameters
    std::vector<types::TypePtr> inferred_type_args;
    for (const auto& g : gen_func.generics) {
        auto it = bindings.find(g.name);
        if (it != bindings.end()) {
            inferred_type_args.push_back(it->second);
        } else {
            // Generic not inferred - use Unit as fallback
            inferred_type_args.push_back(types::make_unit());
        }
    }

    // Register and get mangled name
    // Use the key from pending_generic_funcs_ (bare name like "forget")
    // rather than fn_name ("mem::forget") so generate_pending_instantiations can find it
    std::string base_name = pending_func_it->first;
    std::string mangled_name = require_func_instantiation(base_name, inferred_type_args);

    // Use bindings as substitution map for return type
    std::unordered_map<std::string, types::TypePtr>& subs = bindings;

    // Get substituted return type
    std::string ret_type = "void";
    if (gen_func.return_type.has_value()) {
        types::TypePtr subbed_ret = resolve_parser_type_with_subs(**gen_func.return_type, subs);
        ret_type = llvm_type_from_semantic(subbed_ret);
    }

    // Generate arguments with expected type context for generic enum constructors
    std::vector<std::pair<std::string, std::string>> arg_vals;
    for (size_t i = 0; i < call.args.size(); ++i) {
        // Set expected enum type for this argument based on parameter type with substitutions
        bool param_takes_ownership = true; // Default to taking ownership
        if (i < gen_func.params.size()) {
            types::TypePtr param_type =
                resolve_parser_type_with_subs(*gen_func.params[i].type, subs);
            std::string llvm_param_type = llvm_type_from_semantic(param_type);
            // Set expected type context for generic enum constructors like Nothing
            if (llvm_param_type.find("%struct.") == 0 &&
                llvm_param_type.find("__") != std::string::npos) {
                expected_enum_type_ = llvm_param_type;
            }
            // Check if parameter is a reference type (doesn't take ownership)
            if (param_type->is<types::RefType>()) {
                param_takes_ownership = false;
            }
            // Str is Copy (pointer copy, not move) — caller retains drop responsibility
            if (param_type->is<types::PrimitiveType>() &&
                param_type->as<types::PrimitiveType>().kind == types::PrimitiveKind::Str) {
                param_takes_ownership = false;
            }
        }
        std::string val = gen_expr(*call.args[i]);
        expected_enum_type_.clear(); // Clear after generating argument
        // Generic function params with FuncType now accept { ptr, ptr } (fat pointer)
        // so no coercion needed — pass the full fat pointer through
        std::string arg_type = last_expr_type_;
        arg_vals.push_back({val, arg_type});

        // CRITICAL: Mark variable as consumed if passed by value (ownership transfer)
        // This prevents double-drop at function return for moved variables
        if (param_takes_ownership && call.args[i]->is<parser::IdentExpr>()) {
            const auto& ident = call.args[i]->as<parser::IdentExpr>();
            mark_var_consumed(ident.name);
        }
        // Handle partial moves: mark struct field as consumed when passed by value
        else if (param_takes_ownership && call.args[i]->is<parser::FieldExpr>()) {
            const auto& field = call.args[i]->as<parser::FieldExpr>();
            // Get the base variable name
            if (field.object->is<parser::IdentExpr>()) {
                const auto& base = field.object->as<parser::IdentExpr>();
                mark_field_consumed(base.name, field.field);
            }
        }
    }

    // Call the instantiated function
    // Generic function instantiations don't use suite prefix - they're typically library
    // functions and should be shared across all test files in a suite
    std::string func_name = "@tml_" + mangled_name;
    std::string dbg_suffix = get_debug_loc_suffix();
    if (ret_type == "void") {
        emit("  call void " + func_name + "(");
        for (size_t i = 0; i < arg_vals.size(); ++i) {
            if (i > 0)
                emit(", ");
            emit(arg_vals[i].second + " " + arg_vals[i].first);
        }
        emit_line(")" + dbg_suffix);
        last_expr_type_ = "void";
        return "0";
    } else {
        std::string result = fresh_reg();
        emit("  " + result + " = call " + ret_type + " " + func_name + "(");
        for (size_t i = 0; i < arg_vals.size(); ++i) {
            if (i > 0)
                emit(", ");
            emit(arg_vals[i].second + " " + arg_vals[i].first);
        }
        emit_line(")" + dbg_suffix);
        last_expr_type_ = ret_type;
        return result;
    }
}

} // namespace tml::codegen
