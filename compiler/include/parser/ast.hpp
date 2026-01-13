//! # Abstract Syntax Tree (AST)
//!
//! This module defines the abstract syntax tree nodes for TML. The AST is
//! produced by the parser and consumed by semantic analysis and code generation.
//!
//! ## Architecture
//!
//! The AST uses a variant-based design with four main node categories:
//!
//! - **Types**: Type annotations and expressions (`Type`)
//! - **Patterns**: Pattern matching constructs (`Pattern`)
//! - **Expressions**: Value-producing constructs (`Expr`)
//! - **Declarations**: Top-level and nested definitions (`Decl`)
//!
//! Each category uses `std::variant` to represent the different node kinds,
//! with helper methods for type checking and casting.
//!
//! ## Ownership Model
//!
//! All child nodes are owned via `Box<T>` (unique pointer). This ensures
//! proper memory management and clear ownership semantics. The type aliases
//! `ExprPtr`, `StmtPtr`, `DeclPtr`, `PatternPtr`, and `TypePtr` are provided
//! for convenience.
//!
//! ## Source Spans
//!
//! Every AST node includes a `SourceSpan` for error reporting and debugging.
//! Spans are preserved through all compiler phases.
//!
//! ## TML-Specific Nodes
//!
//! - `WhenExpr` - Pattern matching (instead of `match`)
//! - `ClosureExpr` - Closures using `do(x) expr` syntax
//! - `TraitDecl` - Behaviors (TML's term for traits)
//! - `DynType` - Dynamic trait objects
//! - `LowlevelExpr` - Unsafe blocks (called `lowlevel` in TML)

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

// ============================================================================
// Forward Declarations
// ============================================================================

struct Expr;
struct Stmt;
struct Decl;
struct Pattern;
struct Type;

/// Owned pointer to an expression node.
using ExprPtr = Box<Expr>;

/// Owned pointer to a statement node.
using StmtPtr = Box<Stmt>;

/// Owned pointer to a declaration node.
using DeclPtr = Box<Decl>;

/// Owned pointer to a pattern node.
using PatternPtr = Box<Pattern>;

/// Owned pointer to a type node.
using TypePtr = Box<Type>;

// ============================================================================
// Type AST
// ============================================================================

/// A qualified type path like `Vec`, `std::io::File`, or `core::Option`.
///
/// Used for named types, trait bounds, and path expressions.
struct TypePath {
    std::vector<std::string> segments; ///< Path segments (e.g., ["std", "io", "File"]).
    SourceSpan span;                   ///< Source location.
};

/// A generic argument, which can be a type, const expression, or binding.
///
/// # Examples
///
/// - Type argument: `[I32]` in `Vec[I32]`
/// - Const argument: `[100]` in `Array[I32, 100]`
/// - Associated type binding: `[Item=I32]` in `Iterator[Item=I32]`
struct GenericArg {
    std::variant<TypePtr, ExprPtr> value; ///< The type or const expression value.
    bool is_const = false;                ///< True if this is a const generic argument.
    std::optional<std::string> name;      ///< Binding name for associated types (e.g., "Item").
    SourceSpan span;                      ///< Source location.

    /// Creates a type argument.
    static GenericArg from_type(TypePtr type, SourceSpan sp) {
        return GenericArg{std::move(type), false, std::nullopt, sp};
    }

    /// Creates a const generic argument.
    static GenericArg from_const(ExprPtr expr, SourceSpan sp) {
        return GenericArg{std::move(expr), true, std::nullopt, sp};
    }

    /// Creates an associated type binding.
    static GenericArg from_binding(std::string binding_name, TypePtr type, SourceSpan sp) {
        return GenericArg{std::move(type), false, std::move(binding_name), sp};
    }

    /// Returns true if this is a type argument.
    [[nodiscard]] bool is_type() const {
        return std::holds_alternative<TypePtr>(value);
    }

    /// Returns true if this is a const expression argument.
    [[nodiscard]] bool is_expr() const {
        return std::holds_alternative<ExprPtr>(value);
    }

    /// Returns true if this is an associated type binding.
    [[nodiscard]] bool is_binding() const {
        return name.has_value();
    }

    /// Gets the type value. Requires `is_type()`.
    [[nodiscard]] TypePtr& as_type() {
        return std::get<TypePtr>(value);
    }

    /// Gets the type value (const). Requires `is_type()`.
    [[nodiscard]] const TypePtr& as_type() const {
        return std::get<TypePtr>(value);
    }

    /// Gets the expression value. Requires `is_expr()`.
    [[nodiscard]] ExprPtr& as_expr() {
        return std::get<ExprPtr>(value);
    }

    /// Gets the expression value (const). Requires `is_expr()`.
    [[nodiscard]] const ExprPtr& as_expr() const {
        return std::get<ExprPtr>(value);
    }
};

/// A list of generic arguments: `[T, U]` or `[I32, 100]`.
struct GenericArgs {
    std::vector<GenericArg> args; ///< The generic arguments.
    SourceSpan span;              ///< Source location.

    /// Counts the number of type arguments (for validation).
    [[nodiscard]] size_t type_arg_count() const {
        size_t count = 0;
        for (const auto& arg : args) {
            if (arg.is_type()) {
                count++;
            }
        }
        return count;
    }

    /// Counts the number of const arguments (for validation).
    [[nodiscard]] size_t const_arg_count() const {
        size_t count = 0;
        for (const auto& arg : args) {
            if (arg.is_const) {
                count++;
            }
        }
        return count;
    }
};

/// Reference type: `ref T` or `mut ref T` (TML syntax for `&T` / `&mut T`).
struct RefType {
    bool is_mut;     ///< True for mutable reference.
    TypePtr inner;   ///< The referenced type.
    SourceSpan span; ///< Source location.
};

/// Raw pointer type: `*const T` or `*mut T`.
struct PtrType {
    bool is_mut;     ///< True for mutable pointer.
    TypePtr inner;   ///< The pointed-to type.
    SourceSpan span; ///< Source location.
};

/// Fixed-size array type: `[T; N]`.
struct ArrayType {
    TypePtr element; ///< Element type.
    ExprPtr size;    ///< Size expression (must be const).
    SourceSpan span; ///< Source location.
};

/// Slice type: `[T]`.
struct SliceType {
    TypePtr element; ///< Element type.
    SourceSpan span; ///< Source location.
};

/// Tuple type: `(T, U, V)`.
struct TupleType {
    std::vector<TypePtr> elements; ///< Element types.
    SourceSpan span;               ///< Source location.
};

/// Function type: `func(A, B) -> R`.
struct FuncType {
    std::vector<TypePtr> params; ///< Parameter types.
    TypePtr return_type;         ///< Return type (nullptr for unit).
    SourceSpan span;             ///< Source location.
};

/// Named type with optional generics: `Vec[T]`, `HashMap[K, V]`.
struct NamedType {
    TypePath path;                       ///< The type path.
    std::optional<GenericArgs> generics; ///< Generic arguments.
    SourceSpan span;                     ///< Source location.
};

/// Inferred type: `_` (let compiler infer).
struct InferType {
    SourceSpan span; ///< Source location.
};

/// Dynamic trait object type: `dyn Behavior[T]`.
///
/// Represents a type-erased value that implements a behavior.
struct DynType {
    TypePath behavior;                   ///< The behavior being used as trait object.
    std::optional<GenericArgs> generics; ///< Generic parameters (e.g., `dyn Iterator[I32]`).
    bool is_mut;                         ///< True for `dyn mut Behavior`.
    SourceSpan span;                     ///< Source location.
};

/// Opaque impl return type: `impl Behavior[T]`.
///
/// Represents "some type that implements Behavior" without revealing
/// the concrete type. Used for return types.
struct ImplBehaviorType {
    TypePath behavior;                   ///< The behavior being implemented.
    std::optional<GenericArgs> generics; ///< Generic parameters.
    SourceSpan span;                     ///< Source location.
};

/// A type expression.
///
/// Encompasses all type constructs in TML: named types, references,
/// pointers, arrays, slices, tuples, functions, and trait objects.
struct Type {
    std::variant<NamedType, RefType, PtrType, ArrayType, SliceType, TupleType, FuncType, InferType,
                 DynType, ImplBehaviorType>
        kind;        ///< The type variant.
    SourceSpan span; ///< Source location.

    /// Checks if this type is of kind `T`.
    template <typename T> [[nodiscard]] auto is() const -> bool {
        return std::holds_alternative<T>(kind);
    }

    /// Gets this type as kind `T`. Throws if not that kind.
    template <typename T> [[nodiscard]] auto as() -> T& {
        if (!is<T>()) {
            throw std::bad_variant_access();
        }
        return std::get<T>(kind);
    }

    /// Gets this type as kind `T` (const). Throws if not that kind.
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

/// Wildcard pattern: `_`.
///
/// Matches any value and discards it.
struct WildcardPattern {
    SourceSpan span; ///< Source location.
};

/// Identifier pattern: `x` or `mut x`.
///
/// Binds a value to a name, optionally with type annotation.
struct IdentPattern {
    std::string name;                       ///< The bound name.
    bool is_mut;                            ///< True for mutable binding.
    std::optional<TypePtr> type_annotation; ///< Optional type annotation.
    SourceSpan span;                        ///< Source location.
};

/// Literal pattern: `42`, `"hello"`, `true`.
///
/// Matches a specific literal value.
struct LiteralPattern {
    lexer::Token literal; ///< The literal token.
    SourceSpan span;      ///< Source location.
};

/// Tuple pattern: `(a, b, c)`.
struct TuplePattern {
    std::vector<PatternPtr> elements; ///< Element patterns.
    SourceSpan span;                  ///< Source location.
};

/// Struct pattern: `Point { x, y }` or `Point { x, .. }`.
struct StructPattern {
    TypePath path;                                          ///< The struct type.
    std::vector<std::pair<std::string, PatternPtr>> fields; ///< Field bindings.
    bool has_rest;                                          ///< True if `..` present.
    SourceSpan span;                                        ///< Source location.
};

/// Enum variant pattern: `Just(x)`, `Nothing`.
struct EnumPattern {
    TypePath path;                                  ///< The enum variant path.
    std::optional<std::vector<PatternPtr>> payload; ///< Variant payload patterns.
    SourceSpan span;                                ///< Source location.
};

/// Or pattern: `a | b | c`.
///
/// Matches if any of the alternatives match.
struct OrPattern {
    std::vector<PatternPtr> patterns; ///< Alternative patterns.
    SourceSpan span;                  ///< Source location.
};

/// Range pattern: `0 to 10` or `'a' through 'z'`.
struct RangePattern {
    std::optional<ExprPtr> start; ///< Start of range (optional for `..end`).
    std::optional<ExprPtr> end;   ///< End of range (optional for `start..`).
    bool inclusive;               ///< True for inclusive (`through`), false for exclusive (`to`).
    SourceSpan span;              ///< Source location.
};

/// Array/slice pattern: `[a, b, c]` or `[head, ..rest]`.
struct ArrayPattern {
    std::vector<PatternPtr> elements; ///< Element patterns.
    std::optional<PatternPtr> rest;   ///< Rest pattern for `[head, ..rest]`.
    SourceSpan span;                  ///< Source location.
};

/// A pattern for destructuring and matching values.
///
/// Patterns are used in:
/// - `let` bindings: `let (x, y) = point`
/// - `when` arms: `when value { Just(x) => ... }`
/// - `for` loops: `for (k, v) in map`
/// - Function parameters: `func add((x, y): Point)`
struct Pattern {
    std::variant<WildcardPattern, IdentPattern, LiteralPattern, TuplePattern, StructPattern,
                 EnumPattern, OrPattern, RangePattern, ArrayPattern>
        kind;        ///< The pattern variant.
    SourceSpan span; ///< Source location.

    /// Checks if this pattern is of kind `T`.
    template <typename T> [[nodiscard]] auto is() const -> bool {
        return std::holds_alternative<T>(kind);
    }

    /// Gets this pattern as kind `T`. Throws if not that kind.
    template <typename T> [[nodiscard]] auto as() -> T& {
        if (!is<T>()) {
            throw std::bad_variant_access();
        }
        return std::get<T>(kind);
    }

    /// Gets this pattern as kind `T` (const). Throws if not that kind.
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

/// Literal expression: `42`, `3.14`, `"hello"`, `'a'`, `true`.
struct LiteralExpr {
    lexer::Token token; ///< The literal token with value.
    SourceSpan span;    ///< Source location.
};

/// Identifier expression: `foo`, `bar`.
struct IdentExpr {
    std::string name; ///< The identifier name.
    SourceSpan span;  ///< Source location.
};

/// Unary operators.
enum class UnaryOp {
    Neg,    ///< `-x` negation
    Not,    ///< `not x` / `!x` logical NOT
    BitNot, ///< `~x` bitwise NOT
    Ref,    ///< `ref x` / `&x` borrow
    RefMut, ///< `mut ref x` / `&mut x` mutable borrow
    Deref,  ///< `*x` dereference
    Inc,    ///< `x++` postfix increment
    Dec,    ///< `x--` postfix decrement
};

/// Unary expression: `-x`, `not x`, `ref x`, `*x`.
struct UnaryExpr {
    UnaryOp op;      ///< The operator.
    ExprPtr operand; ///< The operand expression.
    SourceSpan span; ///< Source location.
};

/// Binary operators.
enum class BinaryOp {
    // Arithmetic
    Add, ///< `+` addition
    Sub, ///< `-` subtraction
    Mul, ///< `*` multiplication
    Div, ///< `/` division
    Mod, ///< `%` remainder
    // Comparison
    Eq, ///< `==` equality
    Ne, ///< `!=` inequality
    Lt, ///< `<` less than
    Gt, ///< `>` greater than
    Le, ///< `<=` less or equal
    Ge, ///< `>=` greater or equal
    // Logical
    And, ///< `and` / `&&` logical AND
    Or,  ///< `or` / `||` logical OR
    // Bitwise
    BitAnd, ///< `&` bitwise AND
    BitOr,  ///< `|` bitwise OR
    BitXor, ///< `xor` / `^` bitwise XOR
    Shl,    ///< `shl` / `<<` shift left
    Shr,    ///< `shr` / `>>` shift right
    // Assignment
    Assign,       ///< `=` assignment
    AddAssign,    ///< `+=` add-assign
    SubAssign,    ///< `-=` sub-assign
    MulAssign,    ///< `*=` mul-assign
    DivAssign,    ///< `/=` div-assign
    ModAssign,    ///< `%=` mod-assign
    BitAndAssign, ///< `&=` bitwise AND-assign
    BitOrAssign,  ///< `|=` bitwise OR-assign
    BitXorAssign, ///< `^=` bitwise XOR-assign
    ShlAssign,    ///< `<<=` shift left-assign
    ShrAssign,    ///< `>>=` shift right-assign
};

/// Binary expression: `a + b`, `a and b`, `x = y`.
struct BinaryExpr {
    BinaryOp op;     ///< The operator.
    ExprPtr left;    ///< Left operand.
    ExprPtr right;   ///< Right operand.
    SourceSpan span; ///< Source location.
};

/// Call expression: `foo(a, b)`.
struct CallExpr {
    ExprPtr callee;            ///< The function being called.
    std::vector<ExprPtr> args; ///< Call arguments.
    SourceSpan span;           ///< Source location.
};

/// Method call: `obj.method(a, b)` or `obj.method[T](a, b)`.
struct MethodCallExpr {
    ExprPtr receiver;               ///< The receiver object.
    std::string method;             ///< Method name.
    std::vector<TypePtr> type_args; ///< Generic type arguments.
    std::vector<ExprPtr> args;      ///< Call arguments.
    SourceSpan span;                ///< Source location.
};

/// Field access: `obj.field`.
struct FieldExpr {
    ExprPtr object;    ///< The object being accessed.
    std::string field; ///< Field name.
    SourceSpan span;   ///< Source location.
};

/// Index expression: `arr[i]`.
struct IndexExpr {
    ExprPtr object;  ///< The indexed collection.
    ExprPtr index;   ///< The index expression.
    SourceSpan span; ///< Source location.
};

/// Tuple expression: `(a, b, c)`.
struct TupleExpr {
    std::vector<ExprPtr> elements; ///< Tuple elements.
    SourceSpan span;               ///< Source location.
};

/// Array expression: `[1, 2, 3]` or `[0; 10]` (repeat syntax).
struct ArrayExpr {
    std::variant<std::vector<ExprPtr>,       ///< Element list: `[1, 2, 3]`
                 std::pair<ExprPtr, ExprPtr> ///< Repeat: `[expr; count]`
                 >
        kind;        ///< Array initialization form.
    SourceSpan span; ///< Source location.
};

/// Struct expression: `Point { x: 1, y: 2 }`.
struct StructExpr {
    TypePath path;                                       ///< Struct type path.
    std::optional<GenericArgs> generics;                 ///< Generic arguments.
    std::vector<std::pair<std::string, ExprPtr>> fields; ///< Field initializers.
    std::optional<ExprPtr> base;                         ///< Base for struct update (`..base`).
    SourceSpan span;                                     ///< Source location.
};

/// If expression: `if cond { then } else { else }`.
struct IfExpr {
    ExprPtr condition;                  ///< Condition expression.
    ExprPtr then_branch;                ///< Then branch.
    std::optional<ExprPtr> else_branch; ///< Optional else branch.
    SourceSpan span;                    ///< Source location.
};

/// Ternary expression: `condition ? true_value : false_value`.
struct TernaryExpr {
    ExprPtr condition;   ///< Condition.
    ExprPtr true_value;  ///< Value if true.
    ExprPtr false_value; ///< Value if false.
    SourceSpan span;     ///< Source location.
};

/// If-let expression: `if let pattern = expr { then } else { else }`.
struct IfLetExpr {
    PatternPtr pattern;                 ///< Pattern to match.
    ExprPtr scrutinee;                  ///< Value to match against.
    ExprPtr then_branch;                ///< Branch if matched.
    std::optional<ExprPtr> else_branch; ///< Branch if not matched.
    SourceSpan span;                    ///< Source location.
};

/// A single arm of a `when` expression.
struct WhenArm {
    PatternPtr pattern;           ///< Pattern to match.
    std::optional<ExprPtr> guard; ///< Optional guard condition.
    ExprPtr body;                 ///< Arm body.
    SourceSpan span;              ///< Source location.
};

/// When (match) expression: `when x { pat => expr, ... }`.
struct WhenExpr {
    ExprPtr scrutinee;         ///< Value being matched.
    std::vector<WhenArm> arms; ///< Match arms.
    SourceSpan span;           ///< Source location.
};

/// Infinite loop: `loop { body }`.
struct LoopExpr {
    std::optional<std::string> label; ///< Optional loop label.
    ExprPtr body;                     ///< Loop body.
    SourceSpan span;                  ///< Source location.
};

/// While loop: `while cond { body }`.
struct WhileExpr {
    std::optional<std::string> label; ///< Optional loop label.
    ExprPtr condition;                ///< Loop condition.
    ExprPtr body;                     ///< Loop body.
    SourceSpan span;                  ///< Source location.
};

/// For loop: `for x in iter { body }`.
struct ForExpr {
    std::optional<std::string> label; ///< Optional loop label.
    PatternPtr pattern;               ///< Loop variable pattern.
    ExprPtr iter;                     ///< Iterator expression.
    ExprPtr body;                     ///< Loop body.
    SourceSpan span;                  ///< Source location.
};

/// Block expression: `{ stmts; expr }`.
struct BlockExpr {
    std::vector<StmtPtr> stmts;  ///< Statements in the block.
    std::optional<ExprPtr> expr; ///< Trailing expression (no semicolon).
    SourceSpan span;             ///< Source location.
};

/// Return expression: `return x`.
struct ReturnExpr {
    std::optional<ExprPtr> value; ///< Return value (optional).
    SourceSpan span;              ///< Source location.
};

/// Break expression: `break 'label x`.
struct BreakExpr {
    std::optional<std::string> label; ///< Target loop label.
    std::optional<ExprPtr> value;     ///< Break value (for loop expressions).
    SourceSpan span;                  ///< Source location.
};

/// Continue expression: `continue 'label`.
struct ContinueExpr {
    std::optional<std::string> label; ///< Target loop label.
    SourceSpan span;                  ///< Source location.
};

/// Closure expression: `do(x, y) x + y`.
///
/// TML uses `do` syntax instead of `|x|` for closures.
struct ClosureExpr {
    std::vector<std::pair<PatternPtr, std::optional<TypePtr>>> params; ///< Parameters.
    std::optional<TypePtr> return_type;                                ///< Optional return type.
    ExprPtr body;                                                      ///< Closure body.
    bool is_move;                                                      ///< True for move closures.
    SourceSpan span;                                                   ///< Source location.

    /// Captured variables (filled during type checking).
    mutable std::vector<std::string> captured_vars;
};

/// Range expression: `a to b`, `a through b`, `to b`, `a to`.
struct RangeExpr {
    std::optional<ExprPtr> start; ///< Start (optional for `to end`).
    std::optional<ExprPtr> end;   ///< End (optional for `start to`).
    bool inclusive;               ///< True for `through` (inclusive).
    SourceSpan span;              ///< Source location.
};

/// Cast expression: `x as T`.
struct CastExpr {
    ExprPtr expr;    ///< Expression to cast.
    TypePtr target;  ///< Target type.
    SourceSpan span; ///< Source location.
};

/// Try expression: `expr?` (error propagation).
struct TryExpr {
    ExprPtr expr;    ///< Expression that may fail.
    SourceSpan span; ///< Source location.
};

/// Await expression: `expr.await`.
struct AwaitExpr {
    ExprPtr expr;    ///< Future to await.
    SourceSpan span; ///< Source location.
};

/// Path expression: `std::io::stdout` or `List[I32]`.
struct PathExpr {
    TypePath path;                       ///< The path.
    std::optional<GenericArgs> generics; ///< Generic arguments.
    SourceSpan span;                     ///< Source location.
};

/// Lowlevel (unsafe) block: `lowlevel { ... }`.
///
/// TML uses `lowlevel` instead of `unsafe` for clarity.
struct LowlevelExpr {
    std::vector<StmtPtr> stmts;  ///< Statements in the block.
    std::optional<ExprPtr> expr; ///< Trailing expression.
    SourceSpan span;             ///< Source location.
};

/// A segment of an interpolated string.
struct InterpolatedSegment {
    std::variant<std::string, ///< Literal text segment.
                 ExprPtr      ///< Interpolated expression: `{expr}`.
                 >
        content;     ///< Segment content.
    SourceSpan span; ///< Source location.
};

/// Interpolated string: `"Hello {name}, you are {age} years old"`.
struct InterpolatedStringExpr {
    std::vector<InterpolatedSegment> segments; ///< String segments.
    SourceSpan span;                           ///< Source location.
};

/// An expression (value-producing construct).
///
/// Expressions are the core of TML programs. They can be literals, operations,
/// control flow, function calls, and more. Every expression has a type and
/// produces a value (even if that value is unit `()`).
struct Expr {
    std::variant<LiteralExpr, IdentExpr, UnaryExpr, BinaryExpr, CallExpr, MethodCallExpr, FieldExpr,
                 IndexExpr, TupleExpr, ArrayExpr, StructExpr, IfExpr, TernaryExpr, IfLetExpr,
                 WhenExpr, LoopExpr, WhileExpr, ForExpr, BlockExpr, ReturnExpr, BreakExpr,
                 ContinueExpr, ClosureExpr, RangeExpr, CastExpr, TryExpr, AwaitExpr, PathExpr,
                 LowlevelExpr, InterpolatedStringExpr>
        kind;        ///< The expression variant.
    SourceSpan span; ///< Source location.

    /// Checks if this expression is of kind `T`.
    template <typename T> [[nodiscard]] auto is() const -> bool {
        return std::holds_alternative<T>(kind);
    }

    /// Gets this expression as kind `T`. Throws if not that kind.
    template <typename T> [[nodiscard]] auto as() -> T& {
        if (!is<T>()) {
            throw std::bad_variant_access();
        }
        return std::get<T>(kind);
    }

    /// Gets this expression as kind `T` (const). Throws if not that kind.
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

/// Let statement: `let x = expr` or `let x: T = expr`.
///
/// Creates an immutable binding (use `var` for mutable).
struct LetStmt {
    PatternPtr pattern;                     ///< Binding pattern.
    std::optional<TypePtr> type_annotation; ///< Optional type annotation.
    std::optional<ExprPtr> init;            ///< Initializer expression.
    SourceSpan span;                        ///< Source location.
};

/// Var statement: `var x = expr` (mutable binding).
///
/// Equivalent to `let mut x = expr`.
struct VarStmt {
    std::string name;                       ///< Variable name.
    std::optional<TypePtr> type_annotation; ///< Optional type annotation.
    ExprPtr init;                           ///< Initializer expression.
    SourceSpan span;                        ///< Source location.
};

/// Expression statement: `expr;`.
struct ExprStmt {
    ExprPtr expr;    ///< The expression.
    SourceSpan span; ///< Source location.
};

/// A statement (side-effecting construct).
///
/// Statements include variable bindings, expression statements,
/// and nested declarations.
struct Stmt {
    std::variant<LetStmt, VarStmt, ExprStmt,
                 DeclPtr ///< Nested declaration.
                 >
        kind;        ///< The statement variant.
    SourceSpan span; ///< Source location.

    /// Checks if this statement is of kind `T`.
    template <typename T> [[nodiscard]] auto is() const -> bool {
        return std::holds_alternative<T>(kind);
    }

    /// Gets this statement as kind `T`. Throws if not that kind.
    template <typename T> [[nodiscard]] auto as() -> T& {
        if (!is<T>()) {
            throw std::bad_variant_access();
        }
        return std::get<T>(kind);
    }

    /// Gets this statement as kind `T` (const). Throws if not that kind.
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

/// Item visibility modifier.
enum class Visibility {
    Private,  ///< Default - private to the current module.
    Public,   ///< `pub` - visible everywhere.
    PubCrate, ///< `pub(crate)` - visible within the current crate only.
};

/// A generic parameter: `T`, `T: Behavior`, `T: Behavior[U]`, or `const N: U64`.
struct GenericParam {
    std::string name; ///< Parameter name.
    std::vector<TypePtr>
        bounds;            ///< Behavior bounds (for type params), supports parameterized bounds.
    bool is_const = false; ///< True for const generic params.
    std::optional<TypePtr> const_type;   ///< Type of const param (e.g., `U64`).
    std::optional<TypePtr> default_type; ///< Default type (e.g., `T = This`).
    SourceSpan span;                     ///< Source location.
};

/// A decorator/attribute: `@derive(Clone, Debug)`, `@test`, `@inline`.
struct Decorator {
    std::string name;          ///< Decorator name.
    std::vector<ExprPtr> args; ///< Optional arguments.
    SourceSpan span;           ///< Source location.
};

/// Where clause: `where T: Clone, U: Hash, T = U`.
struct WhereClause {
    /// Behavior bounds: `T: Behavior1 + Behavior2`.
    std::vector<std::pair<TypePtr, std::vector<TypePtr>>> constraints;
    /// Type equalities: `T = U`.
    std::vector<std::pair<TypePtr, TypePtr>> type_equalities;
    SourceSpan span; ///< Source location.
};

/// A function parameter.
struct FuncParam {
    PatternPtr pattern; ///< Parameter pattern.
    TypePtr type;       ///< Parameter type.
    SourceSpan span;    ///< Source location.
};

/// Function declaration.
///
/// # Examples
///
/// ```tml
/// func add(a: I32, b: I32) -> I32 { a + b }
/// pub async func fetch[T](url: String) -> Outcome[T, Error] { ... }
/// @extern("c") func printf(fmt: *const I8, ...) -> I32
/// ```
struct FuncDecl {
    std::optional<std::string> doc;          ///< Documentation comment (from `///`).
    std::vector<Decorator> decorators;       ///< Decorators.
    Visibility vis;                          ///< Visibility.
    std::string name;                        ///< Function name.
    std::vector<GenericParam> generics;      ///< Generic parameters.
    std::vector<FuncParam> params;           ///< Parameters.
    std::optional<TypePtr> return_type;      ///< Return type.
    std::optional<WhereClause> where_clause; ///< Where clause.
    std::optional<BlockExpr> body;           ///< Body (none for signatures/extern).
    bool is_async;                           ///< True for `async func`.
    bool is_unsafe;                          ///< True for `lowlevel func`.
    SourceSpan span;                         ///< Source location.

    // FFI support
    std::optional<std::string> extern_abi;  ///< ABI: "c", "c++", etc.
    std::optional<std::string> extern_name; ///< Symbol name if different.
    std::vector<std::string> link_libs;     ///< Libraries to link.
};

/// A struct field.
struct StructField {
    std::optional<std::string> doc; ///< Documentation comment (from `///`).
    Visibility vis;                 ///< Field visibility.
    std::string name;               ///< Field name.
    TypePtr type;                   ///< Field type.
    SourceSpan span;                ///< Source location.
};

/// Struct declaration.
///
/// # Example
///
/// ```tml
/// type Point[T] {
///     x: T,
///     y: T,
/// }
/// ```
struct StructDecl {
    std::optional<std::string> doc;          ///< Documentation comment (from `///`).
    std::vector<Decorator> decorators;       ///< Decorators.
    Visibility vis;                          ///< Visibility.
    std::string name;                        ///< Struct name.
    std::vector<GenericParam> generics;      ///< Generic parameters.
    std::vector<StructField> fields;         ///< Fields.
    std::optional<WhereClause> where_clause; ///< Where clause.
    SourceSpan span;                         ///< Source location.
};

/// An enum variant.
struct EnumVariant {
    std::optional<std::string> doc;                        ///< Documentation comment (from `///`).
    std::string name;                                      ///< Variant name.
    std::optional<std::vector<TypePtr>> tuple_fields;      ///< Tuple variant fields.
    std::optional<std::vector<StructField>> struct_fields; ///< Struct variant fields.
    SourceSpan span;                                       ///< Source location.
};

/// Enum declaration.
///
/// # Example
///
/// ```tml
/// type Maybe[T] {
///     Just(T),
///     Nothing,
/// }
/// ```
struct EnumDecl {
    std::optional<std::string> doc;          ///< Documentation comment (from `///`).
    std::vector<Decorator> decorators;       ///< Decorators.
    Visibility vis;                          ///< Visibility.
    std::string name;                        ///< Enum name.
    std::vector<GenericParam> generics;      ///< Generic parameters.
    std::vector<EnumVariant> variants;       ///< Variants.
    std::optional<WhereClause> where_clause; ///< Where clause.
    SourceSpan span;                         ///< Source location.
};

/// Associated type declaration in a behavior.
///
/// # Examples
///
/// ```tml
/// type Item                    // Basic
/// type Item = I32              // With default
/// type Item[T]                 // GAT (Generic Associated Type)
/// type Item: Display           // With bounds
/// ```
struct AssociatedType {
    std::string name;                    ///< Type name.
    std::vector<GenericParam> generics;  ///< GAT generic parameters.
    std::vector<TypePtr> bounds;         ///< Behavior bounds (supports parameterized bounds).
    std::optional<TypePtr> default_type; ///< Optional default.
    SourceSpan span;                     ///< Source location.
};

/// Associated type binding in an impl block.
///
/// # Example
///
/// ```tml
/// impl Iterator for MyIter {
///     type Item = I32
/// }
/// ```
struct AssociatedTypeBinding {
    std::string name;                   ///< Type name.
    std::vector<GenericParam> generics; ///< GAT parameters in binding.
    TypePtr type;                       ///< The concrete type.
    SourceSpan span;                    ///< Source location.
};

/// Behavior (trait) declaration.
///
/// TML uses `behavior` instead of Rust's `trait`.
///
/// # Example
///
/// ```tml
/// behavior Iterator {
///     type Item
///     func next(mut this) -> Maybe[This::Item]
/// }
/// ```
struct TraitDecl {
    std::optional<std::string> doc;               ///< Documentation comment (from `///`).
    std::vector<Decorator> decorators;            ///< Decorators.
    Visibility vis;                               ///< Visibility.
    std::string name;                             ///< Behavior name.
    std::vector<GenericParam> generics;           ///< Generic parameters.
    std::vector<TypePtr> super_traits;            ///< Super-behaviors.
    std::vector<AssociatedType> associated_types; ///< Associated types.
    std::vector<FuncDecl> methods;                ///< Method signatures/defaults.
    std::optional<WhereClause> where_clause;      ///< Where clause.
    SourceSpan span;                              ///< Source location.
};

// Forward declaration for ConstDecl (defined below)
struct ConstDecl;

/// Implementation block.
///
/// # Examples
///
/// ```tml
/// impl Point {                    // Inherent impl
///     func new(x: I32, y: I32) -> This { ... }
/// }
///
/// impl Display for Point {        // Behavior impl
///     func fmt(this, f: mut Formatter) -> Outcome[(), Error] { ... }
/// }
/// ```
struct ImplDecl {
    std::optional<std::string> doc;                   ///< Documentation comment (from `///`).
    std::vector<GenericParam> generics;               ///< Generic parameters.
    TypePtr trait_type;                               ///< Behavior being implemented (optional).
    TypePtr self_type;                                ///< Type being implemented.
    std::vector<AssociatedTypeBinding> type_bindings; ///< Associated type bindings.
    std::vector<ConstDecl> constants;                 ///< Associated constants.
    std::vector<FuncDecl> methods;                    ///< Method implementations.
    std::optional<WhereClause> where_clause;          ///< Where clause.
    SourceSpan span;                                  ///< Source location.
};

/// Type alias: `type Alias = OriginalType`.
struct TypeAliasDecl {
    std::optional<std::string> doc;     ///< Documentation comment (from `///`).
    Visibility vis;                     ///< Visibility.
    std::string name;                   ///< Alias name.
    std::vector<GenericParam> generics; ///< Generic parameters.
    TypePtr type;                       ///< Aliased type.
    SourceSpan span;                    ///< Source location.
};

/// Constant declaration: `const PI: F64 = 3.14159`.
struct ConstDecl {
    std::optional<std::string> doc; ///< Documentation comment (from `///`).
    Visibility vis;                 ///< Visibility.
    std::string name;               ///< Constant name.
    TypePtr type;                   ///< Constant type.
    ExprPtr value;                  ///< Constant value.
    SourceSpan span;                ///< Source location.
};

/// Use declaration for imports.
///
/// # Examples
///
/// ```tml
/// use std::io::Read               // Single import
/// use std::math::{abs, sqrt}      // Grouped import
/// use std::collections::*         // Glob import
/// use std::io::Read as IoRead     // Aliased import
/// ```
struct UseDecl {
    Visibility vis;                                  ///< Visibility.
    TypePath path;                                   ///< Import path.
    std::optional<std::string> alias;                ///< Alias (`as Name`).
    std::optional<std::vector<std::string>> symbols; ///< Grouped symbols.
    bool is_glob = false;                            ///< True for `*` imports.
    SourceSpan span;                                 ///< Source location.
};

/// Module declaration.
///
/// # Examples
///
/// ```tml
/// mod foo;                        // External file
/// mod bar { ... }                 // Inline module
/// pub mod utils { ... }           // Public module
/// ```
struct ModDecl {
    Visibility vis;                            ///< Visibility.
    std::string name;                          ///< Module name.
    std::optional<std::vector<DeclPtr>> items; ///< Items (none for file modules).
    SourceSpan span;                           ///< Source location.
};

/// A top-level or nested declaration.
///
/// Declarations define named items: functions, types, behaviors, constants,
/// modules, and imports.
struct Decl {
    std::variant<FuncDecl, StructDecl, EnumDecl, TraitDecl, ImplDecl, TypeAliasDecl, ConstDecl,
                 UseDecl, ModDecl>
        kind;        ///< The declaration variant.
    SourceSpan span; ///< Source location.

    /// Checks if this declaration is of kind `T`.
    template <typename T> [[nodiscard]] auto is() const -> bool {
        return std::holds_alternative<T>(kind);
    }

    /// Gets this declaration as kind `T`. Throws if not that kind.
    template <typename T> [[nodiscard]] auto as() -> T& {
        if (!is<T>()) {
            throw std::bad_variant_access();
        }
        return std::get<T>(kind);
    }

    /// Gets this declaration as kind `T` (const). Throws if not that kind.
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

/// A TML module (compilation unit).
///
/// Represents a single source file after parsing. Contains all top-level
/// declarations.
struct Module {
    std::string name;                     ///< Module name.
    std::vector<std::string> module_docs; ///< Module-level documentation (from `//!`).
    std::vector<DeclPtr> decls;           ///< Top-level declarations.
    SourceSpan span;                      ///< Source location.
};

// ============================================================================
// AST Utilities
// ============================================================================

/// Creates a literal expression from a token.
auto make_literal_expr(lexer::Token token) -> ExprPtr;

/// Creates an identifier expression.
auto make_ident_expr(std::string name, SourceSpan span) -> ExprPtr;

/// Creates a binary expression.
auto make_binary_expr(BinaryOp op, ExprPtr left, ExprPtr right, SourceSpan span) -> ExprPtr;

/// Creates a unary expression.
auto make_unary_expr(UnaryOp op, ExprPtr operand, SourceSpan span) -> ExprPtr;

/// Creates a call expression.
auto make_call_expr(ExprPtr callee, std::vector<ExprPtr> args, SourceSpan span) -> ExprPtr;

/// Creates a block expression.
auto make_block_expr(std::vector<StmtPtr> stmts, std::optional<ExprPtr> expr, SourceSpan span)
    -> ExprPtr;

/// Creates a named type.
auto make_named_type(std::string name, SourceSpan span) -> TypePtr;

/// Creates a reference type.
auto make_ref_type(bool is_mut, TypePtr inner, SourceSpan span) -> TypePtr;

/// Creates an identifier pattern.
auto make_ident_pattern(std::string name, bool is_mut, SourceSpan span) -> PatternPtr;

/// Creates a wildcard pattern.
auto make_wildcard_pattern(SourceSpan span) -> PatternPtr;

} // namespace tml::parser

#endif // TML_PARSER_AST_HPP
