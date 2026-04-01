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

// Helper: Substitute inner type params in an associated type returned by lookup_associated_type.
// For generic iterators like SliceIter[I32], lookup_associated_type("SliceIter", "Item")
// returns the abstract form (ref T). This function substitutes T=I32 to get ref I32.
static types::TypePtr substitute_inner_assoc_type(types::TypePtr item_type,
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

// Helper: Recursively match a parser type pattern against a concrete semantic type
// to extract type parameter bindings. Handles nested generics like Maybe[T] -> Maybe[I32],
// and reference types like ref T -> ref I32.
static void match_where_pattern(const parser::Type& pattern, const types::TypePtr& concrete,
                                std::unordered_map<std::string, types::TypePtr>& type_subs) {
    if (!concrete) {
        return;
    }

    // Handle RefType pattern: `ref T` matches against `RefType{inner=I32}` -> T = I32
    if (pattern.is<parser::RefType>()) {
        const auto& ref_pattern = pattern.as<parser::RefType>();
        if (ref_pattern.inner && concrete->is<types::RefType>()) {
            const auto& concrete_ref = concrete->as<types::RefType>();
            if (concrete_ref.inner) {
                match_where_pattern(*ref_pattern.inner, concrete_ref.inner, type_subs);
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
        // Simple name like "T" — add mapping if not already present, or override placeholder
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

// Helper: Extract return type and params from a semantic type that may be FuncType or ClosureType.
// Returns true if the type is a function-like type, populating ret and params.
static bool extract_func_signature(const types::TypePtr& type, types::TypePtr& ret,
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
// Matches concrete types in type_subs against patterns to derive additional bindings.
//
// Handles three patterns:
// 1. `where F = func() -> T` — simple type param → function type equality
// 2. `where I::Item = ref T` — associated type path equality
// 3. `where F = func(I::Item) -> Maybe[B]` — function with associated type params
static void resolve_impl_where_clause(const parser::WhereClause& where_clause,
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

        // Case 1: Associated type path (2+ segments), e.g., `I::Item = ref T`
        // Resolve by looking up the first segment in type_subs, then finding
        // its associated type to get the concrete value for matching against RHS.
        if (segments.size() >= 2) {
            const std::string& type_param = segments[0];
            const std::string& assoc_name = segments[1];

            // Check if the type parameter is resolved in type_subs
            auto param_it = type_subs.find(type_param);
            if (param_it == type_subs.end() || !param_it->second) {
                continue;
            }

            // Also check if we already have the associated type key directly
            // (e.g., "I::Item" already resolved from the generic param setup)
            std::string assoc_key = type_param + "::" + assoc_name;
            auto assoc_it = type_subs.find(assoc_key);
            if (assoc_it != type_subs.end() && assoc_it->second) {
                // We have the concrete associated type value — match it against RHS pattern
                match_where_pattern(*rhs, assoc_it->second, type_subs);
            }
            continue;
        }

        // Case 2: Simple type param (1 segment), e.g., `F = func() -> T`
        const std::string& lhs_name = segments.back();
        auto sub_it = type_subs.find(lhs_name);
        if (sub_it == type_subs.end() || !sub_it->second) {
            continue;
        }
        const auto& concrete = sub_it->second;

        // Match against FuncType pattern on the RHS
        if (rhs->is<parser::FuncType>()) {
            types::TypePtr con_ret;
            std::vector<types::TypePtr> con_params;
            if (extract_func_signature(concrete, con_ret, con_params)) {
                const auto& pat = rhs->as<parser::FuncType>();
                if (pat.return_type && con_ret) {
                    match_where_pattern(*pat.return_type, con_ret, type_subs);
                }
                for (size_t pi = 0; pi < pat.params.size() && pi < con_params.size(); ++pi) {
                    if (pat.params[pi] && con_params[pi]) {
                        // If the param pattern is an associated type path (e.g., I::Item),
                        // skip matching it — it's a constraint, not a type variable to resolve.
                        // The concrete param type is already determined by the function signature.
                        if (pat.params[pi]->is<parser::NamedType>()) {
                            const auto& param_named = pat.params[pi]->as<parser::NamedType>();
                            if (param_named.path.segments.size() >= 2) {
                                // This is like `I::Item` — it's an associated type reference,
                                // not a type variable. Don't try to match it as a binding.
                                continue;
                            }
                        }
                        match_where_pattern(*pat.params[pi], con_params[pi], type_subs);
                    }
                }
            }
        }
        // Match against non-function RHS (e.g., `where T = SomeType`)
        else {
            match_where_pattern(*rhs, concrete, type_subs);
        }
    }
}

auto LLVMIRGen::try_gen_impl_method_call(const parser::MethodCallExpr& call,
                                         const std::string& receiver,
                                         const std::string& receiver_ptr,
                                         const types::TypePtr& receiver_type)
    -> std::optional<std::string> {
    const std::string& method = call.method;

    TML_DEBUG_LN("[IMPL_METHOD] try_gen_impl_method_call: method="
                 << method << " receiver_type="
                 << (receiver_type ? types::type_to_string(receiver_type) : "null"));

    // Convert TupleType to a synthetic NamedType for dispatch.
    // Tuple impls are registered under "Tuple2", "Tuple3", etc. in pending_generic_impls_.
    types::TypePtr effective_receiver = receiver_type;
    if (receiver_type && receiver_type->is<types::TupleType>()) {
        const auto& tuple = receiver_type->as<types::TupleType>();
        std::string tuple_name = "Tuple" + std::to_string(tuple.elements.size());
        auto synth = std::make_shared<types::Type>();
        synth->kind = types::NamedType{tuple_name, "", tuple.elements};
        effective_receiver = synth;
    }

    // Convert ArrayType to a synthetic NamedType("Array") for dispatch.
    // Array impls (e.g., impl[const N: I64] Array[U8, N]) are registered
    // under "Array::method" in the module functions map. The element type
    // is stored as a type arg so specialized impls can be matched.
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

    // Only handle NamedType receivers (including synthesized tuple types)
    if (!effective_receiver || !effective_receiver->is<types::NamedType>()) {
        return std::nullopt;
    }

    // Pin-dispatch: when the receiver is Pin[ref T] or Pin[mut ref T], behavior methods
    // like Future::poll are registered under the inner type (e.g., Ready::poll), not Pin::poll.
    // Unwrap Pin to get the inner type for method lookup, while keeping the Pin value as receiver.
    {
        const auto& outer_named = effective_receiver->as<types::NamedType>();
        if (outer_named.name == "Pin" && !outer_named.type_args.empty()) {
            types::TypePtr inner = outer_named.type_args[0];
            if (inner->is<types::RefType>()) {
                inner = inner->as<types::RefType>().inner;
            }
            if (inner->is<types::NamedType>()) {
                // Check if Pin::method exists first — only unwrap if it doesn't
                std::string pin_qualified = "Pin::" + method;
                auto pin_func = env_.lookup_func(pin_qualified);
                if (!pin_func && env_.module_registry()) {
                    for (const auto& [mod_name, mod] : env_.module_registry()->get_all_modules()) {
                        auto func_it = mod.functions.find(pin_qualified);
                        if (func_it != mod.functions.end()) {
                            pin_func = func_it->second;
                            break;
                        }
                    }
                }
                if (!pin_func) {
                    // Pin::method not found — use the inner type for dispatch
                    effective_receiver = inner;
                }
            }
        }
    }

    const auto& named = effective_receiver->as<types::NamedType>();
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
        // Also search GlobalModuleCache
        if (!func_sig) {
            for (const auto& [mod_path, mod] : types::GlobalModuleCache::instance().get_all()) {
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

    // Determine the offset for func_sig->params: if 'this' is included, offset=1.
    // Default behavior methods from module binary cache may omit 'this' (offset=0).
    size_t param_offset = (func_sig->params.size() > call.args.size()) ? 1 : 0;

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
                        method_type_suffix += "__";
                    }
                    method_type_suffix += mangle_type(semantic_type);
                }
            }
        }
    }
    // Infer method-level type parameters from argument types
    else if (call.type_args.empty() && !func_sig->type_params.empty()) {
        size_t impl_param_count = named.type_args.size();
        // If the receiver has more type_args than the func_sig has type_params,
        // the impl-level params were already substituted in the func_sig. All remaining
        // type_params are method-level (e.g., fold[B] on ListIter[I64] where
        // func_sig->type_params=["B"] but named.type_args=["I64"]).
        if (impl_param_count >= func_sig->type_params.size()) {
            impl_param_count = 0;
        }
        // Two-pass inference: first infer from FuncType/ClosureType params (more specific,
        // e.g., closure return type), then from bare type params (less specific, e.g., literals).
        // This ensures fold(0, do(acc: I64, x: I64) -> I64 { ... }) infers B=I64 from
        // the closure, not B=I32 from the literal 0.
        for (int pass = 0; pass < 2; ++pass) {
            for (size_t tp_idx = impl_param_count; tp_idx < func_sig->type_params.size();
                 ++tp_idx) {
                const std::string& type_param = func_sig->type_params[tp_idx];
                for (size_t p_idx = param_offset;
                     p_idx < func_sig->params.size() && (p_idx - param_offset) < call.args.size();
                     ++p_idx) {
                    const auto& param_type = func_sig->params[p_idx];

                    // Pass 0: only FuncType/ClosureType params (more specific inference)
                    // Pass 1: only bare GenericType/NamedType params (less specific, e.g.,
                    // literals)
                    if (pass == 1) {
                        // Handle GenericType parameters (e.g., fold[B] where init: B)
                        if (param_type && param_type->is<types::GenericType>()) {
                            const auto& gen = param_type->as<types::GenericType>();
                            if (gen.name == type_param) {
                                if (!type_subs.contains(type_param)) {
                                    auto arg_type =
                                        infer_expr_type(*call.args[p_idx - param_offset]);
                                    if (arg_type) {
                                        type_subs[type_param] = arg_type;
                                        if (!method_type_suffix.empty()) {
                                            method_type_suffix += "__";
                                        }
                                        method_type_suffix += mangle_type(arg_type);
                                    }
                                }
                            }
                        }
                        if (param_type && param_type->is<types::NamedType>()) {
                            const auto& param_named = param_type->as<types::NamedType>();
                            // Case 1: Bare type parameter — param IS the type param (e.g., err: E)
                            if (param_named.name == type_param && param_named.type_args.empty()) {
                                if (!type_subs.contains(type_param)) {
                                    auto arg_type =
                                        infer_expr_type(*call.args[p_idx - param_offset]);
                                    if (arg_type) {
                                        type_subs[type_param] = arg_type;
                                        if (!method_type_suffix.empty()) {
                                            method_type_suffix += "__";
                                        }
                                        method_type_suffix += mangle_type(arg_type);
                                    }
                                }
                            }
                            // Case 2: Type param inside generic type args (e.g., Maybe[U])
                            else {
                                for (size_t ta_idx = 0; ta_idx < param_named.type_args.size();
                                     ++ta_idx) {
                                    const auto& ta = param_named.type_args[ta_idx];
                                    if (ta && ta->is<types::NamedType>()) {
                                        const auto& ta_named = ta->as<types::NamedType>();
                                        if (ta_named.name == type_param) {
                                            if (!type_subs.contains(type_param)) {
                                                auto arg_type = infer_expr_type(
                                                    *call.args[p_idx - param_offset]);
                                                if (arg_type && arg_type->is<types::NamedType>()) {
                                                    const auto& arg_named =
                                                        arg_type->as<types::NamedType>();
                                                    if (ta_idx < arg_named.type_args.size()) {
                                                        auto inferred = arg_named.type_args[ta_idx];
                                                        if (inferred) {
                                                            type_subs[type_param] = inferred;
                                                            if (!method_type_suffix.empty()) {
                                                                method_type_suffix += "__";
                                                            }
                                                            method_type_suffix +=
                                                                mangle_type(inferred);
                                                        }
                                                    }
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    } // end pass == 1

                    if (pass == 0) {
                        // Handle FuncType parameters: func(E) -> F where F is the type param
                        if (param_type && param_type->is<types::FuncType>()) {
                            const auto& func_type = param_type->as<types::FuncType>();
                            // Check if the return type is the type parameter we're looking for
                            // Handle both NamedType("B") and GenericType("B") representations
                            bool ret_matches_param = false;
                            if (func_type.return_type &&
                                func_type.return_type->is<types::GenericType>()) {
                                ret_matches_param =
                                    func_type.return_type->as<types::GenericType>().name ==
                                    type_param;
                            } else if (func_type.return_type &&
                                       func_type.return_type->is<types::NamedType>()) {
                                const auto& ret_named =
                                    func_type.return_type->as<types::NamedType>();
                                ret_matches_param =
                                    ret_named.name == type_param && ret_named.type_args.empty();
                            }
                            if (ret_matches_param) {
                                if (!type_subs.contains(type_param)) {
                                    // Infer from the argument's return type
                                    auto arg_type =
                                        infer_expr_type(*call.args[p_idx - param_offset]);
                                    if (arg_type && arg_type->is<types::FuncType>()) {
                                        const auto& arg_func = arg_type->as<types::FuncType>();
                                        if (arg_func.return_type) {
                                            type_subs[type_param] = arg_func.return_type;
                                            if (!method_type_suffix.empty()) {
                                                method_type_suffix += "__";
                                            }
                                            method_type_suffix += mangle_type(arg_func.return_type);
                                        }
                                    }
                                    // Also try ClosureType arguments
                                    else if (arg_type && arg_type->is<types::ClosureType>()) {
                                        const auto& arg_clos = arg_type->as<types::ClosureType>();
                                        if (arg_clos.return_type) {
                                            type_subs[type_param] = arg_clos.return_type;
                                            if (!method_type_suffix.empty()) {
                                                method_type_suffix += "__";
                                            }
                                            method_type_suffix += mangle_type(arg_clos.return_type);
                                        }
                                    }
                                }
                            }
                        }
                    } // end pass == 0
                }
            }
        } // end for (pass)
    }

    // Handle generic type arguments
    if (!named.type_args.empty()) {
        mangled_type_name = mangle_struct_name(named.name, named.type_args);

        // If the library already emitted methods using the base type name
        // (e.g., tml_BTreeMap_insert from gen_impl_method), use the base name
        // so user code calls the existing function instead of a non-existent mangled one.
        // BUT: skip this fallback when inside a generic impl body (current_impl_type_
        // contains "__"), because the inner type may be from a different module than the
        // base-name function. E.g., inside Take__Repeat__I32::size_hint (async_iter module),
        // Repeat::size_hint from iter module is irrelevant — we need Repeat__I32::size_hint.
        bool inside_generic_impl = current_impl_type_.find("__") != std::string::npos;
        if (mangled_type_name != named.name && !inside_generic_impl) {
            std::string base_fn_check = "@" + mangle_impl_method(named.name, method);
            if (generated_functions_.contains(base_fn_check)) {
                mangled_type_name = named.name;
            }
        }

        std::string method_for_key = method;
        if (!method_type_suffix.empty()) {
            method_for_key += "__" + method_type_suffix;
        }
        std::string mangled_method_name = mangle_impl_method(mangled_type_name, method_for_key);

        // Check locally defined impls first.
        // Use pending_generic_impls_all_ to find the impl with the most matching
        // generics. Types like Cloned have impl[I,T] Iterator (2 params) AND
        // impl[I] Sync (1 param). The mangled type args may include extra params
        // (e.g., Cloned[SliceIter, I32] = I + T), so we need the impl with the
        // most generics to correctly map all type params.
        auto impl_it = pending_generic_impls_.find(named.name);
        if (impl_it != pending_generic_impls_.end()) {
            const parser::ImplDecl* best_impl = impl_it->second;
            // Check all impls for one with more matching generics
            auto best_impl_it = pending_generic_impls_all_.find(named.name);
            if (best_impl_it != pending_generic_impls_all_.end()) {
                for (const auto* candidate : best_impl_it->second) {
                    if (candidate->generics.size() > best_impl->generics.size() &&
                        candidate->generics.size() <= named.type_args.size()) {
                        best_impl = candidate;
                    }
                }
            }
            const auto& impl = *best_impl;
            for (size_t i = 0; i < impl.generics.size() && i < named.type_args.size(); ++i) {
                type_subs[impl.generics[i].name] = named.type_args[i];
                // Also resolve associated types for concrete type arguments
                // e.g., for I: Iterator where I = Counter, resolve I::Item = Counter::Item = I32
                if (named.type_args[i] && named.type_args[i]->is<types::NamedType>()) {
                    const auto& arg_named = named.type_args[i]->as<types::NamedType>();
                    auto item_type = lookup_associated_type(arg_named.name, "Item");
                    item_type = substitute_inner_assoc_type(item_type, arg_named, env_);
                    if (item_type) {
                        std::string assoc_key = impl.generics[i].name + "::Item";
                        type_subs[assoc_key] = item_type;
                        type_subs["Item"] = item_type;
                    }
                }
            }

            // For specialized impls (e.g., impl[T,E] Outcome[Outcome[T,E],E]), the
            // flat mapping above uses ENUM's T/E positions which gives wrong subs.
            // Check all registered impls for a specialized one that has the method,
            // and if found, use match_where_pattern to derive correct subs.
            auto specialized_it = pending_generic_impls_all_.find(named.name);
            if (specialized_it != pending_generic_impls_all_.end()) {
                for (const auto* alt_impl : specialized_it->second) {
                    if (!alt_impl->self_type || !alt_impl->self_type->is<parser::NamedType>()) {
                        continue;
                    }
                    const auto& self_named = alt_impl->self_type->as<parser::NamedType>();
                    if (!self_named.generics || self_named.generics->args.empty()) {
                        continue;
                    }
                    // Is this a specialized impl? (type arg is not just a bare param)
                    bool is_specialized = false;
                    for (const auto& arg : self_named.generics->args) {
                        if (arg.is_type() && arg.as_type()->is<parser::NamedType>()) {
                            const auto& arg_named = arg.as_type()->as<parser::NamedType>();
                            if (arg_named.generics && !arg_named.generics->args.empty()) {
                                is_specialized = true;
                                break;
                            }
                        }
                    }
                    if (!is_specialized) {
                        continue;
                    }
                    // Does this impl contain the method?
                    bool has_method = false;
                    for (const auto& m : alt_impl->methods) {
                        if (m.name == method) {
                            has_method = true;
                            break;
                        }
                    }
                    if (!has_method) {
                        continue;
                    }
                    // Derive correct type subs by matching self_type pattern against receiver
                    std::unordered_map<std::string, types::TypePtr> spec_subs;
                    match_where_pattern(*alt_impl->self_type, effective_receiver, spec_subs);
                    if (!spec_subs.empty()) {
                        for (const auto& [k, v] : spec_subs) {
                            type_subs[k] = v;
                        }
                    }
                    break;
                }
            }

            // Detect specialized impl nesting pattern. When the receiver is e.g.
            // Maybe[Maybe[I32]], the naive mapping gives T=Maybe[I32]. But for a
            // specialized impl like impl[T] Maybe[Maybe[T]], T should be I32.
            //
            // Detection heuristic: if type_subs[P] is a NamedType with the SAME
            // name as the receiver's outer type, AND the func_sig return type after
            // substitution equals the receiver type (indicating no unwrapping happened),
            // then "unwrap" by re-mapping from the inner type's type_args.
            //
            // This safely avoids unwrapping for methods from the general impl
            // (e.g., is_just, unwrap) because their return types don't match the
            // receiver type after substitution.
            if (func_sig && func_sig->return_type) {
                // Check if any type_sub has nesting
                bool has_nesting = false;
                for (const auto& [param, concrete] : type_subs) {
                    if (concrete && concrete->is<types::NamedType>()) {
                        const auto& cn = concrete->as<types::NamedType>();
                        if (cn.name == named.name && !cn.type_args.empty()) {
                            has_nesting = true;
                            break;
                        }
                    }
                }
                if (has_nesting) {
                    // Tentatively substitute to see if the result matches the receiver type
                    auto test_ret = types::substitute_type(func_sig->return_type, type_subs);
                    bool ret_matches_receiver = false;
                    if (test_ret && test_ret->is<types::NamedType>()) {
                        const auto& test_named = test_ret->as<types::NamedType>();
                        if (test_named.name == named.name &&
                            test_named.type_args.size() == named.type_args.size()) {
                            ret_matches_receiver = true;
                            // Deep compare type_args
                            for (size_t ti = 0; ti < named.type_args.size(); ++ti) {
                                if (types::type_to_string(test_named.type_args[ti]) !=
                                    types::type_to_string(named.type_args[ti])) {
                                    ret_matches_receiver = false;
                                    break;
                                }
                            }
                        }
                    }
                    if (ret_matches_receiver) {
                        // The substituted return type equals the receiver type, meaning
                        // the naive type_subs didn't unwrap. Apply the unwrapping.
                        std::vector<std::string> base_type_params;
                        auto base_enum = env_.lookup_enum(named.name);
                        if (base_enum && !base_enum->type_params.empty()) {
                            base_type_params = base_enum->type_params;
                        }
                        if (base_type_params.empty() && env_.module_registry()) {
                            for (const auto& [mn, mm] : env_.module_registry()->get_all_modules()) {
                                auto sit = mm.structs.find(named.name);
                                if (sit != mm.structs.end() && !sit->second.type_params.empty()) {
                                    base_type_params = sit->second.type_params;
                                    break;
                                }
                            }
                        }
                        if (!base_type_params.empty()) {
                            for (size_t si = 0;
                                 si < base_type_params.size() && si < named.type_args.size();
                                 ++si) {
                                auto sub_it = type_subs.find(base_type_params[si]);
                                if (sub_it == type_subs.end() || !sub_it->second)
                                    continue;
                                const auto& concrete = sub_it->second;
                                if (!concrete->is<types::NamedType>())
                                    continue;
                                const auto& concrete_named = concrete->as<types::NamedType>();
                                if (concrete_named.name != named.name)
                                    continue;
                                if (concrete_named.type_args.empty())
                                    continue;
                                // Unwrap: re-map from inner type's type_args
                                for (size_t ip = 0; ip < base_type_params.size() &&
                                                    ip < concrete_named.type_args.size();
                                     ++ip) {
                                    type_subs[base_type_params[ip]] = concrete_named.type_args[ip];
                                }
                                break;
                            }
                        }
                    }
                }
            }

            // Check if this type is from an imported module (even though impl is in
            // pending_generic_impls_, it may have been registered from an imported module)
            if (env_.module_registry()) {
                const auto& all_modules = env_.module_registry()->get_all_modules();
                for (const auto& [mod_name, mod] : all_modules) {
                    if (mod.structs.contains(named.name) || mod.enums.contains(named.name)) {
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
                        item_type = substitute_inner_assoc_type(item_type, arg_named, env_);
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
                    // Check structs (public and internal)
                    const types::StructDef* found_struct = nullptr;
                    auto struct_it = mod.structs.find(named.name);
                    if (struct_it != mod.structs.end() && !struct_it->second.type_params.empty()) {
                        found_struct = &struct_it->second;
                    }
                    if (!found_struct) {
                        auto internal_it = mod.internal_structs.find(named.name);
                        if (internal_it != mod.internal_structs.end() &&
                            !internal_it->second.type_params.empty()) {
                            found_struct = &internal_it->second;
                        }
                    }
                    if (found_struct) {
                        imported_type_params = found_struct->type_params;
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
                    // Check enums (public and internal)
                    const types::EnumDef* found_enum = nullptr;
                    auto enum_it = mod.enums.find(named.name);
                    if (enum_it != mod.enums.end() && !enum_it->second.type_params.empty()) {
                        found_enum = &enum_it->second;
                    }
                    if (!found_enum) {
                        auto internal_enum_it = mod.internal_enums.find(named.name);
                        if (internal_enum_it != mod.internal_enums.end() &&
                            !internal_enum_it->second.type_params.empty()) {
                            found_enum = &internal_enum_it->second;
                        }
                    }
                    if (found_enum) {
                        imported_type_params = found_enum->type_params;
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
            // Check ALL impls for where clauses, not just the single entry in
            // pending_generic_impls_ (which only stores the LAST registered impl).
            // Types like OnceWith have multiple impls (Iterator, Send, Sync),
            // and the where clause (e.g., `where F = func() -> T`) may be on any of them.
            auto all_impl_it = pending_generic_impls_all_.find(named.name);
            if (all_impl_it != pending_generic_impls_all_.end()) {
                for (const auto* local_impl : all_impl_it->second) {
                    if (local_impl->where_clause) {
                        resolve_impl_where_clause(*local_impl->where_clause, type_subs);
                    }
                }
            }
            // Also search imported module source for specialized impls,
            // where clauses, and constraint-based type destructuring.
            // This runs even when pending_generic_impls_all_ has entries,
            // because those may only contain behavior impls (e.g., PartialEq,
            // Clone) from other modules, while the specialized impl (e.g.,
            // impl[T] Maybe[Maybe[T]] with flatten) is in a different module
            // (e.g., core::types::option) that wasn't registered in
            // pending_generic_impls_all_.
            if (env_.module_registry()) {
                const auto& all_modules = env_.module_registry()->get_all_modules();

                for (const auto& [mod_name, mod] : all_modules) {
                    // Check structs (public + internal), enums (public + internal),
                    // and also check if this module defines the method we're calling
                    // (handles builtin enums like Maybe/Outcome whose impls are in
                    // library modules but the enum itself is pre-registered in TypeEnv)
                    bool type_in_module = mod.structs.count(named.name) > 0 ||
                                          mod.internal_structs.count(named.name) > 0 ||
                                          mod.enums.count(named.name) > 0 ||
                                          mod.internal_enums.count(named.name) > 0 ||
                                          mod.functions.count(named.name + "::" + method) > 0;
                    if (!type_in_module || mod.source_code.empty()) {
                        continue;
                    }
                    // Get parsed AST from cache or parse
                    const parser::Module* parsed_mod_ptr = nullptr;
                    parser::Module local_parsed_mod;
                    if (GlobalASTCache::should_cache(mod_name)) {
                        parsed_mod_ptr = GlobalASTCache::instance().get(mod_name);
                    }
                    if (parsed_mod_ptr == nullptr) {
                        auto source = lexer::Source::from_string(mod.source_code, mod.file_path);
                        lexer::Lexer lex(source);
                        auto tokens = lex.tokenize();
                        if (lex.has_errors()) {
                            continue;
                        }
                        parser::Parser mod_parser(std::move(tokens));
                        auto module_name_stem = mod_name;
                        if (auto pos = module_name_stem.rfind("::"); pos != std::string::npos) {
                            module_name_stem = module_name_stem.substr(pos + 2);
                        }
                        auto parse_result = mod_parser.parse_module(module_name_stem);
                        if (!std::holds_alternative<parser::Module>(parse_result)) {
                            continue;
                        }
                        local_parsed_mod = std::get<parser::Module>(std::move(parse_result));
                        if (GlobalASTCache::should_cache(mod_name)) {
                            GlobalASTCache::instance().put(mod_name, std::move(local_parsed_mod));
                            parsed_mod_ptr = GlobalASTCache::instance().get(mod_name);
                        } else {
                            parsed_mod_ptr = &local_parsed_mod;
                        }
                    }
                    if (parsed_mod_ptr == nullptr) {
                        continue;
                    }
                    // Iterate all impl blocks for our type
                    for (const auto& decl : parsed_mod_ptr->decls) {
                        if (!decl->is<parser::ImplDecl>()) {
                            continue;
                        }
                        const auto& imp = decl->as<parser::ImplDecl>();
                        if (!imp.self_type || !imp.self_type->is<parser::NamedType>()) {
                            continue;
                        }
                        const auto& target = imp.self_type->as<parser::NamedType>();
                        if (target.path.segments.empty() ||
                            target.path.segments.back() != named.name) {
                            continue;
                        }

                        // Check for specialized impl (e.g., impl[T] Maybe[Maybe[T]],
                        // impl[T,E] Outcome[Outcome[T,E], E]). A specialized impl has
                        // type args that contain nested generics, not just bare params.
                        if (target.generics && !target.generics->args.empty()) {
                            bool is_specialized = false;
                            for (const auto& arg : target.generics->args) {
                                if (arg.is_type() && arg.as_type()->is<parser::NamedType>()) {
                                    const auto& arg_named = arg.as_type()->as<parser::NamedType>();
                                    if (arg_named.generics && !arg_named.generics->args.empty()) {
                                        is_specialized = true;
                                        break;
                                    }
                                }
                            }
                            if (is_specialized) {
                                // Does this specialized impl contain the method?
                                bool has_method = false;
                                for (const auto& m : imp.methods) {
                                    if (m.name == method) {
                                        has_method = true;
                                        break;
                                    }
                                }
                                if (has_method) {
                                    // Derive correct type subs by matching self_type
                                    // pattern against receiver type.
                                    // e.g., Maybe[Maybe[T]] matched against Maybe[Maybe[I32]]
                                    //        gives T = I32 (not T = Maybe[I32])
                                    std::unordered_map<std::string, types::TypePtr> spec_subs;
                                    match_where_pattern(*imp.self_type, effective_receiver,
                                                        spec_subs);
                                    if (!spec_subs.empty()) {
                                        for (const auto& [k, v] : spec_subs) {
                                            type_subs[k] = v;
                                        }
                                    }
                                }
                            }
                        }

                        // Process where clause type equalities
                        if (imp.where_clause) {
                            resolve_impl_where_clause(*imp.where_clause, type_subs);

                            // Also process where clause constraints for type destructuring.
                            // e.g., `where T: Outcome[T, E]` on impl[T] Maybe[T]
                            // When T = Outcome[I32, Str], matching against Outcome[T, E]
                            // extracts T = I32, E = Str (overriding the outer T).
                            for (const auto& [lhs, bounds] : imp.where_clause->constraints) {
                                if (!lhs || !lhs->is<parser::NamedType>()) {
                                    continue;
                                }
                                const auto& lhs_named = lhs->as<parser::NamedType>();
                                if (lhs_named.path.segments.empty()) {
                                    continue;
                                }
                                const std::string& param_name = lhs_named.path.segments.back();
                                // Look up the concrete type for this parameter
                                auto param_it = type_subs.find(param_name);
                                if (param_it == type_subs.end() || !param_it->second) {
                                    continue;
                                }
                                const auto& concrete = param_it->second;
                                // Check each bound — if the bound is a generic type
                                // (not a behavior), treat as destructuring pattern
                                for (const auto& bound : bounds) {
                                    if (!bound || !bound->is<parser::NamedType>()) {
                                        continue;
                                    }
                                    const auto& bound_named = bound->as<parser::NamedType>();
                                    if (!bound_named.generics ||
                                        bound_named.generics->args.empty()) {
                                        continue;
                                    }
                                    // This is a type destructuring constraint like T: Maybe[T]
                                    // or T: Outcome[T, E]. Match the concrete type against
                                    // the pattern to extract inner type params.
                                    if (concrete->is<types::NamedType>()) {
                                        const auto& concrete_named =
                                            concrete->as<types::NamedType>();
                                        if (concrete_named.name ==
                                            bound_named.path.segments.back()) {
                                            match_where_pattern(*bound, concrete, type_subs);
                                        }
                                    }
                                }
                            }
                        }
                    }
                    break;
                }
            }
        }

        // Resolve unresolved method-level type params AND detect where-clause
        // constraint destructuring patterns. Handles e.g.:
        //   func transpose[E](this) -> Outcome[Maybe[T], E] where T: Outcome[T, E]
        // When T = Outcome[I32, Str], this extracts T = I32, E = Str from Outcome's
        // type_args by matching against Outcome's type_params.
        //
        // The approach: for each concrete type in type_subs that is a NamedType
        // with type_args, look up the concrete type's own type_params. If any of
        // those type_params match an existing type_sub key or an unresolved type
        // param in func_sig, extract the corresponding type_arg.
        //
        // IMPORTANT: Do NOT override impl-level type params that were already resolved
        // from named.type_args. When Shared[T] has T=PromiseState[I32], and
        // PromiseState itself also has a param named "T", the inner "T" must NOT
        // override the outer impl-level "T". Only destructure into params that are
        // genuinely unresolved (method-level type params, not impl-level ones).
        if (func_sig && !func_sig->type_params.empty()) {
            // Collect impl-level generic param names — these must NOT be overridden
            // by the destructuring logic below, since they were already correctly
            // resolved from the receiver's type_args at line 556-557.
            std::unordered_set<std::string> impl_level_params;
            if (impl_it != pending_generic_impls_.end()) {
                auto best_impl_it2 = pending_generic_impls_all_.find(named.name);
                const parser::ImplDecl* best_impl2 = impl_it->second;
                if (best_impl_it2 != pending_generic_impls_all_.end()) {
                    for (const auto* candidate : best_impl_it2->second) {
                        if (candidate->generics.size() > best_impl2->generics.size() &&
                            candidate->generics.size() <= named.type_args.size()) {
                            best_impl2 = candidate;
                        }
                    }
                }
                for (const auto& g : best_impl2->generics) {
                    impl_level_params.insert(g.name);
                }
            }

            // Collect entries to update (can't modify type_subs while iterating)
            std::unordered_map<std::string, types::TypePtr> new_subs;
            for (const auto& [existing_param, existing_concrete] : type_subs) {
                if (!existing_concrete || !existing_concrete->is<types::NamedType>())
                    continue;
                const auto& ec_named = existing_concrete->as<types::NamedType>();
                if (ec_named.type_args.empty())
                    continue;
                // Look up the concrete type's type_params
                std::vector<std::string> ec_type_params;
                auto ec_enum = env_.lookup_enum(ec_named.name);
                if (ec_enum && !ec_enum->type_params.empty()) {
                    ec_type_params = ec_enum->type_params;
                }
                if (ec_type_params.empty() && env_.module_registry()) {
                    for (const auto& [mn, mm] : env_.module_registry()->get_all_modules()) {
                        auto sit = mm.structs.find(ec_named.name);
                        if (sit != mm.structs.end() && !sit->second.type_params.empty()) {
                            ec_type_params = sit->second.type_params;
                            break;
                        }
                    }
                }
                if (ec_type_params.empty())
                    continue;
                // For each of the concrete type's type_params, check if it matches
                // a func_sig type_param (resolved or unresolved)
                for (size_t epi = 0; epi < ec_type_params.size() && epi < ec_named.type_args.size();
                     ++epi) {
                    const std::string& inner_param = ec_type_params[epi];
                    // Skip if this inner param would override the SAME outer param
                    // it was derived from. E.g., Shared[T] has T=PromiseState[I32],
                    // and PromiseState also has a param "T". Extracting inner "T"=I32
                    // would overwrite the outer T=PromiseState[I32] — wrong!
                    // But for transpose where T=Outcome[I32,Str] and we extract E=Str
                    // from Outcome's params, that's a DIFFERENT key — allowed.
                    if (inner_param == existing_param && impl_level_params.count(inner_param))
                        continue;
                    // Check if inner_param is in func_sig->type_params
                    bool is_func_tp = false;
                    for (const auto& ftp : func_sig->type_params) {
                        if (ftp == inner_param) {
                            is_func_tp = true;
                            break;
                        }
                    }
                    if (!is_func_tp)
                        continue;
                    // This inner_param is a type param that should be resolved
                    // from the concrete type's type_args
                    if (!new_subs.count(inner_param)) {
                        new_subs[inner_param] = ec_named.type_args[epi];
                    }
                }
            }
            // Apply new subs
            for (const auto& [k, v] : new_subs) {
                type_subs[k] = v;
            }
        }

        TML_DEBUG_LN("[IMPL_METHOD]   generic path: mangled="
                     << mangled_type_name << " is_imported=" << is_imported
                     << " imported_type_params=" << imported_type_params.size()
                     << " is_local=" << (impl_it != pending_generic_impls_.end())
                     << " mangled_method=" << mangled_method_name);

        // Map Self/This to the concrete implementing type so that trait method
        // parameters (e.g., ref Self in ne/lt/le/gt/ge defaults) resolve correctly
        // when building the call instruction's argument types.
        {
            auto self_type = std::make_shared<types::Type>();
            self_type->kind = types::NamedType{named.name, "", named.type_args};
            type_subs["Self"] = self_type;
            type_subs["This"] = self_type;
        }

        if (!generated_impl_methods_.contains(mangled_method_name)) {
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
        std::string mangled_method_name =
            mangle_impl_method(mangled_type_name, full_method_for_key);

        if (!generated_impl_methods_.contains(mangled_method_name)) {
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
            std::string mangled_method_name = mangle_impl_method(mangled_type_name, method);
            if (!generated_impl_methods_.contains(mangled_method_name)) {
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
            // Use mangle_impl_method which handles module path lookup for library types
            // and suite prefix for local types automatically.
            fn_name = "@" + mangle_impl_method(mangled_type_name, full_method_name);
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
    std::string this_arg_type = is_primitive_impl ? impl_llvm_type : "ptr";

    // No special by-value struct handling needed here.
    // All struct params (including non-this/self like ManuallyDrop::into_inner(slot))
    // are passed as ptr in both the call site and the function definition.
    // The function body loads the struct from the ptr if needed (see impl.cpp).

    // Skip 'this' argument for Unit type — Unit methods have no 'this' parameter
    // because void is not a valid LLVM parameter type (see impl.cpp void guard).
    bool is_unit_type = (impl_llvm_type == "void" || impl_llvm_type == "{}");
    if (!is_unit_type) {
        typed_args.push_back({this_arg_type, impl_receiver_val});
    }

    for (size_t i = 0; i < call.args.size(); ++i) {
        std::string val = gen_expr(*call.args[i]);
        // Function/closure parameters now use fat pointer { ptr, ptr } — no coercion needed
        // The fat pointer preserves the env_ptr for capturing closures
        std::string actual_type = last_expr_type_;
        std::string expected_type = "i32";
        types::TypePtr param_type_resolved;
        // Determine parameter index: func_sig may or may not include 'this' at index 0.
        // Use param_offset (computed earlier) for consistent indexing.
        // param_offset = 1 when 'this' is present, 0 when omitted.
        size_t sig_idx = i + param_offset;
        if (func_sig && sig_idx < func_sig->params.size()) {
            param_type_resolved = func_sig->params[sig_idx];
            if (!type_subs.empty()) {
                param_type_resolved = types::substitute_type(param_type_resolved, type_subs);
            }
            expected_type = llvm_type_from_semantic(param_type_resolved);
            // Function-typed parameters use fat pointer { ptr, ptr }
            if (param_type_resolved->is<types::FuncType>()) {
                expected_type = "{ ptr, ptr }";
            }
        }
        // Fallback: when func_sig is unavailable (e.g., default behavior methods like
        // for_each, map, filter), use the registered LLVM param types from functions_
        // For functions_ lookup, use the registered param count which already reflects
        // the actual LLVM signature (no 'this' for Unit). Use is_unit_type to adjust.
        else if (method_it != functions_.end()) {
            size_t fn_offset = is_unit_type ? 0 : 1;
            if ((i + fn_offset) < method_it->second.param_types.size()) {
                expected_type = method_it->second.param_types[i + fn_offset];
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
        // struct/enum → ptr ABI fix (see impl.cpp:282)
        // i+1 because typed_args[0] is 'this'
        if (method_it != functions_.end() && (i + 1) < method_it->second.param_types.size()) {
            const auto& expected_def = method_it->second.param_types[i + 1];
            if (expected_def == "ptr" &&
                (expected_type.find("%struct.") == 0 || expected_type.find("%enum.") == 0)) {
                std::string temp = fresh_reg();
                emit_line("  " + temp + " = alloca " + expected_type);
                emit_line("  store " + expected_type + " " + val + ", ptr " + temp);
                val = temp;
                expected_type = "ptr";
            }
        }
        typed_args.push_back({expected_type, val});
    }

    auto return_type = func_sig->return_type;
    if (!type_subs.empty()) {
        return_type = types::substitute_type(return_type, type_subs);
    }
    std::string ret_type = llvm_type_from_semantic(return_type);

    // If return type resolved to 'ptr' (may be an unresolved associated type like
    // Maybe[This::Item]) but the function is already registered in functions_ with
    // a concrete return type (e.g., %struct.Maybe__I32 from generate_default_method),
    // use the registered type to avoid calling convention mismatches.
    if (ret_type == "ptr" && method_it != functions_.end() && !method_it->second.ret_type.empty() &&
        method_it->second.ret_type != "ptr") {
        ret_type = method_it->second.ret_type;
    }

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

} // namespace tml::codegen
