# Tasks: LLVM Source Code Coverage

**Status**: Complete (function-level coverage working)

## Phase 1: Infrastructure Setup

- [x] 1.1.1 Add coverage compilation flags to ObjectCompileOptions
- [x] 1.1.2 Add coverage flags to CompilerOptions (--coverage-source)
- [x] 1.1.3 Detect llvm-profdata and llvm-cov tools in compiler_setup
- [x] 1.1.4 Create coverage output directory structure
- [x] 1.1.5 Find and link clang_rt.profile runtime library

## Phase 2: Test Runner Integration

- [x] 2.1.1 Add --coverage-source flag to tml test command
- [x] 2.1.2 Set LLVM_PROFILE_FILE environment variable
- [x] 2.1.3 Link coverage runtime with test DLLs
- [x] 2.1.4 Export __llvm_profile_write_file from DLLs
- [x] 2.1.5 Call __llvm_profile_write_file before DLL unload

## Phase 3: Profile Data Collection

- [x] 3.1.1 Collect .profraw files after test execution
- [x] 3.1.2 Implement llvm-profdata merge
- [x] 3.1.3 Handle Windows path issues with cmd.exe

## Phase 4: Report Generation

- [x] 4.1.1 Implement llvm-cov show for HTML reports
- [x] 4.1.2 Implement llvm-cov report for summary
- [x] 4.1.3 Implement llvm-cov export for LCOV format
- [ ] 4.1.4 Generate JSON report for programmatic access

## Phase 5: Coverage Reporting UI

- [x] 5.1.1 Display coverage summary in test output
- [x] 5.1.2 Show uncovered files list
- [x] 5.1.3 Color-coded output (green/yellow/red thresholds)
- [ ] 5.1.4 Integration with --verbose for detailed output

## Phase 6: TML Source Instrumentation

- [x] 6.1.1 Add LLVM llvm.instrprof.increment intrinsic to codegen
- [x] 6.1.2 Inject instrumentation calls at function entry
- [x] 6.1.3 Create function name globals with linkonce_odr linkage
- [x] 6.1.4 Wire up llvm_source_coverage option in LLVMGenOptions
- [x] 6.1.5 Handle suite mode without duplicate symbol conflicts

## Phase 7: Line-Level Coverage Mapping (Optional Enhancement)

- [ ] 7.1.1 Generate __llvm_coverage_mapping section
- [ ] 7.1.2 Encode file ID mapping in LEB128 format
- [ ] 7.1.3 Create counter expressions for branch coverage
- [ ] 7.1.4 Generate mapping regions for each code block
- [ ] 7.1.5 Map TML source spans to instrumentation counters

## Notes

### Current Status

Coverage tracks **test code only** (functions defined in test files).
Library code is NOT tracked due to duplicate symbol issues in suite mode.

### Usage

```bash
# LLVM source coverage (test functions only)
tml test --coverage-source --coverage-source-dir=./coverage

# TML runtime coverage (test functions only)
tml test --coverage
```

### Limitation

Both coverage modes only instrument code compiled in test files.
Library functions are imported but not instrumented because:
1. In suite mode, library is compiled into each suite
2. Instrumenting library would cause duplicate symbols

### Future: Full Library Coverage

To track library coverage properly would require:
1. Compile library as separate instrumented DLL/rlib
2. Link tests against instrumented library
3. Coverage accumulates across all tests

This is a significant architectural change (Phase 8).

### What Works Now

- Test function call counts
- Module-grouped coverage report (vitest-style)
- Most-called functions display
- Low coverage module warnings
