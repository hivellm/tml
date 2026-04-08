TML_MODULE("compiler")

#include "types/parsed_module_file.hpp"

#include "lexer/lexer.hpp"
#include "parser/parser.hpp"

#include <cstdint>
#include <cstdio>
#include <string>

namespace tml::types {

std::string get_tml_type_name(const parser::TypePtr& type) {
    if (!type) {
        return "I64"; // Default fallback
    }

    if (type->is<parser::NamedType>()) {
        const auto& named = type->as<parser::NamedType>();
        if (!named.path.segments.empty()) {
            return named.path.segments.back();
        }
    } else if (type->is<parser::TupleType>()) {
        const auto& tuple = type->as<parser::TupleType>();
        if (tuple.elements.empty()) {
            return "()";
        }
        std::string result = "(";
        for (size_t i = 0; i < tuple.elements.size(); ++i) {
            if (i > 0) {
                result += ", ";
            }
            result += get_tml_type_name(tuple.elements[i]);
        }
        result += ")";
        return result;
    }
    return "I64"; // Default for unknown types
}

std::string format_float_const(double val) {
    char buf[64];
    snprintf(buf, sizeof(buf), "%.20g", val);
    // Ensure the string contains a decimal point so LLVM parses it as float
    std::string s(buf);
    if (s.find('.') == std::string::npos && s.find('e') == std::string::npos &&
        s.find('E') == std::string::npos && s.find("inf") == std::string::npos &&
        s.find("nan") == std::string::npos) {
        s += ".0";
    }
    return s;
}

std::string try_extract_scalar_const_value(const parser::Expr* expr) {
    if (expr == nullptr) {
        return "";
    }
    if (expr->is<parser::CastExpr>()) {
        const auto& cast = expr->as<parser::CastExpr>();
        if (cast.expr && cast.expr->is<parser::LiteralExpr>()) {
            expr = cast.expr.get();
        } else if (cast.expr && cast.expr->is<parser::UnaryExpr>()) {
            const auto& unary = cast.expr->as<parser::UnaryExpr>();
            if (unary.op == parser::UnaryOp::Neg && unary.operand->is<parser::LiteralExpr>()) {
                const auto& lit = unary.operand->as<parser::LiteralExpr>();
                if (lit.token.kind == lexer::TokenKind::IntLiteral) {
                    return std::to_string(-static_cast<int64_t>(lit.token.int_value().value));
                }
                if (lit.token.kind == lexer::TokenKind::FloatLiteral) {
                    return format_float_const(-lit.token.float_value().value);
                }
            }
            return "";
        } else {
            return "";
        }
    }
    if (expr->is<parser::UnaryExpr>()) {
        const auto& unary = expr->as<parser::UnaryExpr>();
        if (unary.op == parser::UnaryOp::Neg && unary.operand->is<parser::LiteralExpr>()) {
            const auto& lit = unary.operand->as<parser::LiteralExpr>();
            if (lit.token.kind == lexer::TokenKind::IntLiteral) {
                return std::to_string(-static_cast<int64_t>(lit.token.int_value().value));
            }
            if (lit.token.kind == lexer::TokenKind::FloatLiteral) {
                return format_float_const(-lit.token.float_value().value);
            }
        }
        if (unary.operand && unary.operand->is<parser::CastExpr>()) {
            const auto& cast = unary.operand->as<parser::CastExpr>();
            if (cast.expr && cast.expr->is<parser::LiteralExpr>()) {
                const auto& lit = cast.expr->as<parser::LiteralExpr>();
                if (lit.token.kind == lexer::TokenKind::IntLiteral) {
                    return std::to_string(-static_cast<int64_t>(lit.token.int_value().value));
                }
                if (lit.token.kind == lexer::TokenKind::FloatLiteral) {
                    return format_float_const(-lit.token.float_value().value);
                }
            }
        }
        return "";
    }
    if (expr->is<parser::LiteralExpr>()) {
        const auto& lit = expr->as<parser::LiteralExpr>();
        if (lit.token.kind == lexer::TokenKind::IntLiteral)
            return std::to_string(lit.token.int_value().value);
        if (lit.token.kind == lexer::TokenKind::FloatLiteral)
            return format_float_const(lit.token.float_value().value);
        if (lit.token.kind == lexer::TokenKind::BoolLiteral)
            return lit.token.bool_value() ? "1" : "0";
        if (lit.token.kind == lexer::TokenKind::NullLiteral)
            return "null";
        if (lit.token.kind == lexer::TokenKind::CharLiteral)
            return std::to_string(static_cast<uint32_t>(lit.token.char_value().value));
        if (lit.token.kind == lexer::TokenKind::StringLiteral)
            return std::string(lit.token.string_value().value);
    }
    return "";
}

std::string try_extract_module_const_value(const parser::ConstDecl& const_decl,
                                           std::string& tml_type) {
    tml_type = get_tml_type_name(const_decl.type);

    if (!const_decl.value)
        return "";

    // Handle tuple expressions: (15, 1, 0)
    if (const_decl.value->is<parser::TupleExpr>()) {
        const auto& tuple = const_decl.value->as<parser::TupleExpr>();
        if (tuple.elements.empty())
            return "zeroinitializer";

        // Get element LLVM types from the declared type
        std::vector<std::string> elem_llvm_types;
        if (const_decl.type && const_decl.type->is<parser::TupleType>()) {
            const auto& tuple_type = const_decl.type->as<parser::TupleType>();
            for (const auto& et : tuple_type.elements) {
                std::string tml_elem = get_tml_type_name(et);
                // Map TML type name to LLVM type for the value representation
                if (tml_elem == "U8" || tml_elem == "I8")
                    elem_llvm_types.push_back("i8");
                else if (tml_elem == "U16" || tml_elem == "I16")
                    elem_llvm_types.push_back("i16");
                else if (tml_elem == "U32" || tml_elem == "I32")
                    elem_llvm_types.push_back("i32");
                else if (tml_elem == "U64" || tml_elem == "I64")
                    elem_llvm_types.push_back("i64");
                else if (tml_elem == "Bool")
                    elem_llvm_types.push_back("i1");
                else
                    elem_llvm_types.push_back("i64");
            }
        }

        std::vector<std::string> elem_values;
        for (size_t i = 0; i < tuple.elements.size(); ++i) {
            std::string val = try_extract_scalar_const_value(tuple.elements[i].get());
            if (val.empty())
                return "";
            elem_values.push_back(val);
        }

        if (elem_llvm_types.size() != elem_values.size()) {
            elem_llvm_types.clear();
            for (size_t i = 0; i < elem_values.size(); ++i)
                elem_llvm_types.push_back("i64");
        }

        // Build LLVM aggregate value: { i8 15, i8 1, i8 0 }
        std::string value = "{ ";
        for (size_t i = 0; i < elem_values.size(); ++i) {
            if (i > 0)
                value += ", ";
            value += elem_llvm_types[i] + " " + elem_values[i];
        }
        value += " }";
        return value;
    }

    // Handle scalar expressions
    return try_extract_scalar_const_value(const_decl.value.get());
}

} // namespace tml::types
