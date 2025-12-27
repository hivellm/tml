# Next Compiler Features Specification

## Purpose

Implement critical missing features for TML compiler including closures with environment capture, where clause enforcement, improved error diagnostics, and quality-of-life syntax improvements.

---

## ADDED Requirements

### Closure Environment Capture

**Requirement**: The compiler SHALL implement closures that capture variables from their surrounding scope.

#### Scenario: Capturing a variable in a closure

**Given** a function with local variable `x: I32 = 10`
**When** a closure `do(y) x + y` is created inside the function
**Then** the closure SHALL capture `x` from the environment
**And** the closure type SHALL include captured variable types
**And** calling the closure SHALL access the captured value

#### Scenario: Closure type checking

**Given** a closure `do(n: I32) -> I32 { count + n }` capturing `count: I32`
**When** the type checker analyzes the closure
**Then** it SHALL infer closure type as `Closure[(I32) -> I32]`
**And** captured variables SHALL be type-checked for usage

#### Scenario: Generating LLVM IR for closures

**Given** a closure with captured environment
**When** the code generator processes the closure
**Then** it SHALL create a closure struct with environment fields
**And** SHALL generate wrapper function for closure invocation
**And** SHALL properly handle variable lifetimes

### Where Clause Type Constraints

**Requirement**: The compiler SHALL enforce where clause constraints on generic type parameters.

#### Scenario: Parsing where clause in function signature

**Given** a function `func sort[T](arr: Vec[T]) where T: Ord`
**When** the parser processes the signature
**Then** it SHALL parse the where clause constraint
**And** SHALL associate `Ord` behavior requirement with type `T`

#### Scenario: Enforcing where constraints at call site

**Given** a function with constraint `where T: Display`
**When** the function is called with type `I32`
**Then** the type checker SHALL verify `I32` implements `Display`
**And** SHALL emit error if constraint is not satisfied

### Use Statement Groups

**Requirement**: The parser SHALL support grouped import syntax `use module::{item1, item2}`.

#### Scenario: Parsing grouped imports

**Given** source code `use std::{option::Maybe, result::Outcome}`
**When** the parser processes the use statement
**Then** it SHALL parse the group as multiple imports
**And** SHALL resolve `std::option::Maybe` and `std::result::Outcome`

### Named Enum Fields

**Requirement**: The compiler SHALL support enum variants with named fields like structs.

#### Scenario: Defining enum variant with named fields

**Given** an enum `type Result = Ok { value: I32 } | Err { message: Str }`
**When** the parser processes the enum definition
**Then** it SHALL parse named fields for each variant
**And** type checker SHALL treat fields like struct fields

#### Scenario: Accessing named enum fields

**Given** a value `result = Ok { value: 42 }`
**When** pattern matching with `Ok { value } => ...`
**Then** the type checker SHALL bind `value` with type `I32`
**And** SHALL verify field names match variant definition

### Improved Error Messages

**Requirement**: The compiler MUST provide contextual error messages with helpful hints.

#### Scenario: Undefined identifier with similar names

**Given** code using undefined identifier `printn`
**When** the compiler encounters the error
**Then** it SHALL suggest similar names like `println`
**And** error message SHALL show "did you mean `println`?"

#### Scenario: Type mismatch with conversion hints

**Given** code passing `I32` where `I64` is expected
**When** type checking fails
**Then** error SHALL suggest explicit conversion using `as I64`
**And** SHALL show both expected and actual types clearly

### String Interpolation

**Requirement**: The compiler SHALL support string interpolation syntax `"text {expr}"`.

#### Scenario: Parsing string interpolation

**Given** source code `"Value: {x + 1}"`
**When** the parser processes the string
**Then** it SHALL parse interpolation expressions
**And** SHALL desugar to `format("Value: {}", x + 1)` call

#### Scenario: Type checking interpolated expressions

**Given** interpolation `"Result: {compute()}"`
**When** the type checker processes the interpolation
**Then** it SHALL type-check `compute()` call
**And** SHALL verify result type can be formatted

### Inline Module Syntax

**Requirement**: The parser SHALL support inline module definitions `mod name { ... }`.

#### Scenario: Parsing inline module

**Given** source code `mod utils { func helper() -> I32 { 42 } }`
**When** the parser processes the module
**Then** it SHALL create module scope for `utils`
**And** SHALL add `helper` function to module namespace

#### Scenario: Using inline module items

**Given** inline module `mod math { pub func add(a: I32, b: I32) -> I32 { a + b } }`
**When** calling `math::add(1, 2)`
**Then** type checker SHALL resolve function from module
**And** SHALL verify function is public

### Documentation Generation

**Requirement**: The compiler SHALL extract doc comments and generate HTML documentation.

#### Scenario: Parsing doc comments

**Given** function with doc comment `/// Adds two numbers`
**When** the parser processes the comment
**Then** it SHALL associate comment with following item
**And** SHALL preserve markdown formatting in comment

#### Scenario: Generating documentation HTML

**Given** a module with documented functions
**When** running `tml doc`
**Then** it SHALL generate HTML files for each module
**And** SHALL include function signatures and descriptions
**And** SHALL create cross-reference links between types

---

## MODIFIED Requirements

### LLVM Runtime Linking

**Requirement**: The code generator SHALL properly link TML runtime library functions.

#### Scenario: Linking println at runtime

**Given** code calling `println("Hello")`
**When** generating executable
**Then** the linker SHALL find `tml_println` runtime function
**And** SHALL link without undefined symbol errors

#### Scenario: Runtime library initialization

**Given** an executable with runtime dependencies
**When** the program starts
**Then** runtime SHALL initialize heap allocator
**And** SHALL set up panic handler

---

## Verification

Requirements will be verified by:
- Unit tests for each feature (parser, type checker, codegen)
- Integration tests with example programs using new features
- Documentation generation for test projects
- Error message quality review with example diagnostics
