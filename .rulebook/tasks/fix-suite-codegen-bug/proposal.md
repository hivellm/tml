# Proposal: fix-suite-codegen-bug

## Why

When multiple test files are compiled into a single suite, the TML compiler generates invalid LLVM IR with type mismatches on function calls, blocking full test suite execution with coverage. This is a critical blocker for CI/coverage reporting.

## What Changes

- Fix symbol resolution in suite merging to pick concrete function signatures instead of generic ones
- Deduplicate function declarations when multiple tests contribute to the same suite
- Verify meta cache loading handles generic/concrete function pairs correctly
- Enable full test suite execution without IR generation errors

## Impact

- **Affected specs**: Compiler test infrastructure (`compiler/src/cli/tester/`)
- **Affected code**:
  - `compiler/src/cli/tester/suite_execution.cpp` - suite merging logic
  - `compiler/src/cli/tester/test_runner.cpp` - function declaration handling
  - `compiler/src/codegen/` - IR generation for function calls
- **Breaking change**: NO
- **User benefit**: Full test suite with coverage will execute successfully; CI pipelines will work correctly
