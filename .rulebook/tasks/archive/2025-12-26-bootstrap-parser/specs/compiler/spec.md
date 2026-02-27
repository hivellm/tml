# Parser Specification

## Purpose

The parser transforms a token stream into an Abstract Syntax Tree (AST) representing the structure of TML programs. It implements the complete TML grammar using LL(1) parsing with Pratt parsing for expressions.

## ADDED Requirements

### Requirement: AST Generation
The parser SHALL produce a complete AST from a valid token stream.

#### Scenario: Simple program parsing
Given a token stream for `func main() { return 0 }`
When the parser processes the tokens
Then it produces an AST with FunctionDecl containing a ReturnStmt with IntLiteral(0)

#### Scenario: Complex program parsing
Given a token stream for a multi-function module with types
When the parser processes the tokens
Then it produces a complete AST representing the entire module structure

### Requirement: Expression Precedence
The parser MUST respect operator precedence as specified in the grammar.

#### Scenario: Arithmetic precedence
Given the expression `1 + 2 * 3`
When the parser parses the expression
Then it produces `Add(1, Mul(2, 3))` not `Mul(Add(1, 2), 3)`

#### Scenario: Comparison chain
Given the expression `a == b and c < d or e`
When the parser parses the expression
Then logical operators have lower precedence than comparisons

#### Scenario: Power operator right associativity
Given the expression `2 ** 3 ** 4`
When the parser parses the expression
Then it produces `Pow(2, Pow(3, 4))` (right associative)

### Requirement: Function Declaration Parsing
The parser SHALL parse function declarations with all modifiers.

#### Scenario: Simple function
Given the tokens for `func add(a: I32, b: I32) -> I32 { return a + b }`
When the parser parses the declaration
Then it produces FunctionDecl with params, return type, and body

#### Scenario: Generic function
Given the tokens for `func first[T](list: List[T]) -> Option[T]`
When the parser parses the declaration
Then it includes generic parameters in the AST

#### Scenario: Async function with caps
Given the tokens for `public async func fetch(url: String) -> Data caps: [io.network]`
When the parser parses the declaration
Then it includes visibility, async modifier, and capability annotations

### Requirement: Type Declaration Parsing
The parser MUST parse struct and enum type declarations.

#### Scenario: Struct type
Given the tokens for `type Point { x: F64, y: F64 }`
When the parser parses the declaration
Then it produces TypeDecl with struct fields

#### Scenario: Enum type
Given the tokens for `type Option[T] = Some(T) | None`
When the parser parses the declaration
Then it produces TypeDecl with enum variants

#### Scenario: Recursive enum
Given the tokens for `type List[T] = Cons(T, Box[List[T]]) | Nil`
When the parser parses the declaration
Then it correctly handles recursive type references

### Requirement: Control Flow Parsing
The parser SHALL parse all control flow constructs.

#### Scenario: If-then-else
Given the tokens for `if x > 0 then positive() else negative()`
When the parser parses the statement
Then it produces IfExpr with condition, then-branch, and else-branch

#### Scenario: When expression
Given the tokens for `when value { Some(x) -> x, None -> 0 }`
When the parser parses the expression
Then it produces WhenExpr with scrutinee and match arms

#### Scenario: Loop variants
Given tokens for `loop i in items { }`, `loop while cond { }`, and `loop { }`
When the parser parses each loop
Then it produces correct LoopStmt variants (for-in, while, infinite)

### Requirement: Pattern Parsing
The parser MUST parse all pattern types for matching.

#### Scenario: Literal patterns
Given a when arm `42 -> "answer"`
When the parser parses the pattern
Then it produces LiteralPattern with Int(42)

#### Scenario: Binding patterns
Given a when arm `Some(x) -> x * 2`
When the parser parses the pattern
Then it produces EnumPattern with binding `x`

#### Scenario: Guard patterns
Given a when arm `x if x > 0 -> positive`
When the parser parses the pattern
Then it produces BindingPattern with guard expression

### Requirement: Closure Parsing
The parser SHALL parse closure expressions (do syntax).

#### Scenario: Simple closure
Given the tokens for `do(x) x * 2`
When the parser parses the expression
Then it produces ClosureExpr with param `x` and body `x * 2`

#### Scenario: Multi-param closure
Given the tokens for `do(a, b) a + b`
When the parser parses the expression
Then it produces ClosureExpr with params `a`, `b`

#### Scenario: Block closure
Given the tokens for `do(x) { let y = x * 2; return y }`
When the parser parses the expression
Then it produces ClosureExpr with block body

### Requirement: Error Propagation Parsing
The parser MUST parse error propagation operators.

#### Scenario: Question mark operator
Given the tokens for `let x = get_value()?`
When the parser parses the expression
Then it produces TryExpr wrapping the call

#### Scenario: Bang operator
Given the tokens for `let x = get_value()!`
When the parser parses the expression
Then it produces PropagateExpr wrapping the call

#### Scenario: Inline else fallback
Given the tokens for `let x = get_value()! else default()`
When the parser parses the expression
Then it produces PropagateExpr with fallback expression

### Requirement: Generic Type Parsing
The parser SHALL parse generic type annotations correctly.

#### Scenario: Simple generics
Given the type annotation `Vec[T]`
When the parser parses the type
Then it produces GenericType with base `Vec` and arg `T`

#### Scenario: Nested generics
Given the type annotation `HashMap[String, Vec[I32]]`
When the parser parses the type
Then it produces correctly nested GenericType nodes

#### Scenario: Function type
Given the type annotation `func(I32, I32) -> I32`
When the parser parses the type
Then it produces FunctionType with params and return type

### Requirement: Error Recovery
The parser MUST recover from syntax errors and continue parsing.

#### Scenario: Missing semicolon recovery
Given tokens with a missing statement terminator
When the parser encounters the error
Then it reports the error and resynchronizes at the next statement

#### Scenario: Unmatched delimiter recovery
Given tokens with unmatched `{` or `(`
When the parser encounters the error
Then it reports the error and attempts to find the matching delimiter

#### Scenario: Invalid expression recovery
Given tokens with an invalid expression `let x = + 5`
When the parser encounters the error
Then it reports the error and attempts to continue parsing

### Requirement: Span Tracking
All AST nodes MUST include accurate source span information.

#### Scenario: Expression spans
Given a parsed expression
When examining the AST node
Then the span covers the entire expression from first to last token

#### Scenario: Declaration spans
Given a parsed function declaration
When examining the AST node
Then the span covers from `func` keyword to closing `}`
