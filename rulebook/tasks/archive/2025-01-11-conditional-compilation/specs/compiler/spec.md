# Conditional Compilation Specification

## Purpose

The Conditional Compilation system enables platform-specific, architecture-specific, and feature-gated code in TML. This implementation follows the existing specification in docs/specs/21-TARGETS.md, docs/specs/07-MODULES.md, and docs/specs/25-DECORATORS.md.

## ADDED Requirements

### Requirement: Target Triple Parsing
The compiler SHALL parse target triples in the format `<arch>-<vendor>-<os>-<env>`.

#### Scenario: Parse standard target triple
Given the target triple "x86_64-pc-windows-msvc"
When parsing the target
Then arch is "x86_64", vendor is "pc", os is "windows", env is "msvc"

#### Scenario: Parse minimal target triple
Given the target triple "x86_64-unknown-linux-gnu"
When parsing the target
Then arch is "x86_64", vendor is "unknown", os is "linux", env is "gnu"

#### Scenario: Parse macOS target triple
Given the target triple "aarch64-apple-darwin"
When parsing the target
Then arch is "aarch64", vendor is "apple", os is "macos", env is ""

### Requirement: Host Target Detection
The compiler MUST auto-detect the host platform when no --target is specified.

#### Scenario: Windows host detection
Given the compiler is running on Windows
When no --target flag is provided
Then target_os is set to "windows"

#### Scenario: Linux host detection
Given the compiler is running on Linux
When no --target flag is provided
Then target_os is set to "linux"

### Requirement: @when Predicate Parsing
The compiler SHALL parse @when decorators with predicate expressions.

#### Scenario: Parse target_os predicate
Given the code `@when(target_os = "linux")`
When parsing the decorator
Then a CfgKeyValue node is created with key="target_os" and value="linux"

#### Scenario: Parse simple flag predicate
Given the code `@when(debug)`
When parsing the decorator
Then a CfgFlag node is created with name="debug"

#### Scenario: Parse not predicate
Given the code `@when(not(target_os = "windows"))`
When parsing the decorator
Then a CfgNot node wrapping a CfgKeyValue is created

#### Scenario: Parse all predicate
Given the code `@when(all(unix, target_arch = "x86_64"))`
When parsing the decorator
Then a CfgAll node with two children is created

#### Scenario: Parse any predicate
Given the code `@when(any(target_os = "linux", target_os = "macos"))`
When parsing the decorator
Then a CfgAny node with two children is created

#### Scenario: Parse alternate syntax
Given the code `@when(os: linux)` using colon syntax
When parsing the decorator
Then it is equivalent to `@when(target_os = "linux")`

### Requirement: @unless Predicate Parsing
The compiler SHALL parse @unless as syntactic sugar for @when(not(...)).

#### Scenario: Parse @unless decorator
Given the code `@unless(target_os = "windows")`
When parsing the decorator
Then a CfgNot node wrapping the predicate is created

### Requirement: Predicate Evaluation - Target OS
The compiler MUST correctly evaluate target_os predicates.

#### Scenario: Matching OS predicate
Given target is x86_64-pc-windows-msvc
When evaluating `@when(target_os = "windows")`
Then the predicate evaluates to true

#### Scenario: Non-matching OS predicate
Given target is x86_64-pc-windows-msvc
When evaluating `@when(target_os = "linux")`
Then the predicate evaluates to false

### Requirement: Predicate Evaluation - Target Architecture
The compiler MUST correctly evaluate target_arch predicates.

#### Scenario: Matching arch predicate
Given target is x86_64-pc-windows-msvc
When evaluating `@when(target_arch = "x86_64")`
Then the predicate evaluates to true

#### Scenario: Non-matching arch predicate
Given target is x86_64-pc-windows-msvc
When evaluating `@when(target_arch = "aarch64")`
Then the predicate evaluates to false

### Requirement: Predicate Evaluation - Pointer Width
The compiler MUST correctly evaluate target_pointer_width predicates.

#### Scenario: 64-bit architecture
Given target is x86_64-unknown-linux-gnu
When evaluating `@when(target_pointer_width = "64")`
Then the predicate evaluates to true

#### Scenario: 32-bit architecture
Given target is i686-unknown-linux-gnu
When evaluating `@when(target_pointer_width = "32")`
Then the predicate evaluates to true

### Requirement: Predicate Evaluation - Endianness
The compiler MUST correctly evaluate target_endian predicates.

#### Scenario: Little-endian architecture
Given target is x86_64-unknown-linux-gnu
When evaluating `@when(target_endian = "little")`
Then the predicate evaluates to true

### Requirement: Predicate Evaluation - OS Family
The compiler MUST correctly evaluate unix and windows family predicates.

#### Scenario: Unix family
Given target is x86_64-unknown-linux-gnu
When evaluating `@when(unix)`
Then the predicate evaluates to true

#### Scenario: Windows family
Given target is x86_64-pc-windows-msvc
When evaluating `@when(windows)`
Then the predicate evaluates to true

#### Scenario: macOS is unix
Given target is aarch64-apple-darwin
When evaluating `@when(unix)`
Then the predicate evaluates to true

### Requirement: Predicate Evaluation - Build Mode
The compiler MUST correctly evaluate debug and release predicates.

#### Scenario: Debug build
Given the compiler is running in debug mode
When evaluating `@when(debug)`
Then the predicate evaluates to true

#### Scenario: Release build
Given the compiler is running in release mode
When evaluating `@when(release)`
Then the predicate evaluates to true

### Requirement: Predicate Evaluation - Feature Flags
The compiler MUST correctly evaluate feature predicates.

#### Scenario: Enabled feature
Given --features includes "async"
When evaluating `@when(feature = "async")`
Then the predicate evaluates to true

#### Scenario: Disabled feature
Given --features does not include "async"
When evaluating `@when(feature = "async")`
Then the predicate evaluates to false

### Requirement: Predicate Evaluation - Logical Operators
The compiler MUST correctly evaluate not(), all(), and any() operators.

#### Scenario: not() operator
Given target is x86_64-pc-windows-msvc
When evaluating `@when(not(target_os = "linux"))`
Then the predicate evaluates to true

#### Scenario: all() operator - all true
Given target is x86_64-unknown-linux-gnu
When evaluating `@when(all(unix, target_arch = "x86_64"))`
Then the predicate evaluates to true

#### Scenario: all() operator - one false
Given target is aarch64-unknown-linux-gnu
When evaluating `@when(all(unix, target_arch = "x86_64"))`
Then the predicate evaluates to false

#### Scenario: any() operator - one true
Given target is x86_64-unknown-linux-gnu
When evaluating `@when(any(target_os = "linux", target_os = "macos"))`
Then the predicate evaluates to true

#### Scenario: any() operator - all false
Given target is x86_64-pc-windows-msvc
When evaluating `@when(any(target_os = "linux", target_os = "macos"))`
Then the predicate evaluates to false

### Requirement: AST Filtering
The compiler SHALL filter out AST nodes whose @when predicates evaluate to false.

#### Scenario: Filter function
Given a function with `@when(target_os = "linux")` and target is windows
When filtering the AST
Then the function is removed from the AST

#### Scenario: Keep function
Given a function with `@when(target_os = "windows")` and target is windows
When filtering the AST
Then the function is kept in the AST

#### Scenario: Filter type
Given a type with `@when(target_arch = "aarch64")` and target is x86_64
When filtering the AST
Then the type is removed from the AST

#### Scenario: Filter struct field
Given a struct with a field with `@when(unix)` and target is windows
When filtering the AST
Then the field is removed from the struct

### Requirement: Conditional Code Generation
The compiler MUST not generate code for filtered items.

#### Scenario: No code for filtered function
Given a function filtered by @when predicate
When generating LLVM IR
Then no IR is emitted for that function

### Requirement: CLI Target Flag
The compiler SHALL support --target flag for cross-compilation.

#### Scenario: Explicit target
Given the command `tml build file.tml --target aarch64-unknown-linux-gnu`
When compiling
Then target is set to aarch64-unknown-linux-gnu

### Requirement: CLI Features Flag
The compiler SHALL support --features flag for feature flags.

#### Scenario: Single feature
Given the command `tml build file.tml --features async`
When compiling
Then feature "async" is enabled

#### Scenario: Multiple features
Given the command `tml build file.tml --features async,logging`
When compiling
Then features "async" and "logging" are enabled

### Requirement: Error Messages
The compiler MUST provide helpful error messages for invalid predicates.

#### Scenario: Unknown predicate key
Given the code `@when(target_ox = "linux")` with typo
When parsing the decorator
Then an error is reported suggesting "target_os"

#### Scenario: Invalid predicate value
Given the code `@when(target_os = "linucks")` with typo
When parsing the decorator
Then an error is reported with valid OS values

### Requirement: Predicate Warnings
The compiler SHOULD warn about always-true or always-false predicates.

#### Scenario: Always-true predicate
Given the code `@when(any(debug, release))`
When analyzing predicates
Then a warning is emitted that predicate is always true

#### Scenario: Always-false predicate
Given the code `@when(all(target_os = "linux", target_os = "windows"))`
When analyzing predicates
Then a warning is emitted that predicate is always false

## Supported Predicates Reference

### Operating Systems
- `target_os = "linux"`
- `target_os = "windows"`
- `target_os = "macos"`
- `target_os = "ios"`
- `target_os = "android"`
- `target_os = "freebsd"`
- `target_os = "none"` (bare metal / wasm)

### Architectures
- `target_arch = "x86_64"`
- `target_arch = "aarch64"`
- `target_arch = "i686"`
- `target_arch = "arm"`
- `target_arch = "wasm32"`
- `target_arch = "riscv64"`

### Other Predicates
- `target_pointer_width = "32" | "64"`
- `target_endian = "little" | "big"`
- `target_env = "gnu" | "msvc" | "musl"`
- `target_vendor = "apple" | "pc" | "unknown"`
- `target_feature = "sse2" | "avx" | "neon" | ...`
- `unix` (family)
- `windows` (family)
- `debug` (build mode)
- `release` (build mode)
- `test` (test mode)
- `feature = "feature_name"` (feature flags)

### Logical Operators
- `not(predicate)` - Negation
- `all(pred1, pred2, ...)` - All must be true (AND)
- `any(pred1, pred2, ...)` - Any must be true (OR)

### Alternate Syntax (Shorthand)
- `os: linux` is equivalent to `target_os = "linux"`
- `arch: x86_64` is equivalent to `target_arch = "x86_64"`
- `feature: async` is equivalent to `feature = "async"`
