# TML Compiler Infrastructure Parity Specification

This specification defines the requirements for achieving architectural parity between the TML compiler and production-grade compilers like rustc, covering query-based compilation, incremental builds, multi-backend support, advanced diagnostics, Polonius borrow checking, THIR layer, and self-hosting preparation.

## ADDED Requirements

### Requirement: Demand-Driven Query System
The TML compiler MUST implement a demand-driven query system where compilation phases are expressed as lazily-evaluated, memoized queries with automatic dependency tracking, replacing the current sequential pipeline architecture.

#### Scenario: Query memoization
Given a function `foo()` has been type-checked during compilation
When the codegen phase requests the type-checked AST of `foo()`
Then the query system SHALL return the cached result without re-executing the type checker

#### Scenario: Dependency tracking
Given query A depends on query B
When the result of query B changes on recompilation
Then query A SHALL be marked as potentially stale and re-evaluated

#### Scenario: Cycle detection
Given query A calls query B which calls query A
When the query system detects this circular dependency
Then it SHALL report a compiler error with the cycle path instead of entering an infinite loop

### Requirement: Fine-Grained Incremental Compilation
The TML compiler MUST support incremental compilation at the query level using a Red-Green algorithm, where only queries whose inputs have changed are re-executed.

#### Scenario: Single-function change
Given a project with 100 source files where one function body is modified
When the project is rebuilt incrementally
Then only the queries affected by that function change SHALL be re-executed and total recompilation time SHALL be under 500ms

#### Scenario: Cache persistence across builds
Given a successful compilation has completed and query results are serialized to disk
When the user modifies a single file and rebuilds
Then the compiler SHALL load cached query results from disk and use the Red-Green algorithm to determine which queries need re-execution

#### Scenario: Cache hit rate
Given a medium-sized project with minor changes between builds
When incremental compilation is enabled (default)
Then the cache hit rate MUST be greater than 80% of total queries

### Requirement: Backend Abstraction Layer
The TML compiler MUST implement a backend-agnostic codegen interface that allows multiple code generation backends to be used interchangeably, similar to rustc's `rustc_codegen_ssa`.

#### Scenario: LLVM backend migration
Given the existing LLVM codegen is refactored behind the `CodegenBackend` interface
When all existing tests are run against the refactored backend
Then all tests SHALL pass with zero regressions

#### Scenario: Backend selection
Given the compiler supports both LLVM and Cranelift backends
When the user specifies `--backend=cranelift`
Then the compiler SHALL use the Cranelift backend for code generation

#### Scenario: Backend feature fallback
Given the Cranelift backend does not support a specific feature (e.g., inline assembly)
When the user compiles code using that feature with `--backend=cranelift`
Then the compiler SHALL either fall back to LLVM for that module or emit a clear error message

### Requirement: Cranelift Backend
The TML compiler MUST support Cranelift as an alternative codegen backend optimized for fast debug compilation, achieving at least 3x faster compile times than LLVM at -O0.

#### Scenario: Debug build speed
Given a project compiled with `--backend=cranelift`
When compared to the same project compiled with `--backend=llvm -O0`
Then the Cranelift build SHALL complete at least 3x faster

#### Scenario: Test compatibility
Given the full test suite is run with `--backend=cranelift`
When Cranelift is used for code generation
Then at least 80% of existing tests SHALL pass

#### Scenario: MIR translation completeness
Given TML MIR includes instructions for arithmetic, control flow, function calls, structs, enums, and closures
When translating MIR to Cranelift IR
Then all core MIR instruction types SHALL have corresponding Cranelift translations

### Requirement: Advanced Diagnostics System
The TML compiler MUST provide professional-grade diagnostics including structured error codes, machine-applicable suggestions, multiline context rendering, and internationalization support.

#### Scenario: Error code lookup
Given the user receives error T0308 during compilation
When they run `tml explain T0308`
Then the compiler SHALL display a detailed explanation with examples and suggested fixes

#### Scenario: Machine-applicable suggestion
Given the user assigns to an immutable variable `let x = 5; x = 10;`
When the compiler reports the immutability error
Then it SHALL include a `MachineApplicable` suggestion to change `let x` to `var x`

#### Scenario: Auto-fix application
Given a source file has errors with `MachineApplicable` suggestions
When the user runs `tml fix source.tml`
Then the compiler SHALL automatically apply all machine-applicable fixes to the file

#### Scenario: Typo correction
Given the user writes `prntln("hello")` where `println` is available
When the compiler encounters the unresolved identifier
Then it SHALL suggest "did you mean `println`?" using edit distance matching

#### Scenario: Error deduplication
Given a type error in a function causes 5 downstream errors in callers
When the compiler renders diagnostics
Then it SHALL deduplicate related errors and show only the root cause with a count of suppressed errors

### Requirement: Polonius Borrow Checker
The TML compiler MUST implement the Polonius algorithm as an alternative borrow checker that accepts strictly more programs than the current NLL checker while maintaining soundness.

#### Scenario: NLL compatibility
Given the full test suite passes with the NLL borrow checker
When all tests are re-run with `--polonius`
Then 100% of previously passing tests SHALL also pass with Polonius

#### Scenario: Conditional return with references
Given a function that borrows a value, conditionally returns the reference, and then uses the original value in the else branch
When the NLL checker rejects this program
Then the Polonius checker (`--polonius`) SHALL accept it as valid

#### Scenario: Performance overhead
Given a large project compiled with NLL and then with Polonius
When compilation times are compared
Then the Polonius overhead SHALL be less than 2x compared to NLL

### Requirement: THIR (Typed HIR) Layer
The TML compiler MUST implement a Typed HIR intermediate representation between HIR and MIR where all implicit coercions, method resolutions, and operator desugaring are made fully explicit.

#### Scenario: Implicit coercion materialization
Given HIR contains an expression `let x: I64 = some_i32_value`
When HIR is lowered to THIR
Then the THIR SHALL contain an explicit widening coercion node: `let x: I64 = widen_i32_to_i64(some_i32_value)`

#### Scenario: Method call resolution
Given HIR contains `x.to_string()` where x is of type I32
When HIR is lowered to THIR
Then the THIR SHALL contain an explicit call: `Display::to_string(ref x)` with the receiver passed explicitly

#### Scenario: MIR lowering migration
Given THIR is generated as an intermediate step
When MIR lowering reads its input
Then it SHALL consume THIR (not HIR) as its primary input representation

### Requirement: Advanced Trait Solver
The TML compiler MUST implement a goal-based trait solver capable of resolving associated type projections, higher-ranked behavior bounds, and specialization.

#### Scenario: Associated type resolution
Given a behavior `Iterator` with associated type `Item` and an impl `Iterator for Range[I32]` where `Item = I32`
When the type checker encounters `range.next()` returning `Maybe[<Range[I32] as Iterator>::Item]`
Then the solver SHALL normalize the projection to `Maybe[I32]`

#### Scenario: Higher-ranked bounds
Given a function `func apply[F](f: F) where F: for[T] Fn(T) -> T`
When the type checker validates a call `apply(identity_function)`
Then the solver SHALL verify the bound holds for all types T

#### Scenario: Specialization
Given a generic impl `Display for [T] where T: Display` and a specialized impl `Display for [I32]`
When resolving `Display` for `[I32]`
Then the solver SHALL select the more specific impl for `[I32]`

### Requirement: Self-Hosting Preparation
The TML project MUST begin the path to self-hosting by rewriting the compiler's lexer and parser in TML, compilable by the existing C++ compiler (Stage 0), producing output identical to the C++ implementation.

#### Scenario: Lexer equivalence
Given the TML lexer is rewritten in TML
When both the C++ lexer and TML lexer tokenize the same source file
Then they SHALL produce identical token streams

#### Scenario: Parser equivalence
Given the TML parser is rewritten in TML
When both the C++ parser and TML parser parse the same source file
Then they SHALL produce identical AST structures

#### Scenario: Bootstrap compilation
Given the TML lexer and parser are written in TML
When the Stage 0 C++ compiler compiles these TML source files
Then it SHALL produce working executables that can tokenize and parse TML source code
