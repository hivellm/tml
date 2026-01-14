//! # Expression AST Nodes
//!
//! This module defines the AST nodes for expressions (value-producing constructs).
//! Expressions are the core of TML programs - they compute values and drive execution.
//!
//! ## Expression Categories
//!
//! - **Literals**: `42`, `3.14`, `"hello"`, `true`
//! - **Identifiers**: `foo`, `bar`
//! - **Operators**: `-x`, `a + b`, `x = y`
//! - **Calls**: `foo(a, b)`, `obj.method(x)`
//! - **Access**: `obj.field`, `arr[i]`
//! - **Composites**: `(a, b)`, `[1, 2, 3]`, `Point { x: 1, y: 2 }`
//! - **Control flow**: `if`, `when`, `loop`, `while`, `for`
//! - **Blocks**: `{ stmts; expr }`
//! - **Jumps**: `return`, `break`, `continue`
//! - **Closures**: `do(x) x * 2`
//! - **Type operations**: `x as T`, `expr?`, `expr.await`
//! - **Lowlevel**: `lowlevel { ... }`
//!
//! ## TML-Specific Expressions
//!
//! - `WhenExpr` - Pattern matching (TML uses `when` instead of `match`)
//! - `ClosureExpr` - Closures use `do(x) expr` syntax instead of `|x| expr`
//! - `RangeExpr` - Ranges use `to`/`through` instead of `..`/`..=`
//! - `LowlevelExpr` - Unsafe blocks use `lowlevel` keyword

#ifndef TML_PARSER_AST_EXPRS_HPP
#define TML_PARSER_AST_EXPRS_HPP

#include "ast_common.hpp"
#include "ast_patterns.hpp"
#include "ast_types.hpp"

namespace tml::parser {

// Forward declaration for statements (used in blocks)
struct Stmt;
using StmtPtr = Box<Stmt>;

// ============================================================================
// Literals and Identifiers
// ============================================================================

/// Literal expression: `42`, `3.14`, `"hello"`, `'a'`, `true`.
///
/// Represents compile-time constant values. The token contains
/// both the kind and value of the literal.
///
/// # Supported Literals
///
/// - Integers: `42`, `0xFF`, `0b1010`, `1_000_000`
/// - Floats: `3.14`, `1.0e-10`
/// - Strings: `"hello"`, `"line1\nline2"`
/// - Characters: `'a'`, `'\n'`
/// - Booleans: `true`, `false`
/// - Null: `null`
struct LiteralExpr {
    lexer::Token token; ///< The literal token with value.
    SourceSpan span;    ///< Source location.
};

/// Identifier expression: `foo`, `bar`.
///
/// References a variable, function, or other named item in scope.
///
/// # Example
///
/// ```tml
/// let x = 42
/// let y = x + 1    // `x` is an IdentExpr
/// ```
struct IdentExpr {
    std::string name; ///< The identifier name.
    SourceSpan span;  ///< Source location.
};

// ============================================================================
// Operators
// ============================================================================

/// Unary operators.
enum class UnaryOp {
    Neg,    ///< `-x` arithmetic negation
    Not,    ///< `not x` / `!x` logical NOT
    BitNot, ///< `~x` bitwise NOT
    Ref,    ///< `ref x` / `&x` immutable borrow
    RefMut, ///< `mut ref x` / `&mut x` mutable borrow
    Deref,  ///< `*x` dereference
    Inc,    ///< `x++` postfix increment
    Dec,    ///< `x--` postfix decrement
};

/// Unary expression: `-x`, `not x`, `ref x`, `*x`.
///
/// Applies a unary operator to a single operand.
///
/// # Examples
///
/// ```tml
/// let neg = -value
/// let borrowed = ref data
/// let dereferenced = *pointer
/// ```
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
///
/// Applies a binary operator to two operands.
///
/// # Examples
///
/// ```tml
/// let sum = a + b
/// let valid = x > 0 and x < 100
/// count = count + 1
/// ```
struct BinaryExpr {
    BinaryOp op;     ///< The operator.
    ExprPtr left;    ///< Left operand.
    ExprPtr right;   ///< Right operand.
    SourceSpan span; ///< Source location.
};

// ============================================================================
// Calls and Access
// ============================================================================

/// Call expression: `foo(a, b)`.
///
/// Calls a function with zero or more arguments.
///
/// # Examples
///
/// ```tml
/// print("hello")
/// let result = calculate(x, y, z)
/// ```
struct CallExpr {
    ExprPtr callee;            ///< The function being called.
    std::vector<ExprPtr> args; ///< Call arguments.
    SourceSpan span;           ///< Source location.
};

/// Method call: `obj.method(a, b)` or `obj.method[T](a, b)`.
///
/// Calls a method on a receiver object, with optional generic type arguments.
///
/// # Examples
///
/// ```tml
/// vec.push(item)
/// str.parse[I32]()
/// list.map(do(x) x * 2)
/// ```
struct MethodCallExpr {
    ExprPtr receiver;               ///< The receiver object.
    std::string method;             ///< Method name.
    std::vector<TypePtr> type_args; ///< Generic type arguments (turbofish).
    std::vector<ExprPtr> args;      ///< Call arguments.
    SourceSpan span;                ///< Source location.
};

/// Field access: `obj.field`.
///
/// Accesses a field of a struct or tuple.
///
/// # Examples
///
/// ```tml
/// let x = point.x
/// let name = person.name
/// let first = tuple.0         // Tuple field access
/// ```
struct FieldExpr {
    ExprPtr object;    ///< The object being accessed.
    std::string field; ///< Field name (or index for tuples).
    SourceSpan span;   ///< Source location.
};

/// Index expression: `arr[i]`.
///
/// Accesses an element of an array, slice, or indexable collection.
///
/// # Examples
///
/// ```tml
/// let first = array[0]
/// map["key"] = value
/// matrix[row][col] = 0
/// ```
struct IndexExpr {
    ExprPtr object;  ///< The indexed collection.
    ExprPtr index;   ///< The index expression.
    SourceSpan span; ///< Source location.
};

// ============================================================================
// Composite Expressions
// ============================================================================

/// Tuple expression: `(a, b, c)`.
///
/// Creates a tuple from multiple values.
///
/// # Examples
///
/// ```tml
/// let point = (10, 20)
/// let triple = (1, "hello", true)
/// ```
struct TupleExpr {
    std::vector<ExprPtr> elements; ///< Tuple elements.
    SourceSpan span;               ///< Source location.
};

/// Array expression: `[1, 2, 3]` or `[0; 10]` (repeat syntax).
///
/// Creates an array either by listing elements or repeating a value.
///
/// # Examples
///
/// ```tml
/// let nums = [1, 2, 3, 4, 5]     // Element list
/// let zeros = [0; 100]           // Repeat: 100 zeros
/// ```
struct ArrayExpr {
    std::variant<std::vector<ExprPtr>,       ///< Element list: `[1, 2, 3]`
                 std::pair<ExprPtr, ExprPtr> ///< Repeat: `[expr; count]`
                 >
        kind;        ///< Array initialization form.
    SourceSpan span; ///< Source location.
};

/// Struct expression: `Point { x: 1, y: 2 }`.
///
/// Creates a struct instance by initializing its fields.
/// Supports struct update syntax with `..base`.
///
/// # Examples
///
/// ```tml
/// let p = Point { x: 10, y: 20 }
/// let q = Point { x: 5, ..p }        // Struct update
/// let config = Config { debug, ..Default::default() }
/// ```
struct StructExpr {
    TypePath path;                                       ///< Struct type path.
    std::optional<GenericArgs> generics;                 ///< Generic arguments.
    std::vector<std::pair<std::string, ExprPtr>> fields; ///< Field initializers.
    std::optional<ExprPtr> base;                         ///< Base for struct update (`..base`).
    SourceSpan span;                                     ///< Source location.
};

// ============================================================================
// Control Flow
// ============================================================================

/// If expression: `if cond { then } else { else }`.
///
/// Conditionally executes branches based on a boolean condition.
/// The else branch is optional.
///
/// # Examples
///
/// ```tml
/// let max = if a > b { a } else { b }
/// if debug { print("debug mode") }
/// ```
struct IfExpr {
    ExprPtr condition;                  ///< Condition expression (must be Bool).
    ExprPtr then_branch;                ///< Then branch.
    std::optional<ExprPtr> else_branch; ///< Optional else branch.
    SourceSpan span;                    ///< Source location.
};

/// Ternary expression: `condition ? true_value : false_value`.
///
/// Compact conditional expression. Equivalent to if-else but inline.
///
/// # Example
///
/// ```tml
/// let sign = x >= 0 ? "positive" : "negative"
/// ```
struct TernaryExpr {
    ExprPtr condition;   ///< Condition.
    ExprPtr true_value;  ///< Value if true.
    ExprPtr false_value; ///< Value if false.
    SourceSpan span;     ///< Source location.
};

/// If-let expression: `if let pattern = expr { then } else { else }`.
///
/// Combines pattern matching with conditional execution.
/// Useful for matching single patterns without full `when`.
///
/// # Example
///
/// ```tml
/// if let Just(value) = maybe_value {
///     process(value)
/// } else {
///     handle_nothing()
/// }
/// ```
struct IfLetExpr {
    PatternPtr pattern;                 ///< Pattern to match.
    ExprPtr scrutinee;                  ///< Value to match against.
    ExprPtr then_branch;                ///< Branch if matched.
    std::optional<ExprPtr> else_branch; ///< Branch if not matched.
    SourceSpan span;                    ///< Source location.
};

/// A single arm of a `when` expression.
///
/// Each arm has a pattern to match, an optional guard condition,
/// and a body expression to execute if matched.
struct WhenArm {
    PatternPtr pattern;           ///< Pattern to match.
    std::optional<ExprPtr> guard; ///< Optional guard condition (`if expr`).
    ExprPtr body;                 ///< Arm body expression.
    SourceSpan span;              ///< Source location.
};

/// When (match) expression: `when x { pat => expr, ... }`.
///
/// TML's pattern matching expression. Evaluates the scrutinee and
/// executes the first matching arm's body.
///
/// # Example
///
/// ```tml
/// when status {
///     Status::Active => "running",
///     Status::Paused => "paused",
///     Status::Stopped => "stopped",
/// }
/// ```
struct WhenExpr {
    ExprPtr scrutinee;         ///< Value being matched.
    std::vector<WhenArm> arms; ///< Match arms.
    SourceSpan span;           ///< Source location.
};

// ============================================================================
// Loops
// ============================================================================

/// Infinite loop: `loop { body }`.
///
/// Loops forever until explicitly broken with `break`.
///
/// # Example
///
/// ```tml
/// loop {
///     if should_stop() { break }
///     do_work()
/// }
/// ```
struct LoopExpr {
    std::optional<std::string> label; ///< Optional loop label (`'label: loop`).
    ExprPtr body;                     ///< Loop body.
    SourceSpan span;                  ///< Source location.
};

/// While loop: `while cond { body }`.
///
/// Loops while condition is true.
///
/// # Example
///
/// ```tml
/// while count < limit {
///     process()
///     count = count + 1
/// }
/// ```
struct WhileExpr {
    std::optional<std::string> label; ///< Optional loop label.
    ExprPtr condition;                ///< Loop condition (must be Bool).
    ExprPtr body;                     ///< Loop body.
    SourceSpan span;                  ///< Source location.
};

/// For loop: `for x in iter { body }`.
///
/// Iterates over an iterable collection.
///
/// # Examples
///
/// ```tml
/// for item in items { process(item) }
/// for (key, value) in map { ... }
/// for i in 0 to 10 { ... }
/// ```
struct ForExpr {
    std::optional<std::string> label; ///< Optional loop label.
    PatternPtr pattern;               ///< Loop variable pattern.
    ExprPtr iter;                     ///< Iterator expression.
    ExprPtr body;                     ///< Loop body.
    SourceSpan span;                  ///< Source location.
};

// ============================================================================
// Blocks and Jumps
// ============================================================================

/// Block expression: `{ stmts; expr }`.
///
/// A sequence of statements optionally followed by a trailing expression.
/// The trailing expression's value becomes the block's value.
///
/// # Example
///
/// ```tml
/// let result = {
///     let x = compute()
///     let y = transform(x)
///     x + y              // Trailing expression - block's value
/// }
/// ```
struct BlockExpr {
    std::vector<StmtPtr> stmts;  ///< Statements in the block.
    std::optional<ExprPtr> expr; ///< Trailing expression (no semicolon).
    SourceSpan span;             ///< Source location.
};

/// Return expression: `return x`.
///
/// Returns a value from the current function.
///
/// # Examples
///
/// ```tml
/// return 42
/// return               // Returns unit ()
/// return Ok(result)
/// ```
struct ReturnExpr {
    std::optional<ExprPtr> value; ///< Return value (optional, defaults to unit).
    SourceSpan span;              ///< Source location.
};

/// Break expression: `break 'label x`.
///
/// Breaks out of a loop, optionally with a label and value.
///
/// # Examples
///
/// ```tml
/// break                    // Break innermost loop
/// break 'outer             // Break labeled loop
/// break 42                 // Break with value (for loop expressions)
/// ```
struct BreakExpr {
    std::optional<std::string> label; ///< Target loop label.
    std::optional<ExprPtr> value;     ///< Break value (for loop expressions).
    SourceSpan span;                  ///< Source location.
};

/// Continue expression: `continue 'label`.
///
/// Continues to the next iteration of a loop.
///
/// # Examples
///
/// ```tml
/// continue                 // Continue innermost loop
/// continue 'outer          // Continue labeled loop
/// ```
struct ContinueExpr {
    std::optional<std::string> label; ///< Target loop label.
    SourceSpan span;                  ///< Source location.
};

// ============================================================================
// Closures and Ranges
// ============================================================================

/// Closure expression: `do(x, y) x + y`.
///
/// TML uses `do` syntax instead of Rust's `|x|` for closures.
/// Closures capture variables from their environment.
///
/// # Examples
///
/// ```tml
/// let double = do(x) x * 2
/// let add = do(a, b) a + b
/// items.filter(do(x) x > 0)
/// ```
struct ClosureExpr {
    std::vector<std::pair<PatternPtr, std::optional<TypePtr>>>
        params;                         ///< Parameters with optional types.
    std::optional<TypePtr> return_type; ///< Optional return type annotation.
    ExprPtr body;                       ///< Closure body.
    bool is_move;                       ///< True for move closures (`do move`).
    SourceSpan span;                    ///< Source location.
    mutable std::vector<std::string>
        captured_vars; ///< Captured variables (filled by type checker).
};

/// Range expression: `a to b`, `a through b`, `to b`, `a to`.
///
/// Creates a range iterator. TML uses keywords instead of Rust's `..`/`..=`.
/// - `to`: exclusive end (like `..`)
/// - `through`: inclusive end (like `..=`)
///
/// # Examples
///
/// ```tml
/// for i in 0 to 10 { ... }         // 0, 1, 2, ..., 9
/// for i in 0 through 10 { ... }    // 0, 1, 2, ..., 10
/// let first_ten = items[0 to 10]
/// ```
struct RangeExpr {
    std::optional<ExprPtr> start; ///< Start (optional for `to end`).
    std::optional<ExprPtr> end;   ///< End (optional for `start to`).
    bool inclusive;               ///< True for `through` (inclusive).
    SourceSpan span;              ///< Source location.
};

// ============================================================================
// Type Operations
// ============================================================================

/// Cast expression: `x as T`.
///
/// Converts a value to a different type.
///
/// # Examples
///
/// ```tml
/// let byte = value as U8
/// let float = integer as F64
/// ```
struct CastExpr {
    ExprPtr expr;    ///< Expression to cast.
    TypePtr target;  ///< Target type.
    SourceSpan span; ///< Source location.
};

/// Type check expression: `expr is Type`.
///
/// Returns a boolean indicating whether the expression's runtime type
/// is the specified type or a subtype of it.
///
/// # Example
///
/// ```tml
/// if animal is Dog {
///     animal.bark()
/// }
/// ```
struct IsExpr {
    ExprPtr expr;    ///< Expression to check.
    TypePtr target;  ///< Type to check against.
    SourceSpan span; ///< Source location.
};

/// Try expression: `expr?` (error propagation).
///
/// Propagates errors by returning early if the expression is `Err`.
/// Unwraps the `Ok` value if successful.
///
/// # Example
///
/// ```tml
/// func read_config() -> Outcome[Config, Error] {
///     let content = read_file("config.toml")?
///     let config = parse_config(content)?
///     Ok(config)
/// }
/// ```
struct TryExpr {
    ExprPtr expr;    ///< Expression that may fail.
    SourceSpan span; ///< Source location.
};

/// Await expression: `expr.await`.
///
/// Awaits an async operation, suspending until the future completes.
///
/// # Example
///
/// ```tml
/// async func fetch_data() -> Data {
///     let response = http::get(url).await
///     response.json().await
/// }
/// ```
struct AwaitExpr {
    ExprPtr expr;    ///< Future to await.
    SourceSpan span; ///< Source location.
};

/// Throw expression: `throw new Error("message")`.
///
/// Throws an exception/error, terminating execution with an error message.
/// Similar to JavaScript/C# throw statements.
///
/// # Example
///
/// ```tml
/// func validate(x: I32) {
///     if x < 0 {
///         throw new Error("x must be positive")
///     }
/// }
/// ```
struct ThrowExpr {
    ExprPtr expr;    ///< Expression to throw (usually an Error).
    SourceSpan span; ///< Source location.
};

// ============================================================================
// Path and Lowlevel
// ============================================================================

/// Path expression: `std::io::stdout` or `List[I32]`.
///
/// References a named item through a qualified path, optionally with
/// generic arguments.
///
/// # Examples
///
/// ```tml
/// let writer = std::io::stdout()
/// let vec = Vec[I32]::new()
/// ```
struct PathExpr {
    TypePath path;                       ///< The path.
    std::optional<GenericArgs> generics; ///< Generic arguments.
    SourceSpan span;                     ///< Source location.
};

/// Lowlevel (unsafe) block: `lowlevel { ... }`.
///
/// TML uses `lowlevel` instead of `unsafe` for clarity.
/// Enables operations that bypass safety checks.
///
/// # Example
///
/// ```tml
/// lowlevel {
///     let ptr = data.as_ptr()
///     *ptr = value
/// }
/// ```
struct LowlevelExpr {
    std::vector<StmtPtr> stmts;  ///< Statements in the block.
    std::optional<ExprPtr> expr; ///< Trailing expression.
    SourceSpan span;             ///< Source location.
};

// ============================================================================
// Interpolated Strings
// ============================================================================

/// A segment of an interpolated string.
///
/// Can be either literal text or an interpolated expression.
struct InterpolatedSegment {
    std::variant<std::string, ///< Literal text segment.
                 ExprPtr      ///< Interpolated expression: `{expr}`.
                 >
        content;     ///< Segment content.
    SourceSpan span; ///< Source location.
};

/// Interpolated string: `"Hello {name}, you are {age} years old"`.
///
/// Strings with embedded expressions. Expressions inside `{}` are
/// evaluated and converted to strings.
///
/// # Example
///
/// ```tml
/// let greeting = "Hello {user.name}!"
/// let info = "Count: {count}, Total: {sum}"
/// ```
struct InterpolatedStringExpr {
    std::vector<InterpolatedSegment> segments; ///< String segments.
    SourceSpan span;                           ///< Source location.
};

// ============================================================================
// OOP Expressions
// ============================================================================

/// Base expression: `base.method()` or `base.field`.
///
/// Accesses a member of the parent class in OOP contexts.
/// Used within class methods to call parent implementations.
///
/// # Example
///
/// ```tml
/// class Dog extends Animal {
///     override func speak(this) -> Str {
///         let parent_sound = base.speak()
///         return parent_sound + " Woof!"
///     }
/// }
/// ```
struct BaseExpr {
    std::string member;             ///< Member name (method or field).
    std::vector<TypePtr> type_args; ///< Generic type arguments.
    std::vector<ExprPtr> args;      ///< Call arguments (if method call).
    bool is_method_call;            ///< True if calling method, false for field access.
    SourceSpan span;                ///< Source location.
};

/// New expression: `new ClassName(args)` (object instantiation).
///
/// Creates a new instance of a class using its constructor.
///
/// # Examples
///
/// ```tml
/// let dog = new Dog("Buddy")
/// let list = new ArrayList[I32]()
/// ```
struct NewExpr {
    TypePath class_type;                 ///< Class to instantiate.
    std::optional<GenericArgs> generics; ///< Generic arguments.
    std::vector<ExprPtr> args;           ///< Constructor arguments.
    SourceSpan span;                     ///< Source location.
};

// ============================================================================
// Expression Variant
// ============================================================================

/// An expression (value-producing construct).
///
/// Expressions are the core of TML programs. They can be literals, operations,
/// control flow, function calls, and more. Every expression has a type and
/// produces a value (even if that value is unit `()`).
///
/// Uses `std::variant` for type-safe sum types with helper methods
/// for type checking and casting.
struct Expr {
    std::variant<LiteralExpr, IdentExpr, UnaryExpr, BinaryExpr, CallExpr, MethodCallExpr, FieldExpr,
                 IndexExpr, TupleExpr, ArrayExpr, StructExpr, IfExpr, TernaryExpr, IfLetExpr,
                 WhenExpr, LoopExpr, WhileExpr, ForExpr, BlockExpr, ReturnExpr, BreakExpr,
                 ContinueExpr, ClosureExpr, RangeExpr, CastExpr, IsExpr, TryExpr, AwaitExpr,
                 ThrowExpr, PathExpr, LowlevelExpr, InterpolatedStringExpr, BaseExpr, NewExpr>
        kind;        ///< The expression variant.
    SourceSpan span; ///< Source location.

    /// Checks if this expression is of kind `T`.
    ///
    /// # Example
    /// ```cpp
    /// if (expr.is<CallExpr>()) { ... }
    /// ```
    template <typename T> [[nodiscard]] auto is() const -> bool {
        return std::holds_alternative<T>(kind);
    }

    /// Gets this expression as kind `T`. Throws `std::bad_variant_access` if wrong kind.
    ///
    /// # Example
    /// ```cpp
    /// auto& call = expr.as<CallExpr>();
    /// ```
    template <typename T> [[nodiscard]] auto as() -> T& {
        if (!is<T>()) {
            throw std::bad_variant_access();
        }
        return std::get<T>(kind);
    }

    /// Gets this expression as kind `T` (const). Throws `std::bad_variant_access` if wrong kind.
    template <typename T> [[nodiscard]] auto as() const -> const T& {
        if (!is<T>()) {
            throw std::bad_variant_access();
        }
        return std::get<T>(kind);
    }
};

} // namespace tml::parser

#endif // TML_PARSER_AST_EXPRS_HPP
