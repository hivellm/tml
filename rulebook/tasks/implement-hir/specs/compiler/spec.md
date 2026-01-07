# HIR Specification

High-level Intermediate Representation for the TML compiler, providing a typed, stable representation between AST and MIR.

## ADDED Requirements

### Requirement: HIR Data Structures
The compiler SHALL provide HIR data structures that preserve type information from the type checker while simplifying the AST for lowering to MIR.

#### Scenario: HIR preserves resolved types
Given a function with generic type parameters
When the AST is lowered to HIR
Then all generic types are resolved to concrete monomorphized types

#### Scenario: HIR tracks source locations
Given any HIR node
When accessed for diagnostics
Then the node provides accurate source span information

### Requirement: AST to HIR Lowering
The compiler SHALL lower type-checked AST to HIR, resolving all type information and performing generic monomorphization.

#### Scenario: Expression lowering
Given a type-checked expression in the AST
When lowered to HIR
Then the HIR expression contains the resolved type

#### Scenario: Generic function monomorphization
Given a generic function called with concrete type arguments
When lowered to HIR
Then a specialized function instance is created with concrete types

#### Scenario: Closure capture analysis
Given a closure expression
When lowered to HIR
Then all captured variables are identified with their capture mode (by value or by reference)

### Requirement: HIR to MIR Lowering
The compiler SHALL lower HIR to MIR, generating control flow graphs and SSA form.

#### Scenario: Control flow lowering
Given an if-else expression in HIR
When lowered to MIR
Then appropriate basic blocks and conditional branches are generated

#### Scenario: Drop insertion
Given a variable that implements Drop
When its scope ends in HIR
Then a drop call is inserted in the MIR

### Requirement: HIR Optimizations
The compiler SHALL perform optimizations at the HIR level to improve generated code quality.

#### Scenario: Constant folding
Given an arithmetic expression with only literal operands
When HIR optimization runs
Then the expression is replaced with the computed literal value

#### Scenario: Dead code elimination
Given code after an unconditional return statement
When HIR optimization runs
Then the unreachable code is removed

#### Scenario: Inline expansion
Given a function marked with @inline attribute
When HIR optimization runs
Then the function body is expanded at call sites

### Requirement: HIR Caching
The compiler SHALL cache HIR to enable incremental compilation.

#### Scenario: HIR serialization
Given a compiled module's HIR
When the module is cached
Then the HIR can be serialized to disk and deserialized later

#### Scenario: Change detection
Given a source file modification
When incremental compilation runs
Then only affected HIR modules are recompiled

### Requirement: Pipeline Integration
The compiler SHALL use the HIR pipeline for all compilation modes.

#### Scenario: Build command uses HIR
Given a tml build command
When compilation runs
Then the AST is lowered to HIR before MIR generation

#### Scenario: Test command uses HIR
Given a tml test command
When test compilation runs
Then the AST is lowered to HIR before MIR generation

## MODIFIED Requirements

### Requirement: MIR Builder Input
The MIR builder SHALL accept HIR as input instead of AST.

#### Scenario: MIR from HIR
Given HIR for a function
When the MIR builder processes it
Then MIR basic blocks are generated from HIR expressions
