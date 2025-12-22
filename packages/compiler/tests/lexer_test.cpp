#include "tml/lexer/lexer.hpp"
#include "tml/lexer/source.hpp"
#include <gtest/gtest.h>
#include <memory>

using namespace tml;
using namespace tml::lexer;

class LexerTest : public ::testing::Test {
protected:
    // Keep source alive so Token.lexeme (string_view) remains valid
    std::unique_ptr<Source> source_;

    auto lex(const std::string& code) -> std::vector<Token> {
        source_ = std::make_unique<Source>(Source::from_string(code));
        Lexer lexer(*source_);
        return lexer.tokenize();
    }

    auto lex_one(const std::string& code) -> Token {
        auto tokens = lex(code);
        EXPECT_GE(tokens.size(), 1);
        return tokens[0];
    }
};

// Keywords
TEST_F(LexerTest, Keywords) {
    EXPECT_EQ(lex_one("func").kind, TokenKind::KwFunc);
    EXPECT_EQ(lex_one("type").kind, TokenKind::KwType);
    EXPECT_EQ(lex_one("behavior").kind, TokenKind::KwBehavior);
    EXPECT_EQ(lex_one("impl").kind, TokenKind::KwImpl);
    EXPECT_EQ(lex_one("let").kind, TokenKind::KwLet);
    EXPECT_EQ(lex_one("if").kind, TokenKind::KwIf);
    EXPECT_EQ(lex_one("then").kind, TokenKind::KwThen);
    EXPECT_EQ(lex_one("else").kind, TokenKind::KwElse);
    EXPECT_EQ(lex_one("when").kind, TokenKind::KwWhen);
    EXPECT_EQ(lex_one("loop").kind, TokenKind::KwLoop);
    EXPECT_EQ(lex_one("for").kind, TokenKind::KwFor);
    EXPECT_EQ(lex_one("return").kind, TokenKind::KwReturn);
    EXPECT_EQ(lex_one("mut").kind, TokenKind::KwMut);
    EXPECT_EQ(lex_one("pub").kind, TokenKind::KwPub);
    EXPECT_EQ(lex_one("do").kind, TokenKind::KwDo);
    EXPECT_EQ(lex_one("this").kind, TokenKind::KwThis);
    EXPECT_EQ(lex_one("This").kind, TokenKind::KwThisType);
    EXPECT_EQ(lex_one("to").kind, TokenKind::KwTo);
    EXPECT_EQ(lex_one("through").kind, TokenKind::KwThrough);
    EXPECT_EQ(lex_one("lowlevel").kind, TokenKind::KwLowlevel);
}

// Identifiers
TEST_F(LexerTest, Identifiers) {
    auto token = lex_one("foo");
    EXPECT_EQ(token.kind, TokenKind::Identifier);
    EXPECT_EQ(token.lexeme, "foo");
}

TEST_F(LexerTest, IdentifierWithUnderscore) {
    auto token = lex_one("_foo_bar");
    EXPECT_EQ(token.kind, TokenKind::Identifier);
    EXPECT_EQ(token.lexeme, "_foo_bar");
}

TEST_F(LexerTest, IdentifierWithNumbers) {
    auto token = lex_one("foo123");
    EXPECT_EQ(token.kind, TokenKind::Identifier);
    EXPECT_EQ(token.lexeme, "foo123");
}

// Integer literals
TEST_F(LexerTest, DecimalInteger) {
    auto token = lex_one("42");
    EXPECT_EQ(token.kind, TokenKind::IntLiteral);
    EXPECT_EQ(token.int_value().value, 42);
    EXPECT_EQ(token.int_value().base, 10);
}

TEST_F(LexerTest, IntegerWithUnderscores) {
    auto token = lex_one("1_000_000");
    EXPECT_EQ(token.kind, TokenKind::IntLiteral);
    EXPECT_EQ(token.int_value().value, 1000000);
}

TEST_F(LexerTest, HexInteger) {
    auto token = lex_one("0xFF");
    EXPECT_EQ(token.kind, TokenKind::IntLiteral);
    EXPECT_EQ(token.int_value().value, 255);
    EXPECT_EQ(token.int_value().base, 16);
}

TEST_F(LexerTest, BinaryInteger) {
    auto token = lex_one("0b1010");
    EXPECT_EQ(token.kind, TokenKind::IntLiteral);
    EXPECT_EQ(token.int_value().value, 10);
    EXPECT_EQ(token.int_value().base, 2);
}

TEST_F(LexerTest, OctalInteger) {
    auto token = lex_one("0o755");
    EXPECT_EQ(token.kind, TokenKind::IntLiteral);
    EXPECT_EQ(token.int_value().value, 493);
    EXPECT_EQ(token.int_value().base, 8);
}

// Float literals
TEST_F(LexerTest, SimpleFloat) {
    auto token = lex_one("3.14");
    EXPECT_EQ(token.kind, TokenKind::FloatLiteral);
    EXPECT_DOUBLE_EQ(token.float_value().value, 3.14);
}

TEST_F(LexerTest, FloatWithExponent) {
    auto token = lex_one("1e10");
    EXPECT_EQ(token.kind, TokenKind::FloatLiteral);
    EXPECT_DOUBLE_EQ(token.float_value().value, 1e10);
}

TEST_F(LexerTest, FloatWithNegativeExponent) {
    auto token = lex_one("2.5e-3");
    EXPECT_EQ(token.kind, TokenKind::FloatLiteral);
    EXPECT_DOUBLE_EQ(token.float_value().value, 2.5e-3);
}

// String literals
TEST_F(LexerTest, SimpleString) {
    auto token = lex_one("\"hello\"");
    EXPECT_EQ(token.kind, TokenKind::StringLiteral);
    EXPECT_EQ(token.string_value().value, "hello");
    EXPECT_FALSE(token.string_value().is_raw);
}

TEST_F(LexerTest, StringWithEscapes) {
    auto token = lex_one("\"line\\nbreak\"");
    EXPECT_EQ(token.kind, TokenKind::StringLiteral);
    EXPECT_EQ(token.string_value().value, "line\nbreak");
}

TEST_F(LexerTest, StringWithUnicodeEscape) {
    auto token = lex_one("\"smile: \\u{1F600}\"");
    EXPECT_EQ(token.kind, TokenKind::StringLiteral);
    // The emoji should be encoded as UTF-8
    EXPECT_FALSE(token.string_value().value.empty());
}

TEST_F(LexerTest, RawString) {
    auto token = lex_one("r\"no\\escapes\"");
    EXPECT_EQ(token.kind, TokenKind::StringLiteral);
    EXPECT_EQ(token.string_value().value, "no\\escapes");
    EXPECT_TRUE(token.string_value().is_raw);
}

// Char literals
TEST_F(LexerTest, SimpleChar) {
    auto token = lex_one("'a'");
    EXPECT_EQ(token.kind, TokenKind::CharLiteral);
    EXPECT_EQ(token.char_value().value, 'a');
}

TEST_F(LexerTest, CharWithEscape) {
    auto token = lex_one("'\\n'");
    EXPECT_EQ(token.kind, TokenKind::CharLiteral);
    EXPECT_EQ(token.char_value().value, '\n');
}

// Bool literals
TEST_F(LexerTest, BoolTrue) {
    auto token = lex_one("true");
    EXPECT_EQ(token.kind, TokenKind::BoolLiteral);
    EXPECT_TRUE(token.bool_value());
}

TEST_F(LexerTest, BoolFalse) {
    auto token = lex_one("false");
    EXPECT_EQ(token.kind, TokenKind::BoolLiteral);
    EXPECT_FALSE(token.bool_value());
}

// Operators
TEST_F(LexerTest, ArithmeticOperators) {
    EXPECT_EQ(lex_one("+").kind, TokenKind::Plus);
    EXPECT_EQ(lex_one("-").kind, TokenKind::Minus);
    EXPECT_EQ(lex_one("*").kind, TokenKind::Star);
    EXPECT_EQ(lex_one("/").kind, TokenKind::Slash);
    EXPECT_EQ(lex_one("%").kind, TokenKind::Percent);
}

TEST_F(LexerTest, ComparisonOperators) {
    EXPECT_EQ(lex_one("==").kind, TokenKind::Eq);
    EXPECT_EQ(lex_one("!=").kind, TokenKind::Ne);
    EXPECT_EQ(lex_one("<").kind, TokenKind::Lt);
    EXPECT_EQ(lex_one(">").kind, TokenKind::Gt);
    EXPECT_EQ(lex_one("<=").kind, TokenKind::Le);
    EXPECT_EQ(lex_one(">=").kind, TokenKind::Ge);
}

TEST_F(LexerTest, LogicalOperators) {
    // TML uses keyword operators instead of symbols
    EXPECT_EQ(lex_one("and").kind, TokenKind::KwAnd);
    EXPECT_EQ(lex_one("or").kind, TokenKind::KwOr);
    EXPECT_EQ(lex_one("not").kind, TokenKind::KwNot);
}

TEST_F(LexerTest, BitwiseOperators) {
    EXPECT_EQ(lex_one("&").kind, TokenKind::BitAnd);
    EXPECT_EQ(lex_one("|").kind, TokenKind::BitOr);
    EXPECT_EQ(lex_one("^").kind, TokenKind::BitXor);
    EXPECT_EQ(lex_one("~").kind, TokenKind::BitNot);
    EXPECT_EQ(lex_one("<<").kind, TokenKind::Shl);
    EXPECT_EQ(lex_one(">>").kind, TokenKind::Shr);
}

TEST_F(LexerTest, AssignmentOperators) {
    EXPECT_EQ(lex_one("=").kind, TokenKind::Assign);
    EXPECT_EQ(lex_one("+=").kind, TokenKind::PlusAssign);
    EXPECT_EQ(lex_one("-=").kind, TokenKind::MinusAssign);
    EXPECT_EQ(lex_one("*=").kind, TokenKind::StarAssign);
    EXPECT_EQ(lex_one("/=").kind, TokenKind::SlashAssign);
}

TEST_F(LexerTest, OtherOperators) {
    EXPECT_EQ(lex_one("->").kind, TokenKind::Arrow);
    EXPECT_EQ(lex_one("=>").kind, TokenKind::FatArrow);
    EXPECT_EQ(lex_one(".").kind, TokenKind::Dot);
    EXPECT_EQ(lex_one("..").kind, TokenKind::DotDot);
    EXPECT_EQ(lex_one(":").kind, TokenKind::Colon);
    EXPECT_EQ(lex_one("::").kind, TokenKind::ColonColon);
    EXPECT_EQ(lex_one("!").kind, TokenKind::Bang);
    EXPECT_EQ(lex_one("**").kind, TokenKind::StarStar);
    EXPECT_EQ(lex_one("$").kind, TokenKind::Dollar);
    EXPECT_EQ(lex_one("${").kind, TokenKind::DollarBrace);
}

// Delimiters
TEST_F(LexerTest, Delimiters) {
    EXPECT_EQ(lex_one("(").kind, TokenKind::LParen);
    EXPECT_EQ(lex_one(")").kind, TokenKind::RParen);
    EXPECT_EQ(lex_one("[").kind, TokenKind::LBracket);
    EXPECT_EQ(lex_one("]").kind, TokenKind::RBracket);
    EXPECT_EQ(lex_one("{").kind, TokenKind::LBrace);
    EXPECT_EQ(lex_one("}").kind, TokenKind::RBrace);
    EXPECT_EQ(lex_one(",").kind, TokenKind::Comma);
    EXPECT_EQ(lex_one(";").kind, TokenKind::Semi);
}

// Comments
TEST_F(LexerTest, LineComment) {
    auto tokens = lex("foo // comment\nbar");
    EXPECT_EQ(tokens[0].kind, TokenKind::Identifier);
    EXPECT_EQ(tokens[0].lexeme, "foo");
    EXPECT_EQ(tokens[1].kind, TokenKind::Newline);
    EXPECT_EQ(tokens[2].kind, TokenKind::Identifier);
    EXPECT_EQ(tokens[2].lexeme, "bar");
}

TEST_F(LexerTest, BlockComment) {
    auto tokens = lex("foo /* comment */ bar");
    EXPECT_EQ(tokens[0].kind, TokenKind::Identifier);
    EXPECT_EQ(tokens[0].lexeme, "foo");
    EXPECT_EQ(tokens[1].kind, TokenKind::Identifier);
    EXPECT_EQ(tokens[1].lexeme, "bar");
}

TEST_F(LexerTest, NestedBlockComment) {
    auto tokens = lex("foo /* outer /* inner */ outer */ bar");
    EXPECT_EQ(tokens[0].kind, TokenKind::Identifier);
    EXPECT_EQ(tokens[0].lexeme, "foo");
    EXPECT_EQ(tokens[1].kind, TokenKind::Identifier);
    EXPECT_EQ(tokens[1].lexeme, "bar");
}

// Complete expressions
TEST_F(LexerTest, FunctionDeclaration) {
    auto tokens = lex("func add(a: I32, b: I32) -> I32 { return a + b }");

    EXPECT_EQ(tokens[0].kind, TokenKind::KwFunc);
    EXPECT_EQ(tokens[1].kind, TokenKind::Identifier);
    EXPECT_EQ(tokens[1].lexeme, "add");
    EXPECT_EQ(tokens[2].kind, TokenKind::LParen);
    EXPECT_EQ(tokens[3].kind, TokenKind::Identifier);
    EXPECT_EQ(tokens[3].lexeme, "a");
    EXPECT_EQ(tokens[4].kind, TokenKind::Colon);
    EXPECT_EQ(tokens[5].kind, TokenKind::Identifier);
    EXPECT_EQ(tokens[5].lexeme, "I32");
}

TEST_F(LexerTest, VariableDeclaration) {
    auto tokens = lex("let x: I32 = 42");

    EXPECT_EQ(tokens[0].kind, TokenKind::KwLet);
    EXPECT_EQ(tokens[1].kind, TokenKind::Identifier);
    EXPECT_EQ(tokens[2].kind, TokenKind::Colon);
    EXPECT_EQ(tokens[3].kind, TokenKind::Identifier);
    EXPECT_EQ(tokens[4].kind, TokenKind::Assign);
    EXPECT_EQ(tokens[5].kind, TokenKind::IntLiteral);
}

TEST_F(LexerTest, GenericType) {
    auto tokens = lex("Vec[I32]");

    EXPECT_EQ(tokens[0].kind, TokenKind::Identifier);
    EXPECT_EQ(tokens[0].lexeme, "Vec");
    EXPECT_EQ(tokens[1].kind, TokenKind::LBracket);
    EXPECT_EQ(tokens[2].kind, TokenKind::Identifier);
    EXPECT_EQ(tokens[2].lexeme, "I32");
    EXPECT_EQ(tokens[3].kind, TokenKind::RBracket);
}

// Source locations
TEST_F(LexerTest, SourceLocation) {
    auto source = Source::from_string("func foo");
    Lexer lexer(source);

    auto func_token = lexer.next_token();
    EXPECT_EQ(func_token.span.start.line, 1);
    EXPECT_EQ(func_token.span.start.column, 1);

    auto foo_token = lexer.next_token();
    EXPECT_EQ(foo_token.span.start.line, 1);
    EXPECT_EQ(foo_token.span.start.column, 6);
}

TEST_F(LexerTest, MultilineSourceLocation) {
    auto source = Source::from_string("foo\nbar\nbaz");
    Lexer lexer(source);

    auto foo = lexer.next_token();
    EXPECT_EQ(foo.span.start.line, 1);

    (void)lexer.next_token(); // newline

    auto bar = lexer.next_token();
    EXPECT_EQ(bar.span.start.line, 2);

    (void)lexer.next_token(); // newline

    auto baz = lexer.next_token();
    EXPECT_EQ(baz.span.start.line, 3);
}

// Error handling
TEST_F(LexerTest, UnterminatedString) {
    auto source = Source::from_string("\"hello");
    Lexer lexer(source);
    auto token = lexer.next_token();

    EXPECT_EQ(token.kind, TokenKind::Error);
    EXPECT_TRUE(lexer.has_errors());
}

TEST_F(LexerTest, InvalidCharacter) {
    // Use a character that's actually invalid in TML
    auto source = Source::from_string("");
    Lexer lexer(source);
    auto token = lexer.next_token();

    EXPECT_EQ(token.kind, TokenKind::Error);
}

TEST_F(LexerTest, EmptyCharLiteral) {
    auto source = Source::from_string("''");
    Lexer lexer(source);
    auto token = lexer.next_token();

    EXPECT_EQ(token.kind, TokenKind::Error);
}
