//! # HIR Expressions
//!
//! This module defines expression types for the HIR. Expressions are the primary
//! value-producing constructs in TML programs.
//!
//! ## Overview
//!
//! Every expression in HIR:
//! - Has a unique `HirId` for identification
//! - Carries a fully-resolved `HirType` (never null in well-formed HIR)
//! - Has a `SourceSpan` pointing back to original source code
//!
//! ## Expression Categories
//!
//! Expressions are grouped into logical categories:
//!
//! | Category | Kinds | Description |
//! |----------|-------|-------------|
//! | **Literals** | `HirLiteralExpr` | Compile-time constant values |
//! | **Variables** | `HirVarExpr` | References to bound names |
//! | **Operations** | `HirBinaryExpr`, `HirUnaryExpr` | Arithmetic, logical, bitwise |
//! | **Calls** | `HirCallExpr`, `HirMethodCallExpr` | Function and method invocation |
//! | **Access** | `HirFieldExpr`, `HirIndexExpr` | Field and element access |
//! | **Constructors** | `HirTupleExpr`, `HirArrayExpr`, `HirStructExpr`, `HirEnumExpr` | Value
//! construction | | **Control Flow** | `HirIfExpr`, `HirWhenExpr`, `HirLoopExpr`, etc. | Branching
//! and iteration | | **Closures** | `HirClosureExpr` | Anonymous functions with captures | |
//! **Special** | `HirReturnExpr`, `HirBreakExpr`, `HirCastExpr`, etc. | Control transfer and type
//! ops |
//!
//! ## Type Resolution
//!
//! Unlike AST expressions which may have unresolved types, all HIR expressions
//! carry fully resolved semantic types. This enables downstream passes to operate
//! without type inference.
//!
//! ## Working with Expressions
//!
//! Use the type-safe accessors to work with expression kinds:
//!
//! ```cpp
//! void process_expr(const HirExpr& expr) {
//!     if (expr.is<HirBinaryExpr>()) {
//!         const auto& binary = expr.as<HirBinaryExpr>();
//!         process_expr(*binary.left);
//!         process_expr(*binary.right);
//!     } else if (expr.is<HirCallExpr>()) {
//!         const auto& call = expr.as<HirCallExpr>();
//!         for (const auto& arg : call.args) {
//!             process_expr(*arg);
//!         }
//!     }
//!     // ... handle other cases
//! }
//! ```
//!
//! ## See Also
//!
//! - `docs/specs/31-HIR.md` - Complete HIR documentation
//! - `hir_stmt.hpp` - Statements that contain expressions
//! - `hir_pattern.hpp` - Patterns used in when/for expressions

#pragma once

#include "hir/hir_id.hpp"
#include "hir/hir_pattern.hpp"

namespace tml::hir {

// ============================================================================
// Binary and Unary Operations
// ============================================================================

/// Binary operation kinds.
///
/// These represent all binary operators available in TML, organized by category.
///
/// ## Categories
///
/// | Category | Operators | Result Type |
/// |----------|-----------|-------------|
/// | Arithmetic | `Add`, `Sub`, `Mul`, `Div`, `Mod` | Same as operands |
/// | Comparison | `Eq`, `Ne`, `Lt`, `Le`, `Gt`, `Ge` | `Bool` |
/// | Logical | `And`, `Or` | `Bool` |
/// | Bitwise | `BitAnd`, `BitOr`, `BitXor`, `Shl`, `Shr` | Integer type |
enum class HirBinOp {
    // Arithmetic
    Add, ///< Addition: `a + b`
    Sub, ///< Subtraction: `a - b`
    Mul, ///< Multiplication: `a * b`
    Div, ///< Division: `a / b`
    Mod, ///< Modulo/remainder: `a % b`
    // Comparison
    Eq, ///< Equality: `a == b`
    Ne, ///< Inequality: `a != b`
    Lt, ///< Less than: `a < b`
    Le, ///< Less than or equal: `a <= b`
    Gt, ///< Greater than: `a > b`
    Ge, ///< Greater than or equal: `a >= b`
    // Logical
    And, ///< Logical AND: `a and b`
    Or,  ///< Logical OR: `a or b`
    // Bitwise
    BitAnd, ///< Bitwise AND: `a & b`
    BitOr,  ///< Bitwise OR: `a | b`
    BitXor, ///< Bitwise XOR: `a ^ b`
    Shl,    ///< Left shift: `a << b`
    Shr,    ///< Right shift: `a >> b`
};

/// Unary operation kinds.
///
/// These represent prefix operators that take a single operand.
///
/// ## Operators
///
/// | Operator | TML Syntax | Description |
/// |----------|------------|-------------|
/// | `Neg` | `-x` | Numeric negation |
/// | `Not` | `not x` | Logical negation |
/// | `BitNot` | `~x` | Bitwise complement |
/// | `Ref` | `ref x` | Create immutable borrow |
/// | `RefMut` | `mut ref x` | Create mutable borrow |
/// | `Deref` | `*x` | Dereference pointer/reference |
enum class HirUnaryOp {
    Neg,    ///< Numeric negation: `-x`
    Not,    ///< Logical NOT: `not x`
    BitNot, ///< Bitwise complement: `~x`
    Ref,    ///< Create reference: `ref x`
    RefMut, ///< Create mutable reference: `mut ref x`
    Deref,  ///< Dereference: `*x`
};

/// Compound assignment operation kinds.
///
/// These represent the operator in compound assignment expressions like `x += 1`.
/// The compound assignment `x op= y` is semantically equivalent to `x = x op y`,
/// but the target is only evaluated once.
///
/// ## Supported Operators
///
/// | TML Syntax | Compound Op | Equivalent |
/// |------------|-------------|------------|
/// | `x += y` | `Add` | `x = x + y` |
/// | `x -= y` | `Sub` | `x = x - y` |
/// | `x *= y` | `Mul` | `x = x * y` |
/// | `x /= y` | `Div` | `x = x / y` |
/// | `x %= y` | `Mod` | `x = x % y` |
/// | `x &= y` | `BitAnd` | `x = x & y` |
/// | `x \|= y` | `BitOr` | `x = x \| y` |
/// | `x ^= y` | `BitXor` | `x = x ^ y` |
/// | `x <<= y` | `Shl` | `x = x << y` |
/// | `x >>= y` | `Shr` | `x = x >> y` |
enum class HirCompoundOp {
    Add,    ///< `+=`
    Sub,    ///< `-=`
    Mul,    ///< `*=`
    Div,    ///< `/=`
    Mod,    ///< `%=`
    BitAnd, ///< `&=`
    BitOr,  ///< `|=`
    BitXor, ///< `^=`
    Shl,    ///< `<<=`
    Shr,    ///< `>>=`
};

// ============================================================================
// Expression Definitions
// ============================================================================

/// Literal expression: `42`, `3.14`, `"hello"`, `true`
///
/// Represents compile-time constant values. The value is stored in a variant
/// that can hold any of the supported literal types.
///
/// ## Fields
/// - `id`: Unique identifier for this expression node
/// - `value`: The literal value (int, uint, float, bool, char, or string)
/// - `type`: The semantic type of this literal
/// - `span`: Source location
///
/// ## Supported Literal Types
///
/// | Type | C++ Storage | Example |
/// |------|-------------|---------|
/// | Signed integers | `int64_t` | `42`, `-100` |
/// | Unsigned integers | `uint64_t` | `42u64` |
/// | Floating point | `double` | `3.14`, `2.5e-10` |
/// | Boolean | `bool` | `true`, `false` |
/// | Character | `char` | `'a'`, `'\n'` |
/// | String | `std::string` | `"hello"` |
struct HirLiteralExpr {
    HirId id;
    std::variant<int64_t, uint64_t, double, bool, char, std::string> value;
    HirType type;
    SourceSpan span;
};

/// Variable reference: `x`
///
/// References a previously bound variable by name. In well-formed HIR,
/// the name always refers to a valid binding in scope.
///
/// ## Fields
/// - `id`: Unique identifier for this expression node
/// - `name`: The variable name being referenced
/// - `type`: The type of the variable
/// - `span`: Source location
///
/// ## Note on Name Resolution
///
/// HIR does not contain scope information directly. The `name` field is
/// sufficient because all names have been validated during type checking.
/// For closures, captured variables are listed separately in `HirClosureExpr::captures`.
struct HirVarExpr {
    HirId id;
    std::string name;
    HirType type;
    SourceSpan span;
};

/// Binary operation: `a + b`, `x == y`
///
/// Represents a binary operator applied to two operands. Both operands
/// are fully-typed expressions.
///
/// ## Fields
/// - `id`: Unique identifier for this expression node
/// - `op`: The binary operation kind
/// - `left`: Left operand expression
/// - `right`: Right operand expression
/// - `type`: Result type of the operation
/// - `span`: Source location
///
/// ## Type Relationships
///
/// For most operations, operand types must match. The result type depends
/// on the operation category:
/// - Arithmetic: same as operand types
/// - Comparison: always `Bool`
/// - Logical: always `Bool`
/// - Bitwise: same as operand types (must be integer)
struct HirBinaryExpr {
    HirId id;
    HirBinOp op;
    HirExprPtr left;
    HirExprPtr right;
    HirType type;
    SourceSpan span;
};

/// Unary operation: `-x`, `not x`, `ref x`, `*x`
///
/// Represents a unary operator applied to a single operand.
///
/// ## Fields
/// - `id`: Unique identifier for this expression node
/// - `op`: The unary operation kind
/// - `operand`: The operand expression
/// - `type`: Result type of the operation
/// - `span`: Source location
///
/// ## Type Relationships
///
/// | Operation | Operand Type | Result Type |
/// |-----------|--------------|-------------|
/// | `Neg` | Numeric | Same as operand |
/// | `Not` | `Bool` | `Bool` |
/// | `BitNot` | Integer | Same as operand |
/// | `Ref` | `T` | `ref T` |
/// | `RefMut` | `T` | `mut ref T` |
/// | `Deref` | `ref T` or `mut ref T` | `T` |
struct HirUnaryExpr {
    HirId id;
    HirUnaryOp op;
    HirExprPtr operand;
    HirType type;
    SourceSpan span;
};

/// Function call: `foo(a, b)`
///
/// Represents a direct function call (not a method call). Generic functions
/// have been monomorphized, so `type_args` contains the concrete types.
///
/// ## Fields
/// - `id`: Unique identifier for this expression node
/// - `func_name`: Name of the function being called
/// - `type_args`: Monomorphized type arguments (empty for non-generic)
/// - `args`: Argument expressions
/// - `type`: Return type of the function
/// - `span`: Source location
///
/// ## Monomorphization
///
/// For generic functions, `func_name` is the base name and `type_args` contains
/// the concrete type instantiation. During codegen, the mangled name is computed
/// from these.
///
/// ## Example
/// ```tml
/// let x = make_vec[I32](10)
/// ```
/// Becomes:
/// ```cpp
/// HirCallExpr {
///     func_name: "make_vec",
///     type_args: [I32],
///     args: [HirLiteralExpr(10)],
///     type: Vec[I32]
/// }
/// ```
struct HirCallExpr {
    HirId id;
    std::string func_name;
    std::vector<HirType> type_args;
    std::vector<HirExprPtr> args;
    HirType type;
    SourceSpan span;
};

/// Method call: `obj.method(a, b)`
///
/// Represents a method invocation on a receiver object. The receiver type
/// is tracked separately to support trait method dispatch.
///
/// ## Fields
/// - `id`: Unique identifier for this expression node
/// - `receiver`: The object expression receiving the method call
/// - `method_name`: Name of the method being called
/// - `type_args`: Monomorphized type arguments for generic methods
/// - `args`: Argument expressions (excluding receiver)
/// - `receiver_type`: Type of the receiver expression
/// - `type`: Return type of the method
/// - `span`: Source location
///
/// ## Method Resolution
///
/// The `receiver_type` is used to look up the method implementation:
/// - For inherent methods: look in impl blocks for the type
/// - For trait methods: look in trait impl blocks
struct HirMethodCallExpr {
    HirId id;
    HirExprPtr receiver;
    std::string method_name;
    std::vector<HirType> type_args;
    std::vector<HirExprPtr> args;
    HirType receiver_type;
    HirType type;
    SourceSpan span;
};

/// Field access: `obj.field`
///
/// Accesses a named field of a struct. The field index is resolved during
/// HIR lowering for efficient codegen.
///
/// ## Fields
/// - `id`: Unique identifier for this expression node
/// - `object`: The struct expression being accessed
/// - `field_name`: Name of the field
/// - `field_index`: Zero-based index of the field in the struct layout
/// - `type`: Type of the field
/// - `span`: Source location
///
/// ## Field Index
///
/// The `field_index` corresponds to the declaration order in the struct
/// definition. This enables direct offset calculation during codegen.
///
/// ## Example
/// ```tml
/// type Point { x: I32, y: I32 }
/// let p = Point { x: 1, y: 2 }
/// let x = p.x  // field_index = 0
/// let y = p.y  // field_index = 1
/// ```
struct HirFieldExpr {
    HirId id;
    HirExprPtr object;
    std::string field_name;
    int field_index;
    HirType type;
    SourceSpan span;
};

/// Index expression: `arr[i]`
///
/// Accesses an element of an array, slice, or other indexable type.
///
/// ## Fields
/// - `id`: Unique identifier for this expression node
/// - `object`: The array/slice expression being indexed
/// - `index`: The index expression (typically integer)
/// - `type`: Element type
/// - `span`: Source location
///
/// ## Bounds Checking
///
/// For arrays with known size, bounds checking may be optimized away.
/// For slices, runtime bounds checking is performed.
struct HirIndexExpr {
    HirId id;
    HirExprPtr object;
    HirExprPtr index;
    HirType type;
    SourceSpan span;
};

/// Tuple expression: `(a, b, c)`
///
/// Constructs a tuple from its elements. Empty tuples `()` represent unit.
///
/// ## Fields
/// - `id`: Unique identifier for this expression node
/// - `elements`: Element expressions (may be empty for unit)
/// - `type`: Tuple type
/// - `span`: Source location
///
/// ## Unit Type
///
/// The empty tuple `()` is TML's unit type, analogous to `void` in C.
/// Functions that don't return a value have return type `()`.
struct HirTupleExpr {
    HirId id;
    std::vector<HirExprPtr> elements;
    HirType type;
    SourceSpan span;
};

/// Array expression: `[1, 2, 3]`
///
/// Constructs an array from explicit element values. All elements must
/// have the same type.
///
/// ## Fields
/// - `id`: Unique identifier for this expression node
/// - `elements`: Element expressions
/// - `element_type`: Type of each element
/// - `size`: Number of elements (equals `elements.size()`)
/// - `type`: Complete array type including size
/// - `span`: Source location
struct HirArrayExpr {
    HirId id;
    std::vector<HirExprPtr> elements;
    HirType element_type;
    size_t size;
    HirType type;
    SourceSpan span;
};

/// Array repeat expression: `[0; 10]`
///
/// Constructs an array by repeating a single value. The value must be
/// copyable (implement `Duplicate`).
///
/// ## Fields
/// - `id`: Unique identifier for this expression node
/// - `value`: The value to repeat
/// - `count`: Number of repetitions
/// - `type`: Complete array type
/// - `span`: Source location
///
/// ## Example
/// ```tml
/// let zeros: [I32; 100] = [0; 100]
/// ```
struct HirArrayRepeatExpr {
    HirId id;
    HirExprPtr value;
    size_t count;
    HirType type;
    SourceSpan span;
};

/// Struct construction: `Point { x: 1, y: 2 }`
///
/// Constructs a struct instance by specifying field values. Supports
/// struct update syntax with a base expression.
///
/// ## Fields
/// - `id`: Unique identifier for this expression node
/// - `struct_name`: Name of the struct type
/// - `type_args`: Type arguments for generic structs
/// - `fields`: List of (field_name, value) pairs
/// - `base`: Optional base expression for struct update (`..base`)
/// - `type`: The struct type
/// - `span`: Source location
///
/// ## Struct Update Syntax
///
/// When `base` is present, unspecified fields are copied from the base:
/// ```tml
/// let p2 = Point { x: 10, ..p1 }  // y comes from p1
/// ```
struct HirStructExpr {
    HirId id;
    std::string struct_name;
    std::vector<HirType> type_args;
    std::vector<std::pair<std::string, HirExprPtr>> fields;
    std::optional<HirExprPtr> base;
    HirType type;
    SourceSpan span;
};

/// Enum variant construction: `Just(x)`, `Nothing`
///
/// Constructs an enum variant, optionally with payload values.
///
/// ## Fields
/// - `id`: Unique identifier for this expression node
/// - `enum_name`: Name of the enum type (e.g., "Maybe")
/// - `variant_name`: Name of the variant (e.g., "Just")
/// - `variant_index`: Zero-based variant index in enum definition
/// - `type_args`: Type arguments for generic enums
/// - `payload`: Payload expressions (empty for unit variants)
/// - `type`: The enum type
/// - `span`: Source location
///
/// ## Variant Index
///
/// The `variant_index` corresponds to declaration order:
/// ```tml
/// type Maybe[T] { Just(T), Nothing }  // Just=0, Nothing=1
/// ```
struct HirEnumExpr {
    HirId id;
    std::string enum_name;
    std::string variant_name;
    int variant_index;
    std::vector<HirType> type_args;
    std::vector<HirExprPtr> payload;
    HirType type;
    SourceSpan span;
};

/// Block expression: `{ stmts; expr }`
///
/// A sequence of statements with an optional trailing expression that
/// determines the block's value.
///
/// ## Fields
/// - `id`: Unique identifier for this expression node
/// - `stmts`: Statements in the block
/// - `expr`: Optional final expression (determines block value)
/// - `type`: Type of the block (type of `expr`, or `()` if None)
/// - `span`: Source location
///
/// ## Value Semantics
///
/// If `expr` is present, the block evaluates to that expression's value.
/// Otherwise, the block evaluates to unit `()`.
struct HirBlockExpr {
    HirId id;
    std::vector<HirStmtPtr> stmts;
    std::optional<HirExprPtr> expr;
    HirType type;
    SourceSpan span;
};

/// If expression: `if cond { then } else { else }`
///
/// Conditional expression with optional else branch. Both branches
/// must have compatible types.
///
/// ## Fields
/// - `id`: Unique identifier for this expression node
/// - `condition`: Boolean condition expression
/// - `then_branch`: Expression evaluated when condition is true
/// - `else_branch`: Optional expression for false case
/// - `type`: Result type (must match both branches, or `()` if no else)
/// - `span`: Source location
///
/// ## Type Rules
///
/// - If `else_branch` is present: both branches must have same type
/// - If `else_branch` is absent: `then_branch` must have type `()`
struct HirIfExpr {
    HirId id;
    HirExprPtr condition;
    HirExprPtr then_branch;
    std::optional<HirExprPtr> else_branch;
    HirType type;
    SourceSpan span;
};

/// Match arm for when expression.
///
/// Represents a single arm in a `when` expression, consisting of a pattern,
/// optional guard, and body expression.
///
/// ## Fields
/// - `pattern`: Pattern to match against the scrutinee
/// - `guard`: Optional boolean guard expression (evaluated if pattern matches)
/// - `body`: Expression to evaluate if this arm is selected
/// - `span`: Source location of the arm
///
/// ## Pattern Guards
///
/// Guards allow additional conditions beyond pattern matching:
/// ```tml
/// when x {
///     n if n > 0 => "positive",
///     n if n < 0 => "negative",
///     _ => "zero"
/// }
/// ```
struct HirWhenArm {
    HirPatternPtr pattern;
    std::optional<HirExprPtr> guard;
    HirExprPtr body;
    SourceSpan span;
};

/// When (match) expression: `when x { pat => expr, ... }`
///
/// Pattern matching expression. Arms are evaluated top-to-bottom until
/// a pattern matches (and its guard, if any, evaluates to true).
///
/// ## Fields
/// - `id`: Unique identifier for this expression node
/// - `scrutinee`: Expression being matched against
/// - `arms`: List of match arms
/// - `type`: Result type (all arm bodies must have this type)
/// - `span`: Source location
///
/// ## Exhaustiveness
///
/// In well-formed HIR, pattern arms are exhaustive - they cover all
/// possible values of the scrutinee type. This is verified during
/// type checking before HIR lowering.
struct HirWhenExpr {
    HirId id;
    HirExprPtr scrutinee;
    std::vector<HirWhenArm> arms;
    HirType type;
    SourceSpan span;
};

/// Loop expression: `loop { body }`
///
/// Infinite loop that can only be exited via `break`. The loop's value
/// is determined by the expression in `break`.
///
/// ## Fields
/// - `id`: Unique identifier for this expression node
/// - `label`: Optional loop label for targeted break/continue
/// - `body`: Loop body expression
/// - `type`: Result type (determined by break expressions)
/// - `span`: Source location
///
/// ## Loop Labels
///
/// Labels allow breaking out of nested loops:
/// ```tml
/// 'outer: loop {
///     loop {
///         break 'outer value
///     }
/// }
/// ```
struct HirLoopExpr {
    HirId id;
    std::optional<std::string> label;
    HirExprPtr body;
    HirType type;
    SourceSpan span;
};

/// While loop: `while cond { body }`
///
/// Conditional loop that executes while the condition is true.
/// The result type is always `()` since the loop may execute zero times.
///
/// ## Fields
/// - `id`: Unique identifier for this expression node
/// - `label`: Optional loop label
/// - `condition`: Boolean condition checked before each iteration
/// - `body`: Loop body expression
/// - `type`: Always unit type `()`
/// - `span`: Source location
struct HirWhileExpr {
    HirId id;
    std::optional<std::string> label;
    HirExprPtr condition;
    HirExprPtr body;
    HirType type;
    SourceSpan span;
};

/// For loop: `for x in iter { body }`
///
/// Iterator loop that binds each element to a pattern.
///
/// ## Fields
/// - `id`: Unique identifier for this expression node
/// - `label`: Optional loop label
/// - `pattern`: Pattern to bind each element
/// - `iter`: Iterator expression
/// - `body`: Loop body expression
/// - `type`: Always unit type `()`
/// - `span`: Source location
///
/// ## Iterator Protocol
///
/// The `iter` expression must implement the `Iterate` behavior,
/// which provides `next() -> Maybe[T]`.
struct HirForExpr {
    HirId id;
    std::optional<std::string> label;
    HirPatternPtr pattern;
    HirExprPtr iter;
    HirExprPtr body;
    HirType type;
    SourceSpan span;
};

/// Return expression: `return x`
///
/// Exits the current function with a value. The return type must match
/// the function's declared return type.
///
/// ## Fields
/// - `id`: Unique identifier for this expression node
/// - `value`: Optional return value (None means return `()`)
/// - `span`: Source location
///
/// ## Control Flow
///
/// Return is a diverging expression - control never continues past it.
/// Its "type" in the expression sense is `!` (never type).
struct HirReturnExpr {
    HirId id;
    std::optional<HirExprPtr> value;
    SourceSpan span;
};

/// Break expression: `break 'label x`
///
/// Exits a loop, optionally with a value and/or label.
///
/// ## Fields
/// - `id`: Unique identifier for this expression node
/// - `label`: Optional label of the loop to break from
/// - `value`: Optional value to produce from the loop
/// - `span`: Source location
///
/// ## Loop Values
///
/// For `loop` expressions, break can carry a value:
/// ```tml
/// let x = loop {
///     if condition { break 42 }
/// }
/// ```
struct HirBreakExpr {
    HirId id;
    std::optional<std::string> label;
    std::optional<HirExprPtr> value;
    SourceSpan span;
};

/// Continue expression: `continue 'label`
///
/// Skips to the next iteration of a loop.
///
/// ## Fields
/// - `id`: Unique identifier for this expression node
/// - `label`: Optional label of the loop to continue
/// - `span`: Source location
struct HirContinueExpr {
    HirId id;
    std::optional<std::string> label;
    SourceSpan span;
};

/// A captured variable in a closure.
///
/// Describes how a variable from an enclosing scope is captured by a closure.
///
/// ## Fields
/// - `name`: Name of the captured variable
/// - `type`: Type of the captured variable
/// - `is_mut`: Whether the variable is mutable
/// - `by_move`: If true, captured by move; if false, captured by reference
///
/// ## Capture Modes
///
/// - **By reference** (`by_move = false`): Closure borrows the variable
/// - **By move** (`by_move = true`): Closure takes ownership
///
/// The capture mode is inferred based on how the variable is used within
/// the closure body.
struct HirCapture {
    std::string name;
    HirType type;
    bool is_mut;
    bool by_move;
};

/// Closure expression: `do(x, y) x + y`
///
/// Anonymous function that can capture variables from its environment.
///
/// ## Fields
/// - `id`: Unique identifier for this expression node
/// - `params`: Parameter list as (name, type) pairs
/// - `body`: Closure body expression
/// - `captures`: List of captured variables from enclosing scope
/// - `type`: Closure type (includes signature and capture info)
/// - `span`: Source location
///
/// ## Capture Analysis
///
/// The `captures` list is populated during HIR lowering by analyzing
/// which names in `body` refer to variables from enclosing scopes.
///
/// ## Example
/// ```tml
/// let multiplier = 10
/// let f = do(x: I32) x * multiplier  // captures 'multiplier'
/// ```
struct HirClosureExpr {
    HirId id;
    std::vector<std::pair<std::string, HirType>> params;
    HirExprPtr body;
    std::vector<HirCapture> captures;
    HirType type;
    SourceSpan span;
};

/// Cast expression: `x as T`
///
/// Explicit type conversion between compatible types.
///
/// ## Fields
/// - `id`: Unique identifier for this expression node
/// - `expr`: Expression to cast
/// - `target_type`: Type to cast to
/// - `type`: Same as `target_type`
/// - `span`: Source location
///
/// ## Valid Casts
///
/// - Numeric conversions (I32 → I64, F64 → I32, etc.)
/// - Pointer/reference conversions
/// - Enum to underlying integer
struct HirCastExpr {
    HirId id;
    HirExprPtr expr;
    HirType target_type;
    HirType type;
    SourceSpan span;
};

/// Try expression: `expr!` (unwrap Maybe/Outcome)
///
/// Unwraps a `Maybe` or `Outcome` value, propagating the error case.
///
/// ## Fields
/// - `id`: Unique identifier for this expression node
/// - `expr`: Expression of type `Maybe[T]` or `Outcome[T, E]`
/// - `type`: The unwrapped type `T`
/// - `span`: Source location
///
/// ## Semantics
///
/// For `Maybe[T]`:
/// - `Just(x)!` → `x`
/// - `Nothing!` → early return with `Nothing`
///
/// For `Outcome[T, E]`:
/// - `Ok(x)!` → `x`
/// - `Err(e)!` → early return with `Err(e)`
struct HirTryExpr {
    HirId id;
    HirExprPtr expr;
    HirType type;
    SourceSpan span;
};

/// Await expression: `expr.await`
///
/// Suspends execution until an async operation completes.
///
/// ## Fields
/// - `id`: Unique identifier for this expression node
/// - `expr`: Expression of type `Future[T]`
/// - `type`: The awaited type `T`
/// - `span`: Source location
///
/// ## Requirements
///
/// Can only appear inside `async` functions or blocks.
struct HirAwaitExpr {
    HirId id;
    HirExprPtr expr;
    HirType type;
    SourceSpan span;
};

/// Assignment expression: `x = y`
///
/// Assigns a new value to a mutable location.
///
/// ## Fields
/// - `id`: Unique identifier for this expression node
/// - `target`: The place being assigned to (variable, field, index)
/// - `value`: The value to assign
/// - `span`: Source location
///
/// ## Requirements
///
/// - Target must be a mutable place (declared with `mut`)
/// - Value type must match target type
struct HirAssignExpr {
    HirId id;
    HirExprPtr target;
    HirExprPtr value;
    SourceSpan span;
};

/// Compound assignment: `x += y`
///
/// Combines an operation with assignment. Equivalent to `x = x op y`,
/// but the target is only evaluated once.
///
/// ## Fields
/// - `id`: Unique identifier for this expression node
/// - `op`: The compound operation kind
/// - `target`: The place being modified
/// - `value`: The right-hand operand
/// - `span`: Source location
struct HirCompoundAssignExpr {
    HirId id;
    HirCompoundOp op;
    HirExprPtr target;
    HirExprPtr value;
    SourceSpan span;
};

/// Lowlevel (unsafe) block: `lowlevel { ... }`
///
/// Contains code that bypasses TML's safety checks.
///
/// ## Fields
/// - `id`: Unique identifier for this expression node
/// - `stmts`: Statements inside the unsafe block
/// - `expr`: Optional final expression
/// - `type`: Type of the block
/// - `span`: Source location
///
/// ## Safety
///
/// Code inside `lowlevel` blocks can:
/// - Dereference raw pointers
/// - Call unsafe functions
/// - Access mutable statics
/// - Perform unchecked casts
struct HirLowlevelExpr {
    HirId id;
    std::vector<HirStmtPtr> stmts;
    std::optional<HirExprPtr> expr;
    HirType type;
    SourceSpan span;
};

// ============================================================================
// HirExpr Container
// ============================================================================

/// An expression in HIR.
///
/// `HirExpr` is a variant container that can hold any of the expression kinds
/// defined above. It provides common accessors for ID, type, and span that
/// work uniformly across all expression kinds.
///
/// ## Type Checking
///
/// Use `is<T>()` to check the expression kind before accessing with `as<T>()`:
/// ```cpp
/// if (expr.is<HirBinaryExpr>()) {
///     const auto& binary = expr.as<HirBinaryExpr>();
///     // ... work with binary expression
/// }
/// ```
///
/// ## Visiting All Cases
///
/// For comprehensive handling, use `std::visit`:
/// ```cpp
/// std::visit([](const auto& e) {
///     // e is the concrete expression type
/// }, expr.kind);
/// ```
struct HirExpr {
    std::variant<HirLiteralExpr, HirVarExpr, HirBinaryExpr, HirUnaryExpr, HirCallExpr,
                 HirMethodCallExpr, HirFieldExpr, HirIndexExpr, HirTupleExpr, HirArrayExpr,
                 HirArrayRepeatExpr, HirStructExpr, HirEnumExpr, HirBlockExpr, HirIfExpr,
                 HirWhenExpr, HirLoopExpr, HirWhileExpr, HirForExpr, HirReturnExpr, HirBreakExpr,
                 HirContinueExpr, HirClosureExpr, HirCastExpr, HirTryExpr, HirAwaitExpr,
                 HirAssignExpr, HirCompoundAssignExpr, HirLowlevelExpr>
        kind;

    /// Get the HIR ID for this expression.
    [[nodiscard]] auto id() const -> HirId;

    /// Get the type of this expression.
    /// @return The fully-resolved semantic type (never null in well-formed HIR)
    [[nodiscard]] auto type() const -> HirType;

    /// Get the source span.
    [[nodiscard]] auto span() const -> SourceSpan;

    /// Check if this expression is of kind `T`.
    /// @tparam T One of the HirXxxExpr types
    /// @return true if this expression holds a T
    template <typename T> [[nodiscard]] auto is() const -> bool {
        return std::holds_alternative<T>(kind);
    }

    /// Get this expression as kind `T` (mutable).
    /// @tparam T One of the HirXxxExpr types
    /// @return Reference to the contained expression
    /// @throws std::bad_variant_access if expression is not of type T
    template <typename T> [[nodiscard]] auto as() -> T& {
        return std::get<T>(kind);
    }

    /// Get this expression as kind `T` (const).
    /// @tparam T One of the HirXxxExpr types
    /// @return Const reference to the contained expression
    /// @throws std::bad_variant_access if expression is not of type T
    template <typename T> [[nodiscard]] auto as() const -> const T& {
        return std::get<T>(kind);
    }
};

// ============================================================================
// Expression Factory Functions
// ============================================================================

/// Create a signed integer literal expression.
/// @param id Unique identifier for this node
/// @param value Integer value
/// @param type Numeric type (I8, I16, I32, I64, I128)
/// @param span Source location
/// @return Heap-allocated expression
auto make_hir_literal(HirId id, int64_t value, HirType type, SourceSpan span) -> HirExprPtr;

/// Create an unsigned integer literal expression.
auto make_hir_literal(HirId id, uint64_t value, HirType type, SourceSpan span) -> HirExprPtr;

/// Create a floating-point literal expression.
auto make_hir_literal(HirId id, double value, HirType type, SourceSpan span) -> HirExprPtr;

/// Create a boolean literal expression.
auto make_hir_literal(HirId id, bool value, HirType type, SourceSpan span) -> HirExprPtr;

/// Create a character literal expression.
auto make_hir_literal(HirId id, char value, HirType type, SourceSpan span) -> HirExprPtr;

/// Create a string literal expression.
auto make_hir_literal(HirId id, const std::string& value, HirType type, SourceSpan span)
    -> HirExprPtr;

/// Create a variable reference expression.
/// @param id Unique identifier for this node
/// @param name Variable name
/// @param type Variable type
/// @param span Source location
/// @return Heap-allocated expression
auto make_hir_var(HirId id, const std::string& name, HirType type, SourceSpan span) -> HirExprPtr;

/// Create a binary operation expression.
/// @param id Unique identifier for this node
/// @param op Binary operation kind
/// @param left Left operand
/// @param right Right operand
/// @param type Result type
/// @param span Source location
/// @return Heap-allocated expression
auto make_hir_binary(HirId id, HirBinOp op, HirExprPtr left, HirExprPtr right, HirType type,
                     SourceSpan span) -> HirExprPtr;

/// Create a unary operation expression.
/// @param id Unique identifier for this node
/// @param op Unary operation kind
/// @param operand Operand expression
/// @param type Result type
/// @param span Source location
/// @return Heap-allocated expression
auto make_hir_unary(HirId id, HirUnaryOp op, HirExprPtr operand, HirType type, SourceSpan span)
    -> HirExprPtr;

/// Create a function call expression.
/// @param id Unique identifier for this node
/// @param func_name Function name
/// @param type_args Monomorphized type arguments
/// @param args Argument expressions
/// @param type Return type
/// @param span Source location
/// @return Heap-allocated expression
auto make_hir_call(HirId id, const std::string& func_name, std::vector<HirType> type_args,
                   std::vector<HirExprPtr> args, HirType type, SourceSpan span) -> HirExprPtr;

/// Create a method call expression.
/// @param id Unique identifier for this node
/// @param receiver Object receiving the method call
/// @param method_name Method name
/// @param type_args Monomorphized type arguments
/// @param args Argument expressions
/// @param receiver_type Type of the receiver
/// @param type Return type
/// @param span Source location
/// @return Heap-allocated expression
auto make_hir_method_call(HirId id, HirExprPtr receiver, const std::string& method_name,
                          std::vector<HirType> type_args, std::vector<HirExprPtr> args,
                          HirType receiver_type, HirType type, SourceSpan span) -> HirExprPtr;

/// Create a field access expression.
/// @param id Unique identifier for this node
/// @param object Struct expression
/// @param field_name Field name
/// @param field_index Resolved field index
/// @param type Field type
/// @param span Source location
/// @return Heap-allocated expression
auto make_hir_field(HirId id, HirExprPtr object, const std::string& field_name, int field_index,
                    HirType type, SourceSpan span) -> HirExprPtr;

/// Create an index expression.
/// @param id Unique identifier for this node
/// @param object Array/slice expression
/// @param index Index expression
/// @param type Element type
/// @param span Source location
/// @return Heap-allocated expression
auto make_hir_index(HirId id, HirExprPtr object, HirExprPtr index, HirType type, SourceSpan span)
    -> HirExprPtr;

/// Create a block expression.
/// @param id Unique identifier for this node
/// @param stmts Statements in the block
/// @param expr Optional trailing expression
/// @param type Block result type
/// @param span Source location
/// @return Heap-allocated expression
auto make_hir_block(HirId id, std::vector<HirStmtPtr> stmts, std::optional<HirExprPtr> expr,
                    HirType type, SourceSpan span) -> HirExprPtr;

/// Create an if expression.
/// @param id Unique identifier for this node
/// @param condition Boolean condition
/// @param then_branch Expression for true case
/// @param else_branch Optional expression for false case
/// @param type Result type
/// @param span Source location
/// @return Heap-allocated expression
auto make_hir_if(HirId id, HirExprPtr condition, HirExprPtr then_branch,
                 std::optional<HirExprPtr> else_branch, HirType type, SourceSpan span)
    -> HirExprPtr;

/// Create a return expression.
/// @param id Unique identifier for this node
/// @param value Optional return value
/// @param span Source location
/// @return Heap-allocated expression
auto make_hir_return(HirId id, std::optional<HirExprPtr> value, SourceSpan span) -> HirExprPtr;

/// Create a break expression.
/// @param id Unique identifier for this node
/// @param label Optional target loop label
/// @param value Optional break value
/// @param span Source location
/// @return Heap-allocated expression
auto make_hir_break(HirId id, std::optional<std::string> label, std::optional<HirExprPtr> value,
                    SourceSpan span) -> HirExprPtr;

/// Create a continue expression.
/// @param id Unique identifier for this node
/// @param label Optional target loop label
/// @param span Source location
/// @return Heap-allocated expression
auto make_hir_continue(HirId id, std::optional<std::string> label, SourceSpan span) -> HirExprPtr;

} // namespace tml::hir
