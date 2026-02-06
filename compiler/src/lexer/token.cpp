//! # Token Utilities
//!
//! This file implements token utility functions.
//!
//! ## Functions
//!
//! - `token_kind_to_string()`: Convert token kind to display string
//! - `is_keyword()`: Check if token is a keyword
//! - `is_literal()`: Check if token is a literal
//! - `is_operator()`: Check if token is an operator
//!
//! ## Token Value Accessors
//!
//! Type-safe accessors for literal values:
//! - `int_value()`: Get `IntValue` from `IntLiteral`
//! - `float_value()`: Get `FloatValue` from `FloatLiteral`
//! - `string_value()`: Get `StringValue` from `StringLiteral`
//! - `char_value()`: Get `CharValue` from `CharLiteral`
//! - `bool_value()`: Get `bool` from `BoolLiteral`

#include "lexer/token.hpp"

#include <cassert>
#include <unordered_map>

namespace tml::lexer {

auto token_kind_to_string(TokenKind kind) -> std::string_view {
    switch (kind) {
    case TokenKind::Eof:
        return "EOF";

    // Literals
    case TokenKind::IntLiteral:
        return "integer";
    case TokenKind::FloatLiteral:
        return "float";
    case TokenKind::StringLiteral:
        return "string";
    case TokenKind::CharLiteral:
        return "char";
    case TokenKind::BoolLiteral:
        return "bool";
    case TokenKind::NullLiteral:
        return "null";

    // Interpolated string tokens
    case TokenKind::InterpStringStart:
        return "interp_string_start";
    case TokenKind::InterpStringMiddle:
        return "interp_string_middle";
    case TokenKind::InterpStringEnd:
        return "interp_string_end";

    // Template literal tokens (produce Text type)
    case TokenKind::TemplateLiteralStart:
        return "template_literal_start";
    case TokenKind::TemplateLiteralMiddle:
        return "template_literal_middle";
    case TokenKind::TemplateLiteralEnd:
        return "template_literal_end";

    // Identifier
    case TokenKind::Identifier:
        return "identifier";

    // Keywords - declarations
    case TokenKind::KwFunc:
        return "func";
    case TokenKind::KwType:
        return "type";
    case TokenKind::KwUnion:
        return "union";
    case TokenKind::KwBehavior:
        return "behavior";
    case TokenKind::KwImpl:
        return "impl";
    case TokenKind::KwMod:
        return "mod";
    case TokenKind::KwUse:
        return "use";
    case TokenKind::KwPub:
        return "pub";
    case TokenKind::KwDecorator:
        return "decorator";
    case TokenKind::KwCrate:
        return "crate";
    case TokenKind::KwSuper:
        return "super";

    // Keywords - variables
    case TokenKind::KwLet:
        return "let";
    case TokenKind::KwVar:
        return "var";
    case TokenKind::KwConst:
        return "const";

    // Keywords - control flow
    case TokenKind::KwIf:
        return "if";
    case TokenKind::KwThen:
        return "then";
    case TokenKind::KwElse:
        return "else";
    case TokenKind::KwWhen:
        return "when";
    case TokenKind::KwLoop:
        return "loop";
    case TokenKind::KwWhile:
        return "while";
    case TokenKind::KwFor:
        return "for";
    case TokenKind::KwIn:
        return "in";
    case TokenKind::KwTo:
        return "to";
    case TokenKind::KwThrough:
        return "through";
    case TokenKind::KwBreak:
        return "break";
    case TokenKind::KwContinue:
        return "continue";
    case TokenKind::KwReturn:
        return "return";

    // Keywords - logical operators
    case TokenKind::KwAnd:
        return "and";
    case TokenKind::KwOr:
        return "or";
    case TokenKind::KwNot:
        return "not";
    case TokenKind::AndAnd:
        return "&&";
    case TokenKind::OrOr:
        return "||";

    // Keywords - bitwise operators
    case TokenKind::KwXor:
        return "xor";
    case TokenKind::KwShl:
        return "shl";
    case TokenKind::KwShr:
        return "shr";

    // Keywords - types
    case TokenKind::KwThis:
        return "this";
    case TokenKind::KwThisType:
        return "This";
    case TokenKind::KwAs:
        return "as";
    case TokenKind::KwIs:
        return "is";

    // Keywords - memory
    case TokenKind::KwMut:
        return "mut";
    case TokenKind::KwRef:
        return "ref";
    case TokenKind::KwLife:
        return "life";
    case TokenKind::KwVolatile:
        return "volatile";

    // Keywords - closures
    case TokenKind::KwDo:
        return "do";
    case TokenKind::KwMove:
        return "move";

    // Keywords - other
    case TokenKind::KwAsync:
        return "async";
    case TokenKind::KwAwait:
        return "await";
    case TokenKind::KwWith:
        return "with";
    case TokenKind::KwLowlevel:
        return "lowlevel";
    case TokenKind::KwQuote:
        return "quote";
    case TokenKind::KwWhere:
        return "where";
    case TokenKind::KwDyn:
        return "dyn";

    // Keywords - OOP (C#-style)
    case TokenKind::KwClass:
        return "class";
    case TokenKind::KwInterface:
        return "interface";
    case TokenKind::KwExtends:
        return "extends";
    case TokenKind::KwImplements:
        return "implements";
    case TokenKind::KwOverride:
        return "override";
    case TokenKind::KwVirtual:
        return "virtual";
    case TokenKind::KwAbstract:
        return "abstract";
    case TokenKind::KwSealed:
        return "sealed";
    case TokenKind::KwNamespace:
        return "namespace";
    case TokenKind::KwBase:
        return "base";
    case TokenKind::KwProtected:
        return "protected";
    case TokenKind::KwPrivate:
        return "private";
    case TokenKind::KwStatic:
        return "static";
    case TokenKind::KwNew:
        return "new";
    case TokenKind::KwProp:
        return "prop";
    case TokenKind::KwThrow:
        return "throw";

    // Operators - arithmetic
    case TokenKind::Plus:
        return "+";
    case TokenKind::Minus:
        return "-";
    case TokenKind::Star:
        return "*";
    case TokenKind::Slash:
        return "/";
    case TokenKind::Percent:
        return "%";
    case TokenKind::StarStar:
        return "**";
    case TokenKind::PlusPlus:
        return "++";
    case TokenKind::MinusMinus:
        return "--";

    // Operators - comparison
    case TokenKind::Eq:
        return "==";
    case TokenKind::Ne:
        return "!=";
    case TokenKind::Lt:
        return "<";
    case TokenKind::Gt:
        return ">";
    case TokenKind::Le:
        return "<=";
    case TokenKind::Ge:
        return ">=";

    // Operators - bitwise
    case TokenKind::BitAnd:
        return "&";
    case TokenKind::BitOr:
        return "|";
    case TokenKind::BitXor:
        return "^";
    case TokenKind::BitNot:
        return "~";
    case TokenKind::Shl:
        return "<<";
    case TokenKind::Shr:
        return ">>";

    // Operators - assignment
    case TokenKind::Assign:
        return "=";
    case TokenKind::PlusAssign:
        return "+=";
    case TokenKind::MinusAssign:
        return "-=";
    case TokenKind::StarAssign:
        return "*=";
    case TokenKind::SlashAssign:
        return "/=";
    case TokenKind::PercentAssign:
        return "%=";
    case TokenKind::BitAndAssign:
        return "&=";
    case TokenKind::BitOrAssign:
        return "|=";
    case TokenKind::BitXorAssign:
        return "^=";
    case TokenKind::ShlAssign:
        return "<<=";
    case TokenKind::ShrAssign:
        return ">>=";

    // Operators - other
    case TokenKind::Arrow:
        return "->";
    case TokenKind::FatArrow:
        return "=>";
    case TokenKind::Dot:
        return ".";
    case TokenKind::DotDot:
        return "..";
    case TokenKind::Colon:
        return ":";
    case TokenKind::ColonColon:
        return "::";
    case TokenKind::Question:
        return "?";
    case TokenKind::Bang:
        return "!";
    case TokenKind::At:
        return "@";
    case TokenKind::Pipe:
        return "|";
    case TokenKind::Dollar:
        return "$";
    case TokenKind::DollarBrace:
        return "${";

    // Delimiters
    case TokenKind::LParen:
        return "(";
    case TokenKind::RParen:
        return ")";
    case TokenKind::LBracket:
        return "[";
    case TokenKind::RBracket:
        return "]";
    case TokenKind::LBrace:
        return "{";
    case TokenKind::RBrace:
        return "}";
    case TokenKind::Comma:
        return ",";
    case TokenKind::Semi:
        return ";";

    // Special
    case TokenKind::Newline:
        return "newline";
    case TokenKind::Error:
        return "error";

    // Documentation comments
    case TokenKind::DocComment:
        return "doc_comment";
    case TokenKind::ModuleDocComment:
        return "module_doc_comment";
    }
    return "unknown";
}

auto is_keyword(TokenKind kind) -> bool {
    return kind >= TokenKind::KwFunc && kind <= TokenKind::KwProp;
}

auto is_literal(TokenKind kind) -> bool {
    return (kind >= TokenKind::IntLiteral && kind <= TokenKind::NullLiteral) ||
           (kind >= TokenKind::InterpStringStart && kind <= TokenKind::InterpStringEnd) ||
           (kind >= TokenKind::TemplateLiteralStart && kind <= TokenKind::TemplateLiteralEnd);
}

auto is_operator(TokenKind kind) -> bool {
    return kind >= TokenKind::Plus && kind <= TokenKind::DollarBrace;
}

auto Token::int_value() const -> const IntValue& {
    assert(kind == TokenKind::IntLiteral);
    return std::get<IntValue>(value);
}

auto Token::float_value() const -> const FloatValue& {
    assert(kind == TokenKind::FloatLiteral);
    return std::get<FloatValue>(value);
}

auto Token::string_value() const -> const StringValue& {
    assert(kind == TokenKind::StringLiteral);
    return std::get<StringValue>(value);
}

auto Token::char_value() const -> const CharValue& {
    assert(kind == TokenKind::CharLiteral);
    return std::get<CharValue>(value);
}

auto Token::bool_value() const -> bool {
    assert(kind == TokenKind::BoolLiteral);
    return std::get<bool>(value);
}

auto Token::doc_value() const -> const DocValue& {
    assert(kind == TokenKind::DocComment || kind == TokenKind::ModuleDocComment);
    return std::get<DocValue>(value);
}

} // namespace tml::lexer
