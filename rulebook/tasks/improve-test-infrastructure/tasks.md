# Tasks: Improve Test Infrastructure

**Status**: In Progress (20%)

## Phase 1: Diagnostic / Error-Checking Tests

- [x] 1.1.1 Design `@expect-error` directive format and parsing in test runner
- [x] 1.1.2 Implement error-matching logic in `diagnostic_execution.cpp`
- [x] 1.1.3 Add MCP test tool support for diagnostic test mode
- [x] 1.2.1 Write diagnostic tests for type checker errors (T001-T020) - 11 error codes covered
- [ ] 1.2.2 Write diagnostic tests for borrow checker errors (B001-B010)
- [x] 1.2.3 Write diagnostic tests for parser/syntax errors (P001-P010) - 2 error codes covered
- [ ] 1.2.4 Write diagnostic tests for semantic errors (S001-S010)
- [ ] 1.2.5 Write diagnostic tests for codegen errors (C001-C005)
- [x] 1.3.1 Verify error message content matches expected patterns
- [ ] 1.3.2 Verify error line/column positions are accurate

**Completed Diagnostic Tests** (3 files, 15 expected errors):
- `lib/core/tests/errors/lexer_errors.error.tml` - L002, L015
- `lib/core/tests/errors/parser_errors.error.tml` - P001, P008
- `lib/core/tests/errors/type_errors.error.tml` - T001 (×3), T004, T008, T013, T014, T016 (×2), T022, T030

## Phase 2: Edge Case / Boundary Tests

- [ ] 2.1.1 Add integer boundary tests for all numeric types (MIN, MAX, overflow)
- [ ] 2.1.2 Add empty collection tests (empty string, zero-length array, empty struct)
- [ ] 2.1.3 Add deeply nested generic type tests
- [ ] 2.1.4 Add deep recursion tests with stack limit verification
- [ ] 2.1.5 Add large match arm tests (50+ arms)
- [ ] 2.1.6 Add functions with many parameters (16+, 32+)
- [ ] 2.1.7 Add unicode edge cases (multi-byte, surrogate pairs, combining chars)
- [ ] 2.1.8 Add zero-size type and unit type edge cases

## Phase 3: Regression Test Convention

- [ ] 3.1.1 Create `lib/core/tests/regression/` directory structure
- [ ] 3.1.2 Create `lib/std/tests/regression/` directory structure
- [ ] 3.1.3 Document regression test naming convention (`issue_NNNN_description.test.tml`)
- [ ] 3.1.4 Retroactively add regression tests for previously fixed bugs
- [ ] 3.1.5 Add CI check that bug-fix PRs include a regression test

## Phase 4: Cross-Feature Interaction Tests

- [ ] 4.1.1 Write tests for generics + borrow checker interactions
- [ ] 4.1.2 Write tests for closures + iterator chaining
- [ ] 4.1.3 Write tests for behaviors + generics + type inference
- [ ] 4.1.4 Write tests for error handling + control flow (when + Outcome)
- [ ] 4.1.5 Write tests for nested behaviors with associated types
- [ ] 4.1.6 Write tests for conditional compilation + platform-specific behaviors
- [ ] 4.1.7 Write tests for complex ownership patterns (moves through generics)
- [ ] 4.1.8 Write tests for fmt + custom types + generics together

## Phase 5: Coverage as CI Gate

- [ ] 5.1.1 Fix coverage report generation to produce consistent JSON output
- [ ] 5.1.2 Verify `mcp__tml__project_coverage` returns accurate data
- [ ] 5.1.3 Define per-module coverage thresholds (target 80%)
- [ ] 5.1.4 Add CI step to run tests with `--coverage` flag
- [ ] 5.1.5 Add CI gate that fails if coverage drops below threshold
- [ ] 5.1.6 Generate coverage trend report (compare against previous run)

## Phase 6: Expanded Fuzzing

- [ ] 6.1.1 Audit existing fuzzer in `compiler/src/cli/tester/fuzzer.cpp`
- [ ] 6.1.2 Add grammar-guided input generation based on TML EBNF
- [ ] 6.1.3 Add crash-only mode (segfault/assertion = fail, error messages = ok)
- [ ] 6.1.4 Add round-trip test: `format(parse(source))` idempotency check
- [ ] 6.1.5 Add timeout-based fuzzing (run N minutes, report all crashes)
- [ ] 6.1.6 Integrate fuzzer into CI as optional nightly job

## Phase 7: Cross-Platform CI

- [ ] 7.1.1 Create Linux build script (`scripts/build.sh`)
- [ ] 7.1.2 Create macOS build script (`scripts/build_macos.sh`)
- [ ] 7.1.3 Set up CI matrix (Windows + Linux + macOS)
- [ ] 7.1.4 Add platform-specific test annotations
- [ ] 7.1.5 Verify conditional compilation paths tested on target platforms
- [ ] 7.1.6 Add cross-compilation smoke tests

## Phase 8: Compilation Performance Benchmarks

- [ ] 8.1.1 Create benchmark suite with 10+ representative TML files of varying size
- [ ] 8.1.2 Implement timing harness for compilation benchmarks
- [ ] 8.1.3 Track incremental compilation cache effectiveness
- [ ] 8.1.4 Track peak memory usage during compilation
- [ ] 8.1.5 Add CI step to detect compilation time regressions (>10% slower)
- [ ] 8.1.6 Store benchmark results history for trend analysis
