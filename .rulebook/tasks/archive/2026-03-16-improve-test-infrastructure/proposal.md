# Proposal: Improve Test Infrastructure

## Status: PROPOSED

## Why

The TML compiler has 5000+ tests across 509 test files, which is a solid foundation. However, the current test suite focuses almost exclusively on **positive testing** - verifying that correct code compiles and runs properly. Mature compilers like GCC (200K+ tests), Clang (50K+), and rustc (20K+) invest equally in negative testing, diagnostics verification, fuzzing, and cross-feature interaction testing.

The current gaps mean that:
- Error messages can regress silently without detection
- Edge cases at type boundaries are under-tested
- Bugs that arise from feature interactions (generics + borrow checker, closures + iterators) go undetected
- There is no systematic way to prevent bug reintroduction
- Coverage data is not consistently generated or tracked as a gate

Addressing these gaps will significantly improve compiler reliability and developer experience.

## What Changes

### 1. Diagnostic / Error-Checking Tests
Add a framework for testing that **invalid code produces the correct error messages**. This requires:
- A test directive format (e.g., `// @expect-error T001`) that the test runner recognizes
- Test runner support to match expected errors against actual compiler output
- A suite of negative tests covering type errors, borrow violations, syntax errors, and semantic errors

### 2. Edge Case / Boundary Tests
Systematic tests for boundary conditions:
- Integer overflow/underflow at MIN/MAX values for all numeric types
- Empty strings, zero-length arrays, empty structs
- Deeply nested generics (`Heap[Maybe[Vec[I32]]]`)
- Deep recursion, large match arms, many function parameters

### 3. Regression Test Convention
Establish a `regression/` test directory with naming convention:
- `issue_NNNN_short_description.test.tml`
- Every bug fix must include a regression test
- CI should enforce this via PR checks

### 4. Cross-Feature Interaction Tests
Tests that exercise multiple compiler features simultaneously:
- Generics + borrow checker
- Closures capturing variables + iterators
- Behaviors (traits) + generics + type inference
- Error handling + async (future)
- Conditional compilation + platform-specific code

### 5. Coverage as CI Gate
- Fix coverage report generation to produce consistent data
- Set minimum coverage thresholds per module (target: 80%)
- CI blocks merge if coverage drops below threshold
- Dashboard or report for tracking coverage trends over time

### 6. Expanded Fuzzing
- Expand the existing fuzzer (`compiler/src/cli/tester/fuzzer.cpp`) to cover more syntax patterns
- Grammar-guided fuzzing to generate syntactically varied but semantically random programs
- Crash-only mode: any compiler crash (segfault, assertion failure) is a bug, error messages are fine
- Round-trip testing: `format(parse(code))` should be idempotent

### 7. Cross-Platform CI
- Add CI matrix for Windows + Linux + macOS
- Platform-specific tests using `#if WINDOWS` / `#if UNIX` directives
- Verify that conditional compilation paths are tested on their target platforms

### 8. Compilation Performance Benchmarks
- Track compilation times for representative files across versions
- Detect regressions in incremental compilation (cache effectiveness)
- Memory usage tracking for the compiler process

## Impact
- Affected specs: `10-TESTING.md`, `09-CLI.md`, `12-ERRORS.md`
- Affected code: `compiler/src/cli/tester/`, `lib/*/tests/`, CI configuration
- Breaking change: NO
- User benefit: More reliable compiler, better error messages, fewer regressions

## Dependencies
- Coverage system must be functional (Phase 5 depends on this)
- Fuzzer infrastructure exists but needs expansion (Phase 6)
- Cross-platform CI requires build scripts for Linux/macOS (Phase 7)

## Success Criteria
1. Diagnostic test framework operational with `@expect-error` directive support
2. At least 100 negative/diagnostic tests covering all major error categories
3. Regression test directory established with convention enforced in CI
4. Cross-feature interaction test suite with 50+ tests
5. Coverage reports generated consistently, with per-module visibility
6. Fuzzer can run for 10 minutes without compiler crashes on random input
7. CI runs on at least 2 platforms (Windows + Linux)
8. Compilation benchmark suite tracking 10+ representative files
