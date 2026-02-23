TML_MODULE("compiler")

//! # Type Checker - Call Expressions
//!
//! This file implements type checking for function calls (check_call).
//! Method call type checking (check_method_call) is in expr_call_method.cpp.
//!
//! ## Call Resolution Order
//!
//! 1. Polymorphic builtins (print, println)
//! 2. Compiler intrinsics (type_id, size_of, align_of, type_name)
//! 3. Named function lookup with overload resolution
//! 4. Enum constructor lookup
//! 5. Static method calls on types
//! 6. Generic function instantiation

#include "common.hpp"
#include "lexer/token.hpp"
#include "types/checker.hpp"

#include <unordered_set>

namespace tml::types {

// Forward declarations from helpers.cpp
bool is_integer_type(const TypePtr& type);
bool types_compatible(const TypePtr& expected, const TypePtr& actual);

/// Extract type parameter bindings by matching parameter type against argument type.
/// For example, matching `ManuallyDrop[T]` against `ManuallyDrop[I64]` extracts {T -> I64}.
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

auto TypeChecker::check_call(const parser::CallExpr& call) -> TypePtr {
    // Check if this is a polymorphic builtin
    if (call.callee->is<parser::IdentExpr>()) {
        const auto& name = call.callee->as<parser::IdentExpr>().name;
        if (name == "print" || name == "println") {
            for (const auto& arg : call.args) {
                check_expr(*arg);
            }
            return make_unit();
        }
    }

    // Check for compiler intrinsics called with generics (e.g., type_id[I32](), size_of[T]())
    if (call.callee->is<parser::PathExpr>()) {
        const auto& path = call.callee->as<parser::PathExpr>();
        if (path.path.segments.size() == 1) {
            const std::string& name = path.path.segments[0];
            // List of intrinsics that take a type parameter and return I64
            if (name == "type_id" || name == "size_of" || name == "align_of") {
                // These intrinsics take a type parameter [T] and return I64
                // The type argument is validated by codegen, we just need to return the right type
                return make_primitive(PrimitiveKind::I64);
            }
            // type_name[T]() returns Str
            if (name == "type_name") {
                return make_primitive(PrimitiveKind::Str);
            }

            // Handle generic free function calls with explicit type args: func[T](args)
            auto func = env_.lookup_func(name);
            if (func) {
                // Build substitutions from explicit type arguments
                std::unordered_map<std::string, TypePtr> substitutions;
                if (path.generics.has_value() && !func->type_params.empty()) {
                    const auto& gen_args = path.generics.value().args;
                    for (size_t i = 0; i < func->type_params.size() && i < gen_args.size(); ++i) {
                        if (gen_args[i].is_type()) {
                            substitutions[func->type_params[i]] =
                                resolve_type(*gen_args[i].as_type());
                        }
                    }
                }

                // Type check arguments with expected param types (after substitution)
                for (size_t i = 0; i < call.args.size() && i < func->params.size(); ++i) {
                    TypePtr expected_param = func->params[i];
                    if (!substitutions.empty()) {
                        expected_param = substitute_type(expected_param, substitutions);
                    }
                    check_expr(*call.args[i], expected_param);
                }

                // Also infer type params from arguments if not all provided explicitly
                if (!func->type_params.empty()) {
                    for (size_t i = 0; i < call.args.size() && i < func->params.size(); ++i) {
                        auto arg_type = check_expr(*call.args[i]);
                        extract_type_params(func->params[i], arg_type, func->type_params,
                                            substitutions);
                    }
                }

                return substitute_type(func->return_type, substitutions);
            }

            // Check imported module functions with explicit type args
            auto imported_path = env_.resolve_imported_symbol(name);
            if (imported_path.has_value()) {
                size_t pos = imported_path->rfind("::");
                if (pos != std::string::npos) {
                    std::string module_path = imported_path->substr(0, pos);
                    auto module = env_.get_module(module_path);
                    if (module) {
                        auto func_it = module->functions.find(name);
                        if (func_it != module->functions.end()) {
                            const auto& func_sig = func_it->second;
                            std::unordered_map<std::string, TypePtr> substitutions;
                            if (path.generics.has_value() && !func_sig.type_params.empty()) {
                                const auto& gen_args = path.generics.value().args;
                                for (size_t i = 0;
                                     i < func_sig.type_params.size() && i < gen_args.size(); ++i) {
                                    if (gen_args[i].is_type()) {
                                        substitutions[func_sig.type_params[i]] =
                                            resolve_type(*gen_args[i].as_type());
                                    }
                                }
                            }
                            for (size_t i = 0; i < call.args.size() && i < func_sig.params.size();
                                 ++i) {
                                TypePtr expected_param = func_sig.params[i];
                                if (!substitutions.empty()) {
                                    expected_param = substitute_type(expected_param, substitutions);
                                }
                                check_expr(*call.args[i], expected_param);
                            }
                            if (!func_sig.type_params.empty()) {
                                for (size_t i = 0;
                                     i < call.args.size() && i < func_sig.params.size(); ++i) {
                                    auto arg_type = check_expr(*call.args[i]);
                                    extract_type_params(func_sig.params[i], arg_type,
                                                        func_sig.type_params, substitutions);
                                }
                            }
                            return substitute_type(func_sig.return_type, substitutions);
                        }
                    }
                }
            }
        }
    }

    // Check function lookup first
    if (call.callee->is<parser::IdentExpr>()) {
        auto& ident = call.callee->as<parser::IdentExpr>();

        // First, check argument types for overload resolution
        std::vector<TypePtr> arg_types;
        for (const auto& arg : call.args) {
            arg_types.push_back(check_expr(*arg));
        }

        // Try to find the right overload based on argument types
        auto func = env_.lookup_func_overload(ident.name, arg_types);
        if (!func) {
            // Fallback to first overload if no exact match
            func = env_.lookup_func(ident.name);
        }
        if (func) {
            // Handle generic functions
            if (!func->type_params.empty()) {
                std::unordered_map<std::string, TypePtr> substitutions;
                for (size_t i = 0; i < call.args.size() && i < func->params.size(); ++i) {
                    // Pass expected parameter type for numeric literal coercion
                    auto arg_type = check_expr(*call.args[i], func->params[i]);
                    if (func->params[i]->is<NamedType>()) {
                        const auto& named = func->params[i]->as<NamedType>();
                        for (const auto& tp : func->type_params) {
                            if (named.name == tp && named.type_args.empty()) {
                                substitutions[tp] = arg_type;
                                break;
                            }
                        }
                    }
                    if (func->params[i]->is<GenericType>()) {
                        const auto& gen = func->params[i]->as<GenericType>();
                        for (const auto& tp : func->type_params) {
                            if (gen.name == tp) {
                                substitutions[tp] = arg_type;
                                break;
                            }
                        }
                    }
                }

                // Check where clause constraints
                for (const auto& constraint : func->where_constraints) {
                    auto it = substitutions.find(constraint.type_param);
                    if (it != substitutions.end()) {
                        // Use the TypePtr directly for type_implements to handle closures
                        TypePtr actual_type = it->second;
                        std::string type_name = type_to_string(actual_type);

                        // Check simple behavior bounds
                        for (const auto& behavior : constraint.required_behaviors) {
                            if (!env_.type_implements(actual_type, behavior)) {
                                error("Type '" + type_name + "' does not implement behavior '" +
                                          behavior + "' required by constraint on " +
                                          constraint.type_param,
                                      call.callee->span, "T026");
                            }
                        }

                        // Check parameterized behavior bounds
                        for (const auto& bound : constraint.parameterized_bounds) {
                            // First check that the type implements the base behavior
                            // Use TypePtr overload to handle closures implementing Fn traits
                            if (!env_.type_implements(actual_type, bound.behavior_name)) {
                                // Build type args string for error message
                                std::string type_args_str;
                                if (!bound.type_args.empty()) {
                                    type_args_str = "[";
                                    for (size_t i = 0; i < bound.type_args.size(); ++i) {
                                        if (i > 0)
                                            type_args_str += ", ";
                                        type_args_str += type_to_string(bound.type_args[i]);
                                    }
                                    type_args_str += "]";
                                }
                                error("Type '" + type_name + "' does not implement behavior '" +
                                          bound.behavior_name + type_args_str +
                                          "' required by constraint on " + constraint.type_param,
                                      call.callee->span, "T026");
                            }
                            // Note: Full parameterized bound checking (verifying type args match)
                            // would require tracking impl blocks with their type arguments.
                            // For now, we just verify the base behavior is implemented.
                        }
                    }
                }

                // Check lifetime bounds (e.g., T: life static)
                for (const auto& [param_name, lifetime_bound] : func->lifetime_bounds) {
                    auto it = substitutions.find(param_name);
                    if (it != substitutions.end()) {
                        TypePtr actual_type = it->second;
                        if (!type_satisfies_lifetime_bound(actual_type, lifetime_bound)) {
                            std::string type_name = type_to_string(actual_type);
                            error("E033: type '" + type_name +
                                      "' may not live long enough - does not satisfy `life " +
                                      lifetime_bound + "` bound on type parameter " + param_name,
                                  call.callee->span, "T054");
                        }
                    }
                }

                return substitute_type(func->return_type, substitutions);
            }
            return func->return_type;
        }

        // Try enum constructor lookup
        for (const auto& [enum_name, enum_def] : env_.all_enums()) {
            for (const auto& [variant_name, payload_types] : enum_def.variants) {
                if (variant_name == ident.name) {
                    if (call.args.size() != payload_types.size()) {
                        error("Enum variant '" + variant_name + "' expects " +
                                  std::to_string(payload_types.size()) + " arguments, but got " +
                                  std::to_string(call.args.size()),
                              call.callee->span, "T034");
                        return make_unit();
                    }

                    for (size_t i = 0; i < call.args.size(); ++i) {
                        // Pass expected payload type for numeric literal coercion
                        auto arg_type = check_expr(*call.args[i], payload_types[i]);
                        (void)arg_type;
                    }

                    auto enum_type = std::make_shared<Type>();
                    enum_type->kind = NamedType{enum_name, "", {}};
                    return enum_type;
                }
            }
        }
    }

    // Check for static method calls on primitive types via PathExpr (e.g., I32::default())
    if (call.callee->is<parser::PathExpr>()) {
        const auto& path = call.callee->as<parser::PathExpr>();
        if (path.path.segments.size() == 2) {
            const std::string& type_name = path.path.segments[0];
            const std::string& method = path.path.segments[1];

            bool is_primitive_type =
                type_name == "I8" || type_name == "I16" || type_name == "I32" ||
                type_name == "I64" || type_name == "I128" || type_name == "U8" ||
                type_name == "U16" || type_name == "U32" || type_name == "U64" ||
                type_name == "U128" || type_name == "F32" || type_name == "F64" ||
                type_name == "Bool" || type_name == "Str";

            if (is_primitive_type && method == "default") {
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

            // Handle Type::from(value) for type conversion
            if (is_primitive_type && method == "from" && !call.args.empty()) {
                // Type check the argument (source type)
                check_expr(*call.args[0]);
                // Return the target type
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

            // Handle imported type static methods (e.g., Layout::from_size_align)
            if (!is_primitive_type) {
                // First check if it's a class constructor call (ClassName::new)
                auto class_def = env_.lookup_class(type_name);
                if (class_def.has_value() && method == "new") {
                    // Type check constructor arguments
                    for (const auto& arg : call.args) {
                        check_expr(*arg);
                    }
                    // Return the class type
                    auto class_type = std::make_shared<Type>();
                    class_type->kind = ClassType{type_name, "", {}};
                    return class_type;
                }

                // Check for class static method call (not constructor)
                if (class_def.has_value()) {
                    for (const auto& m : class_def->methods) {
                        if (m.sig.name == method && m.is_static) {
                            // Type check arguments
                            for (const auto& arg : call.args) {
                                check_expr(*arg);
                            }
                            // Check visibility
                            check_member_visibility(m.vis, type_name, method, call.callee->span);
                            // Apply type arguments for generic static methods
                            if (path.generics.has_value() && !m.sig.type_params.empty()) {
                                std::unordered_map<std::string, TypePtr> subs;
                                const auto& gen_args = path.generics.value().args;
                                for (size_t i = 0;
                                     i < m.sig.type_params.size() && i < gen_args.size(); ++i) {
                                    if (gen_args[i].is_type()) {
                                        subs[m.sig.type_params[i]] =
                                            resolve_type(*gen_args[i].as_type());
                                    }
                                }
                                return substitute_type(m.sig.return_type, subs);
                            }
                            return m.sig.return_type;
                        }
                    }
                }

                // Check for local struct/enum static methods (before imports)
                // This handles Type::method() calls for types defined in the current file
                std::string qualified_func = type_name + "::" + method;
                auto local_func = env_.lookup_func(qualified_func);
                if (local_func) {
                    // Type check arguments and collect their types
                    std::vector<TypePtr> arg_types;
                    for (size_t i = 0; i < call.args.size(); ++i) {
                        // Pass expected param type for numeric literal coercion
                        TypePtr expected_param =
                            (i < local_func->params.size()) ? local_func->params[i] : nullptr;
                        arg_types.push_back(check_expr(*call.args[i], expected_param));
                    }

                    // Handle generic functions - apply explicit type args then infer from arguments
                    if (!local_func->type_params.empty()) {
                        std::unordered_map<std::string, TypePtr> substitutions;

                        // First: apply explicit type arguments (e.g., mem::zeroed[I32]())
                        if (path.generics.has_value()) {
                            const auto& gen_args = path.generics.value().args;
                            for (size_t i = 0;
                                 i < local_func->type_params.size() && i < gen_args.size(); ++i) {
                                if (gen_args[i].is_type()) {
                                    substitutions[local_func->type_params[i]] =
                                        resolve_type(*gen_args[i].as_type());
                                }
                            }
                        }

                        // For static methods on generic types (like Wrapper[T]::unwrap),
                        // extract type args from arguments that match the type pattern.
                        // E.g., if arg is Wrapper[I64] and param is Wrapper[T], extract T=I64
                        for (size_t i = 0; i < arg_types.size() && i < local_func->params.size();
                             ++i) {
                            const auto& arg_type = arg_types[i];
                            const auto& param_type = local_func->params[i];

                            // If both arg and param are NamedType with same base name,
                            // directly map type_params to arg's type_args (like instance methods)
                            if (arg_type->is<NamedType>() && param_type->is<NamedType>()) {
                                const auto& arg_named = arg_type->as<NamedType>();
                                const auto& param_named = param_type->as<NamedType>();
                                if (arg_named.name == param_named.name &&
                                    !arg_named.type_args.empty() &&
                                    param_named.type_args.size() == arg_named.type_args.size()) {
                                    // Map type params to concrete types from argument
                                    for (size_t j = 0; j < local_func->type_params.size() &&
                                                       j < arg_named.type_args.size();
                                         ++j) {
                                        substitutions[local_func->type_params[j]] =
                                            arg_named.type_args[j];
                                    }
                                }
                            }
                            // Also use extract_type_params for more complex cases
                            extract_type_params(param_type, arg_type, local_func->type_params,
                                                substitutions);
                        }
                        return substitute_type(local_func->return_type, substitutions);
                    }
                    return local_func->return_type;
                }

                // Try to resolve type_name as an imported symbol
                auto imported_path = env_.resolve_imported_symbol(type_name);
                if (imported_path.has_value()) {
                    std::string module_path;
                    size_t pos = imported_path->rfind("::");
                    if (pos != std::string::npos) {
                        module_path = imported_path->substr(0, pos);
                    }

                    // Look up the qualified function name in the module
                    // (reuse qualified_func from above)
                    auto module = env_.get_module(module_path);
                    if (module) {
                        auto func_it = module->functions.find(qualified_func);
                        if (func_it != module->functions.end()) {
                            const auto& func = func_it->second;

                            // Handle generic functions - apply explicit type args first
                            std::unordered_map<std::string, TypePtr> substitutions;
                            if (!func.type_params.empty() && path.generics.has_value()) {
                                const auto& gen_args = path.generics.value().args;
                                for (size_t i = 0;
                                     i < func.type_params.size() && i < gen_args.size(); ++i) {
                                    if (gen_args[i].is_type()) {
                                        substitutions[func.type_params[i]] =
                                            resolve_type(*gen_args[i].as_type());
                                    }
                                }
                            }

                            // Type check arguments and collect their types
                            std::vector<TypePtr> arg_types;
                            for (size_t i = 0; i < call.args.size(); ++i) {
                                // Pass expected param type for numeric literal coercion
                                TypePtr expected_param =
                                    (i < func.params.size()) ? func.params[i] : nullptr;
                                if (expected_param && !substitutions.empty()) {
                                    expected_param = substitute_type(expected_param, substitutions);
                                }
                                arg_types.push_back(check_expr(*call.args[i], expected_param));
                            }

                            // Infer remaining type params from arguments
                            if (!func.type_params.empty()) {
                                for (size_t i = 0; i < arg_types.size() && i < func.params.size();
                                     ++i) {
                                    const auto& arg_type = arg_types[i];
                                    const auto& param_type = func.params[i];

                                    // Direct mapping for matching NamedTypes
                                    if (arg_type->is<NamedType>() && param_type->is<NamedType>()) {
                                        const auto& arg_named = arg_type->as<NamedType>();
                                        const auto& param_named = param_type->as<NamedType>();
                                        if (arg_named.name == param_named.name &&
                                            !arg_named.type_args.empty() &&
                                            param_named.type_args.size() ==
                                                arg_named.type_args.size()) {
                                            for (size_t j = 0; j < func.type_params.size() &&
                                                               j < arg_named.type_args.size();
                                                 ++j) {
                                                substitutions[func.type_params[j]] =
                                                    arg_named.type_args[j];
                                            }
                                        }
                                    }
                                    extract_type_params(param_type, arg_type, func.type_params,
                                                        substitutions);
                                }
                            }
                            if (!substitutions.empty()) {
                                return substitute_type(func.return_type, substitutions);
                            }
                            return func.return_type;
                        }
                    }
                }
            }
        }
    }

    // Fallback: check callee type
    auto callee_type = check_expr(*call.callee);
    if (callee_type->is<FuncType>()) {
        auto& func = callee_type->as<FuncType>();
        if (call.args.size() != func.params.size()) {
            error("Wrong number of arguments", call.callee->span, "T004");
        }

        // Infer generic type substitutions from argument types
        // This is needed for generic enum variant constructors like Option::Some(42)
        std::unordered_map<std::string, TypePtr> substitutions;

        for (size_t i = 0; i < std::min(call.args.size(), func.params.size()); ++i) {
            auto& param_type = func.params[i];
            // Pass expected parameter type for numeric literal coercion
            auto arg_type = check_expr(*call.args[i], param_type);

            // If the parameter type is a NamedType that could be a type parameter (T, U, etc.)
            // and it has no type_args, it might be a generic type parameter
            if (param_type->is<NamedType>()) {
                const auto& named = param_type->as<NamedType>();
                // Check if this is a type parameter: empty type_args, empty module_path,
                // and not a known type (struct, enum, primitive, etc.)
                if (named.type_args.empty() && named.module_path.empty() && !named.name.empty()) {
                    bool is_type_param = true;
                    // Check if it's a known type (struct, enum, primitive, etc.)
                    if (env_.lookup_struct(named.name) || env_.lookup_enum(named.name)) {
                        is_type_param = false;
                    }
                    auto& builtins = env_.builtin_types();
                    if (builtins.find(named.name) != builtins.end()) {
                        is_type_param = false;
                    }
                    if (is_type_param) {
                        substitutions[named.name] = arg_type;
                    }
                }
            } else if (param_type->is<GenericType>()) {
                const auto& gen = param_type->as<GenericType>();
                substitutions[gen.name] = arg_type;
            }
        }

        // Apply substitutions to the return type
        TypePtr return_type = func.return_type;
        if (!substitutions.empty()) {
            return_type = substitute_type(return_type, substitutions);
        }

        return return_type;
    }

    // Handle closure types (closures that capture variables)
    if (callee_type->is<ClosureType>()) {
        auto& closure = callee_type->as<ClosureType>();
        if (call.args.size() != closure.params.size()) {
            error("Wrong number of arguments", call.callee->span, "T004");
        }
        for (size_t i = 0; i < std::min(call.args.size(), closure.params.size()); ++i) {
            check_expr(*call.args[i], closure.params[i]);
        }
        return closure.return_type;
    }

    return make_unit();
}

} // namespace tml::types
