# Type Checker Specification

## Purpose

The type checker verifies type safety of TML programs through type inference and checking. It produces a Typed AST where all expressions have resolved concrete types, enabling subsequent compiler stages to operate on fully-typed code.

## ADDED Requirements

### Requirement: Type Inference
The type checker SHALL infer types for expressions without explicit annotations.

#### Scenario: Literal inference
Given the expression `let x = 42`
When the type checker processes the declaration
Then it infers the type of `x` as `I32` (default integer type)

#### Scenario: Function return inference
Given `func add(a: I32, b: I32) { return a + b }`
When the type checker processes the function
Then it infers the return type as `I32`

#### Scenario: Generic inference
Given `let list = Vec.new()` followed by `list.push(42)`
When the type checker processes both statements
Then it infers `list` has type `Vec[I32]`

### Requirement: Type Unification
The type checker MUST unify types correctly during inference.

#### Scenario: Simple unification
Given type variable `?T` and concrete type `I32`
When unification is performed
Then `?T` is bound to `I32`

#### Scenario: Recursive unification
Given `Vec[?T]` and `Vec[String]`
When unification is performed
Then `?T` is bound to `String`

#### Scenario: Occurs check failure
Given `?T` and `Vec[?T]`
When unification is attempted
Then it fails with "infinite type" error

### Requirement: Generic Instantiation
The type checker SHALL correctly instantiate generic types.

#### Scenario: Function instantiation
Given generic function `func id[T](x: T) -> T` called as `id(42)`
When the type checker processes the call
Then `T` is instantiated to `I32`

#### Scenario: Type instantiation
Given generic type `Vec[T]` used as `Vec[String]`
When the type checker processes the type
Then it produces concrete type with `T = String`

#### Scenario: Multiple type parameters
Given `HashMap[K, V]` used as `HashMap[String, I32]`
When the type checker processes the type
Then it correctly binds `K = String` and `V = I32`

### Requirement: Trait Bound Checking
The type checker MUST enforce trait bounds on generic parameters.

#### Scenario: Satisfied trait bound
Given `func print[T: Display](x: T)` called with `String`
When the type checker processes the call
Then it succeeds because `String` implements `Display`

#### Scenario: Unsatisfied trait bound
Given `func print[T: Display](x: T)` called with custom type without `Display`
When the type checker processes the call
Then it reports error "type does not implement trait Display"

#### Scenario: Multiple trait bounds
Given `func compare[T: Eq + Ord](a: T, b: T)`
When the type checker processes a call
Then it verifies the type implements both `Eq` and `Ord`

### Requirement: Method Resolution
The type checker SHALL resolve method calls correctly.

#### Scenario: Inherent method
Given `point.distance(other)` where `Point` has method `distance`
When the type checker processes the call
Then it resolves to `Point::distance`

#### Scenario: Trait method
Given `value.to_string()` where `Display` provides `to_string`
When the type checker processes the call
Then it resolves to the trait implementation

#### Scenario: Ambiguous method
Given multiple traits providing same method name
When the type checker cannot determine which to use
Then it reports "ambiguous method" error

### Requirement: Binary Operator Type Checking
The type checker MUST verify operand types for binary operations.

#### Scenario: Numeric operations
Given `a + b` where both are `I32`
When the type checker processes the expression
Then result type is `I32`

#### Scenario: Type mismatch
Given `a + b` where `a: I32` and `b: String`
When the type checker processes the expression
Then it reports "cannot add I32 and String"

#### Scenario: Custom operator
Given `a + b` where type implements `Add` trait
When the type checker processes the expression
Then it uses the trait's output type

### Requirement: Function Type Checking
The type checker SHALL verify function bodies match their signatures.

#### Scenario: Correct return type
Given function returning `I32` with body `return 42`
When the type checker processes the function
Then it succeeds

#### Scenario: Wrong return type
Given function returning `String` with body `return 42`
When the type checker processes the function
Then it reports "expected String, found I32"

#### Scenario: Missing return
Given function returning `I32` with body that doesn't return
When the type checker processes the function
Then it reports "function must return a value"

### Requirement: Pattern Type Checking
The type checker MUST verify patterns match their scrutinee types.

#### Scenario: Enum pattern
Given `when opt { Some(x) -> x, None -> 0 }` with `opt: Option[I32]`
When the type checker processes the when expression
Then patterns are verified against `Option[I32]`

#### Scenario: Exhaustiveness check
Given a when expression missing some variants
When the type checker processes the expression
Then it reports "non-exhaustive patterns"

#### Scenario: Unreachable pattern
Given a when expression with duplicate patterns
When the type checker processes the expression
Then it reports "unreachable pattern"

### Requirement: Mutability Checking
The type checker SHALL enforce mutability rules.

#### Scenario: Mutable variable
Given `var x = 1; x = 2`
When the type checker processes the assignment
Then it succeeds

#### Scenario: Immutable variable
Given `let x = 1; x = 2`
When the type checker processes the assignment
Then it reports "cannot assign to immutable variable"

#### Scenario: Mutable reference
Given `let r = &mut x` where `x` is declared with `var`
When the type checker processes the reference
Then it succeeds

### Requirement: Effect Checking
The type checker MUST verify capability annotations.

#### Scenario: Effect propagation
Given function with `caps: [io.file]` calling another with same cap
When the type checker processes the call
Then it succeeds

#### Scenario: Missing capability
Given function without caps calling one with `caps: [io.file]`
When the type checker processes the call
Then it reports "missing capability io.file"

### Requirement: Type Error Messages
All type errors MUST include helpful context.

#### Scenario: Expected vs found
Given a type mismatch error
When the error is reported
Then it includes both expected and found types

#### Scenario: Source location
Given any type error
When the error is reported
Then it includes file, line, and column information

#### Scenario: Suggestion
Given a common mistake (e.g., I32 vs I64)
When the error is reported
Then it may include a suggestion for fixing
