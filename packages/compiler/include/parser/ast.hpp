#ifndef TML_PARSER_AST_HPP
#define TML_PARSER_AST_HPP

#include "common.hpp"
#include "lexer/token.hpp"

#include <memory>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace tml::parser {

// Forward declarations
struct Expr;
struct Stmt;
struct Decl;
struct Pattern;
struct Type;

using ExprPtr = Box<Expr>;
using StmtPtr = Box<Stmt>;
using DeclPtr = Box<Decl>;
using PatternPtr = Box<Pattern>;
using TypePtr = Box<Type>;

// ============================================================================
// Type AST
// ============================================================================

// Type path: Vec, std::io::File, etc.
struct TypePath {
    std::vector<std::string> segments;
    SourceSpan span;
};

// Generic arguments: [T, U]
struct GenericArgs {
    std::vector<TypePtr> args;
    SourceSpan span;
};

// Reference type: &T, &mut T
struct RefType {
    bool is_mut;
    TypePtr inner;
    SourceSpan span;
};

// Pointer type: *const T, *mut T
struct PtrType {
    bool is_mut;
    TypePtr inner;
    SourceSpan span;
};

// Array type: [T; N]
struct ArrayType {
    TypePtr element;
    ExprPtr size;
    SourceSpan span;
};

// Slice type: [T]
struct SliceType {
    TypePtr element;
    SourceSpan span;
};

// Tuple type: (T, U, V)
struct TupleType {
    std::vector<TypePtr> elements;
    SourceSpan span;
};

// Function type: func(A, B) -> R
struct FuncType {
    std::vector<TypePtr> params;
    TypePtr return_type; // nullptr for unit
    SourceSpan span;
};

// Named type with optional generics: Vec[T], HashMap[K, V]
struct NamedType {
    TypePath path;
    std::optional<GenericArgs> generics;
    SourceSpan span;
};

// Inferred type: _ (let compiler infer)
struct InferType {
    SourceSpan span;
};

// Dynamic trait object type: dyn Behavior[T]
struct DynType {
    TypePath behavior;                   // The behavior being used as trait object
    std::optional<GenericArgs> generics; // Generic parameters: dyn Iterator[I32]
    bool is_mut;                         // dyn mut Behavior
    SourceSpan span;
};

// Type variant
struct Type {
    std::variant<NamedType, RefType, PtrType, ArrayType, SliceType, TupleType, FuncType, InferType,
                 DynType>
        kind;
    SourceSpan span;

    template <typename T> [[nodiscard]] auto is() const -> bool {
        return std::holds_alternative<T>(kind);
    }

    template <typename T> [[nodiscard]] auto as() -> T& {
        if (!is<T>()) {
            throw std::bad_variant_access();
        }
        return std::get<T>(kind);
    }

    template <typename T> [[nodiscard]] auto as() const -> const T& {
        if (!is<T>()) {
            throw std::bad_variant_access();
        }
        return std::get<T>(kind);
    }
};

// ============================================================================
// Pattern AST
// ============================================================================

// Wildcard pattern: _
struct WildcardPattern {
    SourceSpan span;
};

// Identifier pattern: x, mut x
struct IdentPattern {
    std::string name;
    bool is_mut;
    std::optional<TypePtr> type_annotation;
    SourceSpan span;
};

// Literal pattern: 42, "hello", true
struct LiteralPattern {
    lexer::Token literal;
    SourceSpan span;
};

// Tuple pattern: (a, b, c)
struct TuplePattern {
    std::vector<PatternPtr> elements;
    SourceSpan span;
};

// Struct pattern: Point { x, y }
struct StructPattern {
    TypePath path;
    std::vector<std::pair<std::string, PatternPtr>> fields;
    bool has_rest; // .. at end
    SourceSpan span;
};

// Enum variant pattern: Some(x), None
struct EnumPattern {
    TypePath path;
    std::optional<std::vector<PatternPtr>> payload;
    SourceSpan span;
};

// Or pattern: a | b | c
struct OrPattern {
    std::vector<PatternPtr> patterns;
    SourceSpan span;
};

// Range pattern: 0..10, 'a'..='z'
struct RangePattern {
    std::optional<ExprPtr> start;
    std::optional<ExprPtr> end;
    bool inclusive;
    SourceSpan span;
};

// Pattern variant
struct Pattern {
    std::variant<WildcardPattern, IdentPattern, LiteralPattern, TuplePattern, StructPattern,
                 EnumPattern, OrPattern, RangePattern>
        kind;
    SourceSpan span;

    template <typename T> [[nodiscard]] auto is() const -> bool {
        return std::holds_alternative<T>(kind);
    }

    template <typename T> [[nodiscard]] auto as() -> T& {
        if (!is<T>()) {
            throw std::bad_variant_access();
        }
        return std::get<T>(kind);
    }

    template <typename T> [[nodiscard]] auto as() const -> const T& {
        if (!is<T>()) {
            throw std::bad_variant_access();
        }
        return std::get<T>(kind);
    }
};

// ============================================================================
// Expression AST
// ============================================================================

// Literal expression: 42, 3.14, "hello", 'a', true
struct LiteralExpr {
    lexer::Token token;
    SourceSpan span;
};

// Identifier expression: foo, bar
struct IdentExpr {
    std::string name;
    SourceSpan span;
};

// Unary expression: -x, !x, &x, &mut x, *x
enum class UnaryOp {
    Neg,    // -
    Not,    // !
    BitNot, // ~
    Ref,    // &
    RefMut, // &mut
    Deref,  // *
    Inc,    // ++ (postfix increment)
    Dec,    // -- (postfix decrement)
};

struct UnaryExpr {
    UnaryOp op;
    ExprPtr operand;
    SourceSpan span;
};

// Binary expression: a + b, a && b, etc.
enum class BinaryOp {
    // Arithmetic
    Add,
    Sub,
    Mul,
    Div,
    Mod,
    // Comparison
    Eq,
    Ne,
    Lt,
    Gt,
    Le,
    Ge,
    // Logical
    And,
    Or,
    // Bitwise
    BitAnd,
    BitOr,
    BitXor,
    Shl,
    Shr,
    // Assignment
    Assign,
    AddAssign,
    SubAssign,
    MulAssign,
    DivAssign,
    ModAssign,
    BitAndAssign,
    BitOrAssign,
    BitXorAssign,
    ShlAssign,
    ShrAssign,
};

struct BinaryExpr {
    BinaryOp op;
    ExprPtr left;
    ExprPtr right;
    SourceSpan span;
};

// Call expression: foo(a, b)
struct CallExpr {
    ExprPtr callee;
    std::vector<ExprPtr> args;
    SourceSpan span;
};

// Method call: obj.method(a, b)
struct MethodCallExpr {
    ExprPtr receiver;
    std::string method;
    std::vector<ExprPtr> args;
    SourceSpan span;
};

// Field access: obj.field
struct FieldExpr {
    ExprPtr object;
    std::string field;
    SourceSpan span;
};

// Index expression: arr[i]
struct IndexExpr {
    ExprPtr object;
    ExprPtr index;
    SourceSpan span;
};

// Tuple expression: (a, b, c)
struct TupleExpr {
    std::vector<ExprPtr> elements;
    SourceSpan span;
};

// Array expression: [1, 2, 3] or [0; 10]
struct ArrayExpr {
    std::variant<std::vector<ExprPtr>,       // [1, 2, 3]
                 std::pair<ExprPtr, ExprPtr> // [expr; count]
                 >
        kind;
    SourceSpan span;
};

// Struct expression: Point { x: 1, y: 2 } or Point[T] { x: 1, y: 2 }
struct StructExpr {
    TypePath path;
    std::optional<GenericArgs> generics; // Generic arguments like [I32]
    std::vector<std::pair<std::string, ExprPtr>> fields;
    std::optional<ExprPtr> base; // ..base for struct update
    SourceSpan span;
};

// If expression: if cond { then } else { else }
struct IfExpr {
    ExprPtr condition;
    ExprPtr then_branch;
    std::optional<ExprPtr> else_branch;
    SourceSpan span;
};

// Ternary expression: condition ? true_value : false_value
struct TernaryExpr {
    ExprPtr condition;
    ExprPtr true_value;
    ExprPtr false_value;
    SourceSpan span;
};

// If-let expression: if let pattern = expr { then } else { else }
struct IfLetExpr {
    PatternPtr pattern;
    ExprPtr scrutinee;
    ExprPtr then_branch;
    std::optional<ExprPtr> else_branch;
    SourceSpan span;
};

// When (match) expression arm
struct WhenArm {
    PatternPtr pattern;
    std::optional<ExprPtr> guard;
    ExprPtr body;
    SourceSpan span;
};

// When expression: when x { pat => expr, ... }
struct WhenExpr {
    ExprPtr scrutinee;
    std::vector<WhenArm> arms;
    SourceSpan span;
};

// Loop expression: loop { body }
struct LoopExpr {
    std::optional<std::string> label;
    ExprPtr body;
    SourceSpan span;
};

// While expression: while cond { body }
struct WhileExpr {
    std::optional<std::string> label;
    ExprPtr condition;
    ExprPtr body;
    SourceSpan span;
};

// For expression: for x in iter { body }
struct ForExpr {
    std::optional<std::string> label;
    PatternPtr pattern;
    ExprPtr iter;
    ExprPtr body;
    SourceSpan span;
};

// Block expression: { stmts; expr }
struct BlockExpr {
    std::vector<StmtPtr> stmts;
    std::optional<ExprPtr> expr; // trailing expression (no semicolon)
    SourceSpan span;
};

// Return expression: return x
struct ReturnExpr {
    std::optional<ExprPtr> value;
    SourceSpan span;
};

// Break expression: break 'label x
struct BreakExpr {
    std::optional<std::string> label;
    std::optional<ExprPtr> value;
    SourceSpan span;
};

// Continue expression: continue 'label
struct ContinueExpr {
    std::optional<std::string> label;
    SourceSpan span;
};

// Closure expression: |x, y| x + y
struct ClosureExpr {
    std::vector<std::pair<PatternPtr, std::optional<TypePtr>>> params;
    std::optional<TypePtr> return_type;
    ExprPtr body;
    bool is_move;
    SourceSpan span;

    // Captured variables (filled by type checker)
    mutable std::vector<std::string> captured_vars;
};

// Range expression: a..b, a..=b, ..b, a..
struct RangeExpr {
    std::optional<ExprPtr> start;
    std::optional<ExprPtr> end;
    bool inclusive;
    SourceSpan span;
};

// Cast expression: x as T
struct CastExpr {
    ExprPtr expr;
    TypePtr target;
    SourceSpan span;
};

// Try expression: expr?
struct TryExpr {
    ExprPtr expr;
    SourceSpan span;
};

// Await expression: expr.await
struct AwaitExpr {
    ExprPtr expr;
    SourceSpan span;
};

// Path expression: std::io::stdout or List[I32]
struct PathExpr {
    TypePath path;
    std::optional<GenericArgs> generics; // Generic arguments like [I32]
    SourceSpan span;
};

// Lowlevel (unsafe) block expression: lowlevel { ... }
struct LowlevelExpr {
    std::vector<StmtPtr> stmts;
    std::optional<ExprPtr> expr; // trailing expression (no semicolon)
    SourceSpan span;
};

// Interpolated string segment: either literal text or an expression
struct InterpolatedSegment {
    std::variant<std::string, // Literal text segment
                 ExprPtr      // Interpolated expression: {expr}
                 >
        content;
    SourceSpan span;
};

// Interpolated string expression: "Hello {name}, you are {age} years old"
struct InterpolatedStringExpr {
    std::vector<InterpolatedSegment> segments;
    SourceSpan span;
};

// Expression variant
struct Expr {
    std::variant<LiteralExpr, IdentExpr, UnaryExpr, BinaryExpr, CallExpr, MethodCallExpr, FieldExpr,
                 IndexExpr, TupleExpr, ArrayExpr, StructExpr, IfExpr, TernaryExpr, IfLetExpr,
                 WhenExpr, LoopExpr, WhileExpr, ForExpr, BlockExpr, ReturnExpr, BreakExpr,
                 ContinueExpr, ClosureExpr, RangeExpr, CastExpr, TryExpr, AwaitExpr, PathExpr,
                 LowlevelExpr, InterpolatedStringExpr>
        kind;
    SourceSpan span;

    template <typename T> [[nodiscard]] auto is() const -> bool {
        return std::holds_alternative<T>(kind);
    }

    template <typename T> [[nodiscard]] auto as() -> T& {
        if (!is<T>()) {
            throw std::bad_variant_access();
        }
        return std::get<T>(kind);
    }

    template <typename T> [[nodiscard]] auto as() const -> const T& {
        if (!is<T>()) {
            throw std::bad_variant_access();
        }
        return std::get<T>(kind);
    }
};

// ============================================================================
// Statement AST
// ============================================================================

// Let statement: let x = expr, let x: T = expr
struct LetStmt {
    PatternPtr pattern;
    std::optional<TypePtr> type_annotation;
    std::optional<ExprPtr> init;
    SourceSpan span;
};

// Var statement: var x = expr (mutable)
struct VarStmt {
    std::string name;
    std::optional<TypePtr> type_annotation;
    ExprPtr init;
    SourceSpan span;
};

// Expression statement: expr;
struct ExprStmt {
    ExprPtr expr;
    SourceSpan span;
};

// Statement variant
struct Stmt {
    std::variant<LetStmt, VarStmt, ExprStmt,
                 DeclPtr // Nested declaration
                 >
        kind;
    SourceSpan span;

    template <typename T> [[nodiscard]] auto is() const -> bool {
        return std::holds_alternative<T>(kind);
    }

    template <typename T> [[nodiscard]] auto as() -> T& {
        if (!is<T>()) {
            throw std::bad_variant_access();
        }
        return std::get<T>(kind);
    }

    template <typename T> [[nodiscard]] auto as() const -> const T& {
        if (!is<T>()) {
            throw std::bad_variant_access();
        }
        return std::get<T>(kind);
    }
};

// ============================================================================
// Declaration AST
// ============================================================================

// Visibility
enum class Visibility {
    Private,
    Public,
};

// Generic parameter: T, T: Trait, T: Trait + Other
struct GenericParam {
    std::string name;
    std::vector<TypePath> bounds;
    SourceSpan span;
};

// Decorator/Attribute: @derive(Clone, Debug), @test, @inline
struct Decorator {
    std::string name;
    std::vector<ExprPtr> args; // Optional arguments
    SourceSpan span;
};

// Where clause: where T: Clone, U: Hash
struct WhereClause {
    std::vector<std::pair<TypePtr, std::vector<TypePath>>> constraints;
    SourceSpan span;
};

// Function parameter
struct FuncParam {
    PatternPtr pattern;
    TypePtr type;
    SourceSpan span;
};

// Function declaration
struct FuncDecl {
    std::vector<Decorator> decorators;
    Visibility vis;
    std::string name;
    std::vector<GenericParam> generics;
    std::vector<FuncParam> params;
    std::optional<TypePtr> return_type;
    std::optional<WhereClause> where_clause;
    std::optional<BlockExpr> body; // None for trait method signatures or @extern
    bool is_async;
    bool is_unsafe;
    SourceSpan span;

    // FFI support (@extern and @link decorators)
    std::optional<std::string> extern_abi;  // "c", "c++", "stdcall", "fastcall", "thiscall"
    std::optional<std::string> extern_name; // symbol name if different from func name
    std::vector<std::string> link_libs;     // libraries to link (.dll, .lib, .so, .a)
};

// Struct field
struct StructField {
    Visibility vis;
    std::string name;
    TypePtr type;
    SourceSpan span;
};

// Struct declaration
struct StructDecl {
    std::vector<Decorator> decorators;
    Visibility vis;
    std::string name;
    std::vector<GenericParam> generics;
    std::vector<StructField> fields;
    std::optional<WhereClause> where_clause;
    SourceSpan span;
};

// Enum variant
struct EnumVariant {
    std::string name;
    std::optional<std::vector<TypePtr>> tuple_fields;
    std::optional<std::vector<StructField>> struct_fields;
    SourceSpan span;
};

// Enum declaration
struct EnumDecl {
    std::vector<Decorator> decorators;
    Visibility vis;
    std::string name;
    std::vector<GenericParam> generics;
    std::vector<EnumVariant> variants;
    std::optional<WhereClause> where_clause;
    SourceSpan span;
};

// Associated type declaration in behavior: type Item
struct AssociatedType {
    std::string name;
    std::vector<TypePath> bounds; // Optional trait bounds: type Item: Display
    SourceSpan span;
};

// Associated type binding in impl: type Item = I32
struct AssociatedTypeBinding {
    std::string name;
    TypePtr type;
    SourceSpan span;
};

// Trait declaration
struct TraitDecl {
    std::vector<Decorator> decorators;
    Visibility vis;
    std::string name;
    std::vector<GenericParam> generics;
    std::vector<TypePath> super_traits;
    std::vector<AssociatedType> associated_types; // Associated types
    std::vector<FuncDecl> methods;
    std::optional<WhereClause> where_clause;
    SourceSpan span;
};

// Impl block
struct ImplDecl {
    std::vector<GenericParam> generics;
    std::optional<TypePath> trait_path;
    TypePtr self_type;
    std::vector<AssociatedTypeBinding> type_bindings; // Associated type bindings
    std::vector<FuncDecl> methods;
    std::optional<WhereClause> where_clause;
    SourceSpan span;
};

// Type alias: type Alias = OriginalType
struct TypeAliasDecl {
    Visibility vis;
    std::string name;
    std::vector<GenericParam> generics;
    TypePtr type;
    SourceSpan span;
};

// Const declaration
struct ConstDecl {
    Visibility vis;
    std::string name;
    TypePtr type;
    ExprPtr value;
    SourceSpan span;
};

// Use declaration: use std::io::Read or use std::math::{abs, sqrt}
struct UseDecl {
    Visibility vis;
    TypePath path;
    std::optional<std::string> alias;                // as Alias
    std::optional<std::vector<std::string>> symbols; // For grouped imports: {abs, sqrt}
    SourceSpan span;
};

// Module declaration
struct ModDecl {
    Visibility vis;
    std::string name;
    std::optional<std::vector<DeclPtr>> items; // None for `mod foo;`
    SourceSpan span;
};

// Declaration variant
struct Decl {
    std::variant<FuncDecl, StructDecl, EnumDecl, TraitDecl, ImplDecl, TypeAliasDecl, ConstDecl,
                 UseDecl, ModDecl>
        kind;
    SourceSpan span;

    template <typename T> [[nodiscard]] auto is() const -> bool {
        return std::holds_alternative<T>(kind);
    }

    template <typename T> [[nodiscard]] auto as() -> T& {
        if (!is<T>()) {
            throw std::bad_variant_access();
        }
        return std::get<T>(kind);
    }

    template <typename T> [[nodiscard]] auto as() const -> const T& {
        if (!is<T>()) {
            throw std::bad_variant_access();
        }
        return std::get<T>(kind);
    }
};

// ============================================================================
// Module (top-level AST)
// ============================================================================

struct Module {
    std::string name;
    std::vector<DeclPtr> decls;
    SourceSpan span;
};

// ============================================================================
// AST Utilities
// ============================================================================

// Create expression helpers
auto make_literal_expr(lexer::Token token) -> ExprPtr;
auto make_ident_expr(std::string name, SourceSpan span) -> ExprPtr;
auto make_binary_expr(BinaryOp op, ExprPtr left, ExprPtr right, SourceSpan span) -> ExprPtr;
auto make_unary_expr(UnaryOp op, ExprPtr operand, SourceSpan span) -> ExprPtr;
auto make_call_expr(ExprPtr callee, std::vector<ExprPtr> args, SourceSpan span) -> ExprPtr;
auto make_block_expr(std::vector<StmtPtr> stmts, std::optional<ExprPtr> expr, SourceSpan span)
    -> ExprPtr;

// Create type helpers
auto make_named_type(std::string name, SourceSpan span) -> TypePtr;
auto make_ref_type(bool is_mut, TypePtr inner, SourceSpan span) -> TypePtr;

// Create pattern helpers
auto make_ident_pattern(std::string name, bool is_mut, SourceSpan span) -> PatternPtr;
auto make_wildcard_pattern(SourceSpan span) -> PatternPtr;

} // namespace tml::parser

#endif // TML_PARSER_AST_HPP
