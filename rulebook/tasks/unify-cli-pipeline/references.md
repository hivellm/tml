# References: CLI Architecture Unification

## Analysis Documents

- **.sandbox/cli_divergence_analysis.md** - Detailed comparison of three command architectures, showing divergence points and shared patterns
- **MEMORY.md** (project persistent memory) - Historical context on CLI optimization work

## Query System (Core of Unified Pipeline)

### Key Files
- **compiler/include/query/query_context.hpp** - QueryContext template class with `force<R>()` method
- **compiler/include/query/query_key.hpp** - 8 query key/result types for each pipeline stage
- **compiler/include/query/query_cache.hpp** - Thread-safe memoization cache with fingerprints
- **compiler/include/query/query_deps.hpp** - Dependency tracking and cycle detection
- **compiler/include/query/query_fingerprint.hpp** - 128-bit CRC32C fingerprinting implementation
- **compiler/include/query/query_incr.hpp** - Incremental cache persistence (PrevSessionCache, IncrCacheWriter)
- **compiler/include/query/query_provider.hpp** - Provider registry with O(1) lookup
- **compiler/src/query/query_core.cpp** - Provider implementations for all 8 stages

### Entry Points
- `compiler/src/cli/builder/build.cpp` - Uses `QueryContext` for build command (line ~150)
- `compiler/src/cli/builder/builder_run.cpp` - Uses `QueryContext` for run command

## Build Command Implementation

### Key Files
- **compiler/src/cli/builder/build.cpp** - Main build orchestration
  - `run_build_with_queries()` - Default query-based pipeline (line ~50)
  - `run_build_impl()` - Legacy sequential pipeline (fallback)
  - Coverage integration (line ~200)

- **compiler/src/cli/builder/parallel_build.cpp** - Multi-threaded compilation

- **compiler/src/cli/builder/object_compiler.cpp** - LLVM IR → object file compilation (in-process)

- **compiler/src/cli/builder/build_cache.cpp** - MIR cache for incremental compilation

- **compiler/src/cli/builder/build_config.cpp** - Configuration parsing

## Run Command Implementation

### Key Files
- **compiler/src/cli/builder/builder_run.cpp** - Run command implementation
  - Uses QueryContext for compilation
  - Subprocess execution for running compiled program
  - Output forwarding to terminal

## Test Command Implementation

### Key Files (Custom Pipeline - to be unified in Phase 2)

- **compiler/src/cli/tester/suite_execution.cpp** - Suite compilation and execution orchestration
  - `main_test_loop()` - Test discovery and grouping
  - `compile_test_suites()` - Custom compilation path (NOT using QueryContext)
  - Suite merging logic (lines ~300-500) - Source of codegen bug
  - Coverage data collection (lines ~681-702)

- **compiler/src/cli/tester/test_runner.cpp** - Test execution and result collection
  - `group_tests_into_suites()` - Groups tests by file/suite (line ~82)
  - `max_per_suite` parameter - Controls grouping (currently hardcoded to 8)
  - Function declaration handling (line ~200+)

- **compiler/src/cli/tester/exe_suite_runner.cpp** - Subprocess test execution
  - `suite_worker` lambda - Parallel subprocess execution (lines ~307-387)
  - Coverage mode synchronization (lines ~681-702)

- **compiler/src/cli/tester/discovery.cpp** - Test file discovery

- **compiler/src/cli/tester/run.cpp** - Test command entry point

- **compiler/src/cli/tester/benchmark.cpp** - Benchmark execution

## Backend System

### Key Files

- **compiler/include/codegen/codegen_backend.hpp** - Backend abstraction interface (required by Phase 1)

- **compiler/src/backend/llvm_backend.cpp** - LLVM C API wrapper for in-memory IR→obj compilation

- **compiler/src/backend/lld_linker.cpp** - LLD in-process linker (COFF/ELF/MachO)

- **compiler/src/codegen/cranelift/** - Cranelift backend (experimental)

## Coverage System

### Build Command
- **compiler/src/cli/builder/build.cpp** (lines ~200) - Handles `--coverage` flag
- Instrumentation: Added during CodegenUnit stage
- Report generation: Post-execution HTML generation

### Test Command
- **compiler/src/cli/tester/suite_execution.cpp** (lines ~681-702) - Coverage data collection during test execution
- LLVM profiling runtime: Environment variable setup (`LLVM_PROFILE_FILE`)
- Report generation: Similar to build command

## CLI Infrastructure

### Key Files

- **compiler/src/cli/dispatcher.cpp** - CLI argument parsing and command routing

- **compiler/src/cli/commands/cmd_test.cpp** - Test command CLI handler (TestOptions struct)

- **compiler/src/cli/commands/cmd_build.cpp** - Build command CLI handler

- **compiler/src/cli/tester/test_runner.hpp** - Test runner interface
  - `run_tests()` - Main test execution function
  - Accepts `max_per_suite` parameter (Phase 1 implementation)

## Key Implementation Notes

### Phase 1 (Feature Parity)
- Add `--backend` support: Check `compiler/include/codegen/codegen_backend.hpp` for interface
- Add `--emit-pipeline` support: Check `compiler/src/cli/builder/build.cpp` for implementation pattern
- Add `--out-dir` support: Check build command for implementation pattern
- Update test command help text: `compiler/src/cli/commands/cmd_test.cpp`

### Phase 2 (Core Unification)
- Refactor test compilation: Replace custom pipeline in `suite_execution.cpp` with QueryContext calls
- Study QueryContext usage in `build.cpp` for reference implementation
- Consolidate coverage: Use `build.cpp`'s coverage model as baseline

### Phase 3 (Bug Fixes)
- Suite merging codegen bug: Investigate `suite_execution.cpp` lines ~350-400
- Symbol deduplication: Check `compiler/src/codegen/mir/instructions.cpp` for function declaration handling
- Remove workaround: Delete lines ~803-811 in `suite_execution.cpp` once bug fixed

### Phase 4 (Documentation)
- Create unified pipeline diagram for `compiler/src/cli/README.md`
- Document architectural decisions in code comments
- Add rationale to `query_context.hpp` header

## Performance Analysis

- Build time profile: `.sandbox/profile_analysis.md` (from previous optimization work)
- Linking bottlenecks: I/O bound during linking phase (37 seconds of 100 total)
- Query cache effectiveness: Typically 90%+ cache hit rate on incremental builds

## Testing Strategy

- Test individual phases against test suite
- Verify cache behavior with `--no-cache` flag
- Benchmark Phase 2 changes: old vs new test execution time
- Run full test suite with coverage after each major phase
