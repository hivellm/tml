#include "lexer/lexer.hpp"
#include "lexer/source.hpp"

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
    EXPECT_EQ(lex_one("enum").kind, TokenKind::KwType); // 'enum' is alias for 'type'
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
    EXPECT_EQ(lex_one("unsafe").kind, TokenKind::KwLowlevel); // 'unsafe' is alias for 'lowlevel'
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

// ============================================================================
// Integration Tests - Complete TML Programs
// ============================================================================

TEST_F(LexerTest, IntegrationCompleteFunction) {
    // Test lexing a complete function with multiple statements
    std::string code = R"(
func fibonacci(n: I64) -> I64 {
    if n <= 1 {
        return n
    }
    return fibonacci(n - 1) + fibonacci(n - 2)
}
)";
    auto tokens = lex(code);
    EXPECT_FALSE(tokens.empty());

    // Verify no error tokens
    for (const auto& token : tokens) {
        EXPECT_NE(token.kind, TokenKind::Error)
            << "Unexpected error token at line " << token.span.start.line;
    }

    // Verify key tokens are present
    bool has_func = false, has_if = false, has_return = false;
    for (const auto& token : tokens) {
        if (token.kind == TokenKind::KwFunc)
            has_func = true;
        if (token.kind == TokenKind::KwIf)
            has_if = true;
        if (token.kind == TokenKind::KwReturn)
            has_return = true;
    }
    EXPECT_TRUE(has_func);
    EXPECT_TRUE(has_if);
    EXPECT_TRUE(has_return);
}

TEST_F(LexerTest, IntegrationStructAndImpl) {
    // Test lexing a struct definition with impl block
    std::string code = R"(
type Point {
    x: F64,
    y: F64
}

impl Point {
    func new(x: F64, y: F64) -> Point {
        return Point { x: x, y: y }
    }

    func distance(self, other: Point) -> F64 {
        let dx: F64 = self.x - other.x
        let dy: F64 = self.y - other.y
        return sqrt(dx * dx + dy * dy)
    }
}
)";
    auto tokens = lex(code);
    EXPECT_FALSE(tokens.empty());

    // Verify no error tokens
    for (const auto& token : tokens) {
        EXPECT_NE(token.kind, TokenKind::Error) << "Unexpected error token: " << token.lexeme;
    }

    // Verify struct and impl keywords
    bool has_type = false, has_impl = false;
    for (const auto& token : tokens) {
        if (token.kind == TokenKind::KwType)
            has_type = true;
        if (token.kind == TokenKind::KwImpl)
            has_impl = true;
    }
    EXPECT_TRUE(has_type);
    EXPECT_TRUE(has_impl);
}

TEST_F(LexerTest, IntegrationTypeAndWhen) {
    // Test lexing a type (enum-style) and when expression
    // TML uses 'type' keyword for both structs and enums
    std::string code = R"(
type Maybe[T] {
    Just(T),
    Nothing
}

func unwrap_or[T](opt: Maybe[T], dflt: T) -> T {
    when (opt) {
        Just(value) => value,
        Nothing => dflt
    }
}
)";
    auto tokens = lex(code);
    EXPECT_FALSE(tokens.empty());

    // Verify no error tokens
    for (const auto& token : tokens) {
        EXPECT_NE(token.kind, TokenKind::Error) << "Unexpected error token: " << token.lexeme;
    }

    // Verify type and when keywords
    bool has_type = false, has_when = false;
    for (const auto& token : tokens) {
        if (token.kind == TokenKind::KwType)
            has_type = true;
        if (token.kind == TokenKind::KwWhen)
            has_when = true;
    }
    EXPECT_TRUE(has_type);
    EXPECT_TRUE(has_when);
}

TEST_F(LexerTest, IntegrationBehaviorAndImpl) {
    // Test lexing a behavior definition with implementation
    std::string code = R"TML(
behavior Printable {
    func to_string(self) -> Str
}

type Counter {
    value: I64
}

impl Printable for Counter {
    func to_string(self) -> Str {
        return format("Counter: {}", self.value)
    }
}
)TML";
    auto tokens = lex(code);
    EXPECT_FALSE(tokens.empty());

    // Verify no error tokens
    for (const auto& token : tokens) {
        EXPECT_NE(token.kind, TokenKind::Error) << "Unexpected error token: " << token.lexeme;
    }

    // Verify behavior keyword
    bool has_behavior = false;
    for (const auto& token : tokens) {
        if (token.kind == TokenKind::KwBehavior)
            has_behavior = true;
    }
    EXPECT_TRUE(has_behavior);
}

TEST_F(LexerTest, IntegrationLoopsAndControl) {
    // Test lexing various loop constructs
    std::string code = R"(
func loops_example() {
    // For loop with range
    for i in 0 to 10 {
        print(i)
    }

    // While-style loop
    var x: I64 = 0
    loop (x < 10) {
        x = x + 1
    }
}
)";
    auto tokens = lex(code);
    EXPECT_FALSE(tokens.empty());

    // Verify no error tokens
    for (const auto& token : tokens) {
        EXPECT_NE(token.kind, TokenKind::Error) << "Unexpected error token: " << token.lexeme;
    }

    // Verify loop-related keywords
    bool has_for = false, has_loop = false, has_break = false, has_in = false;
    for (const auto& token : tokens) {
        if (token.kind == TokenKind::KwFor)
            has_for = true;
        if (token.kind == TokenKind::KwLoop)
            has_loop = true;
        if (token.kind == TokenKind::KwBreak)
            has_break = true;
        if (token.kind == TokenKind::KwIn)
            has_in = true;
    }
    EXPECT_TRUE(has_for);
    EXPECT_TRUE(has_loop);
    EXPECT_TRUE(has_break);
    EXPECT_TRUE(has_in);
}

TEST_F(LexerTest, IntegrationFFIDeclarations) {
    // Test lexing FFI function declarations
    std::string code = R"(
@link("SDL2")
@extern("c")
pub func SDL_Init(flags: U32) -> I32

@link("user32")
@extern("stdcall")
func MessageBoxA(hwnd: I32, text: Str, caption: Str, utype: I32) -> I32

func main() -> I32 {
    let result: I32 = SDL2::SDL_Init(0)
    return result
}
)";
    auto tokens = lex(code);
    EXPECT_FALSE(tokens.empty());

    // Verify no error tokens
    for (const auto& token : tokens) {
        EXPECT_NE(token.kind, TokenKind::Error) << "Unexpected error token: " << token.lexeme;
    }

    // Verify @ decorator tokens
    int at_count = 0;
    for (const auto& token : tokens) {
        if (token.kind == TokenKind::At)
            at_count++;
    }
    EXPECT_GE(at_count, 4); // @link and @extern for each function
}

TEST_F(LexerTest, IntegrationAllLiteralTypes) {
    // Test all literal types in a single program
    std::string code = R"(
func literals_test() {
    let dec: I64 = 42
    let hex: I64 = 0xFF
    let bin: I64 = 0b1010
    let oct: I64 = 0o755
    let with_sep: I64 = 1_000_000

    let simple_float: F64 = 3.14
    let exp_float: F64 = 1.5e10
    let neg_exp: F64 = 2.5e-3

    let str1: Str = "hello world"
    let str2: Str = "escape: \n\t"

    let ch: Char = 'x'
    let esc_ch: Char = '\n'

    let t: Bool = true
    let f: Bool = false
}
)";
    auto tokens = lex(code);
    EXPECT_FALSE(tokens.empty());

    // Count different literal types
    int int_count = 0, float_count = 0, str_count = 0, char_count = 0, bool_count = 0;
    for (const auto& token : tokens) {
        EXPECT_NE(token.kind, TokenKind::Error) << "Unexpected error token: " << token.lexeme;

        if (token.kind == TokenKind::IntLiteral)
            int_count++;
        if (token.kind == TokenKind::FloatLiteral)
            float_count++;
        if (token.kind == TokenKind::StringLiteral)
            str_count++;
        if (token.kind == TokenKind::CharLiteral)
            char_count++;
        if (token.kind == TokenKind::BoolLiteral)
            bool_count++;
    }

    EXPECT_GE(int_count, 5);   // At least 5 integer literals
    EXPECT_GE(float_count, 3); // At least 3 float literals
    EXPECT_GE(str_count, 2);   // At least 2 string literals
    EXPECT_GE(char_count, 2);  // At least 2 char literals
    EXPECT_EQ(bool_count, 2);  // Exactly 2 bool literals
}

TEST_F(LexerTest, IntegrationOperatorChains) {
    // Test complex operator chains are tokenized correctly
    std::string code = R"(
func operators() {
    let a: I64 = 1 + 2 * 3 - 4 / 5
    let b: Bool = a > 0 and a < 100
    let c: Bool = not (b or false)
    let d: I64 = a << 2 | 0xFF
    let e: I64 = a >> 1 & 0x0F
}
)";
    auto tokens = lex(code);
    EXPECT_FALSE(tokens.empty());

    // Verify no error tokens
    for (const auto& token : tokens) {
        EXPECT_NE(token.kind, TokenKind::Error) << "Unexpected error token: " << token.lexeme;
    }

    // Verify key operators are present
    bool has_plus = false, has_star = false, has_and = false, has_or = false;
    bool has_shl = false, has_shr = false, has_bitor = false, has_bitand = false;
    for (const auto& token : tokens) {
        if (token.kind == TokenKind::Plus)
            has_plus = true;
        if (token.kind == TokenKind::Star)
            has_star = true;
        if (token.kind == TokenKind::KwAnd)
            has_and = true;
        if (token.kind == TokenKind::KwOr)
            has_or = true;
        if (token.kind == TokenKind::Shl)
            has_shl = true;
        if (token.kind == TokenKind::Shr)
            has_shr = true;
        if (token.kind == TokenKind::BitOr)
            has_bitor = true;
        if (token.kind == TokenKind::BitAnd)
            has_bitand = true;
    }
    EXPECT_TRUE(has_plus);
    EXPECT_TRUE(has_star);
    EXPECT_TRUE(has_and);
    EXPECT_TRUE(has_or);
    EXPECT_TRUE(has_shl);
    EXPECT_TRUE(has_shr);
    EXPECT_TRUE(has_bitor);
    EXPECT_TRUE(has_bitand);
}

// ============================================================================
// Interpolated String Tests
// ============================================================================

TEST_F(LexerTest, InterpolatedStringSimple) {
    // "Hello {name}!" should produce:
    // InterpStringStart("Hello ") + Identifier(name) + InterpStringEnd("!")
    auto tokens = lex("\"Hello {name}!\"");

    ASSERT_GE(tokens.size(), 3);
    EXPECT_EQ(tokens[0].kind, TokenKind::InterpStringStart);
    EXPECT_EQ(tokens[1].kind, TokenKind::Identifier);
    EXPECT_EQ(tokens[2].kind, TokenKind::InterpStringEnd);
}

TEST_F(LexerTest, InterpolatedStringMultiple) {
    // "Hello {name}, you are {age} years old"
    auto tokens = lex("\"Hello {name}, you are {age} years old\"");

    // Expected: InterpStringStart + Identifier + InterpStringMiddle + Identifier + InterpStringEnd
    ASSERT_GE(tokens.size(), 5);
    EXPECT_EQ(tokens[0].kind, TokenKind::InterpStringStart);
    EXPECT_EQ(tokens[1].kind, TokenKind::Identifier);
    EXPECT_EQ(tokens[2].kind, TokenKind::InterpStringMiddle);
    EXPECT_EQ(tokens[3].kind, TokenKind::Identifier);
    EXPECT_EQ(tokens[4].kind, TokenKind::InterpStringEnd);
}

TEST_F(LexerTest, RegularString) {
    // "Hello World" - no interpolation, should be regular StringLiteral
    auto tokens = lex("\"Hello World\"");
    ASSERT_GE(tokens.size(), 1);
    EXPECT_EQ(tokens[0].kind, TokenKind::StringLiteral);
}

// ============================================================================
// Symbol Logical Operators (&&, ||)
// ============================================================================

TEST_F(LexerTest, LogicalAndSymbol) {
    // && should be tokenized as AndAnd
    auto token = lex_one("&&");
    EXPECT_EQ(token.kind, TokenKind::AndAnd);
}

TEST_F(LexerTest, LogicalOrSymbol) {
    // || should be tokenized as OrOr
    auto token = lex_one("||");
    EXPECT_EQ(token.kind, TokenKind::OrOr);
}

TEST_F(LexerTest, LogicalNotSymbolPrefix) {
    // ! should be tokenized as Bang (can be used as prefix NOT)
    auto token = lex_one("!");
    EXPECT_EQ(token.kind, TokenKind::Bang);
}

TEST_F(LexerTest, LogicalOperatorsInExpression) {
    // Test && and || in a complete expression
    auto tokens = lex("a && b || !c");

    ASSERT_GE(tokens.size(), 6);
    EXPECT_EQ(tokens[0].kind, TokenKind::Identifier); // a
    EXPECT_EQ(tokens[1].kind, TokenKind::AndAnd);     // &&
    EXPECT_EQ(tokens[2].kind, TokenKind::Identifier); // b
    EXPECT_EQ(tokens[3].kind, TokenKind::OrOr);       // ||
    EXPECT_EQ(tokens[4].kind, TokenKind::Bang);       // !
    EXPECT_EQ(tokens[5].kind, TokenKind::Identifier); // c
}

TEST_F(LexerTest, MixedLogicalOperators) {
    // Test mixing word and symbol operators
    auto tokens = lex("a and b && c or d || e");

    // Should have: a and b && c or d || e
    int and_count = 0, andand_count = 0, or_count = 0, oror_count = 0;
    for (const auto& token : tokens) {
        if (token.kind == TokenKind::KwAnd)
            and_count++;
        if (token.kind == TokenKind::AndAnd)
            andand_count++;
        if (token.kind == TokenKind::KwOr)
            or_count++;
        if (token.kind == TokenKind::OrOr)
            oror_count++;
    }
    EXPECT_EQ(and_count, 1);
    EXPECT_EQ(andand_count, 1);
    EXPECT_EQ(or_count, 1);
    EXPECT_EQ(oror_count, 1);
}

TEST_F(LexerTest, BitwiseVsLogical) {
    // Test that & and && are different, | and || are different
    auto tokens = lex("a & b && c | d || e");

    int bitand_count = 0, andand_count = 0, bitor_count = 0, oror_count = 0;
    for (const auto& token : tokens) {
        if (token.kind == TokenKind::BitAnd)
            bitand_count++;
        if (token.kind == TokenKind::AndAnd)
            andand_count++;
        if (token.kind == TokenKind::BitOr)
            bitor_count++;
        if (token.kind == TokenKind::OrOr)
            oror_count++;
    }
    EXPECT_EQ(bitand_count, 1);
    EXPECT_EQ(andand_count, 1);
    EXPECT_EQ(bitor_count, 1);
    EXPECT_EQ(oror_count, 1);
}

TEST_F(LexerTest, LogicalOperatorsWithParens) {
    // Test logical operators with parentheses
    auto tokens = lex("(a && b) || !(c && d)");

    // Verify no error tokens
    for (const auto& token : tokens) {
        EXPECT_NE(token.kind, TokenKind::Error) << "Unexpected error token: " << token.lexeme;
    }

    // Count operators
    int andand_count = 0, oror_count = 0, bang_count = 0;
    for (const auto& token : tokens) {
        if (token.kind == TokenKind::AndAnd)
            andand_count++;
        if (token.kind == TokenKind::OrOr)
            oror_count++;
        if (token.kind == TokenKind::Bang)
            bang_count++;
    }
    EXPECT_EQ(andand_count, 2);
    EXPECT_EQ(oror_count, 1);
    EXPECT_EQ(bang_count, 1);
}

TEST_F(LexerTest, AsKeyword) {
    // Test 'as' keyword for type casting
    auto token = lex_one("as");
    EXPECT_EQ(token.kind, TokenKind::KwAs);
}

TEST_F(LexerTest, AsCastExpression) {
    // Test 'as' in a cast expression
    auto tokens = lex("x as I64");

    ASSERT_GE(tokens.size(), 3);
    EXPECT_EQ(tokens[0].kind, TokenKind::Identifier); // x
    EXPECT_EQ(tokens[1].kind, TokenKind::KwAs);       // as
    EXPECT_EQ(tokens[2].kind, TokenKind::Identifier); // I64
}
