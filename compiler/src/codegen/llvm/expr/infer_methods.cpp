TML_MODULE("codegen_x86")

//! # LLVM IR Generator - Type Inference (Method Calls & Remaining Expressions)
//!
//! This file is the continuation of infer.cpp, handling:
//! - Method call expressions (MethodCallExpr)
//! - Tuple, array, index, and cast expressions
//! - Deref coercion helpers
//! - Struct field lookup

#include "codegen/llvm/llvm_ir_gen.hpp"
#include "lexer/lexer.hpp"
#include "parser/parser.hpp"
#include "types/module.hpp"

#include <iostream>
#include <unordered_set>

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
// to extract type parameter bindings for where-clause resolution in type inference.
// For example: where F = func() -> T, with F -> func()->I32 derives T -> I32.
//              where I::Item = ref T, with ref I32 -> derives T -> I32.
static void infer_match_where_pattern(const parser::Type& pattern, const types::TypePtr& concrete,
                                      std::unordered_map<std::string, types::TypePtr>& type_subs) {
    if (!concrete)
        return;

    // Handle RefType pattern: `ref T` matches against `RefType{inner=I32}` -> T = I32
    if (pattern.is<parser::RefType>()) {
        const auto& ref_pattern = pattern.as<parser::RefType>();
        if (ref_pattern.inner && concrete->is<types::RefType>()) {
            const auto& concrete_ref = concrete->as<types::RefType>();
            if (concrete_ref.inner) {
                infer_match_where_pattern(*ref_pattern.inner, concrete_ref.inner, type_subs);
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
                        infer_match_where_pattern(*pt, concrete_named.type_args[i], type_subs);
                    }
                }
            }
        }
    }
}

// Helper: Extract return type and params from a semantic type that may be FuncType or ClosureType.
static bool infer_extract_func_signature(const types::TypePtr& type, types::TypePtr& ret,
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

// Helper: Resolve where clause type equalities to derive additional type substitutions
// for type inference. Mirrors resolve_impl_where_clause in method_impl.cpp.
//
// Handles three patterns:
// 1. `where F = func() -> T` — simple type param → function type equality
// 2. `where I::Item = ref T` — associated type path equality
// 3. `where F = func(I::Item) -> Maybe[B]` — function with associated type params
static void infer_resolve_where_clause(const parser::WhereClause& where_clause,
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
                infer_match_where_pattern(*rhs, assoc_it->second, type_subs);
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
            if (infer_extract_func_signature(concrete, con_ret, con_params)) {
                const auto& pat = rhs->as<parser::FuncType>();
                if (pat.return_type && con_ret) {
                    infer_match_where_pattern(*pat.return_type, con_ret, type_subs);
                }
                for (size_t pi = 0; pi < pat.params.size() && pi < con_params.size(); ++pi) {
                    if (pat.params[pi] && con_params[pi]) {
                        // Skip associated type paths in params (e.g., I::Item)
                        if (pat.params[pi]->is<parser::NamedType>()) {
                            const auto& param_named = pat.params[pi]->as<parser::NamedType>();
                            if (param_named.path.segments.size() >= 2) {
                                continue;
                            }
                        }
                        infer_match_where_pattern(*pat.params[pi], con_params[pi], type_subs);
                    }
                }
            }
        }
        // Match against non-function RHS (e.g., `where T = SomeType`)
        else {
            infer_match_where_pattern(*rhs, concrete, type_subs);
        }
    }
}

// Helper: infer semantic type from method call, tuple, array, index, cast expressions
auto LLVMIRGen::infer_expr_type_continued(const parser::Expr& expr) -> types::TypePtr {
    // Handle method call expressions (need to know return type of methods)
    if (expr.is<parser::MethodCallExpr>()) {
        const auto& call = expr.as<parser::MethodCallExpr>();

        // Check for static method calls on primitive types (e.g., I32::default())
        if (call.receiver->is<parser::IdentExpr>()) {
            const auto& type_name = call.receiver->as<parser::IdentExpr>().name;
            if (call.method == "default") {
                if (type_name == "I8")
                    return types::make_primitive(types::PrimitiveKind::I8);
                if (type_name == "I16")
                    return types::make_primitive(types::PrimitiveKind::I16);
                if (type_name == "I32")
                    return types::make_i32();
                if (type_name == "I64")
                    return types::make_i64();
                if (type_name == "I128")
                    return types::make_primitive(types::PrimitiveKind::I128);
                if (type_name == "U8")
                    return types::make_primitive(types::PrimitiveKind::U8);
                if (type_name == "U16")
                    return types::make_primitive(types::PrimitiveKind::U16);
                if (type_name == "U32")
                    return types::make_primitive(types::PrimitiveKind::U32);
                if (type_name == "U64")
                    return types::make_primitive(types::PrimitiveKind::U64);
                if (type_name == "U128")
                    return types::make_primitive(types::PrimitiveKind::U128);
                if (type_name == "F32")
                    return types::make_primitive(types::PrimitiveKind::F32);
                if (type_name == "F64")
                    return types::make_primitive(types::PrimitiveKind::F64);
                if (type_name == "Bool")
                    return types::make_bool();
                if (type_name == "Str")
                    return types::make_str();
            }

            // Check for static method calls on user-defined types (e.g., Request::builder())
            // First check if this type_name is a known struct/type (not a local variable)
            if (locals_.find(type_name) == locals_.end()) {
                std::string qualified_name = type_name + "::" + call.method;

                // Look up the static method in the environment
                auto func_sig = env_.lookup_func(qualified_name);

                // If not found locally, search all modules
                if (!func_sig && env_.module_registry()) {
                    const auto& all_modules = env_.module_registry()->get_all_modules();
                    for (const auto& [mod_name, mod] : all_modules) {
                        auto func_it = mod.functions.find(qualified_name);
                        if (func_it != mod.functions.end()) {
                            func_sig = func_it->second;
                            break;
                        }
                    }
                }

                if (func_sig && func_sig->return_type) {
                    return func_sig->return_type;
                }

                // Check if type_name is a class and look up static method
                auto class_def = env_.lookup_class(type_name);
                if (class_def.has_value()) {
                    for (const auto& m : class_def->methods) {
                        if (m.sig.name == call.method && m.is_static) {
                            return m.sig.return_type;
                        }
                    }
                }
            }
        }

        types::TypePtr receiver_type = infer_expr_type(*call.receiver);

        // Auto-deref: unwrap RefType for method dispatch (ref T -> T)
        if (receiver_type && receiver_type->is<types::RefType>()) {
            receiver_type = receiver_type->as<types::RefType>().inner;
        }

        // Handle optional chaining: expr?.method(args)
        // The receiver is Maybe[T]. We look up the method on T and wrap the
        // result in Maybe[ReturnType]. If the method already returns Maybe[V],
        // we flatten to Maybe[V].
        if (call.optional_chain && receiver_type && receiver_type->is<types::NamedType>()) {
            const auto& named = receiver_type->as<types::NamedType>();
            if (named.name == "Maybe" && !named.type_args.empty()) {
                types::TypePtr inner_type = named.type_args[0];

                // Look up the method on the inner type
                types::TypePtr method_ret;
                if (inner_type->is<types::NamedType>()) {
                    const auto& inner_named = inner_type->as<types::NamedType>();
                    std::string qualified = inner_named.name + "::" + call.method;
                    auto func_sig = env_.lookup_func(qualified);
                    if (!func_sig && env_.module_registry()) {
                        for (const auto& [mod_name, mod] :
                             env_.module_registry()->get_all_modules()) {
                            auto func_it = mod.functions.find(qualified);
                            if (func_it != mod.functions.end()) {
                                func_sig = func_it->second;
                                break;
                            }
                        }
                    }
                    if (func_sig && func_sig->return_type) {
                        method_ret = func_sig->return_type;
                        // Substitute type params from inner type's type_args
                        if (!func_sig->type_params.empty() && !inner_named.type_args.empty()) {
                            std::unordered_map<std::string, types::TypePtr> subs;
                            for (size_t i = 0; i < func_sig->type_params.size() &&
                                               i < inner_named.type_args.size();
                                 ++i) {
                                subs[func_sig->type_params[i]] = inner_named.type_args[i];
                            }
                            method_ret = types::substitute_type(method_ret, subs);
                        }
                    }
                } else if (inner_type->is<types::PrimitiveType>()) {
                    // Look up primitive type methods
                    std::string type_name;
                    auto kind = inner_type->as<types::PrimitiveType>().kind;
                    if (kind == types::PrimitiveKind::Str)
                        type_name = "Str";
                    else if (kind == types::PrimitiveKind::I32)
                        type_name = "I32";
                    else if (kind == types::PrimitiveKind::I64)
                        type_name = "I64";
                    if (!type_name.empty()) {
                        std::string qualified = type_name + "::" + call.method;
                        auto func_sig = env_.lookup_func(qualified);
                        if (func_sig && func_sig->return_type) {
                            method_ret = func_sig->return_type;
                        }
                    }
                }

                if (method_ret) {
                    // Flatten: if method already returns Maybe[V], return Maybe[V]
                    if (method_ret->is<types::NamedType>() &&
                        method_ret->as<types::NamedType>().name == "Maybe") {
                        return method_ret;
                    }
                    // Wrap in Maybe[ReturnType]
                    auto result = std::make_shared<types::Type>();
                    result->kind = types::NamedType{"Maybe", "", {method_ret}};
                    return result;
                }
            }
        }

        // Check for Ordering methods
        if (receiver_type && receiver_type->is<types::NamedType>()) {
            const auto& named = receiver_type->as<types::NamedType>();
            if (named.name == "Ordering") {
                // is_less, is_equal, is_greater return Bool
                if (call.method == "is_less" || call.method == "is_equal" ||
                    call.method == "is_greater") {
                    return types::make_bool();
                }
                // reverse, then_cmp return Ordering
                if (call.method == "reverse" || call.method == "then_cmp") {
                    auto result = std::make_shared<types::Type>();
                    result->kind = types::NamedType{"Ordering", "", {}};
                    return result;
                }
            }

            // Outcome[T, E] methods that return T
            if (named.name == "Outcome" && !named.type_args.empty()) {
                if (call.method == "unwrap" || call.method == "unwrap_or" ||
                    call.method == "unwrap_or_else" || call.method == "expect") {
                    return named.type_args[0]; // Return T
                }
                // is_ok, is_err return Bool
                if (call.method == "is_ok" || call.method == "is_err") {
                    return types::make_bool();
                }
            }

            // Shared[T] / Sync[T] get_mut returns Maybe[mut ref T]
            if ((named.name == "Shared" || named.name == "Sync" || named.name == "Arc") &&
                !named.type_args.empty() && call.method == "get_mut") {
                auto mut_ref = std::make_shared<types::Type>();
                mut_ref->kind = types::RefType{
                    .is_mut = true, .inner = named.type_args[0], .lifetime = std::nullopt};
                auto result = std::make_shared<types::Type>();
                result->kind = types::NamedType{"Maybe", "", {mut_ref}};
                return result;
            }

            // Maybe[T] methods that return T
            if (named.name == "Maybe" && !named.type_args.empty()) {
                if (call.method == "unwrap" || call.method == "unwrap_or" ||
                    call.method == "unwrap_or_else" || call.method == "expect") {
                    return named.type_args[0]; // Return T
                }
                // is_just, is_nothing return Bool
                if (call.method == "is_just" || call.method == "is_nothing") {
                    return types::make_bool();
                }
            }
        }

        // Check for class type methods
        if (receiver_type && receiver_type->is<types::ClassType>()) {
            const auto& class_type = receiver_type->as<types::ClassType>();
            // Search for the method in class hierarchy
            std::string current_class = class_type.name;
            while (!current_class.empty()) {
                auto class_def = env_.lookup_class(current_class);
                if (!class_def.has_value())
                    break;
                for (const auto& m : class_def->methods) {
                    if (m.sig.name == call.method && !m.is_static) {
                        return m.sig.return_type;
                    }
                }
                // Move to parent class
                current_class = class_def->base_class.value_or("");
            }
        }

        // Check for primitive type methods
        if (receiver_type && receiver_type->is<types::PrimitiveType>()) {
            const auto& prim = receiver_type->as<types::PrimitiveType>();
            auto kind = prim.kind;

            bool is_numeric =
                (kind == types::PrimitiveKind::I8 || kind == types::PrimitiveKind::I16 ||
                 kind == types::PrimitiveKind::I32 || kind == types::PrimitiveKind::I64 ||
                 kind == types::PrimitiveKind::I128 || kind == types::PrimitiveKind::U8 ||
                 kind == types::PrimitiveKind::U16 || kind == types::PrimitiveKind::U32 ||
                 kind == types::PrimitiveKind::U64 || kind == types::PrimitiveKind::U128 ||
                 kind == types::PrimitiveKind::F32 || kind == types::PrimitiveKind::F64);

            // cmp returns Ordering
            if (is_numeric && call.method == "cmp") {
                auto result = std::make_shared<types::Type>();
                result->kind = types::NamedType{"Ordering", "", {}};
                return result;
            }

            // max, min return the same type
            if (is_numeric && (call.method == "max" || call.method == "min")) {
                return receiver_type;
            }

            // Arithmetic methods return the same type
            if (is_numeric &&
                (call.method == "add" || call.method == "sub" || call.method == "mul" ||
                 call.method == "div" || call.method == "rem" || call.method == "neg")) {
                return receiver_type;
            }

            // negate returns Bool
            if (kind == types::PrimitiveKind::Bool && call.method == "negate") {
                return receiver_type;
            }

            // duplicate returns the same type (copy semantics)
            if (call.method == "duplicate") {
                return receiver_type;
            }

            // to_string returns Str (Display behavior)
            if (call.method == "to_string") {
                return types::make_str();
            }

            // debug_string returns Str (Debug behavior)
            if (call.method == "debug_string") {
                return types::make_str();
            }

            // hash returns I64
            if (call.method == "hash") {
                return types::make_i64();
            }

            // to_owned returns the same type (ToOwned behavior)
            if (call.method == "to_owned") {
                return receiver_type;
            }

            // borrow returns ref T (Borrow behavior)
            if (call.method == "borrow") {
                auto ref_type = std::make_shared<types::Type>();
                ref_type->kind = types::RefType{
                    .is_mut = false, .inner = receiver_type, .lifetime = std::nullopt};
                return ref_type;
            }

            // borrow_mut returns mut ref T (BorrowMut behavior)
            if (call.method == "borrow_mut") {
                auto ref_type = std::make_shared<types::Type>();
                ref_type->kind = types::RefType{
                    .is_mut = true, .inner = receiver_type, .lifetime = std::nullopt};
                return ref_type;
            }
        }

        // Check for array methods
        if (receiver_type && receiver_type->is<types::ArrayType>()) {
            const auto& arr_type = receiver_type->as<types::ArrayType>();
            types::TypePtr elem_type = arr_type.element;

            // len returns I64
            if (call.method == "len") {
                return types::make_i64();
            }

            // is_empty returns Bool
            if (call.method == "is_empty") {
                return types::make_bool();
            }

            // get, first, last return Maybe[ref T]
            if (call.method == "get" || call.method == "first" || call.method == "last") {
                auto ref_type = std::make_shared<types::Type>();
                ref_type->kind =
                    types::RefType{.is_mut = false, .inner = elem_type, .lifetime = std::nullopt};
                std::vector<types::TypePtr> type_args = {ref_type};
                auto result = std::make_shared<types::Type>();
                result->kind = types::NamedType{"Maybe", "", std::move(type_args)};
                return result;
            }

            // get_mut, first_mut, last_mut return Maybe[mut ref T]
            if (call.method == "get_mut" || call.method == "first_mut" ||
                call.method == "last_mut") {
                auto ref_type = std::make_shared<types::Type>();
                ref_type->kind =
                    types::RefType{.is_mut = true, .inner = elem_type, .lifetime = std::nullopt};
                std::vector<types::TypePtr> type_args = {ref_type};
                auto result = std::make_shared<types::Type>();
                result->kind = types::NamedType{"Maybe", "", std::move(type_args)};
                return result;
            }

            // each_ref returns Array[ref T, N]
            if (call.method == "each_ref") {
                auto ref_type = std::make_shared<types::Type>();
                ref_type->kind =
                    types::RefType{.is_mut = false, .inner = elem_type, .lifetime = std::nullopt};
                auto result = std::make_shared<types::Type>();
                result->kind = types::ArrayType{ref_type, arr_type.size};
                return result;
            }

            // each_mut returns Array[mut ref T, N]
            if (call.method == "each_mut") {
                auto ref_type = std::make_shared<types::Type>();
                ref_type->kind =
                    types::RefType{.is_mut = true, .inner = elem_type, .lifetime = std::nullopt};
                auto result = std::make_shared<types::Type>();
                result->kind = types::ArrayType{ref_type, arr_type.size};
                return result;
            }

            // map returns the same array type (simplified)
            if (call.method == "map") {
                return receiver_type;
            }

            // eq, ne return Bool
            if (call.method == "eq" || call.method == "ne") {
                return types::make_bool();
            }

            // cmp returns Ordering
            if (call.method == "cmp") {
                auto result = std::make_shared<types::Type>();
                result->kind = types::NamedType{"Ordering", "", {}};
                return result;
            }

            // as_slice returns Slice[T] (SliceType)
            if (call.method == "as_slice") {
                auto result = std::make_shared<types::Type>();
                result->kind = types::SliceType{elem_type};
                return result;
            }

            // as_mut_slice returns MutSlice[T]
            if (call.method == "as_mut_slice") {
                std::vector<types::TypePtr> type_args = {elem_type};
                auto result = std::make_shared<types::Type>();
                result->kind = types::NamedType{"MutSlice", "", std::move(type_args)};
                return result;
            }

            // iter returns ArrayIter
            if (call.method == "iter" || call.method == "into_iter") {
                std::vector<types::TypePtr> type_args = {elem_type};
                auto result = std::make_shared<types::Type>();
                result->kind = types::NamedType{"ArrayIter", "", std::move(type_args)};
                return result;
            }

            // duplicate returns same type
            if (call.method == "duplicate") {
                return receiver_type;
            }

            // to_string returns Str
            if (call.method == "to_string" || call.method == "debug_string") {
                return types::make_str();
            }
        }

        // Check for user-defined struct methods by looking up function signature
        if (receiver_type && receiver_type->is<types::NamedType>()) {
            const auto& named = receiver_type->as<types::NamedType>();
            std::string qualified_name = named.name + "::" + call.method;

            // Build type substitution map from receiver's type args
            std::unordered_map<std::string, types::TypePtr> type_subs;
            std::vector<std::string> type_param_names;
            if (!named.type_args.empty()) {
                // Look up the struct/impl to get generic parameter names.
                // Use pending_generic_impls_all_ to find the impl with the most matching
                // generics, since types like Cloned have impl[I,T] Iterator (2 params)
                // AND impl[I] Sync (1 param). pending_generic_impls_ stores only the LAST.
                auto impl_it = pending_generic_impls_.find(named.name);
                if (impl_it != pending_generic_impls_.end()) {
                    const parser::ImplDecl* best_impl = impl_it->second;
                    auto all_impl_it = pending_generic_impls_all_.find(named.name);
                    if (all_impl_it != pending_generic_impls_all_.end()) {
                        for (const auto* candidate : all_impl_it->second) {
                            if (candidate->generics.size() > best_impl->generics.size() &&
                                candidate->generics.size() <= named.type_args.size()) {
                                best_impl = candidate;
                            }
                        }
                    }
                    for (size_t i = 0; i < best_impl->generics.size() && i < named.type_args.size();
                         ++i) {
                        type_subs[best_impl->generics[i].name] = named.type_args[i];
                        type_param_names.push_back(best_impl->generics[i].name);
                    }
                } else if (env_.module_registry()) {
                    // Check imported structs for type params
                    const auto& all_modules = env_.module_registry()->get_all_modules();
                    for (const auto& [mod_name, mod] : all_modules) {
                        auto struct_it = mod.structs.find(named.name);
                        if (struct_it == mod.structs.end()) {
                            // Also check internal_structs
                            struct_it = mod.internal_structs.find(named.name);
                            if (struct_it == mod.internal_structs.end()) {
                                continue;
                            }
                        }
                        if (!struct_it->second.type_params.empty()) {
                            for (size_t i = 0; i < struct_it->second.type_params.size() &&
                                               i < named.type_args.size();
                                 ++i) {
                                type_subs[struct_it->second.type_params[i]] = named.type_args[i];
                                type_param_names.push_back(struct_it->second.type_params[i]);
                            }
                            break;
                        }
                    }
                }

                // Also add associated type mappings (e.g., I::Item -> ref I32)
                // For generic iterator types like SliceIter[I32], lookup_associated_type
                // may return the abstract form (e.g., ref T). We need to substitute
                // the inner type's type params to get the concrete associated type.
                for (size_t i = 0; i < type_param_names.size() && i < named.type_args.size(); ++i) {
                    const auto& arg = named.type_args[i];
                    if (arg && arg->is<types::NamedType>()) {
                        const auto& arg_named = arg->as<types::NamedType>();
                        auto item_type = lookup_associated_type(arg_named.name, "Item");
                        item_type = substitute_inner_assoc_type(item_type, arg_named, env_);
                        if (item_type) {
                            std::string assoc_key = type_param_names[i] + "::Item";
                            type_subs[assoc_key] = item_type;
                            type_subs["Item"] = item_type;
                        }
                    }
                }
            }

            // Resolve where clause type equalities to derive additional type subs.
            // For example: `impl[F, T] Iterator for RepeatWith[F] where F = func() -> T`
            // With F already mapped to func() -> I32, this derives T -> I32.
            // IMPORTANT: Use pending_generic_impls_all_ (NOT pending_generic_impls_) because
            // pending_generic_impls_ only stores the LAST registered impl per type.
            // Types like RepeatWith have multiple impls (Iterator, Send, Sync), and the
            // where clause may be on any of them — not necessarily the last one.
            {
                auto all_impl_it = pending_generic_impls_all_.find(named.name);
                if (all_impl_it != pending_generic_impls_all_.end()) {
                    for (const auto* local_impl : all_impl_it->second) {
                        if (local_impl->where_clause) {
                            infer_resolve_where_clause(*local_impl->where_clause, type_subs);
                        }
                    }
                } else if (env_.module_registry()) {
                    // For imported types, search module source for impl with where clause
                    const auto& all_modules = env_.module_registry()->get_all_modules();
                    for (const auto& [mod_name, mod] : all_modules) {
                        auto struct_it2 = mod.structs.find(named.name);
                        if (struct_it2 == mod.structs.end() || mod.source_code.empty())
                            continue;
                        // Parse module AST to find impl with where clause
                        const parser::Module* parsed_mod_ptr = nullptr;
                        parser::Module local_parsed_mod;
                        if (GlobalASTCache::should_cache(mod_name)) {
                            parsed_mod_ptr = GlobalASTCache::instance().get(mod_name);
                        }
                        if (!parsed_mod_ptr) {
                            auto source =
                                lexer::Source::from_string(mod.source_code, mod.file_path);
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
                                GlobalASTCache::instance().put(mod_name,
                                                               std::move(local_parsed_mod));
                                parsed_mod_ptr = GlobalASTCache::instance().get(mod_name);
                            } else {
                                parsed_mod_ptr = &local_parsed_mod;
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
                            if (target.path.segments.empty() ||
                                target.path.segments.back() != named.name)
                                continue;
                            if (!imp.where_clause)
                                continue;
                            infer_resolve_where_clause(*imp.where_clause, type_subs);
                        }
                        break;
                    }
                }
            }

            // Look up function in environment
            auto func_sig = env_.lookup_func(qualified_name);
            if (func_sig) {
                if (!type_subs.empty()) {
                    return types::substitute_type(func_sig->return_type, type_subs);
                }
                return func_sig->return_type;
            }

            // Also check imported modules if the receiver has a module path
            if (!named.module_path.empty()) {
                auto module = env_.get_module(named.module_path);
                if (module) {
                    auto func_it = module->functions.find(qualified_name);
                    if (func_it != module->functions.end()) {
                        if (!type_subs.empty()) {
                            return types::substitute_type(func_it->second.return_type, type_subs);
                        }
                        return func_it->second.return_type;
                    }
                }
            }

            // Check via imported symbol resolution
            auto imported_path = env_.resolve_imported_symbol(named.name);
            if (imported_path.has_value()) {
                std::string module_path;
                size_t pos = imported_path->rfind("::");
                if (pos != std::string::npos) {
                    module_path = imported_path->substr(0, pos);
                }

                auto module = env_.get_module(module_path);
                if (module) {
                    auto func_it = module->functions.find(qualified_name);
                    if (func_it != module->functions.end()) {
                        if (!type_subs.empty()) {
                            return types::substitute_type(func_it->second.return_type, type_subs);
                        }
                        return func_it->second.return_type;
                    }
                }
            }

            // Check if named type is a class and look up instance methods
            auto class_def = env_.lookup_class(named.name);
            if (class_def.has_value()) {
                std::string current_class = named.name;
                while (!current_class.empty()) {
                    auto cls = env_.lookup_class(current_class);
                    if (!cls.has_value())
                        break;
                    for (const auto& m : cls->methods) {
                        if (m.sig.name == call.method && !m.is_static) {
                            return m.sig.return_type;
                        }
                    }
                    // Move to parent class
                    current_class = cls->base_class.value_or("");
                }
            }

            // Look up methods in pending_generic_impls_ for generic types
            auto impl_it = pending_generic_impls_.find(named.name);
            if (impl_it != pending_generic_impls_.end()) {
                for (const auto& method : impl_it->second->methods) {
                    if (method.name == call.method) {
                        if (method.return_type.has_value()) {
                            // Convert parser type to semantic type with substitution
                            types::TypePtr ret_type = resolve_parser_type_with_subs(
                                *method.return_type.value(), type_subs);
                            return ret_type;
                        }
                        return types::make_unit();
                    }
                }
            }
        }

        // Default: try to return receiver type
        return receiver_type ? receiver_type : types::make_i32();
    }
    // Handle tuple expressions
    if (expr.is<parser::TupleExpr>()) {
        const auto& tuple = expr.as<parser::TupleExpr>();
        std::vector<types::TypePtr> element_types;
        for (const auto& elem : tuple.elements) {
            element_types.push_back(infer_expr_type(*elem));
        }
        return types::make_tuple(std::move(element_types));
    }
    // Handle array expressions [elem1, elem2, ...] or [expr; count]
    if (expr.is<parser::ArrayExpr>()) {
        const auto& arr = expr.as<parser::ArrayExpr>();

        if (std::holds_alternative<std::vector<parser::ExprPtr>>(arr.kind)) {
            const auto& elements = std::get<std::vector<parser::ExprPtr>>(arr.kind);
            if (elements.empty()) {
                // Empty array - use I32 as default element type
                auto result = std::make_shared<types::Type>();
                result->kind = types::ArrayType{types::make_i32(), 0};
                return result;
            }
            // Infer element type from first element
            types::TypePtr elem_type = infer_expr_type(*elements[0]);
            auto result = std::make_shared<types::Type>();
            result->kind = types::ArrayType{elem_type, elements.size()};
            return result;
        } else {
            // [expr; count] form
            const auto& pair = std::get<std::pair<parser::ExprPtr, parser::ExprPtr>>(arr.kind);
            types::TypePtr elem_type = infer_expr_type(*pair.first);

            // Get the count - must be a compile-time constant
            size_t count = 0;
            if (pair.second->is<parser::LiteralExpr>()) {
                const auto& lit = pair.second->as<parser::LiteralExpr>();
                if (lit.token.kind == lexer::TokenKind::IntLiteral) {
                    const auto& val = lit.token.int_value();
                    count = static_cast<size_t>(val.value);
                }
            }

            auto result = std::make_shared<types::Type>();
            result->kind = types::ArrayType{elem_type, count};
            return result;
        }
    }
    // Handle index expressions arr[i] or tuple.0
    if (expr.is<parser::IndexExpr>()) {
        const auto& idx = expr.as<parser::IndexExpr>();
        types::TypePtr obj_type = infer_expr_type(*idx.object);

        // If the object is an array, return element type
        if (obj_type && obj_type->is<types::ArrayType>()) {
            return obj_type->as<types::ArrayType>().element;
        }

        // If the object is a tuple, return the element type at the index
        if (obj_type && obj_type->is<types::TupleType>()) {
            const auto& tuple_type = obj_type->as<types::TupleType>();
            // Get the index value (tuple indices are literals like .0, .1, etc.)
            if (idx.index && idx.index->is<parser::LiteralExpr>()) {
                const auto& lit = idx.index->as<parser::LiteralExpr>();
                if (lit.token.kind == lexer::TokenKind::IntLiteral) {
                    size_t index = static_cast<size_t>(lit.token.int_value().value);
                    if (index < tuple_type.elements.size()) {
                        return tuple_type.elements[index];
                    }
                }
            }
            // If we can't determine the index, return the first element type as fallback
            if (!tuple_type.elements.empty()) {
                return tuple_type.elements[0];
            }
        }

        // Default: assume I32 for list element
        return types::make_i32();
    }

    // Handle cast expressions (x as I64)
    if (expr.is<parser::CastExpr>()) {
        const auto& cast = expr.as<parser::CastExpr>();
        // The type of a cast expression is its target type
        if (cast.target && cast.target->is<parser::NamedType>()) {
            const auto& named = cast.target->as<parser::NamedType>();
            if (!named.path.segments.empty()) {
                const std::string& type_name = named.path.segments.back();
                // Handle primitive types
                if (type_name == "I8")
                    return types::make_primitive(types::PrimitiveKind::I8);
                if (type_name == "I16")
                    return types::make_primitive(types::PrimitiveKind::I16);
                if (type_name == "I32")
                    return types::make_i32();
                if (type_name == "I64")
                    return types::make_i64();
                if (type_name == "I128")
                    return types::make_primitive(types::PrimitiveKind::I128);
                if (type_name == "U8")
                    return types::make_primitive(types::PrimitiveKind::U8);
                if (type_name == "U16")
                    return types::make_primitive(types::PrimitiveKind::U16);
                if (type_name == "U32")
                    return types::make_primitive(types::PrimitiveKind::U32);
                if (type_name == "U64")
                    return types::make_primitive(types::PrimitiveKind::U64);
                if (type_name == "U128")
                    return types::make_primitive(types::PrimitiveKind::U128);
                if (type_name == "F32")
                    return types::make_primitive(types::PrimitiveKind::F32);
                if (type_name == "F64")
                    return types::make_f64();
                if (type_name == "Bool")
                    return types::make_bool();
                if (type_name == "Str")
                    return types::make_str();
                if (type_name == "Char")
                    return types::make_primitive(types::PrimitiveKind::Char);
                // For other named types (classes, etc.), return a NamedType
                auto result = std::make_shared<types::Type>();
                result->kind = types::NamedType{type_name, "", {}};
                return result;
            }
        }
        // For pointer types in casts
        if (cast.target && cast.target->is<parser::PtrType>()) {
            const auto& ptr = cast.target->as<parser::PtrType>();
            auto inner = std::make_shared<types::Type>();
            if (ptr.inner && ptr.inner->is<parser::NamedType>()) {
                const auto& inner_named = ptr.inner->as<parser::NamedType>();
                if (!inner_named.path.segments.empty()) {
                    inner->kind = types::NamedType{inner_named.path.segments.back(), "", {}};
                }
            } else {
                inner = types::make_unit();
            }
            return types::make_ptr(inner);
        }
    }

    // Default: I32
    return types::make_i32();
}

// =============================================================================
// Deref Coercion Helpers
// =============================================================================

auto LLVMIRGen::get_deref_target_type(const types::TypePtr& type) -> types::TypePtr {
    if (!type || !type->is<types::NamedType>()) {
        return nullptr;
    }

    const auto& named = type->as<types::NamedType>();

    // Known smart pointer types that implement Deref
    // Arc[T] -> T (via deref)
    // Box[T] -> T (via deref)
    // Heap[T] -> T (via deref) - TML's name for Box
    // Rc[T] -> T (via deref)
    // Shared[T] -> T (via deref) - TML's name for Rc
    // Ptr[T] -> T (via deref in lowlevel blocks)
    // MutexGuard[T] -> T (via deref)

    static const std::unordered_set<std::string> deref_types = {
        "Arc",
        "Box",
        "Heap",
        "Rc",
        "Shared",
        "Weak",
        "Ptr",
        "MutexGuard",
        "RwLockReadGuard",
        "RwLockWriteGuard",
        "Ref",
        "RefMut",
    };

    if (deref_types.count(named.name) && !named.type_args.empty()) {
        // For these types, Deref::Target is the first type argument
        return named.type_args[0];
    }

    return nullptr;
}

auto LLVMIRGen::struct_has_field(const std::string& struct_name, const std::string& field_name)
    -> bool {
    // Check dynamic struct_fields_ registry first
    auto it = struct_fields_.find(struct_name);
    if (it != struct_fields_.end()) {
        for (const auto& field : it->second) {
            if (field.name == field_name) {
                return true;
            }
        }
    }

    // Check type environment
    auto struct_def = env_.lookup_struct(struct_name);
    if (struct_def) {
        for (const auto& fld : struct_def->fields) {
            if (fld.name == field_name) {
                return true;
            }
        }
    }

    // Search in module registry
    if (env_.module_registry()) {
        const auto& all_modules = env_.module_registry()->get_all_modules();
        for (const auto& [mod_name, mod] : all_modules) {
            auto mod_struct_it = mod.structs.find(struct_name);
            if (mod_struct_it != mod.structs.end()) {
                for (const auto& fld : mod_struct_it->second.fields) {
                    if (fld.name == field_name) {
                        return true;
                    }
                }
            }
            // Also check internal_structs
            auto internal_it = mod.internal_structs.find(struct_name);
            if (internal_it != mod.internal_structs.end()) {
                for (const auto& fld : internal_it->second.fields) {
                    if (fld.name == field_name) {
                        return true;
                    }
                }
            }
        }
    }

    return false;
}

} // namespace tml::codegen
