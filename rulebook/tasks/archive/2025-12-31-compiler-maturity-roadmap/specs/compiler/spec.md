# Compiler Specification Deltas

## ADDED Requirements

### Requirement: Mid-level Intermediate Representation (MIR)
The compiler SHALL implement a mid-level intermediate representation in SSA form between AST and LLVM IR codegen.

#### Scenario: MIR Generation
Given a valid TML source file
When the compiler parses and type-checks the source
Then the compiler MUST generate valid MIR in SSA form
And the MIR MUST preserve all semantic information from the AST
And the MIR MUST be suitable for optimization passes

#### Scenario: MIR Debug Output
Given the `--emit-mir` flag is provided
When the compiler processes a source file
Then the compiler MUST output human-readable MIR representation
And the output MUST include basic block labels
And the output MUST show phi nodes for SSA

---

### Requirement: Constant Folding Optimization
The compiler SHALL implement constant folding as an optimization pass on MIR.

#### Scenario: Integer Constant Folding
Given MIR containing `%1 = add i32 2, 3`
When the constant folding pass executes
Then the instruction MUST be replaced with `%1 = 5`

#### Scenario: Boolean Constant Folding
Given MIR containing `%1 = and i1 true, false`
When the constant folding pass executes
Then the instruction MUST be replaced with `%1 = false`

---

### Requirement: Dead Code Elimination
The compiler SHALL implement dead code elimination as an optimization pass.

#### Scenario: Unused Variable Elimination
Given a variable that is never read after assignment
When the DCE pass executes
Then the assignment MUST be removed
And any side-effect-free expressions used in assignment MUST be removed

#### Scenario: Unreachable Code Elimination
Given code after an unconditional return statement
When the DCE pass executes
Then all code after the return MUST be removed

---

### Requirement: Escape Analysis
The compiler SHALL implement escape analysis to determine object lifetimes.

#### Scenario: Stack Allocation Promotion
Given an object that does not escape the current function
When escape analysis completes
Then the object MAY be allocated on the stack instead of heap

#### Scenario: Escape Detection
Given a reference that is stored in a global or returned from function
When escape analysis runs
Then the referenced object MUST be marked as escaping

---

### Requirement: Function Inlining
The compiler SHALL implement function inlining with configurable heuristics.

#### Scenario: Small Function Inlining
Given a function with body smaller than inline threshold
When called with constant arguments
Then the function call MAY be replaced with inlined body

#### Scenario: Inline Attribute
Given a function marked with `@inline`
When the function is called
Then the compiler SHOULD inline the function body
And recursive calls MUST respect recursion depth limit

---

### Requirement: Incremental Compilation
The compiler SHALL support incremental compilation for faster rebuilds.

#### Scenario: Unchanged File Skip
Given a source file that has not changed since last build
When building the project
Then the compiler MUST reuse cached compilation artifacts

#### Scenario: Dependency Tracking
Given module A depends on module B
When module B's interface changes
Then module A MUST be recompiled
And modules not depending on B MUST NOT be recompiled

---

### Requirement: Enhanced Error Diagnostics
The compiler SHALL provide rich error diagnostics with suggestions.

#### Scenario: Typo Suggestion
Given an undefined identifier that is similar to a defined one
When reporting the error
Then the compiler MUST suggest the similar identifier
And similarity MUST be determined by edit distance

#### Scenario: Multi-span Errors
Given an error involving multiple source locations
When reporting the error
Then the compiler MUST show all relevant locations
And locations MUST be labeled with their role (e.g., "first borrow here")

---

### Requirement: Debug Information Generation
The compiler SHALL generate DWARF debug information.

#### Scenario: Source Location Mapping
Given debug mode is enabled (`-g` flag)
When generating object code
Then DWARF info MUST map instructions to source locations
And debugger MUST be able to set breakpoints by line number

#### Scenario: Variable Inspection
Given a local variable in debug build
When stopped at breakpoint in debugger
Then the variable's value MUST be inspectable
And the variable's type MUST be displayed correctly

---

### Requirement: Parallel Compilation
The compiler SHALL support parallel compilation of independent modules.

#### Scenario: Multi-file Parallel Build
Given a project with multiple independent source files
When building with `-j N` flag (N > 1)
Then the compiler MUST parse files in parallel
And the compiler MUST generate code for independent modules in parallel

---

### Requirement: Cross Compilation
The compiler SHALL support cross-compilation to different targets.

#### Scenario: Target Selection
Given the `--target <triple>` flag
When compiling
Then the compiler MUST generate code for the specified target
And type sizes MUST match target ABI

---

### Requirement: Package Manager
The compiler SHALL include a package manager for dependency management.

#### Scenario: Dependency Resolution
Given a `tml.toml` file with dependencies
When running `tml build`
Then dependencies MUST be resolved and downloaded
And conflicting versions MUST produce an error

---

### Requirement: Language Server Protocol
The compiler SHALL provide an LSP server for IDE integration.

#### Scenario: Autocompletion
Given a partial identifier in the editor
When requesting completions
Then the LSP MUST return valid completions from scope
And completions MUST include type information

#### Scenario: Go to Definition
Given an identifier usage
When requesting definition location
Then the LSP MUST return the declaration location
And location MUST include file path and position

---

### Requirement: Documentation Generator
The compiler SHALL generate documentation from source comments.

#### Scenario: Doc Comment Extraction
Given functions with `///` doc comments
When running `tml doc`
Then HTML documentation MUST be generated
And documentation MUST include function signatures and descriptions

---

## MODIFIED Requirements

### Requirement: Build Caching (Enhancement)
The existing build caching SHALL be enhanced to support finer-grained caching.

#### Scenario: Function-level Caching
Given a source file with multiple functions
When only one function changes
Then only that function's code MUST be regenerated
And other functions MUST use cached codegen

---

### Requirement: Error Messages (Enhancement)
Existing error messages SHALL be enhanced with error codes and fix-its.

#### Scenario: Error Codes
Given any compiler error
When the error is reported
Then the error MUST include a unique error code (e.g., E0001)
And documentation for the error code MUST be available

---

## Project Structure Changes

### RENAMED Paths

| Old Path | New Path |
|----------|----------|
| `packages/compiler/` | `compiler/` |
| `packages/core/` | `lib/core/` |
| `packages/std/` | `lib/std/` |

### ADDED Directories

| Path | Purpose |
|------|---------|
| `compiler/src/mir/` | Mid-level IR implementation |
| `compiler/src/opt/` | Optimization passes |
| `compiler/src/lsp/` | Language Server Protocol |
| `compiler/src/pkg/` | Package manager |
| `tools/` | Auxiliary tools (doc gen, etc.) |
