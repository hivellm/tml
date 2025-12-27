#include "types/type.hpp"

#include <sstream>

namespace tml::types {

namespace {
uint64_t next_type_id = 1;
}

auto make_primitive(PrimitiveKind kind) -> TypePtr {
    auto type = std::make_shared<Type>();
    type->kind = PrimitiveType{kind};
    type->id = next_type_id++;
    return type;
}

auto make_unit() -> TypePtr {
    return make_primitive(PrimitiveKind::Unit);
}

auto make_bool() -> TypePtr {
    return make_primitive(PrimitiveKind::Bool);
}

auto make_i32() -> TypePtr {
    return make_primitive(PrimitiveKind::I32);
}

auto make_i64() -> TypePtr {
    return make_primitive(PrimitiveKind::I64);
}

auto make_f64() -> TypePtr {
    return make_primitive(PrimitiveKind::F64);
}

auto make_str() -> TypePtr {
    return make_primitive(PrimitiveKind::Str);
}

auto make_never() -> TypePtr {
    return make_primitive(PrimitiveKind::Never);
}

auto make_tuple(std::vector<TypePtr> elements) -> TypePtr {
    auto type = std::make_shared<Type>();
    type->kind = TupleType{std::move(elements)};
    type->id = next_type_id++;
    return type;
}

auto make_func(std::vector<TypePtr> params, TypePtr ret) -> TypePtr {
    auto type = std::make_shared<Type>();
    type->kind = FuncType{std::move(params), std::move(ret), false};
    type->id = next_type_id++;
    return type;
}

auto make_closure(std::vector<TypePtr> params, TypePtr ret,
                  std::vector<CapturedVar> captures) -> TypePtr {
    auto type = std::make_shared<Type>();
    type->kind = ClosureType{std::move(params), std::move(ret), std::move(captures)};
    type->id = next_type_id++;
    return type;
}

auto make_ref(TypePtr inner, bool is_mut) -> TypePtr {
    auto type = std::make_shared<Type>();
    type->kind = RefType{is_mut, std::move(inner)};
    type->id = next_type_id++;
    return type;
}

auto make_ptr(TypePtr inner, bool is_mut) -> TypePtr {
    auto type = std::make_shared<Type>();
    type->kind = PtrType{is_mut, std::move(inner)};
    type->id = next_type_id++;
    return type;
}

auto make_array(TypePtr element, size_t size) -> TypePtr {
    auto type = std::make_shared<Type>();
    type->kind = ArrayType{std::move(element), size};
    type->id = next_type_id++;
    return type;
}

auto make_slice(TypePtr element) -> TypePtr {
    auto type = std::make_shared<Type>();
    type->kind = SliceType{std::move(element)};
    type->id = next_type_id++;
    return type;
}

auto primitive_kind_to_string(PrimitiveKind kind) -> std::string {
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
        return "()";
    case PrimitiveKind::Never:
        return "!";
    }
    return "unknown";
}

auto type_to_string(const TypePtr& type) -> std::string {
    if (!type)
        return "<null>";

    return std::visit(
        [](const auto& t) -> std::string {
            using T = std::decay_t<decltype(t)>;

            if constexpr (std::is_same_v<T, PrimitiveType>) {
                return primitive_kind_to_string(t.kind);
            } else if constexpr (std::is_same_v<T, NamedType>) {
                std::ostringstream ss;
                ss << t.name;
                if (!t.type_args.empty()) {
                    ss << "[";
                    for (size_t i = 0; i < t.type_args.size(); ++i) {
                        if (i > 0)
                            ss << ", ";
                        ss << type_to_string(t.type_args[i]);
                    }
                    ss << "]";
                }
                return ss.str();
            } else if constexpr (std::is_same_v<T, RefType>) {
                return (t.is_mut ? "mut ref " : "ref ") + type_to_string(t.inner);
            } else if constexpr (std::is_same_v<T, PtrType>) {
                return (t.is_mut ? "*mut " : "*") + type_to_string(t.inner);
            } else if constexpr (std::is_same_v<T, ArrayType>) {
                return "[" + type_to_string(t.element) + "; " + std::to_string(t.size) + "]";
            } else if constexpr (std::is_same_v<T, SliceType>) {
                return "[" + type_to_string(t.element) + "]";
            } else if constexpr (std::is_same_v<T, TupleType>) {
                std::ostringstream ss;
                ss << "(";
                for (size_t i = 0; i < t.elements.size(); ++i) {
                    if (i > 0)
                        ss << ", ";
                    ss << type_to_string(t.elements[i]);
                }
                ss << ")";
                return ss.str();
            } else if constexpr (std::is_same_v<T, FuncType>) {
                std::ostringstream ss;
                if (t.is_async)
                    ss << "async ";
                ss << "func(";
                for (size_t i = 0; i < t.params.size(); ++i) {
                    if (i > 0)
                        ss << ", ";
                    ss << type_to_string(t.params[i]);
                }
                ss << ") -> " << type_to_string(t.return_type);
                return ss.str();
            } else if constexpr (std::is_same_v<T, ClosureType>) {
                std::ostringstream ss;
                ss << "Closure[(";
                for (size_t i = 0; i < t.params.size(); ++i) {
                    if (i > 0)
                        ss << ", ";
                    ss << type_to_string(t.params[i]);
                }
                ss << ") -> " << type_to_string(t.return_type);
                if (!t.captures.empty()) {
                    ss << " captures: {";
                    for (size_t i = 0; i < t.captures.size(); ++i) {
                        if (i > 0)
                            ss << ", ";
                        ss << t.captures[i].name << ": " << type_to_string(t.captures[i].type);
                    }
                    ss << "}";
                }
                ss << "]";
                return ss.str();
            } else if constexpr (std::is_same_v<T, TypeVar>) {
                return "?" + std::to_string(t.id);
            } else if constexpr (std::is_same_v<T, GenericType>) {
                return t.name;
            } else {
                return "<unknown>";
            }
        },
        type->kind);
}

auto types_equal(const TypePtr& a, const TypePtr& b) -> bool {
    if (!a && !b)
        return true;
    if (!a || !b)
        return false;
    if (a->id == b->id)
        return true;

    return std::visit(
        [&b](const auto& ta) -> bool {
            using T = std::decay_t<decltype(ta)>;

            if (!std::holds_alternative<T>(b->kind))
                return false;
            const auto& tb = std::get<T>(b->kind);

            if constexpr (std::is_same_v<T, PrimitiveType>) {
                return ta.kind == tb.kind;
            } else if constexpr (std::is_same_v<T, NamedType>) {
                if (ta.name != tb.name || ta.module_path != tb.module_path)
                    return false;
                if (ta.type_args.size() != tb.type_args.size())
                    return false;
                for (size_t i = 0; i < ta.type_args.size(); ++i) {
                    if (!types_equal(ta.type_args[i], tb.type_args[i]))
                        return false;
                }
                return true;
            } else if constexpr (std::is_same_v<T, RefType>) {
                return ta.is_mut == tb.is_mut && types_equal(ta.inner, tb.inner);
            } else if constexpr (std::is_same_v<T, PtrType>) {
                return ta.is_mut == tb.is_mut && types_equal(ta.inner, tb.inner);
            } else if constexpr (std::is_same_v<T, ArrayType>) {
                return ta.size == tb.size && types_equal(ta.element, tb.element);
            } else if constexpr (std::is_same_v<T, SliceType>) {
                return types_equal(ta.element, tb.element);
            } else if constexpr (std::is_same_v<T, TupleType>) {
                if (ta.elements.size() != tb.elements.size())
                    return false;
                for (size_t i = 0; i < ta.elements.size(); ++i) {
                    if (!types_equal(ta.elements[i], tb.elements[i]))
                        return false;
                }
                return true;
            } else if constexpr (std::is_same_v<T, FuncType>) {
                if (ta.is_async != tb.is_async)
                    return false;
                if (!types_equal(ta.return_type, tb.return_type))
                    return false;
                if (ta.params.size() != tb.params.size())
                    return false;
                for (size_t i = 0; i < ta.params.size(); ++i) {
                    if (!types_equal(ta.params[i], tb.params[i]))
                        return false;
                }
                return true;
            } else if constexpr (std::is_same_v<T, ClosureType>) {
                if (!types_equal(ta.return_type, tb.return_type))
                    return false;
                if (ta.params.size() != tb.params.size())
                    return false;
                for (size_t i = 0; i < ta.params.size(); ++i) {
                    if (!types_equal(ta.params[i], tb.params[i]))
                        return false;
                }
                if (ta.captures.size() != tb.captures.size())
                    return false;
                for (size_t i = 0; i < ta.captures.size(); ++i) {
                    if (ta.captures[i].name != tb.captures[i].name)
                        return false;
                    if (!types_equal(ta.captures[i].type, tb.captures[i].type))
                        return false;
                    if (ta.captures[i].is_mut != tb.captures[i].is_mut)
                        return false;
                }
                return true;
            } else if constexpr (std::is_same_v<T, TypeVar>) {
                return ta.id == tb.id;
            } else if constexpr (std::is_same_v<T, GenericType>) {
                return ta.name == tb.name;
            } else {
                return false;
            }
        },
        a->kind);
}

// Generic type substitution - replaces GenericType with concrete types
auto substitute_type(const TypePtr& type,
                     const std::unordered_map<std::string, TypePtr>& subs) -> TypePtr {
    if (!type)
        return type;
    if (subs.empty())
        return type;

    return std::visit(
        [&subs](const auto& t) -> TypePtr {
            using T = std::decay_t<decltype(t)>;

            // GenericType: look up in substitution map
            if constexpr (std::is_same_v<T, GenericType>) {
                auto it = subs.find(t.name);
                if (it != subs.end()) {
                    return it->second;
                }
                // Not found in subs, return as-is (might be unbound)
                auto result = std::make_shared<Type>();
                result->kind = t;
                return result;
            }
            // NamedType: check if name matches substitution (for type params like T)
            // then recursively substitute type_args
            else if constexpr (std::is_same_v<T, NamedType>) {
                // First check if the name itself is a type parameter
                auto it = subs.find(t.name);
                if (it != subs.end() && t.type_args.empty()) {
                    return it->second;
                }
                if (t.type_args.empty()) {
                    auto result = std::make_shared<Type>();
                    result->kind = t;
                    return result;
                }
                std::vector<TypePtr> new_args;
                new_args.reserve(t.type_args.size());
                for (const auto& arg : t.type_args) {
                    new_args.push_back(substitute_type(arg, subs));
                }
                auto result = std::make_shared<Type>();
                result->kind = NamedType{t.name, t.module_path, std::move(new_args)};
                return result;
            }
            // RefType: substitute inner type
            else if constexpr (std::is_same_v<T, RefType>) {
                auto result = std::make_shared<Type>();
                result->kind = RefType{t.is_mut, substitute_type(t.inner, subs)};
                return result;
            }
            // PtrType: substitute inner type
            else if constexpr (std::is_same_v<T, PtrType>) {
                auto result = std::make_shared<Type>();
                result->kind = PtrType{t.is_mut, substitute_type(t.inner, subs)};
                return result;
            }
            // ArrayType: substitute element type
            else if constexpr (std::is_same_v<T, ArrayType>) {
                auto result = std::make_shared<Type>();
                result->kind = ArrayType{substitute_type(t.element, subs), t.size};
                return result;
            }
            // SliceType: substitute element type
            else if constexpr (std::is_same_v<T, SliceType>) {
                auto result = std::make_shared<Type>();
                result->kind = SliceType{substitute_type(t.element, subs)};
                return result;
            }
            // TupleType: substitute all element types
            else if constexpr (std::is_same_v<T, TupleType>) {
                std::vector<TypePtr> new_elements;
                new_elements.reserve(t.elements.size());
                for (const auto& elem : t.elements) {
                    new_elements.push_back(substitute_type(elem, subs));
                }
                auto result = std::make_shared<Type>();
                result->kind = TupleType{std::move(new_elements)};
                return result;
            }
            // FuncType: substitute params and return type
            else if constexpr (std::is_same_v<T, FuncType>) {
                std::vector<TypePtr> new_params;
                new_params.reserve(t.params.size());
                for (const auto& param : t.params) {
                    new_params.push_back(substitute_type(param, subs));
                }
                auto result = std::make_shared<Type>();
                result->kind = FuncType{std::move(new_params), substitute_type(t.return_type, subs),
                                        t.is_async};
                return result;
            }
            // ClosureType: substitute params, return type, and captures
            else if constexpr (std::is_same_v<T, ClosureType>) {
                std::vector<TypePtr> new_params;
                new_params.reserve(t.params.size());
                for (const auto& param : t.params) {
                    new_params.push_back(substitute_type(param, subs));
                }
                std::vector<CapturedVar> new_captures;
                new_captures.reserve(t.captures.size());
                for (const auto& cap : t.captures) {
                    new_captures.push_back(
                        CapturedVar{cap.name, substitute_type(cap.type, subs), cap.is_mut});
                }
                auto result = std::make_shared<Type>();
                result->kind =
                    ClosureType{std::move(new_params), substitute_type(t.return_type, subs),
                                std::move(new_captures)};
                return result;
            }
            // PrimitiveType, TypeVar: no substitution needed
            else {
                auto result = std::make_shared<Type>();
                result->kind = t;
                return result;
            }
        },
        type->kind);
}

} // namespace tml::types
