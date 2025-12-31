// MIR Builder - Type Conversion Implementation
//
// This file contains type conversion functions for converting parser types
// and semantic types to MIR types.

#include "mir/mir_builder.hpp"

namespace tml::mir {

auto MirBuilder::convert_type(const parser::Type& type) -> MirTypePtr {
    return std::visit(
        [this](const auto& t) -> MirTypePtr {
            using T = std::decay_t<decltype(t)>;

            if constexpr (std::is_same_v<T, parser::NamedType>) {
                auto name = t.path.segments.empty() ? "" : t.path.segments.back();

                // Primitive types
                if (name == "Unit" || name == "()")
                    return make_unit_type();
                if (name == "Bool")
                    return make_bool_type();
                if (name == "I8")
                    return make_i8_type();
                if (name == "I16")
                    return make_i16_type();
                if (name == "I32")
                    return make_i32_type();
                if (name == "I64")
                    return make_i64_type();
                if (name == "U8")
                    return make_i32_type();
                if (name == "U16")
                    return make_i32_type();
                if (name == "U32")
                    return make_i32_type();
                if (name == "U64")
                    return make_i64_type();
                if (name == "F32")
                    return make_f32_type();
                if (name == "F64")
                    return make_f64_type();
                if (name == "Str")
                    return make_str_type();

                // Generic types
                std::vector<MirTypePtr> type_args;
                if (t.generics.has_value()) {
                    for (const auto& arg : t.generics->args) {
                        // Only handle type arguments for now (not const generics)
                        if (arg.is_type()) {
                            type_args.push_back(convert_type(*arg.as_type()));
                        }
                    }
                }

                // Check if it's an enum or struct
                if (env_.lookup_enum(name).has_value()) {
                    return make_enum_type(name, std::move(type_args));
                }
                return make_struct_type(name, std::move(type_args));
            } else if constexpr (std::is_same_v<T, parser::RefType>) {
                return make_pointer_type(convert_type(*t.inner), t.is_mut);
            } else if constexpr (std::is_same_v<T, parser::PtrType>) {
                return make_pointer_type(convert_type(*t.inner), t.is_mut);
            } else if constexpr (std::is_same_v<T, parser::ArrayType>) {
                // Try to evaluate size expression as a constant integer literal
                size_t size = 0;
                if (t.size && t.size->template is<parser::LiteralExpr>()) {
                    const auto& lit = t.size->template as<parser::LiteralExpr>();
                    if (lit.token.kind == lexer::TokenKind::IntLiteral) {
                        try {
                            size = std::stoull(std::string(lit.token.lexeme));
                        } catch (...) {
                            size = 0;
                        }
                    }
                }
                return make_array_type(convert_type(*t.element), size);
            } else if constexpr (std::is_same_v<T, parser::SliceType>) {
                auto slice = std::make_shared<MirType>();
                slice->kind = MirSliceType{convert_type(*t.element)};
                return slice;
            } else if constexpr (std::is_same_v<T, parser::TupleType>) {
                std::vector<MirTypePtr> elements;
                for (const auto& elem : t.elements) {
                    elements.push_back(convert_type(*elem));
                }
                return make_tuple_type(std::move(elements));
            } else if constexpr (std::is_same_v<T, parser::FuncType>) {
                std::vector<MirTypePtr> params;
                for (const auto& p : t.params) {
                    params.push_back(convert_type(*p));
                }
                auto ret = t.return_type ? convert_type(*t.return_type) : make_unit_type();
                auto func = std::make_shared<MirType>();
                func->kind = MirFunctionType{std::move(params), std::move(ret)};
                return func;
            } else if constexpr (std::is_same_v<T, parser::InferType>) {
                // Inferred type - should be resolved by type checker
                return make_i32_type(); // Fallback
            } else if constexpr (std::is_same_v<T, parser::DynType>) {
                // Trait object - pointer to vtable
                return make_ptr_type();
            } else {
                return make_unit_type();
            }
        },
        type.kind);
}

auto MirBuilder::convert_semantic_type(const types::TypePtr& type) -> MirTypePtr {
    if (!type)
        return make_unit_type();

    return std::visit(
        [this](const auto& t) -> MirTypePtr {
            using T = std::decay_t<decltype(t)>;

            if constexpr (std::is_same_v<T, types::PrimitiveType>) {
                switch (t.kind) {
                case types::PrimitiveKind::Unit:
                    return make_unit_type();
                case types::PrimitiveKind::Bool:
                    return make_bool_type();
                case types::PrimitiveKind::I8:
                case types::PrimitiveKind::I16:
                case types::PrimitiveKind::I32:
                    return make_i32_type();
                case types::PrimitiveKind::I64:
                case types::PrimitiveKind::I128:
                    return make_i64_type();
                case types::PrimitiveKind::U8:
                case types::PrimitiveKind::U16:
                case types::PrimitiveKind::U32:
                    return make_i32_type();
                case types::PrimitiveKind::U64:
                case types::PrimitiveKind::U128:
                    return make_i64_type();
                case types::PrimitiveKind::F32:
                    return make_f32_type();
                case types::PrimitiveKind::F64:
                    return make_f64_type();
                case types::PrimitiveKind::Str:
                    return make_str_type();
                default:
                    return make_unit_type();
                }
            } else if constexpr (std::is_same_v<T, types::NamedType>) {
                std::vector<MirTypePtr> type_args;
                for (const auto& arg : t.type_args) {
                    type_args.push_back(convert_semantic_type(arg));
                }
                if (env_.lookup_enum(t.name).has_value()) {
                    return make_enum_type(t.name, std::move(type_args));
                }
                return make_struct_type(t.name, std::move(type_args));
            } else if constexpr (std::is_same_v<T, types::RefType>) {
                return make_pointer_type(convert_semantic_type(t.inner), t.is_mut);
            } else if constexpr (std::is_same_v<T, types::PtrType>) {
                return make_pointer_type(convert_semantic_type(t.inner), t.is_mut);
            } else if constexpr (std::is_same_v<T, types::ArrayType>) {
                return make_array_type(convert_semantic_type(t.element), t.size);
            } else if constexpr (std::is_same_v<T, types::SliceType>) {
                auto slice = std::make_shared<MirType>();
                slice->kind = MirSliceType{convert_semantic_type(t.element)};
                return slice;
            } else if constexpr (std::is_same_v<T, types::TupleType>) {
                std::vector<MirTypePtr> elements;
                for (const auto& elem : t.elements) {
                    elements.push_back(convert_semantic_type(elem));
                }
                return make_tuple_type(std::move(elements));
            } else if constexpr (std::is_same_v<T, types::FuncType>) {
                std::vector<MirTypePtr> params;
                for (const auto& p : t.params) {
                    params.push_back(convert_semantic_type(p));
                }
                auto ret = convert_semantic_type(t.return_type);
                auto func = std::make_shared<MirType>();
                func->kind = MirFunctionType{std::move(params), std::move(ret)};
                return func;
            } else if constexpr (std::is_same_v<T, types::GenericType>) {
                // Generic type parameter - should be instantiated
                return make_i32_type(); // Fallback
            } else {
                return make_unit_type();
            }
        },
        type->kind);
}

} // namespace tml::mir
