#include "tml/lexer/lexer.hpp"
#include "tml/parser/parser.hpp"
#include <gtest/gtest.h>

using namespace tml;
using namespace tml::lexer;
using namespace tml::parser;

class ParserTest : public ::testing::Test {
protected:
    auto parse(const std::string& code) -> Result<Module, std::vector<ParseError>> {
        auto source = Source::from_string(code);
        Lexer lexer(source);
        auto tokens = lexer.tokenize();
        Parser parser(std::move(tokens));
        return parser.parse_module("test");
    }

    auto parse_expr(const std::string& code) -> Result<ExprPtr, ParseError> {
        auto source = Source::from_string(code);
        Lexer lexer(source);
        auto tokens = lexer.tokenize();
        Parser parser(std::move(tokens));
        return parser.parse_expr();
    }

    auto parse_stmt(const std::string& code) -> Result<StmtPtr, ParseError> {
        auto source = Source::from_string(code);
        Lexer lexer(source);
        auto tokens = lexer.tokenize();
        Parser parser(std::move(tokens));
        return parser.parse_stmt();
    }
};

// Expression tests
TEST_F(ParserTest, LiteralExpressions) {
    auto result = parse_expr("42");
    ASSERT_TRUE(is_ok(result));
    EXPECT_TRUE(unwrap(result)->is<LiteralExpr>());
}

TEST_F(ParserTest, IdentifierExpressions) {
    auto result = parse_expr("foo");
    ASSERT_TRUE(is_ok(result));
    EXPECT_TRUE(unwrap(result)->is<IdentExpr>());
    EXPECT_EQ(unwrap(result)->as<IdentExpr>().name, "foo");
}

TEST_F(ParserTest, BinaryExpressions) {
    auto result = parse_expr("a + b");
    ASSERT_TRUE(is_ok(result));
    EXPECT_TRUE(unwrap(result)->is<BinaryExpr>());

    auto& bin = unwrap(result)->as<BinaryExpr>();
    EXPECT_EQ(bin.op, BinaryOp::Add);
}

TEST_F(ParserTest, BinaryPrecedence) {
    // a + b * c should parse as a + (b * c)
    auto result = parse_expr("a + b * c");
    ASSERT_TRUE(is_ok(result));
    EXPECT_TRUE(unwrap(result)->is<BinaryExpr>());

    auto& add = unwrap(result)->as<BinaryExpr>();
    EXPECT_EQ(add.op, BinaryOp::Add);
    EXPECT_TRUE(add.right->is<BinaryExpr>());

    auto& mul = add.right->as<BinaryExpr>();
    EXPECT_EQ(mul.op, BinaryOp::Mul);
}

TEST_F(ParserTest, UnaryExpressions) {
    auto result = parse_expr("-x");
    ASSERT_TRUE(is_ok(result));
    EXPECT_TRUE(unwrap(result)->is<UnaryExpr>());

    auto& unary = unwrap(result)->as<UnaryExpr>();
    EXPECT_EQ(unary.op, UnaryOp::Neg);
}

TEST_F(ParserTest, ReferenceExpressions) {
    auto result = parse_expr("&x");
    ASSERT_TRUE(is_ok(result));
    EXPECT_TRUE(unwrap(result)->is<UnaryExpr>());
    EXPECT_EQ(unwrap(result)->as<UnaryExpr>().op, UnaryOp::Ref);

    auto mut_result = parse_expr("&mut x");
    ASSERT_TRUE(is_ok(mut_result));
    EXPECT_TRUE(unwrap(mut_result)->is<UnaryExpr>());
    EXPECT_EQ(unwrap(mut_result)->as<UnaryExpr>().op, UnaryOp::RefMut);
}

TEST_F(ParserTest, CallExpressions) {
    auto result = parse_expr("foo(a, b)");
    ASSERT_TRUE(is_ok(result));
    EXPECT_TRUE(unwrap(result)->is<CallExpr>());

    auto& call = unwrap(result)->as<CallExpr>();
    EXPECT_EQ(call.args.size(), 2);
}

TEST_F(ParserTest, MethodCallExpressions) {
    auto result = parse_expr("obj.method(a)");
    ASSERT_TRUE(is_ok(result));
    EXPECT_TRUE(unwrap(result)->is<MethodCallExpr>());

    auto& method = unwrap(result)->as<MethodCallExpr>();
    EXPECT_EQ(method.method, "method");
}

TEST_F(ParserTest, FieldAccessExpressions) {
    auto result = parse_expr("obj.field");
    ASSERT_TRUE(is_ok(result));
    EXPECT_TRUE(unwrap(result)->is<FieldExpr>());

    auto& field = unwrap(result)->as<FieldExpr>();
    EXPECT_EQ(field.field, "field");
}

TEST_F(ParserTest, IndexExpressions) {
    auto result = parse_expr("arr[0]");
    ASSERT_TRUE(is_ok(result));
    EXPECT_TRUE(unwrap(result)->is<IndexExpr>());
}

TEST_F(ParserTest, TupleExpressions) {
    auto result = parse_expr("(a, b, c)");
    ASSERT_TRUE(is_ok(result));
    EXPECT_TRUE(unwrap(result)->is<TupleExpr>());

    auto& tuple = unwrap(result)->as<TupleExpr>();
    EXPECT_EQ(tuple.elements.size(), 3);
}

TEST_F(ParserTest, ArrayExpressions) {
    auto result = parse_expr("[1, 2, 3]");
    ASSERT_TRUE(is_ok(result));
    EXPECT_TRUE(unwrap(result)->is<ArrayExpr>());
}

TEST_F(ParserTest, ArrayRepeatExpression) {
    auto result = parse_expr("[0; 10]");
    ASSERT_TRUE(is_ok(result));
    EXPECT_TRUE(unwrap(result)->is<ArrayExpr>());
}

TEST_F(ParserTest, BlockExpressions) {
    auto result = parse_expr("{ x }");
    ASSERT_TRUE(is_ok(result));
    EXPECT_TRUE(unwrap(result)->is<BlockExpr>());
}

TEST_F(ParserTest, IfExpressions) {
    auto result = parse_expr("if cond { a } else { b }");
    ASSERT_TRUE(is_ok(result));
    EXPECT_TRUE(unwrap(result)->is<IfExpr>());

    auto& if_expr = unwrap(result)->as<IfExpr>();
    EXPECT_TRUE(if_expr.else_branch.has_value());
}

TEST_F(ParserTest, IfExpressionWithoutElse) {
    auto result = parse_expr("if cond { a }");
    ASSERT_TRUE(is_ok(result));
    EXPECT_TRUE(unwrap(result)->is<IfExpr>());

    auto& if_expr = unwrap(result)->as<IfExpr>();
    EXPECT_FALSE(if_expr.else_branch.has_value());
}

TEST_F(ParserTest, WhileExpressions) {
    auto result = parse_expr("while cond { body }");
    ASSERT_TRUE(is_ok(result));
    EXPECT_TRUE(unwrap(result)->is<WhileExpr>());
}

TEST_F(ParserTest, LoopExpressions) {
    auto result = parse_expr("loop { body }");
    ASSERT_TRUE(is_ok(result));
    EXPECT_TRUE(unwrap(result)->is<LoopExpr>());
}

TEST_F(ParserTest, ForExpressions) {
    auto result = parse_expr("for x in items { body }");
    ASSERT_TRUE(is_ok(result));
    EXPECT_TRUE(unwrap(result)->is<ForExpr>());
}

TEST_F(ParserTest, ReturnExpressions) {
    auto result = parse_expr("return 42");
    ASSERT_TRUE(is_ok(result));
    EXPECT_TRUE(unwrap(result)->is<ReturnExpr>());
    EXPECT_TRUE(unwrap(result)->as<ReturnExpr>().value.has_value());
}

TEST_F(ParserTest, TryExpressions) {
    auto result = parse_expr("foo()?");
    ASSERT_TRUE(is_ok(result));
    EXPECT_TRUE(unwrap(result)->is<TryExpr>());
}

// Statement tests
TEST_F(ParserTest, LetStatements) {
    auto result = parse_stmt("let x = 42");
    ASSERT_TRUE(is_ok(result));
    EXPECT_TRUE(unwrap(result)->is<LetStmt>());
}

TEST_F(ParserTest, LetStatementsWithType) {
    auto result = parse_stmt("let x: I32 = 42");
    ASSERT_TRUE(is_ok(result));
    EXPECT_TRUE(unwrap(result)->is<LetStmt>());

    auto& let_stmt = unwrap(result)->as<LetStmt>();
    EXPECT_TRUE(let_stmt.type_annotation.has_value());
}

TEST_F(ParserTest, VarStatements) {
    auto result = parse_stmt("var x = 42");
    ASSERT_TRUE(is_ok(result));
    EXPECT_TRUE(unwrap(result)->is<VarStmt>());
}

// Declaration tests
TEST_F(ParserTest, SimpleFunctionDecl) {
    auto result = parse("func foo() {}");
    ASSERT_TRUE(is_ok(result));
    EXPECT_EQ(unwrap(result).decls.size(), 1);
    EXPECT_TRUE(unwrap(result).decls[0]->is<FuncDecl>());
}

TEST_F(ParserTest, FunctionWithParams) {
    auto result = parse("func add(a: I32, b: I32) -> I32 { a + b }");
    ASSERT_TRUE(is_ok(result));
    EXPECT_EQ(unwrap(result).decls.size(), 1);

    auto& func = unwrap(result).decls[0]->as<FuncDecl>();
    EXPECT_EQ(func.name, "add");
    EXPECT_EQ(func.params.size(), 2);
    EXPECT_TRUE(func.return_type.has_value());
}

TEST_F(ParserTest, GenericFunction) {
    auto result = parse("func id[T](x: T) -> T { x }");
    ASSERT_TRUE(is_ok(result));
    EXPECT_EQ(unwrap(result).decls.size(), 1);

    auto& func = unwrap(result).decls[0]->as<FuncDecl>();
    EXPECT_EQ(func.generics.size(), 1);
    EXPECT_EQ(func.generics[0].name, "T");
}

TEST_F(ParserTest, PublicFunction) {
    auto result = parse("pub func public_fn() {}");
    ASSERT_TRUE(is_ok(result));

    auto& func = unwrap(result).decls[0]->as<FuncDecl>();
    EXPECT_EQ(func.vis, Visibility::Public);
}

TEST_F(ParserTest, StructDecl) {
    auto result = parse(R"(
        type Point {
            x: F64
            y: F64
        }
    )");
    ASSERT_TRUE(is_ok(result));
    EXPECT_EQ(unwrap(result).decls.size(), 1);
    EXPECT_TRUE(unwrap(result).decls[0]->is<StructDecl>());

    auto& struct_decl = unwrap(result).decls[0]->as<StructDecl>();
    EXPECT_EQ(struct_decl.name, "Point");
    EXPECT_EQ(struct_decl.fields.size(), 2);
}

TEST_F(ParserTest, GenericStruct) {
    auto result = parse(R"(
        type Container[T] {
            value: T
        }
    )");
    ASSERT_TRUE(is_ok(result));

    auto& struct_decl = unwrap(result).decls[0]->as<StructDecl>();
    EXPECT_EQ(struct_decl.generics.size(), 1);
}

TEST_F(ParserTest, TypeAlias) {
    auto result = parse("type Integer = I32");
    ASSERT_TRUE(is_ok(result));
    EXPECT_TRUE(unwrap(result).decls[0]->is<TypeAliasDecl>());
}

// Type tests
TEST_F(ParserTest, SimpleType) {
    auto result = parse("func foo(x: I32) {}");
    ASSERT_TRUE(is_ok(result));

    auto& func = unwrap(result).decls[0]->as<FuncDecl>();
    EXPECT_TRUE(func.params[0].type->is<NamedType>());
}

TEST_F(ParserTest, GenericType) {
    auto result = parse("func foo(x: Vec[I32]) {}");
    ASSERT_TRUE(is_ok(result));

    auto& func = unwrap(result).decls[0]->as<FuncDecl>();
    auto& named = func.params[0].type->as<NamedType>();
    EXPECT_TRUE(named.generics.has_value());
    EXPECT_EQ(named.generics->args.size(), 1);
}

TEST_F(ParserTest, ReferenceType) {
    auto result = parse("func foo(x: &I32) {}");
    ASSERT_TRUE(is_ok(result));

    auto& func = unwrap(result).decls[0]->as<FuncDecl>();
    EXPECT_TRUE(func.params[0].type->is<RefType>());
    EXPECT_FALSE(func.params[0].type->as<RefType>().is_mut);
}

TEST_F(ParserTest, MutableReferenceType) {
    auto result = parse("func foo(x: &mut I32) {}");
    ASSERT_TRUE(is_ok(result));

    auto& func = unwrap(result).decls[0]->as<FuncDecl>();
    EXPECT_TRUE(func.params[0].type->is<RefType>());
    EXPECT_TRUE(func.params[0].type->as<RefType>().is_mut);
}

TEST_F(ParserTest, SliceType) {
    auto result = parse("func foo(x: [I32]) {}");
    ASSERT_TRUE(is_ok(result));

    auto& func = unwrap(result).decls[0]->as<FuncDecl>();
    EXPECT_TRUE(func.params[0].type->is<SliceType>());
}

// Pattern tests
TEST_F(ParserTest, IdentifierPattern) {
    auto result = parse_stmt("let x = 1");
    ASSERT_TRUE(is_ok(result));

    auto& let_stmt = unwrap(result)->as<LetStmt>();
    EXPECT_TRUE(let_stmt.pattern->is<IdentPattern>());
}

TEST_F(ParserTest, MutablePattern) {
    auto result = parse_stmt("let mut x = 1");
    ASSERT_TRUE(is_ok(result));

    auto& let_stmt = unwrap(result)->as<LetStmt>();
    auto& ident = let_stmt.pattern->as<IdentPattern>();
    EXPECT_TRUE(ident.is_mut);
}

TEST_F(ParserTest, TuplePattern) {
    auto result = parse_stmt("let (a, b) = pair");
    ASSERT_TRUE(is_ok(result));

    auto& let_stmt = unwrap(result)->as<LetStmt>();
    EXPECT_TRUE(let_stmt.pattern->is<TuplePattern>());
}

TEST_F(ParserTest, WildcardPattern) {
    auto result = parse_stmt("let _ = unused");
    ASSERT_TRUE(is_ok(result));

    auto& let_stmt = unwrap(result)->as<LetStmt>();
    EXPECT_TRUE(let_stmt.pattern->is<WildcardPattern>());
}

// When expression tests
TEST_F(ParserTest, WhenExpression) {
    auto result = parse_expr(R"(
        when x {
            0 => "zero"
            1 => "one"
            _ => "other"
        }
    )");
    ASSERT_TRUE(is_ok(result));
    EXPECT_TRUE(unwrap(result)->is<WhenExpr>());

    auto& when_expr = unwrap(result)->as<WhenExpr>();
    EXPECT_EQ(when_expr.arms.size(), 3);
}

TEST_F(ParserTest, WhenWithEnumPattern) {
    auto result = parse_expr(R"(
        when opt {
            Some(x) => x
            None => 0
        }
    )");
    ASSERT_TRUE(is_ok(result));

    auto& when_expr = unwrap(result)->as<WhenExpr>();
    EXPECT_TRUE(when_expr.arms[0].pattern->is<EnumPattern>());
}

// Error handling tests
TEST_F(ParserTest, MissingClosingBrace) {
    auto result = parse("func foo() {");
    EXPECT_TRUE(is_err(result));
}

TEST_F(ParserTest, MissingFunctionName) {
    auto result = parse("func () {}");
    EXPECT_TRUE(is_err(result));
}

// Struct expression tests
TEST_F(ParserTest, StructExpression) {
    auto result = parse_expr("Point { x: 1, y: 2 }");
    ASSERT_TRUE(is_ok(result));
    EXPECT_TRUE(unwrap(result)->is<StructExpr>());

    auto& struct_expr = unwrap(result)->as<StructExpr>();
    EXPECT_EQ(struct_expr.fields.size(), 2);
}

TEST_F(ParserTest, StructExpressionShorthand) {
    auto result = parse_expr("Point { x, y }");
    ASSERT_TRUE(is_ok(result));
    EXPECT_TRUE(unwrap(result)->is<StructExpr>());
}

// Integration test
TEST_F(ParserTest, CompleteProgram) {
    auto result = parse(R"(
        type Point {
            x: F64
            y: F64
        }

        func distance(p1: &Point, p2: &Point) -> F64 {
            let dx = p2.x - p1.x
            let dy = p2.y - p1.y
            sqrt(dx * dx + dy * dy)
        }

        func main() {
            let p1 = Point { x: 0.0, y: 0.0 }
            let p2 = Point { x: 3.0, y: 4.0 }
            let d = distance(&p1, &p2)
            print(d)
        }
    )");

    ASSERT_TRUE(is_ok(result));
    EXPECT_EQ(unwrap(result).decls.size(), 3);
}
