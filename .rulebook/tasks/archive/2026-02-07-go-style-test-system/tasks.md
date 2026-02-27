# Tasks: Go-Style Test System

**Status**: Archived (76%)
**Note**: Archived 2026-02-07. v2 EXE runner works but v1 DLL remains default (3.7x faster). Optimized v2 to use --run-all (1 subprocess per suite instead of 1 per test). Phase 5 cleanup skipped — DLL infrastructure kept as primary runner.

## Phase 1: Single Codegen Pass Per Package

- [x] 1.1 Modify test_runner to accept multiple test files and produce ONE .ll file per package
- [x] 1.2 Generate library IR once per package (not once per file)
- [x] 1.3 Generate all test function bodies in the same .ll file
- [x] 1.4 Generate test dispatcher main() that runs test by index
- [x] 1.5 Compile single .ll to .obj to .exe per package
- [ ] 1.6 Verify all tests pass with new single-codegen pipeline

## Phase 2: Subprocess Execution

- [x] 2.1 Replace DLL LoadLibrary/GetProcAddress with subprocess launch
- [x] 2.2 Implement stdout/stderr capture from test subprocess
- [x] 2.3 Parse test results from subprocess output (pass/fail/skip)
- [x] 2.4 Handle test crashes gracefully (process exit code != 0)
- [x] 2.5 Implement timeout per test subprocess
- [ ] 2.6 Remove DllMain and DLL export code from test codegen

## Phase 3: Cross-Package Parallelism

- [x] 3.1 Group tests by directory into packages
- [x] 3.2 Compile packages in parallel (1 thread per package, max CPU/2)
- [x] 3.3 Execute test binaries in parallel (separate processes)
- [x] 3.4 Collect and merge results from parallel executions
- [x] 3.5 Maintain ordered output despite parallel execution

## Phase 4: Binary Cache

- [x] 4.1 Content-address test binaries (hash of source + deps + compiler)
- [x] 4.2 Skip compilation when cached binary exists and inputs unchanged
- [x] 4.3 Cache test results alongside binary (skip execution when inputs unchanged)
- [x] 4.4 Invalidate cache when source files, library files, or compiler changes

## Phase 5: Cleanup & Polish (SKIPPED — DLL v1 kept as default)

- [ ] 5.1 Remove legacy DLL-based test infrastructure
- [ ] 5.2 Remove shared library mechanism (no longer needed)
- [ ] 5.3 Update --profile output for new pipeline
- [ ] 5.4 Update --coverage for new pipeline
- [ ] 5.5 Benchmark full suite: target < 3 minutes from scratch
- [ ] 5.6 Benchmark cached suite: target < 5 seconds
