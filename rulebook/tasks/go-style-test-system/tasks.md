# Tasks: Go-Style Test System

**Status**: Not Started (0%)
**Priority**: MAXIMUM - blocks all other development

## Phase 1: Single Codegen Pass Per Package

- [ ] 1.1 Modify test_runner to accept multiple test files and produce ONE .ll file per package
- [ ] 1.2 Generate library IR once per package (not once per file)
- [ ] 1.3 Generate all test function bodies in the same .ll file
- [ ] 1.4 Generate test dispatcher main() that runs test by index
- [ ] 1.5 Compile single .ll to .obj to .exe per package
- [ ] 1.6 Verify all tests pass with new single-codegen pipeline

## Phase 2: Subprocess Execution

- [ ] 2.1 Replace DLL LoadLibrary/GetProcAddress with subprocess launch
- [ ] 2.2 Implement stdout/stderr capture from test subprocess
- [ ] 2.3 Parse test results from subprocess output (pass/fail/skip)
- [ ] 2.4 Handle test crashes gracefully (process exit code != 0)
- [ ] 2.5 Implement timeout per test subprocess
- [ ] 2.6 Remove DllMain and DLL export code from test codegen

## Phase 3: Cross-Package Parallelism

- [ ] 3.1 Group tests by directory into packages
- [ ] 3.2 Compile packages in parallel (1 thread per package, max CPU/2)
- [ ] 3.3 Execute test binaries in parallel (separate processes)
- [ ] 3.4 Collect and merge results from parallel executions
- [ ] 3.5 Maintain ordered output despite parallel execution

## Phase 4: Binary Cache

- [ ] 4.1 Content-address test binaries (hash of source + deps + compiler)
- [ ] 4.2 Skip compilation when cached binary exists and inputs unchanged
- [ ] 4.3 Cache test results alongside binary (skip execution when inputs unchanged)
- [ ] 4.4 Invalidate cache when source files, library files, or compiler changes

## Phase 5: Cleanup & Polish

- [ ] 5.1 Remove legacy DLL-based test infrastructure
- [ ] 5.2 Remove shared library mechanism (no longer needed)
- [ ] 5.3 Update --profile output for new pipeline
- [ ] 5.4 Update --coverage for new pipeline
- [ ] 5.5 Benchmark full suite: target < 3 minutes from scratch
- [ ] 5.6 Benchmark cached suite: target < 5 seconds
