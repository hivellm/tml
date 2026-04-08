TML_MODULE("compiler")

//! # Type Checker - Method Call Resolution for Builtin Types
//!
//! This file implements type-specific method resolution for builtin/stdlib types:
//! Ordering, Maybe[T], Outcome[T,E], List[T], ArrayType, SliceType, Fn traits, TupleType.
//!
//! Split from expr_call_method.cpp for maintainability.
//! Called from TypeChecker::check_method_call via check_method_call_builtin_types.

#include "common.hpp"
#include "types/checker.hpp"
#include "types/module.hpp"

namespace tml::types {

/// Resolve the return type for method calls on builtin types (Maybe, Outcome, List,
/// Array, Slice, Fn traits, Tuple, Ordering).
///
/// Returns the resolved TypePtr wrapped in std::optional if the method was recognized,
/// or std::nullopt if the method does not belong to any of the handled types (allowing
/// the caller to continue with fallback resolution).
auto TypeChecker::check_method_call_builtin_types(const parser::MethodCallExpr& call,
                                                  const TypePtr& obj_type,
                                                  const std::string& method_name)
    -> std::optional<TypePtr> {
    // Handle Ordering enum methods
    if (obj_type->is<NamedType>()) {
        auto& named = obj_type->as<NamedType>();
        if (named.name == "Ordering") {
            // is_less, is_equal, is_greater return Bool
            if (method_name == "is_less" || method_name == "is_equal" ||
                method_name == "is_greater") {
                return make_primitive(PrimitiveKind::Bool);
            }
            // reverse, then_cmp return Ordering
            if (method_name == "reverse" || method_name == "then_cmp") {
                return obj_type;
            }
            // to_string, debug_string return Str
            if (method_name == "to_string" || method_name == "debug_string") {
                return make_primitive(PrimitiveKind::Str);
            }
        }

        // Handle Maybe[T] methods
        if (named.name == "Maybe" && !named.type_args.empty()) {
            TypePtr inner_type = named.type_args[0];

            // is_just(), is_nothing() return Bool
            if (method_name == "is_just" || method_name == "is_nothing") {
                return make_primitive(PrimitiveKind::Bool);
            }

            // unwrap(), expect(msg) return T
            if (method_name == "unwrap" || method_name == "expect") {
                return inner_type;
            }

            // unwrap_or(default), unwrap_or_else(f), unwrap_or_default() return T
            if (method_name == "unwrap_or" || method_name == "unwrap_or_else" ||
                method_name == "unwrap_or_default") {
                return inner_type;
            }

            // map(f: T -> U) returns Maybe[U]
            if (method_name == "map") {
                if (!call.args.empty()) {
                    auto f_type = check_expr(*call.args[0]);
                    TypePtr u_type = nullptr;
                    if (f_type && f_type->is<ClosureType>())
                        u_type = f_type->as<ClosureType>().return_type;
                    else if (f_type && f_type->is<FuncType>())
                        u_type = f_type->as<FuncType>().return_type;
                    if (u_type) {
                        auto result = std::make_shared<Type>();
                        result->kind = NamedType{"Maybe", "", {u_type}};
                        return result;
                    }
                }
                return obj_type;
            }

            // and_then(f: T -> Maybe[U]) returns Maybe[U]
            if (method_name == "and_then") {
                if (!call.args.empty()) {
                    auto f_type = check_expr(*call.args[0]);
                    TypePtr ret = nullptr;
                    if (f_type && f_type->is<ClosureType>())
                        ret = f_type->as<ClosureType>().return_type;
                    else if (f_type && f_type->is<FuncType>())
                        ret = f_type->as<FuncType>().return_type;
                    // ret is already Maybe[U] — return it directly
                    if (ret)
                        return ret;
                }
                return obj_type;
            }

            // or_else(f) returns Maybe[T]
            if (method_name == "or_else") {
                return obj_type;
            }

            // contains(value) returns Bool
            if (method_name == "contains") {
                return make_primitive(PrimitiveKind::Bool);
            }

            // filter(predicate) returns Maybe[T]
            if (method_name == "filter") {
                return obj_type;
            }

            // alt(other) returns Maybe[T]
            if (method_name == "alt") {
                return obj_type;
            }

            // xor(other) returns Maybe[T] - renamed to one_of because xor is a keyword
            if (method_name == "xor" || method_name == "one_of") {
                return obj_type;
            }

            // also(other) returns Maybe[U] - returns the other Maybe type
            if (method_name == "also") {
                if (!call.args.empty()) {
                    return check_expr(*call.args[0]);
                }
                return obj_type;
            }

            // map_or(default, f) returns U
            if (method_name == "map_or") {
                if (call.args.size() >= 1) {
                    return check_expr(*call.args[0]); // Type of default
                }
                return inner_type;
            }

            // ok_or(err) returns Outcome[T, E]
            if (method_name == "ok_or") {
                if (call.args.size() >= 1) {
                    TypePtr err_type = check_expr(*call.args[0]);
                    std::vector<TypePtr> type_args = {inner_type, err_type};
                    return std::make_shared<Type>(NamedType{"Outcome", "", std::move(type_args)});
                }
                return obj_type;
            }

            // ok_or_else(f) returns Outcome[T, E]
            if (method_name == "ok_or_else") {
                // Infer E from the closure's return type
                if (!call.args.empty()) {
                    TypePtr closure_type = check_expr(*call.args[0]);
                    TypePtr err_type;
                    if (closure_type && closure_type->is<FuncType>()) {
                        err_type = closure_type->as<FuncType>().return_type;
                    }
                    if (!err_type) {
                        err_type = make_unit();
                    }
                    std::vector<TypePtr> type_args = {inner_type, err_type};
                    return std::make_shared<Type>(NamedType{"Outcome", "", std::move(type_args)});
                }
                return obj_type;
            }

            // flatten() for Maybe[Maybe[T]] returns Maybe[T]
            if (method_name == "flatten") {
                if (inner_type->is<NamedType>()) {
                    auto& inner_named = inner_type->as<NamedType>();
                    if (inner_named.name == "Maybe" && !inner_named.type_args.empty()) {
                        return inner_type;
                    }
                }
                return obj_type;
            }

            // duplicate() returns Maybe[T]
            if (method_name == "duplicate") {
                return obj_type;
            }

            // as_ref() returns Maybe[ref T]
            if (method_name == "as_ref") {
                auto ref_inner = std::make_shared<Type>(
                    RefType{.is_mut = false, .inner = inner_type, .lifetime = std::nullopt});
                std::vector<TypePtr> type_args = {ref_inner};
                return std::make_shared<Type>(NamedType{"Maybe", "", std::move(type_args)});
            }

            // as_mut() returns Maybe[mut ref T]
            if (method_name == "as_mut") {
                auto ref_inner = std::make_shared<Type>(
                    RefType{.is_mut = true, .inner = inner_type, .lifetime = std::nullopt});
                std::vector<TypePtr> type_args = {ref_inner};
                return std::make_shared<Type>(NamedType{"Maybe", "", std::move(type_args)});
            }

            // inspect(f) returns Maybe[T] (passes ref to closure, returns self)
            if (method_name == "inspect") {
                // Type-check the closure argument
                if (!call.args.empty()) {
                    check_expr(*call.args[0]);
                }
                return obj_type;
            }

            // take() returns Maybe[T] (takes value out, leaving Nothing)
            if (method_name == "take") {
                return obj_type;
            }

            // replace(value) returns Maybe[T] (old value)
            if (method_name == "replace") {
                if (!call.args.empty()) {
                    check_expr(*call.args[0], inner_type);
                }
                return obj_type;
            }

            // zip(other) returns Maybe[(T, U)]
            if (method_name == "zip") {
                if (!call.args.empty()) {
                    TypePtr other_type = check_expr(*call.args[0]);
                    // Extract U from Maybe[U]
                    if (other_type && other_type->is<NamedType>()) {
                        auto& other_named = other_type->as<NamedType>();
                        if (other_named.name == "Maybe" && !other_named.type_args.empty()) {
                            TypePtr u_type = other_named.type_args[0];
                            auto tuple_type = std::make_shared<Type>(
                                TupleType{std::vector<TypePtr>{inner_type, u_type}});
                            std::vector<TypePtr> type_args = {tuple_type};
                            return std::make_shared<Type>(
                                NamedType{"Maybe", "", std::move(type_args)});
                        }
                    }
                }
                return obj_type;
            }

            // zip_with(other, f) returns Maybe[V]
            if (method_name == "zip_with") {
                if (call.args.size() >= 2) {
                    TypePtr other_type = check_expr(*call.args[0]);
                    TypePtr closure_type = check_expr(*call.args[1]);
                    // Infer V from the closure's return type
                    if (closure_type && closure_type->is<FuncType>()) {
                        TypePtr v_type = closure_type->as<FuncType>().return_type;
                        std::vector<TypePtr> type_args = {v_type};
                        return std::make_shared<Type>(NamedType{"Maybe", "", std::move(type_args)});
                    }
                }
                return obj_type;
            }

            // is_just_and(pred) returns Bool
            if (method_name == "is_just_and") {
                if (!call.args.empty()) {
                    check_expr(*call.args[0]);
                }
                return make_primitive(PrimitiveKind::Bool);
            }

            // get_or_insert(value), get_or_insert_with(f) return Unit (mutate in-place)
            if (method_name == "get_or_insert" || method_name == "get_or_insert_with") {
                if (!call.args.empty()) {
                    check_expr(*call.args[0]);
                }
                return make_unit();
            }

            // map_or_else(default_fn, f) returns U
            if (method_name == "map_or_else") {
                if (call.args.size() >= 1) {
                    TypePtr default_fn_type = check_expr(*call.args[0]);
                    // Infer U from default_fn's return type
                    if (default_fn_type && default_fn_type->is<FuncType>()) {
                        return default_fn_type->as<FuncType>().return_type;
                    }
                }
                return inner_type;
            }

            // iter() returns MaybeIter[T]
            if (method_name == "iter") {
                std::vector<TypePtr> type_args = {inner_type};
                return std::make_shared<Type>(NamedType{"MaybeIter", "", std::move(type_args)});
            }

            // transpose() returns Outcome[Maybe[T_inner], E] for Maybe[Outcome[T_inner, E]]
            if (method_name == "transpose") {
                if (inner_type->is<NamedType>()) {
                    auto& inner_named = inner_type->as<NamedType>();
                    if (inner_named.name == "Outcome" && inner_named.type_args.size() >= 2) {
                        TypePtr t_inner = inner_named.type_args[0];
                        TypePtr e_type = inner_named.type_args[1];
                        std::vector<TypePtr> maybe_args = {t_inner};
                        auto maybe_type =
                            std::make_shared<Type>(NamedType{"Maybe", "", std::move(maybe_args)});
                        std::vector<TypePtr> outcome_args = {maybe_type, e_type};
                        return std::make_shared<Type>(
                            NamedType{"Outcome", "", std::move(outcome_args)});
                    }
                }
                return obj_type;
            }

            // unzip() returns (Maybe[A], Maybe[B]) for Maybe[(A, B)]
            if (method_name == "unzip") {
                if (inner_type->is<TupleType>()) {
                    auto& tuple = inner_type->as<TupleType>();
                    if (tuple.elements.size() >= 2) {
                        auto maybe_a = std::make_shared<Type>(
                            NamedType{"Maybe", "", std::vector<TypePtr>{tuple.elements[0]}});
                        auto maybe_b = std::make_shared<Type>(
                            NamedType{"Maybe", "", std::vector<TypePtr>{tuple.elements[1]}});
                        return std::make_shared<Type>(
                            TupleType{std::vector<TypePtr>{maybe_a, maybe_b}});
                    }
                }
                return obj_type;
            }

            // to_string(), debug_string() return Str (Display/Debug behavior)
            if (method_name == "to_string" || method_name == "debug_string") {
                return make_primitive(PrimitiveKind::Str);
            }
        }

        // Handle atomic type methods that return Outcome
        // compare_exchange and compare_exchange_weak return Outcome[T, T]
        if (method_name == "compare_exchange" || method_name == "compare_exchange_weak") {
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
            if (method_name == "is_ok" || method_name == "is_err") {
                return make_primitive(PrimitiveKind::Bool);
            }

            // is_ok_and(predicate), is_err_and(predicate) return Bool
            if (method_name == "is_ok_and" || method_name == "is_err_and") {
                return make_primitive(PrimitiveKind::Bool);
            }

            // unwrap() returns T
            if (method_name == "unwrap" || method_name == "expect") {
                return ok_type;
            }

            // unwrap_err() returns E
            if (method_name == "unwrap_err" || method_name == "expect_err") {
                return err_type;
            }

            // unwrap_or(default), unwrap_or_else(f), unwrap_or_default() return T
            if (method_name == "unwrap_or" || method_name == "unwrap_or_else" ||
                method_name == "unwrap_or_default") {
                return ok_type;
            }

            // map(f: T -> U) returns Outcome[U, E]
            if (method_name == "map") {
                if (!call.args.empty()) {
                    auto f_type = check_expr(*call.args[0]);
                    TypePtr u_type = nullptr;
                    if (f_type && f_type->is<ClosureType>())
                        u_type = f_type->as<ClosureType>().return_type;
                    else if (f_type && f_type->is<FuncType>())
                        u_type = f_type->as<FuncType>().return_type;
                    if (u_type) {
                        auto result = std::make_shared<Type>();
                        result->kind = NamedType{"Outcome", "", {u_type, err_type}};
                        return result;
                    }
                }
                return obj_type;
            }

            // map_err(f: E -> F) returns Outcome[T, F]
            if (method_name == "map_err") {
                if (!call.args.empty()) {
                    auto f_type = check_expr(*call.args[0]);
                    TypePtr f_err = nullptr;
                    if (f_type && f_type->is<ClosureType>())
                        f_err = f_type->as<ClosureType>().return_type;
                    else if (f_type && f_type->is<FuncType>())
                        f_err = f_type->as<FuncType>().return_type;
                    if (f_err) {
                        auto result = std::make_shared<Type>();
                        result->kind = NamedType{"Outcome", "", {ok_type, f_err}};
                        return result;
                    }
                }
                return obj_type;
            }

            // map_or(default, f) returns U (the default/mapped type)
            if (method_name == "map_or") {
                if (call.args.size() >= 1) {
                    return check_expr(*call.args[0]); // Type of default
                }
                return ok_type;
            }

            // map_or_else(default_fn, f) returns U
            if (method_name == "map_or_else") {
                if (call.args.size() >= 1) {
                    TypePtr default_fn_type = check_expr(*call.args[0]);
                    if (default_fn_type && default_fn_type->is<FuncType>()) {
                        return default_fn_type->as<FuncType>().return_type;
                    }
                }
                return ok_type;
            }

            // and_then(f: T -> Outcome[U, E]) returns Outcome[U, E]
            if (method_name == "and_then") {
                if (!call.args.empty()) {
                    auto f_type = check_expr(*call.args[0]);
                    TypePtr ret = nullptr;
                    if (f_type && f_type->is<ClosureType>())
                        ret = f_type->as<ClosureType>().return_type;
                    else if (f_type && f_type->is<FuncType>())
                        ret = f_type->as<FuncType>().return_type;
                    // ret is already Outcome[U, E] — return directly
                    if (ret)
                        return ret;
                }
                return obj_type;
            }

            // or_else(f) returns Outcome[T, F]
            if (method_name == "or_else") {
                return obj_type;
            }

            // alt(other) returns Outcome[T, E]
            if (method_name == "alt") {
                return obj_type;
            }

            // also(other) returns Outcome[U, E]
            if (method_name == "also") {
                if (!call.args.empty()) {
                    return check_expr(*call.args[0]);
                }
                return obj_type;
            }

            // ok() returns Maybe[T]
            if (method_name == "ok") {
                std::vector<TypePtr> type_args = {ok_type};
                return std::make_shared<Type>(NamedType{"Maybe", "", type_args});
            }

            // err() returns Maybe[E]
            if (method_name == "err") {
                std::vector<TypePtr> type_args = {err_type};
                return std::make_shared<Type>(NamedType{"Maybe", "", type_args});
            }

            // contains(ref T), contains_err(ref E) return Bool
            if (method_name == "contains" || method_name == "contains_err") {
                return make_primitive(PrimitiveKind::Bool);
            }

            // flatten() for Outcome[Outcome[T, E], E] returns Outcome[T, E]
            if (method_name == "flatten") {
                if (ok_type->is<NamedType>()) {
                    auto& inner_named = ok_type->as<NamedType>();
                    if (inner_named.name == "Outcome" && !inner_named.type_args.empty()) {
                        return ok_type; // Return the inner Outcome type
                    }
                }
                return obj_type;
            }

            // iter() returns OutcomeIter[T]
            if (method_name == "iter") {
                std::vector<TypePtr> type_args = {ok_type};
                return std::make_shared<Type>(NamedType{"OutcomeIter", "", type_args});
            }

            // duplicate(), copied(), duplicated() return Outcome[T, E]
            if (method_name == "duplicate" || method_name == "copied" ||
                method_name == "duplicated") {
                return obj_type;
            }

            // inspect(f) returns Outcome[T, E] (passes ref T to closure, returns self)
            if (method_name == "inspect") {
                if (!call.args.empty()) {
                    check_expr(*call.args[0]);
                }
                return obj_type;
            }

            // inspect_err(f) returns Outcome[T, E] (passes ref E to closure, returns self)
            if (method_name == "inspect_err") {
                if (!call.args.empty()) {
                    check_expr(*call.args[0]);
                }
                return obj_type;
            }

            // as_ref() returns Outcome[ref T, ref E]
            if (method_name == "as_ref") {
                auto ref_ok = std::make_shared<Type>(
                    RefType{.is_mut = false, .inner = ok_type, .lifetime = std::nullopt});
                auto ref_err = std::make_shared<Type>(
                    RefType{.is_mut = false, .inner = err_type, .lifetime = std::nullopt});
                std::vector<TypePtr> type_args = {ref_ok, ref_err};
                return std::make_shared<Type>(NamedType{"Outcome", "", std::move(type_args)});
            }

            // as_mut() returns Outcome[mut ref T, mut ref E]
            if (method_name == "as_mut") {
                auto ref_ok = std::make_shared<Type>(
                    RefType{.is_mut = true, .inner = ok_type, .lifetime = std::nullopt});
                auto ref_err = std::make_shared<Type>(
                    RefType{.is_mut = true, .inner = err_type, .lifetime = std::nullopt});
                std::vector<TypePtr> type_args = {ref_ok, ref_err};
                return std::make_shared<Type>(NamedType{"Outcome", "", std::move(type_args)});
            }

            // transpose() returns Maybe[Outcome[T_inner, E]] for Outcome[Maybe[T_inner], E]
            if (method_name == "transpose") {
                if (ok_type->is<NamedType>()) {
                    auto& ok_named = ok_type->as<NamedType>();
                    if (ok_named.name == "Maybe" && !ok_named.type_args.empty()) {
                        TypePtr t_inner = ok_named.type_args[0];
                        std::vector<TypePtr> outcome_args = {t_inner, err_type};
                        auto outcome_type = std::make_shared<Type>(
                            NamedType{"Outcome", "", std::move(outcome_args)});
                        std::vector<TypePtr> maybe_args = {outcome_type};
                        return std::make_shared<Type>(
                            NamedType{"Maybe", "", std::move(maybe_args)});
                    }
                }
                return obj_type;
            }

            // to_string(), debug_string() return Str (Display/Debug behavior)
            if (method_name == "to_string" || method_name == "debug_string") {
                return make_primitive(PrimitiveKind::Str);
            }
        }

        // Handle List[T] methods (NamedType with name "List")
        if (named.name == "List" && !named.type_args.empty()) {
            TypePtr elem_type = named.type_args[0];

            // len() returns I64
            if (method_name == "len") {
                return make_primitive(PrimitiveKind::I64);
            }

            // is_empty() returns Bool
            if (method_name == "is_empty") {
                return make_primitive(PrimitiveKind::Bool);
            }

            // get(index) returns Maybe[ref T]
            if (method_name == "get") {
                auto ref_type = std::make_shared<Type>(
                    RefType{.is_mut = false, .inner = elem_type, .lifetime = std::nullopt});
                std::vector<TypePtr> type_args = {ref_type};
                return std::make_shared<Type>(NamedType{"Maybe", "", type_args});
            }

            // first(), last() return Maybe[ref T]
            if (method_name == "first" || method_name == "last") {
                auto ref_type = std::make_shared<Type>(
                    RefType{.is_mut = false, .inner = elem_type, .lifetime = std::nullopt});
                std::vector<TypePtr> type_args = {ref_type};
                return std::make_shared<Type>(NamedType{"Maybe", "", type_args});
            }

            // push(elem) returns unit
            if (method_name == "push") {
                return make_unit();
            }

            // push_str(s) returns unit (for List[U8] / Text)
            if (method_name == "push_str") {
                return make_unit();
            }

            // pop() returns Maybe[T]
            if (method_name == "pop") {
                std::vector<TypePtr> type_args = {elem_type};
                return std::make_shared<Type>(NamedType{"Maybe", "", type_args});
            }

            // clear() returns unit
            if (method_name == "clear") {
                return make_unit();
            }

            // iter() returns ListIter[T]
            if (method_name == "iter" || method_name == "into_iter") {
                std::vector<TypePtr> type_args = {elem_type};
                return std::make_shared<Type>(NamedType{"ListIter", "", type_args});
            }

            // contains(value) returns Bool
            if (method_name == "contains") {
                return make_primitive(PrimitiveKind::Bool);
            }

            // reverse() returns unit (in-place)
            if (method_name == "reverse") {
                return make_unit();
            }

            // sort() returns unit (in-place)
            if (method_name == "sort") {
                return make_unit();
            }

            // duplicate() returns List[T]
            if (method_name == "duplicate") {
                return obj_type;
            }

            // to_string(), debug_string() return Str
            if (method_name == "to_string" || method_name == "debug_string") {
                return make_primitive(PrimitiveKind::Str);
            }

            // slice(start, end) returns List[T]
            if (method_name == "slice") {
                return obj_type;
            }

            // extend(other) returns unit
            if (method_name == "extend") {
                return make_unit();
            }

            // insert(index, elem) returns unit
            if (method_name == "insert") {
                return make_unit();
            }

            // remove(index) returns T
            if (method_name == "remove") {
                return elem_type;
            }

            // swap(i, j) returns unit
            if (method_name == "swap") {
                return make_unit();
            }

            // Index operator [] returns T (or ref T)
            // This is handled via __index__ method lookup
        }
    }

    // Handle ArrayType methods (e.g., [I32; 3].len(), [I32; 3].get(0), etc.)
    if (obj_type->is<ArrayType>()) {
        auto& arr = obj_type->as<ArrayType>();
        TypePtr elem_type = arr.element;
        (void)arr.size; // Size used for array methods like map that preserve size

        // len() returns I64
        if (method_name == "len") {
            return make_primitive(PrimitiveKind::I64);
        }

        // is_empty() returns Bool
        if (method_name == "is_empty") {
            return make_primitive(PrimitiveKind::Bool);
        }

        // get(index) returns Maybe[ref T]
        if (method_name == "get") {
            auto ref_type = std::make_shared<Type>(
                RefType{.is_mut = false, .inner = elem_type, .lifetime = std::nullopt});
            std::vector<TypePtr> type_args = {ref_type};
            return std::make_shared<Type>(NamedType{"Maybe", "", type_args});
        }

        // first(), last() return Maybe[ref T]
        if (method_name == "first" || method_name == "last") {
            auto ref_type = std::make_shared<Type>(
                RefType{.is_mut = false, .inner = elem_type, .lifetime = std::nullopt});
            std::vector<TypePtr> type_args = {ref_type};
            return std::make_shared<Type>(NamedType{"Maybe", "", type_args});
        }

        // map(f) returns [U; N] where U is inferred from the closure
        if (method_name == "map") {
            // For now, return the same array type (simplified)
            // The actual mapped type would require closure inference
            return obj_type;
        }

        // eq(other) and ne(other) return Bool
        if (method_name == "eq" || method_name == "ne") {
            return make_primitive(PrimitiveKind::Bool);
        }

        // cmp(other) returns Ordering
        if (method_name == "cmp") {
            return std::make_shared<Type>(NamedType{"Ordering", "", {}});
        }

        // partial_cmp(other) returns Maybe[Ordering]
        if (method_name == "partial_cmp") {
            auto ordering = std::make_shared<Type>(NamedType{"Ordering", "", {}});
            std::vector<TypePtr> type_args = {ordering};
            return std::make_shared<Type>(NamedType{"Maybe", "", type_args});
        }

        // default() returns [T; N] (same type)
        if (method_name == "default") {
            return obj_type;
        }

        // clone() returns [T; N] (same type)
        if (method_name == "clone") {
            return obj_type;
        }

        // eq_slice(other) returns Bool
        if (method_name == "eq_slice") {
            return make_primitive(PrimitiveKind::Bool);
        }

        // as_ref() returns Slice[T]
        if (method_name == "as_ref") {
            return std::make_shared<Type>(SliceType{elem_type});
        }

        // as_slice() returns Slice[T]
        if (method_name == "as_slice") {
            return std::make_shared<Type>(SliceType{elem_type});
        }

        // as_mut_slice() returns MutSlice[T]
        if (method_name == "as_mut_slice") {
            std::vector<TypePtr> type_args = {elem_type};
            return std::make_shared<Type>(NamedType{"MutSlice", "", type_args});
        }

        // iter() returns ArrayIter[T, N]
        if (method_name == "iter" || method_name == "into_iter") {
            std::vector<TypePtr> type_args = {elem_type};
            return std::make_shared<Type>(NamedType{"ArrayIter", "", type_args});
        }

        // duplicate() returns [T; N] (same type)
        if (method_name == "duplicate") {
            return obj_type;
        }

        // hash() returns I64
        if (method_name == "hash") {
            return make_primitive(PrimitiveKind::I64);
        }

        // to_string() returns Str
        if (method_name == "to_string" || method_name == "debug_string") {
            return make_primitive(PrimitiveKind::Str);
        }
    }

    // Handle SliceType methods (e.g., [T].len(), [T].get(0), etc.)
    if (obj_type->is<SliceType>()) {
        auto& slice = obj_type->as<SliceType>();
        TypePtr elem_type = slice.element;

        // len() returns I64
        if (method_name == "len") {
            return make_primitive(PrimitiveKind::I64);
        }

        // is_empty() returns Bool
        if (method_name == "is_empty") {
            return make_primitive(PrimitiveKind::Bool);
        }

        // get(index) returns Maybe[ref T]
        if (method_name == "get") {
            auto ref_type = std::make_shared<Type>(
                RefType{.is_mut = false, .inner = elem_type, .lifetime = std::nullopt});
            std::vector<TypePtr> type_args = {ref_type};
            return std::make_shared<Type>(NamedType{"Maybe", "", type_args});
        }

        // first(), last() return Maybe[ref T]
        if (method_name == "first" || method_name == "last") {
            auto ref_type = std::make_shared<Type>(
                RefType{.is_mut = false, .inner = elem_type, .lifetime = std::nullopt});
            std::vector<TypePtr> type_args = {ref_type};
            return std::make_shared<Type>(NamedType{"Maybe", "", type_args});
        }

        // slice(start, end) returns Slice[T]
        if (method_name == "slice") {
            return obj_type;
        }

        // iter() returns SliceIter[T]
        if (method_name == "iter" || method_name == "into_iter") {
            std::vector<TypePtr> type_args = {elem_type};
            return std::make_shared<Type>(NamedType{"SliceIter", "", type_args});
        }

        // push() returns unit (for dynamic slices)
        if (method_name == "push") {
            return make_unit();
        }

        // pop() returns Maybe[T]
        if (method_name == "pop") {
            std::vector<TypePtr> type_args = {elem_type};
            return std::make_shared<Type>(NamedType{"Maybe", "", type_args});
        }

        // to_string(), debug_string() return Str
        if (method_name == "to_string" || method_name == "debug_string") {
            return make_primitive(PrimitiveKind::Str);
        }
    }

    // Handle Fn trait method calls on closures and function types
    // call(), call_mut(), call_once() invoke the callable
    TypePtr callable_type = obj_type;
    if (obj_type->is<RefType>()) {
        callable_type = obj_type->as<RefType>().inner;
    }
    if (callable_type->is<ClosureType>()) {
        const auto& closure = callable_type->as<ClosureType>();
        if (method_name == "call" || method_name == "call_mut" || method_name == "call_once") {
            // Return the closure's return type
            return closure.return_type;
        }
    }
    if (callable_type->is<FuncType>()) {
        const auto& func = callable_type->as<FuncType>();
        if (method_name == "call" || method_name == "call_mut" || method_name == "call_once") {
            // Return the function's return type
            return func.return_type;
        }
    }

    // Handle TupleType methods — PartialEq, Eq, PartialOrd, Ord, Clone, Default
    // Tuple impls are generic (e.g., impl[A: PartialEq, B: PartialEq] PartialEq for (A, B))
    // but the type checker needs explicit return type resolution here.
    TypePtr tuple_receiver = obj_type;
    if (tuple_receiver->is<RefType>()) {
        tuple_receiver = tuple_receiver->as<RefType>().inner;
    }
    if (tuple_receiver->is<TupleType>()) {
        // eq(other), ne(other) return Bool
        if (method_name == "eq" || method_name == "ne") {
            return make_primitive(PrimitiveKind::Bool);
        }
        // partial_cmp(other) returns Maybe[Ordering]
        if (method_name == "partial_cmp") {
            auto ordering = std::make_shared<Type>(NamedType{"Ordering", "", {}});
            std::vector<TypePtr> type_args = {ordering};
            return std::make_shared<Type>(NamedType{"Maybe", "", type_args});
        }
        // cmp(other) returns Ordering
        if (method_name == "cmp") {
            return std::make_shared<Type>(NamedType{"Ordering", "", {}});
        }
        // clone() returns the same tuple type
        if (method_name == "clone" || method_name == "duplicate") {
            return tuple_receiver;
        }
        // to_string(), debug_string() return Str
        if (method_name == "to_string" || method_name == "debug_string") {
            return make_primitive(PrimitiveKind::Str);
        }
        // hash(hasher) returns Unit
        if (method_name == "hash") {
            return make_unit();
        }
    }

    return std::nullopt;
}

} // namespace tml::types
