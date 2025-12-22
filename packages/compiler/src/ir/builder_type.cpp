#include "tml/ir/ir.hpp"

namespace tml::ir {

auto IRBuilder::build_type_expr(const parser::Type& type) -> IRTypeExpr {
    return std::visit([this](const auto& t) -> IRTypeExpr {
        using T = std::decay_t<decltype(t)>;

        if constexpr (std::is_same_v<T, parser::NamedType>) {
            IRTypeRef ref;
            for (const auto& seg : t.path.segments) {
                if (ref.name.empty()) {
                    ref.name = seg;
                } else {
                    ref.name += "::" + seg;
                }
            }
            if (t.generics) {
                for (const auto& arg : t.generics->args) {
                    auto inner = build_type_expr(*arg);
                    ref.type_args.push_back(make_box<IRTypeRef>(
                        std::move(std::get<IRTypeRef>(inner.kind))));
                }
            }
            return IRTypeExpr{std::move(ref)};
        }
        else if constexpr (std::is_same_v<T, parser::RefType>) {
            IRRefType ref;
            ref.is_mut = t.is_mut;
            auto inner = build_type_expr(*t.inner);
            ref.inner = make_box<IRTypeRef>(std::move(std::get<IRTypeRef>(inner.kind)));
            return IRTypeExpr{std::move(ref)};
        }
        else if constexpr (std::is_same_v<T, parser::SliceType>) {
            IRSliceType slice;
            auto inner = build_type_expr(*t.element);
            slice.element = make_box<IRTypeRef>(std::move(std::get<IRTypeRef>(inner.kind)));
            return IRTypeExpr{std::move(slice)};
        }
        else if constexpr (std::is_same_v<T, parser::ArrayType>) {
            IRArrayType arr;
            auto inner = build_type_expr(*t.element);
            arr.element = make_box<IRTypeRef>(std::move(std::get<IRTypeRef>(inner.kind)));
            // Try to extract literal size from the expression
            if (t.size && t.size->is<parser::LiteralExpr>()) {
                const auto& lit = t.size->as<parser::LiteralExpr>();
                if (std::holds_alternative<lexer::IntValue>(lit.token.value)) {
                    arr.size = static_cast<size_t>(std::get<lexer::IntValue>(lit.token.value).value);
                } else {
                    arr.size = 0; // Unknown size
                }
            } else {
                arr.size = 0; // Dynamic or unknown size
            }
            return IRTypeExpr{std::move(arr)};
        }
        else if constexpr (std::is_same_v<T, parser::TupleType>) {
            IRTupleType tuple;
            for (const auto& elem : t.elements) {
                auto inner = build_type_expr(*elem);
                tuple.elements.push_back(make_box<IRTypeRef>(
                    std::move(std::get<IRTypeRef>(inner.kind))));
            }
            return IRTypeExpr{std::move(tuple)};
        }
        else if constexpr (std::is_same_v<T, parser::FuncType>) {
            IRFuncType func;
            for (const auto& param : t.params) {
                auto inner = build_type_expr(*param);
                func.params.push_back(make_box<IRTypeRef>(
                    std::move(std::get<IRTypeRef>(inner.kind))));
            }
            auto ret = build_type_expr(*t.return_type);
            func.ret = make_box<IRTypeRef>(std::move(std::get<IRTypeRef>(ret.kind)));
            return IRTypeExpr{std::move(func)};
        }
        else {
            // Default: unit type
            return IRTypeExpr{IRTypeRef{"Unit", {}}};
        }
    }, type.kind);
}

} // namespace tml::ir

