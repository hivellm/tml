TML_MODULE("compiler")

//! # Type Checker - Method Call Expressions
//!
//! This file implements type checking for method calls (receiver.method(args)).
//! Split from expr_call.cpp for maintainability.
//!
//! ## Method Call Resolution Order
//!
//! 1. Static methods on primitive type names
//! 2. Static methods on class types
//! 3. Instance methods on receiver type
//! 4. Behavior method lookup (for dyn types)
//! 5. Where clause bound methods (for generic type parameters)
//! 6. Primitive type builtin methods
//! 7. Named type methods (Maybe, Outcome, List, Array, Slice)

#include "common.hpp"
#include "lexer/token.hpp"
#include "types/checker.hpp"
#include "types/module.hpp"

#include <functional>
#include <set>
#include <unordered_set>

namespace tml::types {

// Forward declarations from helpers.cpp
bool is_integer_type(const TypePtr& type);
bool types_compatible(const TypePtr& expected, const TypePtr& actual);

// Helper to get primitive name as string (duplicated from expr_call.cpp)
static std::string primitive_to_string(PrimitiveKind kind) {
    switch (kind) {
    case PrimitiveKind::I8:
        return "I8";
    case PrimitiveKind::I16:
        return "I16";
    case PrimitiveKind::I32:
        return "I32";
    case PrimitiveKind::I64:
        return "I64";
    case PrimitiveKind::I128:
        return "I128";
    case PrimitiveKind::U8:
        return "U8";
    case PrimitiveKind::U16:
        return "U16";
    case PrimitiveKind::U32:
        return "U32";
    case PrimitiveKind::U64:
        return "U64";
    case PrimitiveKind::U128:
        return "U128";
    case PrimitiveKind::F32:
        return "F32";
    case PrimitiveKind::F64:
        return "F64";
    case PrimitiveKind::Bool:
        return "Bool";
    case PrimitiveKind::Char:
        return "Char";
    case PrimitiveKind::Str:
        return "Str";
    case PrimitiveKind::Unit:
        return "Unit";
    case PrimitiveKind::Never:
        return "Never";
    }
    return "unknown";
}

/// Extract type parameter bindings by matching parameter type against argument type.
/// For example, matching `ManuallyDrop[T]` against `ManuallyDrop[I64]` extracts {T -> I64}.
/// (duplicated from expr_call.cpp)
static void extract_type_params(const TypePtr& param_type, const TypePtr& arg_type,
                                const std::vector<std::string>& type_params,
                                std::unordered_map<std::string, TypePtr>& substitutions) {
    if (!param_type || !arg_type) {
        return;
    }

    // If param_type is a NamedType that matches a type parameter directly
    if (param_type->is<NamedType>()) {
        const auto& named = param_type->as<NamedType>();
        // Check if this is a type parameter (simple name with no type_args)
        if (named.type_args.empty() && named.module_path.empty()) {
            for (const auto& tp : type_params) {
                if (named.name == tp) {
                    substitutions[tp] = arg_type;
                    return;
                }
            }
        }
        // If both are NamedType with same name, recursively match type_args
        if (arg_type->is<NamedType>()) {
            const auto& arg_named = arg_type->as<NamedType>();
            if (named.name == arg_named.name &&
                named.type_args.size() == arg_named.type_args.size()) {
                for (size_t i = 0; i < named.type_args.size(); ++i) {
                    extract_type_params(named.type_args[i], arg_named.type_args[i], type_params,
                                        substitutions);
                }
            }
        }
        return;
    }

    // If param_type is a GenericType
    if (param_type->is<GenericType>()) {
        const auto& gen = param_type->as<GenericType>();
        for (const auto& tp : type_params) {
            if (gen.name == tp) {
                substitutions[tp] = arg_type;
                return;
            }
        }
        return;
    }

    // RefType: match inner types
    if (param_type->is<RefType>() && arg_type->is<RefType>()) {
        const auto& param_ref = param_type->as<RefType>();
        const auto& arg_ref = arg_type->as<RefType>();
        extract_type_params(param_ref.inner, arg_ref.inner, type_params, substitutions);
        return;
    }

    // TupleType: match element types
    if (param_type->is<TupleType>() && arg_type->is<TupleType>()) {
        const auto& param_tuple = param_type->as<TupleType>();
        const auto& arg_tuple = arg_type->as<TupleType>();
        if (param_tuple.elements.size() == arg_tuple.elements.size()) {
            for (size_t i = 0; i < param_tuple.elements.size(); ++i) {
                extract_type_params(param_tuple.elements[i], arg_tuple.elements[i], type_params,
                                    substitutions);
            }
        }
        return;
    }

    // ArrayType: match element types
    if (param_type->is<ArrayType>() && arg_type->is<ArrayType>()) {
        const auto& param_arr = param_type->as<ArrayType>();
        const auto& arg_arr = arg_type->as<ArrayType>();
        extract_type_params(param_arr.element, arg_arr.element, type_params, substitutions);
        return;
    }

    // SliceType: match element types
    if (param_type->is<SliceType>() && arg_type->is<SliceType>()) {
        const auto& param_slice = param_type->as<SliceType>();
        const auto& arg_slice = arg_type->as<SliceType>();
        extract_type_params(param_slice.element, arg_slice.element, type_params, substitutions);
        return;
    }

    // FuncType: match parameter and return types
    if (param_type->is<FuncType>() && arg_type->is<FuncType>()) {
        const auto& param_func = param_type->as<FuncType>();
        const auto& arg_func = arg_type->as<FuncType>();
        // Match parameter types
        if (param_func.params.size() == arg_func.params.size()) {
            for (size_t i = 0; i < param_func.params.size(); ++i) {
                extract_type_params(param_func.params[i], arg_func.params[i], type_params,
                                    substitutions);
            }
        }
        // Match return type
        extract_type_params(param_func.return_type, arg_func.return_type, type_params,
                            substitutions);
        return;
    }

    // FuncType param vs ClosureType arg: closures are compatible with func types
    if (param_type->is<FuncType>() && arg_type->is<ClosureType>()) {
        const auto& param_func = param_type->as<FuncType>();
        const auto& arg_closure = arg_type->as<ClosureType>();
        // Match parameter types
        if (param_func.params.size() == arg_closure.params.size()) {
            for (size_t i = 0; i < param_func.params.size(); ++i) {
                extract_type_params(param_func.params[i], arg_closure.params[i], type_params,
                                    substitutions);
            }
        }
        // Match return type
        extract_type_params(param_func.return_type, arg_closure.return_type, type_params,
                            substitutions);
        return;
    }
}

auto TypeChecker::check_method_call(const parser::MethodCallExpr& call) -> TypePtr {
    // =========================================================================
    // Handle optional chaining: expr?.method(args)
    // The receiver must be Maybe[T]. We resolve the method on T (the inner type),
    // then wrap the result in Maybe[ReturnType].
    // If the method already returns Maybe[V], we flatten to Maybe[V] to avoid
    // Maybe[Maybe[V]].
    // =========================================================================
    if (call.optional_chain) {
        auto receiver_type = check_expr(*call.receiver);
        TypePtr check_type = receiver_type;
        if (check_type && check_type->is<RefType>())
            check_type = check_type->as<RefType>().inner;
        if (!check_type || !check_type->is<NamedType>() ||
            check_type->as<NamedType>().name != "Maybe" ||
            check_type->as<NamedType>().type_args.empty()) {
            error("Optional chaining `?.` requires receiver of type Maybe[T], got " +
                      type_to_string(receiver_type),
                  call.span, "T090");
            return make_unit();
        }
        TypePtr inner_type = check_type->as<NamedType>().type_args[0];

        // Resolve the method on the inner type T by temporarily clearing optional_chain
        // and re-invoking check_method_call. The receiver expression will be type-checked
        // again (returning Maybe[T]), but we override receiver_type to T below via the
        // override mechanism. To avoid that complexity, we use const_cast to temporarily
        // clear the flag and rely on the existing method resolution to pick up the method
        // on Maybe[T]'s inner type through check_method_call_builtin_types and the
        // named-type method lookup paths which already handle unwrapping.
        //
        // Instead, we directly resolve the method on inner_type by delegating to
        // check_method_call_builtin_types with the inner type.
        if (auto result = check_method_call_builtin_types(call, inner_type, call.method)) {
            TypePtr method_ret = *result;
            // Flatten: if method already returns Maybe[V], return Maybe[V] (not Maybe[Maybe[V]])
            if (method_ret && method_ret->is<NamedType>() &&
                method_ret->as<NamedType>().name == "Maybe") {
                return method_ret;
            }
            return std::make_shared<Type>(Type{NamedType{"Maybe", "", {method_ret}}});
        }

        // Try named type method lookup on the inner type
        if (inner_type->is<NamedType>()) {
            auto& named = inner_type->as<NamedType>();
            std::string qualified = named.name + "::" + call.method;

            auto func = env_.lookup_func(qualified);
            if (func) {
                TypePtr method_ret = func->return_type;
                // Substitute type params from inner type's type_args
                if (!func->type_params.empty()) {
                    std::unordered_map<std::string, TypePtr> subs;
                    for (size_t i = 0; i < func->type_params.size() && i < named.type_args.size();
                         ++i) {
                        subs[func->type_params[i]] = named.type_args[i];
                    }
                    // Also infer from arguments
                    for (size_t i = 0; i < call.args.size() && i + 1 < func->params.size(); ++i) {
                        TypePtr param_type = func->params[i + 1];
                        TypePtr expected_param = substitute_type(param_type, subs);
                        TypePtr arg_type = check_expr(*call.args[i], expected_param);
                        extract_type_params(param_type, arg_type, func->type_params, subs);
                    }
                    method_ret = substitute_type(method_ret, subs);
                }
                // Flatten Maybe[Maybe[V]] -> Maybe[V]
                if (method_ret && method_ret->is<NamedType>() &&
                    method_ret->as<NamedType>().name == "Maybe") {
                    return method_ret;
                }
                return std::make_shared<Type>(Type{NamedType{"Maybe", "", {method_ret}}});
            }

            // Search all loaded modules for the method
            for (const auto& [mod_path, mod] : env_.get_all_modules()) {
                auto func_it = mod.functions.find(qualified);
                if (func_it != mod.functions.end()) {
                    TypePtr method_ret = func_it->second.return_type;
                    if (!func_it->second.type_params.empty() && !named.type_args.empty()) {
                        std::unordered_map<std::string, TypePtr> subs;
                        for (size_t i = 0;
                             i < func_it->second.type_params.size() && i < named.type_args.size();
                             ++i) {
                            subs[func_it->second.type_params[i]] = named.type_args[i];
                        }
                        method_ret = substitute_type(method_ret, subs);
                    }
                    if (method_ret && method_ret->is<NamedType>() &&
                        method_ret->as<NamedType>().name == "Maybe") {
                        return method_ret;
                    }
                    return std::make_shared<Type>(Type{NamedType{"Maybe", "", {method_ret}}});
                }
            }

            // Search GlobalModuleCache
            for (const auto& [mod_path, mod] : GlobalModuleCache::instance().get_all()) {
                auto func_it = mod.functions.find(qualified);
                if (func_it != mod.functions.end()) {
                    TypePtr method_ret = func_it->second.return_type;
                    if (!func_it->second.type_params.empty() && !named.type_args.empty()) {
                        std::unordered_map<std::string, TypePtr> subs;
                        for (size_t i = 0;
                             i < func_it->second.type_params.size() && i < named.type_args.size();
                             ++i) {
                            subs[func_it->second.type_params[i]] = named.type_args[i];
                        }
                        method_ret = substitute_type(method_ret, subs);
                    }
                    if (method_ret && method_ret->is<NamedType>() &&
                        method_ret->as<NamedType>().name == "Maybe") {
                        return method_ret;
                    }
                    return std::make_shared<Type>(Type{NamedType{"Maybe", "", {method_ret}}});
                }
            }
        }

        // Try primitive type method lookup on the inner type
        if (inner_type->is<PrimitiveType>()) {
            auto& prim = inner_type->as<PrimitiveType>();
            std::string type_name = primitive_to_string(prim.kind);
            std::string qualified = type_name + "::" + call.method;
            auto func = env_.lookup_func(qualified);
            if (func) {
                TypePtr method_ret = func->return_type;
                if (method_ret && method_ret->is<NamedType>() &&
                    method_ret->as<NamedType>().name == "Maybe") {
                    return method_ret;
                }
                return std::make_shared<Type>(Type{NamedType{"Maybe", "", {method_ret}}});
            }
        }

        // Method not found on inner type
        error("No method '" + call.method + "' found on type " + type_to_string(inner_type) +
                  " (from optional chaining on " + type_to_string(receiver_type) + ")",
              call.span, "T079");
        return make_unit();
    }

    // Check for static method calls on primitive type names (e.g., I32::default())
    if (call.receiver->is<parser::IdentExpr>()) {
        const auto& type_name = call.receiver->as<parser::IdentExpr>().name;
        // Check if this is a primitive type name used as a static receiver
        bool is_primitive_type = type_name == "I8" || type_name == "I16" || type_name == "I32" ||
                                 type_name == "I64" || type_name == "I128" || type_name == "U8" ||
                                 type_name == "U16" || type_name == "U32" || type_name == "U64" ||
                                 type_name == "U128" || type_name == "F32" || type_name == "F64" ||
                                 type_name == "Bool" || type_name == "Str";

        if (is_primitive_type && call.method == "default") {
            // Return the primitive type itself
            if (type_name == "I8") {
                return make_primitive(PrimitiveKind::I8);
            }
            if (type_name == "I16") {
                return make_primitive(PrimitiveKind::I16);
            }
            if (type_name == "I32") {
                return make_primitive(PrimitiveKind::I32);
            }
            if (type_name == "I64") {
                return make_primitive(PrimitiveKind::I64);
            }
            if (type_name == "I128") {
                return make_primitive(PrimitiveKind::I128);
            }
            if (type_name == "U8") {
                return make_primitive(PrimitiveKind::U8);
            }
            if (type_name == "U16") {
                return make_primitive(PrimitiveKind::U16);
            }
            if (type_name == "U32") {
                return make_primitive(PrimitiveKind::U32);
            }
            if (type_name == "U64") {
                return make_primitive(PrimitiveKind::U64);
            }
            if (type_name == "U128") {
                return make_primitive(PrimitiveKind::U128);
            }
            if (type_name == "F32") {
                return make_primitive(PrimitiveKind::F32);
            }
            if (type_name == "F64") {
                return make_primitive(PrimitiveKind::F64);
            }
            if (type_name == "Bool") {
                return make_primitive(PrimitiveKind::Bool);
            }
            if (type_name == "Str") {
                return make_primitive(PrimitiveKind::Str);
            }
        }

        // Check for static method calls on class types (e.g., Counter.get_count())
        if (!is_primitive_type) {
            auto class_def = env_.lookup_class(type_name);
            if (class_def.has_value()) {
                // Look for static method
                for (const auto& method : class_def->methods) {
                    if (method.sig.name == call.method && method.is_static) {
                        // Check visibility
                        if (!check_member_visibility(method.vis, type_name, call.method,
                                                     call.receiver->span)) {
                            return method.sig.return_type; // Return type for error recovery
                        }
                        // Apply type arguments for generic static methods
                        if (!call.type_args.empty() && !method.sig.type_params.empty()) {
                            std::unordered_map<std::string, TypePtr> subs;
                            for (size_t i = 0;
                                 i < method.sig.type_params.size() && i < call.type_args.size();
                                 ++i) {
                                subs[method.sig.type_params[i]] = resolve_type(*call.type_args[i]);
                            }
                            return substitute_type(method.sig.return_type, subs);
                        }
                        return method.sig.return_type;
                    }
                }
            }
        }
    }

    auto receiver_type = check_expr(*call.receiver);

    // Expand type aliases before method resolution
    // e.g., CryptoResult[X509Certificate] -> Outcome[X509Certificate, CryptoError]
    // so that methods like .unwrap(), .is_ok() are properly recognized
    {
        TypePtr alias_target = receiver_type;
        if (alias_target->is<RefType>()) {
            alias_target = alias_target->as<RefType>().inner;
        }
        if (alias_target->is<NamedType>()) {
            auto& pre_named = alias_target->as<NamedType>();
            auto alias_base = env_.lookup_type_alias(pre_named.name);
            std::optional<std::vector<std::string>> alias_generics;

            // If local lookup fails, search all loaded modules for the type alias
            if (!alias_base && env_.module_registry()) {
                for (const auto& [mod_path, mod] : env_.module_registry()->get_all_modules()) {
                    auto it = mod.type_aliases.find(pre_named.name);
                    if (it != mod.type_aliases.end()) {
                        alias_base = it->second;
                        auto gen_it = mod.type_alias_generics.find(pre_named.name);
                        if (gen_it != mod.type_alias_generics.end()) {
                            alias_generics = gen_it->second;
                        }
                        break;
                    }
                }
            } else if (alias_base) {
                alias_generics = env_.lookup_type_alias_generics(pre_named.name);
            }

            if (alias_base) {
                if (alias_generics && !alias_generics->empty() && !pre_named.type_args.empty()) {
                    std::unordered_map<std::string, TypePtr> subs;
                    for (size_t i = 0; i < alias_generics->size() && i < pre_named.type_args.size();
                         ++i) {
                        subs[(*alias_generics)[i]] = pre_named.type_args[i];
                    }
                    receiver_type = substitute_type(*alias_base, subs);
                } else {
                    receiver_type = *alias_base;
                }
            }
        }
    }

    // Helper lambda to apply type arguments to a function signature
    auto apply_type_args = [&](const FuncSig& func) -> TypePtr {
        if (!call.type_args.empty() && !func.type_params.empty()) {
            // Build substitution map from explicit type arguments
            // Need to resolve parser types to semantic types
            std::unordered_map<std::string, TypePtr> subs;
            for (size_t i = 0; i < func.type_params.size() && i < call.type_args.size(); ++i) {
                subs[func.type_params[i]] = resolve_type(*call.type_args[i]);
            }
            return substitute_type(func.return_type, subs);
        }
        return func.return_type;
    };

    // Handle method calls on pointer types (*T)
    // Methods: read(), write(value), is_null(), offset(count)
    if (receiver_type->is<PtrType>()) {
        const auto& ptr_type = receiver_type->as<PtrType>();
        TypePtr inner = ptr_type.inner;

        if (call.method == "read") {
            // p.read() -> T - dereference the pointer and read the value
            if (!call.args.empty()) {
                error("Pointer read() takes no arguments", call.receiver->span, "T080");
            }
            return inner;
        } else if (call.method == "write") {
            // p.write(value) -> () - write value through the pointer
            if (call.args.size() != 1) {
                error("Pointer write() requires exactly one argument", call.receiver->span, "T081");
            } else {
                TypePtr arg_type = check_expr(*call.args[0]);
                TypePtr resolved_inner = env_.resolve(inner);
                TypePtr resolved_arg = env_.resolve(arg_type);
                if (!types_compatible(resolved_inner, resolved_arg)) {
                    error("Type mismatch in pointer write: expected " + type_to_string(inner) +
                              ", got " + type_to_string(arg_type),
                          call.args[0]->span, "T057");
                }
            }
            return make_unit();
        } else if (call.method == "is_null") {
            // p.is_null() -> Bool
            if (!call.args.empty()) {
                error("Pointer is_null() takes no arguments", call.receiver->span, "T082");
            }
            return make_bool();
        } else if (call.method == "offset") {
            // p.offset(count) -> *T - returns pointer offset by count elements
            if (call.args.size() != 1) {
                error("Pointer offset() requires exactly one argument", call.receiver->span,
                      "T083");
            } else {
                TypePtr arg_type = check_expr(*call.args[0]);
                // Allow I32 or I64 for offset
                bool valid_offset = (arg_type->is<PrimitiveType>() &&
                                     (arg_type->as<PrimitiveType>().kind == PrimitiveKind::I32 ||
                                      arg_type->as<PrimitiveType>().kind == PrimitiveKind::I64));
                if (!valid_offset) {
                    error("Pointer offset() requires I32 or I64 argument", call.args[0]->span,
                          "T057");
                }
            }
            return receiver_type; // Return same pointer type
        } else {
            error("Unknown pointer method '" + call.method + "'", call.receiver->span, "T084");
            return make_unit();
        }
    }

    // Handle impl method calls on NamedType
    // Unwrap reference type if present for method lookup
    TypePtr impl_receiver = receiver_type;
    if (receiver_type->is<RefType>()) {
        impl_receiver = receiver_type->as<RefType>().inner;
    }
    if (impl_receiver->is<NamedType>()) {
        auto& named = impl_receiver->as<NamedType>();
        std::string qualified = named.name + "::" + call.method;

        // Helper: build substitution map from receiver type args, using impl_self_type_args
        // patterns when available to handle specialized impls like impl[T] Pin[ref T].
        auto build_receiver_subs = [&](const FuncSig& sig,
                                       std::unordered_map<std::string, TypePtr>& subs) {
            // Check if impl_self_type_args has specialized patterns (e.g., ref T)
            // that differ from simple type param names. Only use pattern-based
            // extraction for specialized impls like impl[T] Pin[ref T].
            bool has_specialized_patterns = false;
            if (!sig.impl_self_type_args.empty() &&
                sig.impl_self_type_args.size() == named.type_args.size()) {
                for (const auto& sta : sig.impl_self_type_args) {
                    // RefType patterns are specialized (e.g., impl[T] Pin[ref T]).
                    if (sta->is<RefType>()) {
                        has_specialized_patterns = true;
                        break;
                    }
                    // NamedType patterns are specialized only when they contain
                    // NamedType patterns with type args containing impl type
                    // params are specialized (e.g., Heap[T] in impl[T] Pin[Heap[T]],
                    // Maybe[T] in impl[T] Maybe[Maybe[T]], Outcome[T,E] in
                    // impl[T,E] Outcome[Outcome[T,E], E]). Pattern-based extraction
                    // correctly decomposes the concrete type args to extract T.
                    if (sta->is<NamedType>()) {
                        const auto& sta_named = sta->as<NamedType>();
                        if (!sta_named.type_args.empty()) {
                            // Count distinct type params from the impl that appear
                            // in this self type arg's inner structure.
                            std::set<std::string> params_in_arg;
                            std::function<void(const TypePtr&)> collect_params;
                            collect_params = [&](const TypePtr& t) {
                                if (!t)
                                    return;
                                if (t->is<NamedType>()) {
                                    const auto& n = t->as<NamedType>();
                                    for (const auto& tp : sig.type_params) {
                                        if (n.name == tp) {
                                            params_in_arg.insert(tp);
                                            return;
                                        }
                                    }
                                    for (const auto& ta : n.type_args)
                                        collect_params(ta);
                                } else if (t->is<RefType>()) {
                                    collect_params(t->as<RefType>().inner);
                                }
                            };
                            for (const auto& ta : sta_named.type_args)
                                collect_params(ta);
                            if (params_in_arg.size() >= 1) {
                                has_specialized_patterns = true;
                                break;
                            }
                        }
                    }
                }
            }
            if (has_specialized_patterns) {
                // Pattern-based extraction: match impl self-type arg patterns
                // against concrete type args to extract type params.
                // E.g., impl[T] Pin[ref T]: pattern [RefType(T)] matched against
                // [RefType(I32)] gives T=I32 (not T=ref I32).
                for (size_t i = 0; i < sig.impl_self_type_args.size(); ++i) {
                    extract_type_params(sig.impl_self_type_args[i], named.type_args[i],
                                        sig.type_params, subs);
                }
            } else {
                // Positional mapping (simple case: impl[T] Wrapper[T])
                for (size_t i = 0; i < sig.type_params.size() && i < named.type_args.size(); ++i) {
                    subs[sig.type_params[i]] = named.type_args[i];
                }
            }
        };

        // For types with multiple specialized impls (e.g., Pin[ref T] and
        // Pin[Heap[T]] both defining get_ref), try a discriminated key first.
        // The key format is "Type[discriminator]::method", where the discriminator
        // is extracted from the receiver's first type arg's structural wrapper.
        if (!named.type_args.empty()) {
            std::string discriminator;
            for (const auto& ta : named.type_args) {
                if (ta->is<NamedType>()) {
                    discriminator = ta->as<NamedType>().name;
                    break;
                } else if (ta->is<RefType>()) {
                    discriminator = ta->as<RefType>().is_mut ? "mut_ref" : "ref";
                    break;
                }
            }
            if (!discriminator.empty()) {
                std::string spec_qualified = named.name + "[" + discriminator + "]::" + call.method;
                auto spec_func = env_.lookup_func(spec_qualified);
                if (spec_func && !spec_func->type_params.empty()) {
                    std::unordered_map<std::string, TypePtr> subs;
                    build_receiver_subs(*spec_func, subs);
                    for (size_t i = 0; i < call.args.size() && i + 1 < spec_func->params.size();
                         ++i) {
                        TypePtr param_type = spec_func->params[i + 1];
                        TypePtr expected_param = substitute_type(param_type, subs);
                        TypePtr arg_type = check_expr(*call.args[i], expected_param);
                        extract_type_params(param_type, arg_type, spec_func->type_params, subs);
                    }
                    return substitute_type(spec_func->return_type, subs);
                }
            }
        }

        auto func = env_.lookup_func(qualified);
        if (func) {
            // For generic impl methods, substitute type parameters from:
            // 1. The receiver's type arguments (e.g., T, E from Outcome[T, E])
            // 2. Inferred from function arguments (e.g., U in map_or[U])
            if (call.type_args.empty() && !func->type_params.empty()) {
                // Fallback: use the first overload (original behavior)
                std::unordered_map<std::string, TypePtr> subs;

                // First, substitute from receiver's type_args
                build_receiver_subs(*func, subs);

                // Then, infer remaining type params from function arguments
                // Check arguments and match against parameter types
                // Note: func->params[0] is 'this', so we offset by 1
                for (size_t i = 0; i < call.args.size() && i + 1 < func->params.size(); ++i) {
                    TypePtr param_type = func->params[i + 1]; // Skip 'this' parameter
                    // Substitute known type params to get concrete expected type
                    // This allows closure params to be inferred (e.g., ref I32 from ref T)
                    TypePtr expected_param = substitute_type(param_type, subs);
                    TypePtr arg_type = check_expr(*call.args[i], expected_param);
                    extract_type_params(param_type, arg_type, func->type_params, subs);
                }

                return substitute_type(func->return_type, subs);
            }
            return apply_type_args(*func);
        }

        // Helper: substitute receiver type args into a module-level function signature
        auto apply_with_receiver_type_args = [&](const FuncSig& func_sig) -> TypePtr {
            if (call.type_args.empty() && !func_sig.type_params.empty() &&
                !named.type_args.empty()) {
                std::unordered_map<std::string, TypePtr> subs;
                build_receiver_subs(func_sig, subs);
                for (size_t i = 0; i < call.args.size() && i + 1 < func_sig.params.size(); ++i) {
                    TypePtr param_type = func_sig.params[i + 1];
                    TypePtr expected_param = substitute_type(param_type, subs);
                    TypePtr arg_type = check_expr(*call.args[i], expected_param);
                    extract_type_params(param_type, arg_type, func_sig.type_params, subs);
                }
                return substitute_type(func_sig.return_type, subs);
            }
            return apply_type_args(func_sig);
        };

        if (!named.module_path.empty()) {
            auto module = env_.get_module(named.module_path);
            if (module) {
                auto func_it = module->functions.find(qualified);
                if (func_it != module->functions.end()) {
                    return apply_with_receiver_type_args(func_it->second);
                }
            }
        }

        auto imported_path = env_.resolve_imported_symbol(named.name);
        if (imported_path.has_value()) {
            std::string module_path;
            size_t pos = imported_path->rfind("::");
            if (pos != std::string::npos) {
                module_path = imported_path->substr(0, pos);
            }

            auto module = env_.get_module(module_path);
            if (module) {
                auto func_it = module->functions.find(qualified);
                if (func_it != module->functions.end()) {
                    return apply_with_receiver_type_args(func_it->second);
                }
            }
        }

        // Fallback: search ALL loaded modules AND global module cache
        // for behavior impl methods defined in separate modules.
        // E.g., `impl PartialEq for List[T]` in std::collections::behaviors
        // while List is defined in std::collections::list
        {
            // Build a specialized key for discriminated lookup (same format as above)
            std::string spec_key;
            if (!named.type_args.empty()) {
                std::string disc;
                for (const auto& ta : named.type_args) {
                    if (ta->is<NamedType>()) {
                        disc = ta->as<NamedType>().name;
                        break;
                    } else if (ta->is<RefType>()) {
                        disc = ta->as<RefType>().is_mut ? "mut_ref" : "ref";
                        break;
                    }
                }
                if (!disc.empty()) {
                    spec_key = named.name + "[" + disc + "]::" + call.method;
                }
            }

            auto all_modules = env_.get_all_modules();
            for (const auto& [mod_path, mod] : all_modules) {
                // Try specialized key first to avoid name collisions
                if (!spec_key.empty()) {
                    auto func_it = mod.functions.find(spec_key);
                    if (func_it != mod.functions.end()) {
                        return apply_with_receiver_type_args(func_it->second);
                    }
                }
                auto func_it = mod.functions.find(qualified);
                if (func_it != mod.functions.end()) {
                    return apply_with_receiver_type_args(func_it->second);
                }
            }
            // Also search GlobalModuleCache for modules not yet loaded
            // into the local registry (e.g., sibling submodules)
            for (const auto& [mod_path, mod] : GlobalModuleCache::instance().get_all()) {
                if (!spec_key.empty()) {
                    auto func_it = mod.functions.find(spec_key);
                    if (func_it != mod.functions.end()) {
                        return apply_with_receiver_type_args(func_it->second);
                    }
                }
                auto func_it = mod.functions.find(qualified);
                if (func_it != mod.functions.end()) {
                    return apply_with_receiver_type_args(func_it->second);
                }
            }
        }

        // Check if NamedType refers to a class - handle class instance methods
        auto class_def = env_.lookup_class(named.name);
        if (class_def.has_value()) {
            std::string current_class = named.name;
            while (!current_class.empty()) {
                auto current_def = env_.lookup_class(current_class);
                if (!current_def.has_value())
                    break;
                for (const auto& method : current_def->methods) {
                    if (method.sig.name == call.method && !method.is_static) {
                        return method.sig.return_type;
                    }
                }
                // Check parent class
                if (current_def->base_class.has_value()) {
                    current_class = current_def->base_class.value();
                } else {
                    break;
                }
            }
        }
    }

    // Handle class type method calls with visibility checking
    // Unwrap reference type if present
    TypePtr class_receiver = receiver_type;
    if (receiver_type->is<RefType>()) {
        class_receiver = receiver_type->as<RefType>().inner;
    }
    if (class_receiver->is<ClassType>()) {
        auto& class_type = class_receiver->as<ClassType>();
        auto class_def = env_.lookup_class(class_type.name);
        if (class_def.has_value()) {
            // Search for the method in this class and its parents
            std::string current_class = class_type.name;
            while (!current_class.empty()) {
                auto current_def = env_.lookup_class(current_class);
                if (!current_def.has_value())
                    break;

                for (const auto& method : current_def->methods) {
                    if (method.sig.name == call.method) {
                        // Check visibility
                        if (!check_member_visibility(method.vis, current_class, call.method,
                                                     call.receiver->span)) {
                            return method.sig.return_type; // Return type for error recovery
                        }
                        return method.sig.return_type;
                    }
                }

                // Check parent class
                if (current_def->base_class.has_value()) {
                    current_class = current_def->base_class.value();
                } else {
                    break;
                }
            }
            error("Unknown method '" + call.method + "' on class '" + class_type.name + "'",
                  call.receiver->span, "T078");
        }
    }

    // Handle dyn dispatch: check both direct DynBehaviorType and ref dyn Behavior
    {
        TypePtr dyn_check_type = receiver_type;
        if (dyn_check_type->is<RefType>()) {
            dyn_check_type = dyn_check_type->as<RefType>().inner;
        }
        if (dyn_check_type && dyn_check_type->is<DynBehaviorType>()) {
            auto& dyn = dyn_check_type->as<DynBehaviorType>();
            auto behavior_def = env_.lookup_behavior(dyn.behavior_name);
            if (behavior_def) {
                for (const auto& method : behavior_def->methods) {
                    if (method.name == call.method) {
                        // Build substitution map from behavior's type params to dyn's type args
                        // e.g., for dyn Processor[I32], map T -> I32
                        if (!dyn.type_args.empty() && !behavior_def->type_params.empty()) {
                            std::unordered_map<std::string, TypePtr> subs;
                            for (size_t i = 0;
                                 i < behavior_def->type_params.size() && i < dyn.type_args.size();
                                 ++i) {
                                subs[behavior_def->type_params[i]] = dyn.type_args[i];
                            }
                            // Substitute both return type and check parameter types
                            auto return_type = substitute_type(method.return_type, subs);
                            return return_type;
                        }
                        return apply_type_args(method);
                    }
                }
                error("Unknown method '" + call.method + "' on behavior '" + dyn.behavior_name +
                          "'",
                      call.receiver->span, "T079");
            }
        }
    }

    // Handle method calls on generic type parameters with behavior bounds from where clauses
    // e.g., func process[C](c: ref C) where C: Container[I32] { c.get(0) }
    TypePtr unwrapped_receiver = receiver_type;
    if (receiver_type->is<RefType>()) {
        unwrapped_receiver = receiver_type->as<RefType>().inner;
    }
    // Extract type parameter name from either NamedType or GenericType
    std::string type_param_name;
    if (unwrapped_receiver->is<NamedType>()) {
        type_param_name = unwrapped_receiver->as<NamedType>().name;
    } else if (unwrapped_receiver->is<GenericType>()) {
        type_param_name = unwrapped_receiver->as<GenericType>().name;
    }
    if (!type_param_name.empty()) {
        // Check if this is a type parameter by looking for it in current where constraints
        for (const auto& constraint : current_where_constraints_) {
            if (constraint.type_param == type_param_name) {
                // Found where constraint for this type parameter
                // Look through parameterized bounds for a behavior with this method
                for (const auto& bound : constraint.parameterized_bounds) {
                    auto behavior_def = env_.lookup_behavior(bound.behavior_name);
                    if (behavior_def) {
                        for (const auto& method : behavior_def->methods) {
                            if (method.name == call.method) {
                                // Build substitution map from behavior type params to bound's type
                                // args e.g., for Container[I32], map T -> I32
                                std::unordered_map<std::string, TypePtr> subs;
                                if (!bound.type_args.empty() &&
                                    !behavior_def->type_params.empty()) {
                                    for (size_t i = 0; i < behavior_def->type_params.size() &&
                                                       i < bound.type_args.size();
                                         ++i) {
                                        subs[behavior_def->type_params[i]] = bound.type_args[i];
                                    }
                                }

                                // Substitute type parameters in return type
                                TypePtr return_type = method.return_type;
                                if (!subs.empty()) {
                                    return_type = substitute_type(return_type, subs);
                                }
                                return return_type;
                            }
                        }
                    }
                }

                // Also check simple (non-parameterized) behavior bounds
                for (const auto& behavior_name : constraint.required_behaviors) {
                    auto behavior_def = env_.lookup_behavior(behavior_name);
                    if (behavior_def) {
                        for (const auto& method : behavior_def->methods) {
                            if (method.name == call.method) {
                                // Substitute Self/This with the type parameter
                                // e.g., for I: Iterator, This::Item in next() -> Maybe[This::Item]
                                // becomes Maybe[I::Item], and This -> I
                                TypePtr return_type = method.return_type;
                                auto type_param = std::make_shared<Type>();
                                type_param->kind = NamedType{constraint.type_param, "", {}};
                                std::unordered_map<std::string, TypePtr> subs;
                                subs["This"] = type_param;
                                subs["Self"] = type_param;
                                return substitute_type(return_type, subs);
                            }
                        }
                    }
                }
            }
        }
    }

    // Handle primitive type builtin methods (core::ops)
    // Unwrap reference type if present
    TypePtr prim_type = receiver_type;
    if (receiver_type->is<RefType>()) {
        prim_type = receiver_type->as<RefType>().inner;
    }
    if (prim_type->is<PrimitiveType>()) {
        auto& prim = prim_type->as<PrimitiveType>();
        auto kind = prim.kind;

        // Integer and float arithmetic methods
        bool is_numeric = (kind == PrimitiveKind::I8 || kind == PrimitiveKind::I16 ||
                           kind == PrimitiveKind::I32 || kind == PrimitiveKind::I64 ||
                           kind == PrimitiveKind::I128 || kind == PrimitiveKind::U8 ||
                           kind == PrimitiveKind::U16 || kind == PrimitiveKind::U32 ||
                           kind == PrimitiveKind::U64 || kind == PrimitiveKind::U128 ||
                           kind == PrimitiveKind::F32 || kind == PrimitiveKind::F64);
        bool is_integer = (kind == PrimitiveKind::I8 || kind == PrimitiveKind::I16 ||
                           kind == PrimitiveKind::I32 || kind == PrimitiveKind::I64 ||
                           kind == PrimitiveKind::I128 || kind == PrimitiveKind::U8 ||
                           kind == PrimitiveKind::U16 || kind == PrimitiveKind::U32 ||
                           kind == PrimitiveKind::U64 || kind == PrimitiveKind::U128);

        // Arithmetic operations that return Self
        if (is_numeric && (call.method == "add" || call.method == "sub" || call.method == "mul" ||
                           call.method == "div" || call.method == "neg")) {
            return receiver_type;
        }

        // Integer-only operations
        if (is_integer && call.method == "rem") {
            return receiver_type;
        }

        // Bool methods
        if (kind == PrimitiveKind::Bool && call.method == "negate") {
            return receiver_type;
        }

        // Comparison methods - cmp returns Ordering, max/min/clamp return Self
        if (is_numeric) {
            if (call.method == "cmp") {
                return std::make_shared<Type>(Type{NamedType{"Ordering", "", {}}});
            }
            if (call.method == "max" || call.method == "min" || call.method == "clamp") {
                return receiver_type;
            }
        }

        // PartialEq / PartialOrd behavior methods return Bool for all primitives
        if (call.method == "eq" || call.method == "ne" || call.method == "lt" ||
            call.method == "le" || call.method == "gt" || call.method == "ge") {
            return make_primitive(PrimitiveKind::Bool);
        }

        // Bitwise operations return Self for integer types
        if (is_integer &&
            (call.method == "bitand" || call.method == "bitor" || call.method == "bitxor" ||
             call.method == "shl" || call.method == "shr" || call.method == "bitnot" ||
             call.method == "shift_left" || call.method == "shift_right" ||
             call.method == "negate")) {
            return receiver_type;
        }

        // duplicate() returns Self for all primitives (copy semantics)
        if (call.method == "duplicate") {
            return receiver_type;
        }

        // to_string() / debug_string() returns Str for all primitives (Display/Debug behavior)
        if (call.method == "to_string" || call.method == "debug_string") {
            return make_primitive(PrimitiveKind::Str);
        }

        // fmt_binary/fmt_octal/fmt_lower_hex/fmt_upper_hex return Str for integer types
        if (is_integer && (call.method == "fmt_binary" || call.method == "fmt_octal" ||
                           call.method == "fmt_lower_hex" || call.method == "fmt_upper_hex")) {
            return make_primitive(PrimitiveKind::Str);
        }

        // fmt_lower_exp/fmt_upper_exp return Str for float types
        if ((kind == PrimitiveKind::F32 || kind == PrimitiveKind::F64) &&
            (call.method == "fmt_lower_exp" || call.method == "fmt_upper_exp")) {
            return make_primitive(PrimitiveKind::Str);
        }

        // partial_cmp() returns Maybe[Ordering] for all numeric types
        if (is_numeric && call.method == "partial_cmp") {
            auto ordering = std::make_shared<Type>(Type{NamedType{"Ordering", "", {}}});
            auto maybe = std::make_shared<Type>(Type{NamedType{"Maybe", "", {ordering}}});
            return maybe;
        }

        // is_zero() / is_one() return Bool for all numeric types
        if (is_numeric && (call.method == "is_zero" || call.method == "is_one")) {
            return make_primitive(PrimitiveKind::Bool);
        }

        // hash() returns I64 for all primitives (Hash behavior)
        if (call.method == "hash") {
            return make_primitive(PrimitiveKind::I64);
        }

        // to_owned() returns Self for all primitives (ToOwned behavior)
        if (call.method == "to_owned") {
            return receiver_type;
        }

        // checked_* arithmetic returns Maybe[Self] for integer types
        if (is_integer && (call.method == "checked_add" || call.method == "checked_sub" ||
                           call.method == "checked_mul" || call.method == "checked_div" ||
                           call.method == "checked_rem" || call.method == "checked_neg" ||
                           call.method == "checked_shl" || call.method == "checked_shr")) {
            auto maybe = std::make_shared<Type>(Type{NamedType{"Maybe", "", {prim_type}}});
            return maybe;
        }

        // saturating_* / wrapping_* arithmetic returns Self for integer types
        if (is_integer && (call.method == "saturating_add" || call.method == "saturating_sub" ||
                           call.method == "saturating_mul" || call.method == "wrapping_add" ||
                           call.method == "wrapping_sub" || call.method == "wrapping_mul" ||
                           call.method == "wrapping_neg")) {
            return receiver_type;
        }

        // borrow() returns ref Self for all primitives (Borrow behavior)
        if (call.method == "borrow") {
            return std::make_shared<Type>(
                RefType{.is_mut = false, .inner = receiver_type, .lifetime = std::nullopt});
        }

        // borrow_mut() returns mut ref Self for all primitives (BorrowMut behavior)
        if (call.method == "borrow_mut") {
            return std::make_shared<Type>(
                RefType{.is_mut = true, .inner = receiver_type, .lifetime = std::nullopt});
        }

        // Dynamic lookup for all impl methods on primitive types.
        // This covers Str methods (len, char_at, find, etc.) and any other
        // impl blocks defined in .tml files (core::str, core::ops::*, etc.).
        // The lookup goes through env_.lookup_func() which searches local scope,
        // module_registry_, and GlobalModuleCache as a last resort.
        std::string type_name = primitive_to_string(kind);
        std::string qualified = type_name + "::" + call.method;
        auto func = env_.lookup_func(qualified);
        if (func) {
            return func->return_type;
        }
    }

    // Type-specific method resolution (Maybe, Outcome, List, Array, Slice, Tuple, etc.)
    // Delegated to expr_call_method_types.cpp
    if (auto result = check_method_call_builtin_types(call, receiver_type, call.method)) {
        return *result;
    }

    // Fallback: Check if "method" is actually a field with a function type
    // This handles cases like vtable.call_fn(args) where call_fn is a field
    // containing a function pointer
    TypePtr method_receiver = receiver_type;
    if (method_receiver->is<RefType>()) {
        method_receiver = method_receiver->as<RefType>().inner;
    }
    if (method_receiver->is<NamedType>()) {
        const auto& named = method_receiver->as<NamedType>();
        auto struct_def = env_.lookup_struct(named.name);
        if (struct_def) {
            // Look for a field with the method name
            for (const auto& fld : struct_def->fields) {
                if (fld.name == call.method) {
                    // Check if the field is a function type
                    if (fld.type->is<FuncType>()) {
                        const auto& func = fld.type->as<FuncType>();
                        // Check argument count
                        if (call.args.size() != func.params.size()) {
                            error("Wrong number of arguments: expected " +
                                      std::to_string(func.params.size()) + ", got " +
                                      std::to_string(call.args.size()),
                                  call.receiver->span, "T004");
                        }
                        // Type check arguments
                        for (size_t i = 0; i < std::min(call.args.size(), func.params.size());
                             ++i) {
                            check_expr(*call.args[i], func.params[i]);
                        }
                        return func.return_type;
                    }
                    break;
                }
            }
        }
    }

    // Last-resort behavior method lookup: if the method wasn't found by any
    // lookup above, check if the receiver type implements a behavior that
    // provides this method. This handles cases like Range::size_hint (from
    // Iterator behavior), Peekable::next, List::extend, etc.
    {
        TypePtr behavior_receiver = receiver_type;
        if (behavior_receiver->is<RefType>()) {
            behavior_receiver = behavior_receiver->as<RefType>().inner;
        }
        if (behavior_receiver->is<NamedType>()) {
            const auto& named = behavior_receiver->as<NamedType>();
            auto behaviors = env_.get_behavior_impls(named.name);
            // Also check module-level behavior_impls for imported types
            if (behaviors.empty()) {
                auto all_modules = env_.get_all_modules();
                for (const auto& [mod_path, mod] : all_modules) {
                    auto bi_it = mod.behavior_impls.find(named.name);
                    if (bi_it != mod.behavior_impls.end()) {
                        behaviors = bi_it->second;
                        break;
                    }
                }
            }
            if (behaviors.empty()) {
                for (const auto& [mod_path, mod] : GlobalModuleCache::instance().get_all()) {
                    auto bi_it = mod.behavior_impls.find(named.name);
                    if (bi_it != mod.behavior_impls.end()) {
                        behaviors = bi_it->second;
                        break;
                    }
                }
            }
            for (const auto& behavior_name : behaviors) {
                auto behavior_def = env_.lookup_behavior(behavior_name);
                if (!behavior_def)
                    continue;
                for (const auto& bmethod : behavior_def->methods) {
                    if (bmethod.name != call.method)
                        continue;
                    // Found the method in a behavior definition.
                    std::unordered_map<std::string, TypePtr> subs;
                    auto self_type = std::make_shared<Type>();
                    self_type->kind = NamedType{named.name, "", named.type_args};
                    subs["Self"] = self_type;
                    subs["This"] = self_type;
                    if (!behavior_def->type_params.empty() && !named.type_args.empty()) {
                        for (size_t i = 0;
                             i < behavior_def->type_params.size() && i < named.type_args.size();
                             ++i) {
                            subs[behavior_def->type_params[i]] = named.type_args[i];
                        }
                    }
                    for (const auto& assoc : behavior_def->associated_types) {
                        std::string assoc_key = "This::" + assoc.name;
                        if (assoc.name == "Item" && !named.type_args.empty()) {
                            subs[assoc_key] = named.type_args[0];
                            subs[assoc.name] = named.type_args[0];
                        }
                    }
                    return substitute_type(bmethod.return_type, subs);
                }
            }
        }
    }

    // Pin-dispatch: when the receiver is Pin[ref T] or Pin[mut ref T] and no method
    // was found on Pin itself, unwrap the Pin to get the inner type T and look up
    // T::method. This handles behavior methods with Pin[mut ref This] receiver types,
    // such as Future::poll(mut this: Pin[mut ref This], ...) -> Poll[This::Output].
    {
        TypePtr pin_receiver = receiver_type;
        if (pin_receiver->is<RefType>()) {
            pin_receiver = pin_receiver->as<RefType>().inner;
        }
        if (pin_receiver->is<NamedType>()) {
            const auto& pin_named = pin_receiver->as<NamedType>();
            if (pin_named.name == "Pin" && !pin_named.type_args.empty()) {
                // Pin[ref T] or Pin[mut ref T] — extract T
                TypePtr inner = pin_named.type_args[0];
                if (inner->is<RefType>()) {
                    inner = inner->as<RefType>().inner;
                }
                if (inner->is<NamedType>()) {
                    const auto& inner_named = inner->as<NamedType>();
                    std::string inner_qualified = inner_named.name + "::" + call.method;

                    // Helper: try to resolve the method on the inner type using a FuncSig
                    auto try_resolve_inner_method =
                        [&](const FuncSig& func_sig) -> std::optional<TypePtr> {
                        if (call.type_args.empty() && !func_sig.type_params.empty()) {
                            std::unordered_map<std::string, TypePtr> subs;
                            // Map type params from inner type args
                            // E.g., Ready[I32] with impl type_params=[T] -> {T -> I32}
                            if (!func_sig.impl_self_type_args.empty()) {
                                for (size_t i = 0; i < func_sig.impl_self_type_args.size() &&
                                                   i < inner_named.type_args.size();
                                     ++i) {
                                    extract_type_params(func_sig.impl_self_type_args[i],
                                                        inner_named.type_args[i],
                                                        func_sig.type_params, subs);
                                }
                            } else {
                                for (size_t i = 0; i < func_sig.type_params.size() &&
                                                   i < inner_named.type_args.size();
                                     ++i) {
                                    subs[func_sig.type_params[i]] = inner_named.type_args[i];
                                }
                            }
                            // Also infer from function arguments
                            for (size_t i = 0;
                                 i < call.args.size() && i + 1 < func_sig.params.size(); ++i) {
                                TypePtr param_type = func_sig.params[i + 1];
                                TypePtr expected_param = substitute_type(param_type, subs);
                                TypePtr arg_type = check_expr(*call.args[i], expected_param);
                                extract_type_params(param_type, arg_type, func_sig.type_params,
                                                    subs);
                            }
                            return substitute_type(func_sig.return_type, subs);
                        }
                        if (!call.type_args.empty() && !func_sig.type_params.empty()) {
                            std::unordered_map<std::string, TypePtr> subs;
                            for (size_t i = 0;
                                 i < func_sig.type_params.size() && i < call.type_args.size();
                                 ++i) {
                                subs[func_sig.type_params[i]] = resolve_type(*call.type_args[i]);
                            }
                            return substitute_type(func_sig.return_type, subs);
                        }
                        return func_sig.return_type;
                    };

                    // Try local function environment
                    auto func = env_.lookup_func(inner_qualified);
                    if (func) {
                        auto result = try_resolve_inner_method(*func);
                        if (result)
                            return *result;
                    }

                    // Try all loaded modules
                    for (const auto& [mod_path, mod] : env_.get_all_modules()) {
                        auto func_it = mod.functions.find(inner_qualified);
                        if (func_it != mod.functions.end()) {
                            auto result = try_resolve_inner_method(func_it->second);
                            if (result)
                                return *result;
                        }
                    }

                    // Try global module cache
                    for (const auto& [mod_path, mod] : GlobalModuleCache::instance().get_all()) {
                        auto func_it = mod.functions.find(inner_qualified);
                        if (func_it != mod.functions.end()) {
                            auto result = try_resolve_inner_method(func_it->second);
                            if (result)
                                return *result;
                        }
                    }

                    // Also try behavior method lookup on the inner type
                    auto inner_behaviors = env_.get_behavior_impls(inner_named.name);
                    if (inner_behaviors.empty()) {
                        for (const auto& [mod_path, mod] : env_.get_all_modules()) {
                            auto bi_it = mod.behavior_impls.find(inner_named.name);
                            if (bi_it != mod.behavior_impls.end()) {
                                inner_behaviors = bi_it->second;
                                break;
                            }
                        }
                    }
                    if (inner_behaviors.empty()) {
                        for (const auto& [mod_path, mod] :
                             GlobalModuleCache::instance().get_all()) {
                            auto bi_it = mod.behavior_impls.find(inner_named.name);
                            if (bi_it != mod.behavior_impls.end()) {
                                inner_behaviors = bi_it->second;
                                break;
                            }
                        }
                    }
                    for (const auto& behavior_name : inner_behaviors) {
                        auto behavior_def = env_.lookup_behavior(behavior_name);
                        if (!behavior_def)
                            continue;
                        for (const auto& bmethod : behavior_def->methods) {
                            if (bmethod.name != call.method)
                                continue;
                            std::unordered_map<std::string, TypePtr> subs;
                            auto self_type = std::make_shared<Type>();
                            self_type->kind =
                                NamedType{inner_named.name, "", inner_named.type_args};
                            subs["Self"] = self_type;
                            subs["This"] = self_type;
                            if (!behavior_def->type_params.empty() &&
                                !inner_named.type_args.empty()) {
                                for (size_t i = 0; i < behavior_def->type_params.size() &&
                                                   i < inner_named.type_args.size();
                                     ++i) {
                                    subs[behavior_def->type_params[i]] = inner_named.type_args[i];
                                }
                            }
                            // Resolve associated types from the impl
                            for (const auto& assoc : behavior_def->associated_types) {
                                std::string assoc_key = "This::" + assoc.name;
                                if (!inner_named.type_args.empty()) {
                                    subs[assoc_key] = inner_named.type_args[0];
                                    subs[assoc.name] = inner_named.type_args[0];
                                }
                            }
                            return substitute_type(bmethod.return_type, subs);
                        }
                    }
                }
            }
        }
    }

    return make_unit();
}

} // namespace tml::types
