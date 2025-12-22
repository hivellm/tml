#include "tml/ir/ir.hpp"

namespace tml::ir {

auto IRBuilder::build_block(const parser::BlockExpr& block) -> IRBlock {
    IRBlock ir_block;
    for (const auto& stmt : block.stmts) {
        ir_block.stmts.push_back(build_stmt(*stmt));
    }
    if (block.expr) {
        ir_block.expr = build_expr(**block.expr);
    }
    return ir_block;
}

auto IRBuilder::build_stmt(const parser::Stmt& stmt) -> IRStmtPtr {
    return std::visit([this](const auto& s) -> IRStmtPtr {
        using T = std::decay_t<decltype(s)>;

        if constexpr (std::is_same_v<T, parser::LetStmt>) {
            IRLet let;
            let.pattern = build_pattern(*s.pattern);
            if (s.type_annotation) {
                let.type_annotation = build_type_expr(**s.type_annotation);
            }
            if (s.init) {
                let.init = build_expr(**s.init);
            } else {
                // No initializer - use unit
                let.init = make_box<IRExpr>(IRExpr{IRLiteral{"()", "Unit"}});
            }
            return make_box<IRStmt>(IRStmt{std::move(let)});
        }
        else if constexpr (std::is_same_v<T, parser::VarStmt>) {
            IRVarMut var;
            var.name = s.name;
            if (s.type_annotation) {
                var.type_annotation = build_type_expr(**s.type_annotation);
            }
            var.init = build_expr(*s.init);
            return make_box<IRStmt>(IRStmt{std::move(var)});
        }
        else if constexpr (std::is_same_v<T, parser::ExprStmt>) {
            // Check if this is an assignment expression
            if (s.expr->is<parser::BinaryExpr>()) {
                const auto& bin = s.expr->as<parser::BinaryExpr>();
                if (bin.op == parser::BinaryOp::Assign) {
                    IRAssign assign;
                    assign.target = build_expr(*bin.left);
                    assign.value = build_expr(*bin.right);
                    return make_box<IRStmt>(IRStmt{std::move(assign)});
                }
            }
            IRExprStmt expr_stmt;
            expr_stmt.expr = build_expr(*s.expr);
            return make_box<IRStmt>(IRStmt{std::move(expr_stmt)});
        }
        else if constexpr (std::is_same_v<T, parser::DeclPtr>) {
            // Nested declaration - convert to expression statement
            IRExprStmt expr_stmt;
            expr_stmt.expr = make_box<IRExpr>(IRExpr{IRLiteral{"()", "Unit"}});
            return make_box<IRStmt>(IRStmt{std::move(expr_stmt)});
        }
        else {
            // Default: empty expression statement
            IRExprStmt expr_stmt;
            expr_stmt.expr = make_box<IRExpr>(IRExpr{IRLiteral{"()", "Unit"}});
            return make_box<IRStmt>(IRStmt{std::move(expr_stmt)});
        }
    }, stmt.kind);
}

auto IRBuilder::build_pattern(const parser::Pattern& pattern) -> IRPatternPtr {
    return std::visit([this](const auto& p) -> IRPatternPtr {
        using T = std::decay_t<decltype(p)>;

        if constexpr (std::is_same_v<T, parser::LiteralPattern>) {
            IRPatternLit lit;
            std::visit([&lit](const auto& val) {
                using V = std::decay_t<decltype(val)>;
                if constexpr (std::is_same_v<V, std::monostate>) {
                    lit.value = "()";
                    lit.type_name = "Unit";
                } else if constexpr (std::is_same_v<V, lexer::IntValue>) {
                    lit.value = std::to_string(val.value);
                    lit.type_name = "I64";
                } else if constexpr (std::is_same_v<V, lexer::FloatValue>) {
                    lit.value = std::to_string(val.value);
                    lit.type_name = "F64";
                } else if constexpr (std::is_same_v<V, lexer::StringValue>) {
                    lit.value = "\"" + val.value + "\"";
                    lit.type_name = "String";
                } else if constexpr (std::is_same_v<V, lexer::CharValue>) {
                    lit.value = std::string("'") + static_cast<char>(val.value) + "'";
                    lit.type_name = "Char";
                } else if constexpr (std::is_same_v<V, bool>) {
                    lit.value = val ? "true" : "false";
                    lit.type_name = "Bool";
                }
            }, p.literal.value);
            return make_box<IRPattern>(IRPattern{std::move(lit)});
        }
        else if constexpr (std::is_same_v<T, parser::IdentPattern>) {
            IRPatternBind bind;
            bind.name = p.name;
            bind.is_mut = p.is_mut;
            return make_box<IRPattern>(IRPattern{std::move(bind)});
        }
        else if constexpr (std::is_same_v<T, parser::WildcardPattern>) {
            return make_box<IRPattern>(IRPattern{IRPatternWild{}});
        }
        else if constexpr (std::is_same_v<T, parser::TuplePattern>) {
            IRPatternTuple tuple;
            for (const auto& elem : p.elements) {
                tuple.elements.push_back(build_pattern(*elem));
            }
            return make_box<IRPattern>(IRPattern{std::move(tuple)});
        }
        else if constexpr (std::is_same_v<T, parser::StructPattern>) {
            IRPatternStruct struct_pat;
            if (!p.path.segments.empty()) {
                struct_pat.type_name = p.path.segments.back();
            }
            for (const auto& [name, pat] : p.fields) {
                struct_pat.fields.push_back({name, build_pattern(*pat)});
            }
            return make_box<IRPattern>(IRPattern{std::move(struct_pat)});
        }
        else if constexpr (std::is_same_v<T, parser::EnumPattern>) {
            IRPatternVariant variant;
            if (!p.path.segments.empty()) {
                variant.variant_name = p.path.segments.back();
            }
            if (p.payload) {
                for (const auto& field : *p.payload) {
                    variant.fields.push_back(build_pattern(*field));
                }
            }
            return make_box<IRPattern>(IRPattern{std::move(variant)});
        }
        else {
            // Default: wildcard
            return make_box<IRPattern>(IRPattern{IRPatternWild{}});
        }
    }, pattern.kind);
}


} // namespace tml::ir
