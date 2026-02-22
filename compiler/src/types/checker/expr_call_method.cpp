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
    if (!param_type || !arg_type)
        return;

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
}

auto TypeChecker::check_method_call(const parser::MethodCallExpr& call) -> TypePtr {
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
            if (type_name == "I8")
                return make_primitive(PrimitiveKind::I8);
            if (type_name == "I16")
                return make_primitive(PrimitiveKind::I16);
            if (type_name == "I32")
                return make_primitive(PrimitiveKind::I32);
            if (type_name == "I64")
                return make_primitive(PrimitiveKind::I64);
            if (type_name == "I128")
                return make_primitive(PrimitiveKind::I128);
            if (type_name == "U8")
                return make_primitive(PrimitiveKind::U8);
            if (type_name == "U16")
                return make_primitive(PrimitiveKind::U16);
            if (type_name == "U32")
                return make_primitive(PrimitiveKind::U32);
            if (type_name == "U64")
                return make_primitive(PrimitiveKind::U64);
            if (type_name == "U128")
                return make_primitive(PrimitiveKind::U128);
            if (type_name == "F32")
                return make_primitive(PrimitiveKind::F32);
            if (type_name == "F64")
                return make_primitive(PrimitiveKind::F64);
            if (type_name == "Bool")
                return make_primitive(PrimitiveKind::Bool);
            if (type_name == "Str")
                return make_primitive(PrimitiveKind::Str);
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

        auto func = env_.lookup_func(qualified);
        if (func) {
            // For generic impl methods, substitute type parameters from:
            // 1. The receiver's type arguments (e.g., T, E from Outcome[T, E])
            // 2. Inferred from function arguments (e.g., U in map_or[U])
            if (call.type_args.empty() && !func->type_params.empty()) {
                std::unordered_map<std::string, TypePtr> subs;

                // First, substitute from receiver's type_args
                for (size_t i = 0; i < func->type_params.size() && i < named.type_args.size();
                     ++i) {
                    subs[func->type_params[i]] = named.type_args[i];
                }

                // Then, infer remaining type params from function arguments
                // Check arguments and match against parameter types
                // Note: func->params[0] is 'this', so we offset by 1
                for (size_t i = 0; i < call.args.size() && i + 1 < func->params.size(); ++i) {
                    TypePtr arg_type = check_expr(*call.args[i]);
                    TypePtr param_type = func->params[i + 1]; // Skip 'this' parameter
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
                for (size_t i = 0; i < func_sig.type_params.size() && i < named.type_args.size();
                     ++i) {
                    subs[func_sig.type_params[i]] = named.type_args[i];
                }
                for (size_t i = 0; i < call.args.size() && i + 1 < func_sig.params.size(); ++i) {
                    TypePtr arg_type = check_expr(*call.args[i]);
                    TypePtr param_type = func_sig.params[i + 1];
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

    if (receiver_type->is<DynBehaviorType>()) {
        auto& dyn = receiver_type->as<DynBehaviorType>();
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
            error("Unknown method '" + call.method + "' on behavior '" + dyn.behavior_name + "'",
                  call.receiver->span, "T079");
        }
    }

    // Handle method calls on generic type parameters with behavior bounds from where clauses
    // e.g., func process[C](c: ref C) where C: Container[I32] { c.get(0) }
    TypePtr unwrapped_receiver = receiver_type;
    if (receiver_type->is<RefType>()) {
        unwrapped_receiver = receiver_type->as<RefType>().inner;
    }
    if (unwrapped_receiver->is<NamedType>()) {
        auto& named_receiver = unwrapped_receiver->as<NamedType>();
        // Check if this is a type parameter by looking for it in current where constraints
        for (const auto& constraint : current_where_constraints_) {
            if (constraint.type_param == named_receiver.name) {
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

    // Handle Ordering enum methods
    if (receiver_type->is<NamedType>()) {
        auto& named = receiver_type->as<NamedType>();
        if (named.name == "Ordering") {
            // is_less, is_equal, is_greater return Bool
            if (call.method == "is_less" || call.method == "is_equal" ||
                call.method == "is_greater") {
                return make_primitive(PrimitiveKind::Bool);
            }
            // reverse, then_cmp return Ordering
            if (call.method == "reverse" || call.method == "then_cmp") {
                return receiver_type;
            }
            // to_string, debug_string return Str
            if (call.method == "to_string" || call.method == "debug_string") {
                return make_primitive(PrimitiveKind::Str);
            }
        }

        // Handle Maybe[T] methods
        if (named.name == "Maybe" && !named.type_args.empty()) {
            TypePtr inner_type = named.type_args[0];

            // is_just(), is_nothing() return Bool
            if (call.method == "is_just" || call.method == "is_nothing") {
                return make_primitive(PrimitiveKind::Bool);
            }

            // unwrap(), expect(msg) return T
            if (call.method == "unwrap" || call.method == "expect") {
                return inner_type;
            }

            // unwrap_or(default), unwrap_or_else(f), unwrap_or_default() return T
            if (call.method == "unwrap_or" || call.method == "unwrap_or_else" ||
                call.method == "unwrap_or_default") {
                return inner_type;
            }

            // map(f) returns Maybe[U] (same structure)
            if (call.method == "map") {
                return receiver_type;
            }

            // and_then(f) returns Maybe[U]
            if (call.method == "and_then") {
                return receiver_type;
            }

            // or_else(f) returns Maybe[T]
            if (call.method == "or_else") {
                return receiver_type;
            }

            // contains(value) returns Bool
            if (call.method == "contains") {
                return make_primitive(PrimitiveKind::Bool);
            }

            // filter(predicate) returns Maybe[T]
            if (call.method == "filter") {
                return receiver_type;
            }

            // alt(other) returns Maybe[T]
            if (call.method == "alt") {
                return receiver_type;
            }

            // xor(other) returns Maybe[T] - renamed to one_of because xor is a keyword
            if (call.method == "xor" || call.method == "one_of") {
                return receiver_type;
            }

            // also(other) returns Maybe[U] - returns the other Maybe type
            if (call.method == "also") {
                if (!call.args.empty()) {
                    return check_expr(*call.args[0]);
                }
                return receiver_type;
            }

            // map_or(default, f) returns U
            if (call.method == "map_or") {
                if (call.args.size() >= 1) {
                    return check_expr(*call.args[0]); // Type of default
                }
                return inner_type;
            }

            // ok_or(err) returns Outcome[T, E]
            if (call.method == "ok_or") {
                if (call.args.size() >= 1) {
                    TypePtr err_type = check_expr(*call.args[0]);
                    std::vector<TypePtr> type_args = {inner_type, err_type};
                    return std::make_shared<Type>(NamedType{"Outcome", "", std::move(type_args)});
                }
                return receiver_type;
            }

            // ok_or_else(f) returns Outcome[T, E]
            if (call.method == "ok_or_else") {
                // For now, return a generic Outcome type
                // The actual error type comes from the closure
                return receiver_type; // Simplified - would need proper inference
            }

            // duplicate() returns Maybe[T]
            if (call.method == "duplicate") {
                return receiver_type;
            }

            // to_string(), debug_string() return Str (Display/Debug behavior)
            if (call.method == "to_string" || call.method == "debug_string") {
                return make_primitive(PrimitiveKind::Str);
            }
        }

        // Handle atomic type methods that return Outcome
        // compare_exchange and compare_exchange_weak return Outcome[T, T]
        if (call.method == "compare_exchange" || call.method == "compare_exchange_weak") {
            TypePtr inner_type;
            bool is_atomic = false;
            if (named.name == "AtomicBool") {
                inner_type = make_primitive(PrimitiveKind::Bool);
                is_atomic = true;
            } else if (named.name == "AtomicI8") {
                inner_type = make_primitive(PrimitiveKind::I8);
                is_atomic = true;
            } else if (named.name == "AtomicI16") {
                inner_type = make_primitive(PrimitiveKind::I16);
                is_atomic = true;
            } else if (named.name == "AtomicI32") {
                inner_type = make_primitive(PrimitiveKind::I32);
                is_atomic = true;
            } else if (named.name == "AtomicI64") {
                inner_type = make_primitive(PrimitiveKind::I64);
                is_atomic = true;
            } else if (named.name == "AtomicI128") {
                inner_type = make_primitive(PrimitiveKind::I128);
                is_atomic = true;
            } else if (named.name == "AtomicU8") {
                inner_type = make_primitive(PrimitiveKind::U8);
                is_atomic = true;
            } else if (named.name == "AtomicU16") {
                inner_type = make_primitive(PrimitiveKind::U16);
                is_atomic = true;
            } else if (named.name == "AtomicU32") {
                inner_type = make_primitive(PrimitiveKind::U32);
                is_atomic = true;
            } else if (named.name == "AtomicU64") {
                inner_type = make_primitive(PrimitiveKind::U64);
                is_atomic = true;
            } else if (named.name == "AtomicU128") {
                inner_type = make_primitive(PrimitiveKind::U128);
                is_atomic = true;
            } else if (named.name == "AtomicPtr" && !named.type_args.empty()) {
                inner_type = make_ptr(named.type_args[0], false);
                is_atomic = true;
            }
            if (is_atomic && inner_type) {
                // Return Outcome[T, T] where T is the atomic's inner type
                auto outcome_type = std::make_shared<Type>();
                outcome_type->kind = NamedType{"Outcome", "", {inner_type, inner_type}};
                return outcome_type;
            }
        }

        // Handle Outcome[T, E] methods
        if (named.name == "Outcome" && named.type_args.size() >= 2) {
            TypePtr ok_type = named.type_args[0];
            TypePtr err_type = named.type_args[1];

            // is_ok(), is_err() return Bool
            if (call.method == "is_ok" || call.method == "is_err") {
                return make_primitive(PrimitiveKind::Bool);
            }

            // is_ok_and(predicate), is_err_and(predicate) return Bool
            if (call.method == "is_ok_and" || call.method == "is_err_and") {
                return make_primitive(PrimitiveKind::Bool);
            }

            // unwrap() returns T
            if (call.method == "unwrap" || call.method == "expect") {
                return ok_type;
            }

            // unwrap_err() returns E
            if (call.method == "unwrap_err" || call.method == "expect_err") {
                return err_type;
            }

            // unwrap_or(default), unwrap_or_else(f), unwrap_or_default() return T
            if (call.method == "unwrap_or" || call.method == "unwrap_or_else" ||
                call.method == "unwrap_or_default") {
                return ok_type;
            }

            // map(f) returns Outcome[U, E] - same structure, potentially different T
            if (call.method == "map") {
                return receiver_type; // Same Outcome type structure
            }

            // map_err(f) returns Outcome[T, F] - same structure, potentially different E
            if (call.method == "map_err") {
                return receiver_type;
            }

            // map_or(default, f) returns U (the default/mapped type)
            if (call.method == "map_or") {
                if (call.args.size() >= 1) {
                    return check_expr(*call.args[0]); // Type of default
                }
                return ok_type;
            }

            // map_or_else(default_f, map_f) returns U
            if (call.method == "map_or_else") {
                return ok_type; // Simplified - returns same type as ok
            }

            // and_then(f) returns Outcome[U, E]
            if (call.method == "and_then") {
                return receiver_type;
            }

            // or_else(f) returns Outcome[T, F]
            if (call.method == "or_else") {
                return receiver_type;
            }

            // alt(other) returns Outcome[T, E]
            if (call.method == "alt") {
                return receiver_type;
            }

            // also(other) returns Outcome[U, E]
            if (call.method == "also") {
                if (!call.args.empty()) {
                    return check_expr(*call.args[0]);
                }
                return receiver_type;
            }

            // ok() returns Maybe[T]
            if (call.method == "ok") {
                std::vector<TypePtr> type_args = {ok_type};
                return std::make_shared<Type>(NamedType{"Maybe", "", type_args});
            }

            // err() returns Maybe[E]
            if (call.method == "err") {
                std::vector<TypePtr> type_args = {err_type};
                return std::make_shared<Type>(NamedType{"Maybe", "", type_args});
            }

            // contains(ref T), contains_err(ref E) return Bool
            if (call.method == "contains" || call.method == "contains_err") {
                return make_primitive(PrimitiveKind::Bool);
            }

            // flatten() for Outcome[Outcome[T, E], E] returns Outcome[T, E]
            if (call.method == "flatten") {
                if (ok_type->is<NamedType>()) {
                    auto& inner_named = ok_type->as<NamedType>();
                    if (inner_named.name == "Outcome" && !inner_named.type_args.empty()) {
                        return ok_type; // Return the inner Outcome type
                    }
                }
                return receiver_type;
            }

            // iter() returns OutcomeIter[T]
            if (call.method == "iter") {
                std::vector<TypePtr> type_args = {ok_type};
                return std::make_shared<Type>(NamedType{"OutcomeIter", "", type_args});
            }

            // duplicate() returns Outcome[T, E]
            if (call.method == "duplicate") {
                return receiver_type;
            }

            // to_string(), debug_string() return Str (Display/Debug behavior)
            if (call.method == "to_string" || call.method == "debug_string") {
                return make_primitive(PrimitiveKind::Str);
            }
        }

        // Handle List[T] methods (NamedType with name "List")
        if (named.name == "List" && !named.type_args.empty()) {
            TypePtr elem_type = named.type_args[0];

            // len() returns I64
            if (call.method == "len") {
                return make_primitive(PrimitiveKind::I64);
            }

            // is_empty() returns Bool
            if (call.method == "is_empty") {
                return make_primitive(PrimitiveKind::Bool);
            }

            // get(index) returns Maybe[ref T]
            if (call.method == "get") {
                auto ref_type = std::make_shared<Type>(
                    RefType{.is_mut = false, .inner = elem_type, .lifetime = std::nullopt});
                std::vector<TypePtr> type_args = {ref_type};
                return std::make_shared<Type>(NamedType{"Maybe", "", type_args});
            }

            // first(), last() return Maybe[ref T]
            if (call.method == "first" || call.method == "last") {
                auto ref_type = std::make_shared<Type>(
                    RefType{.is_mut = false, .inner = elem_type, .lifetime = std::nullopt});
                std::vector<TypePtr> type_args = {ref_type};
                return std::make_shared<Type>(NamedType{"Maybe", "", type_args});
            }

            // push(elem) returns unit
            if (call.method == "push") {
                return make_unit();
            }

            // push_str(s) returns unit (for List[U8] / Text)
            if (call.method == "push_str") {
                return make_unit();
            }

            // pop() returns Maybe[T]
            if (call.method == "pop") {
                std::vector<TypePtr> type_args = {elem_type};
                return std::make_shared<Type>(NamedType{"Maybe", "", type_args});
            }

            // clear() returns unit
            if (call.method == "clear") {
                return make_unit();
            }

            // iter() returns ListIter[T]
            if (call.method == "iter" || call.method == "into_iter") {
                std::vector<TypePtr> type_args = {elem_type};
                return std::make_shared<Type>(NamedType{"ListIter", "", type_args});
            }

            // contains(value) returns Bool
            if (call.method == "contains") {
                return make_primitive(PrimitiveKind::Bool);
            }

            // reverse() returns unit (in-place)
            if (call.method == "reverse") {
                return make_unit();
            }

            // sort() returns unit (in-place)
            if (call.method == "sort") {
                return make_unit();
            }

            // duplicate() returns List[T]
            if (call.method == "duplicate") {
                return receiver_type;
            }

            // to_string(), debug_string() return Str
            if (call.method == "to_string" || call.method == "debug_string") {
                return make_primitive(PrimitiveKind::Str);
            }

            // slice(start, end) returns List[T]
            if (call.method == "slice") {
                return receiver_type;
            }

            // extend(other) returns unit
            if (call.method == "extend") {
                return make_unit();
            }

            // insert(index, elem) returns unit
            if (call.method == "insert") {
                return make_unit();
            }

            // remove(index) returns T
            if (call.method == "remove") {
                return elem_type;
            }

            // swap(i, j) returns unit
            if (call.method == "swap") {
                return make_unit();
            }

            // Index operator [] returns T (or ref T)
            // This is handled via __index__ method lookup
        }
    }

    // Handle ArrayType methods (e.g., [I32; 3].len(), [I32; 3].get(0), etc.)
    if (receiver_type->is<ArrayType>()) {
        auto& arr = receiver_type->as<ArrayType>();
        TypePtr elem_type = arr.element;
        (void)arr.size; // Size used for array methods like map that preserve size

        // len() returns I64
        if (call.method == "len") {
            return make_primitive(PrimitiveKind::I64);
        }

        // is_empty() returns Bool
        if (call.method == "is_empty") {
            return make_primitive(PrimitiveKind::Bool);
        }

        // get(index) returns Maybe[ref T]
        if (call.method == "get") {
            auto ref_type = std::make_shared<Type>(
                RefType{.is_mut = false, .inner = elem_type, .lifetime = std::nullopt});
            std::vector<TypePtr> type_args = {ref_type};
            return std::make_shared<Type>(NamedType{"Maybe", "", type_args});
        }

        // first(), last() return Maybe[ref T]
        if (call.method == "first" || call.method == "last") {
            auto ref_type = std::make_shared<Type>(
                RefType{.is_mut = false, .inner = elem_type, .lifetime = std::nullopt});
            std::vector<TypePtr> type_args = {ref_type};
            return std::make_shared<Type>(NamedType{"Maybe", "", type_args});
        }

        // map(f) returns [U; N] where U is inferred from the closure
        if (call.method == "map") {
            // For now, return the same array type (simplified)
            // The actual mapped type would require closure inference
            return receiver_type;
        }

        // eq(other) and ne(other) return Bool
        if (call.method == "eq" || call.method == "ne") {
            return make_primitive(PrimitiveKind::Bool);
        }

        // cmp(other) returns Ordering
        if (call.method == "cmp") {
            return std::make_shared<Type>(NamedType{"Ordering", "", {}});
        }

        // as_slice() returns Slice[T]
        if (call.method == "as_slice") {
            return std::make_shared<Type>(SliceType{elem_type});
        }

        // as_mut_slice() returns MutSlice[T]
        if (call.method == "as_mut_slice") {
            std::vector<TypePtr> type_args = {elem_type};
            return std::make_shared<Type>(NamedType{"MutSlice", "", type_args});
        }

        // iter() returns ArrayIter[T, N]
        if (call.method == "iter" || call.method == "into_iter") {
            std::vector<TypePtr> type_args = {elem_type};
            return std::make_shared<Type>(NamedType{"ArrayIter", "", type_args});
        }

        // duplicate() returns [T; N] (same type)
        if (call.method == "duplicate") {
            return receiver_type;
        }

        // hash() returns I64
        if (call.method == "hash") {
            return make_primitive(PrimitiveKind::I64);
        }

        // to_string() returns Str
        if (call.method == "to_string" || call.method == "debug_string") {
            return make_primitive(PrimitiveKind::Str);
        }
    }

    // Handle SliceType methods (e.g., [T].len(), [T].get(0), etc.)
    if (receiver_type->is<SliceType>()) {
        auto& slice = receiver_type->as<SliceType>();
        TypePtr elem_type = slice.element;

        // len() returns I64
        if (call.method == "len") {
            return make_primitive(PrimitiveKind::I64);
        }

        // is_empty() returns Bool
        if (call.method == "is_empty") {
            return make_primitive(PrimitiveKind::Bool);
        }

        // get(index) returns Maybe[ref T]
        if (call.method == "get") {
            auto ref_type = std::make_shared<Type>(
                RefType{.is_mut = false, .inner = elem_type, .lifetime = std::nullopt});
            std::vector<TypePtr> type_args = {ref_type};
            return std::make_shared<Type>(NamedType{"Maybe", "", type_args});
        }

        // first(), last() return Maybe[ref T]
        if (call.method == "first" || call.method == "last") {
            auto ref_type = std::make_shared<Type>(
                RefType{.is_mut = false, .inner = elem_type, .lifetime = std::nullopt});
            std::vector<TypePtr> type_args = {ref_type};
            return std::make_shared<Type>(NamedType{"Maybe", "", type_args});
        }

        // slice(start, end) returns Slice[T]
        if (call.method == "slice") {
            return receiver_type;
        }

        // iter() returns SliceIter[T]
        if (call.method == "iter" || call.method == "into_iter") {
            std::vector<TypePtr> type_args = {elem_type};
            return std::make_shared<Type>(NamedType{"SliceIter", "", type_args});
        }

        // push() returns unit (for dynamic slices)
        if (call.method == "push") {
            return make_unit();
        }

        // pop() returns Maybe[T]
        if (call.method == "pop") {
            std::vector<TypePtr> type_args = {elem_type};
            return std::make_shared<Type>(NamedType{"Maybe", "", type_args});
        }

        // to_string(), debug_string() return Str
        if (call.method == "to_string" || call.method == "debug_string") {
            return make_primitive(PrimitiveKind::Str);
        }
    }

    // Handle Fn trait method calls on closures and function types
    // call(), call_mut(), call_once() invoke the callable
    TypePtr callable_type = receiver_type;
    if (receiver_type->is<RefType>()) {
        callable_type = receiver_type->as<RefType>().inner;
    }
    if (callable_type->is<ClosureType>()) {
        const auto& closure = callable_type->as<ClosureType>();
        if (call.method == "call" || call.method == "call_mut" || call.method == "call_once") {
            // Return the closure's return type
            return closure.return_type;
        }
    }
    if (callable_type->is<FuncType>()) {
        const auto& func = callable_type->as<FuncType>();
        if (call.method == "call" || call.method == "call_mut" || call.method == "call_once") {
            // Return the function's return type
            return func.return_type;
        }
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

    return make_unit();
}

} // namespace tml::types
