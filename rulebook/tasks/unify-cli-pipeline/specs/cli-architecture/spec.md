# CLI Unified Pipeline Specification

## Document Status
- **Version**: 1.0
- **Phase**: Planning (to be implemented in phases 1-4)
- **Last Updated**: 2026-02-26

## Overview

The TML CLI implements three main commands: `build`, `run`, and `test`. Each command processes TML source files through a compilation pipeline. This specification defines the **unified pipeline architecture** that SHALL be used by all three commands, with command-specific output/execution layers.

All three commands SHALL use the same underlying `QueryContext` compilation pipeline to ensure:
- Consistent behavior across commands
- Automatic feature parity (new flags work everywhere)
- Shared cache utilization
- Easier maintenance and testing

## Unified Pipeline

The unified pipeline SHALL implement these stages in the following order:

### Stage 1: Read Source
- Input: File path or module name
- Action: Load source code from disk
- Caching: File hash-based memoization
- Output: Raw source string

### Stage 2: Tokenize (Lexical Analysis)
- Input: Raw source string
- Action: Break source into tokens
- Caching: Fingerprint-based memoization
- Output: Token stream

### Stage 3: Parse (Syntax Analysis)
- Input: Token stream
- Action: Build Abstract Syntax Tree (AST)
- Caching: Fingerprint-based memoization
- Output: AST with line/column metadata

### Stage 4: Type Check (Semantic Analysis)
- Input: AST
- Action: Verify type correctness, infer types, check function signatures
- Caching: Fingerprint-based memoization
- Output: Typed AST with type annotations

### Stage 5: Borrow Check (Lifetime Analysis)
- Input: Typed AST
- Action: Verify ownership rules, detect use-after-free, check borrowing
- Caching: Fingerprint-based memoization
- Output: Borrow-checked AST

### Stage 6: HIR Lowering (High-Level IR)
- Input: Borrow-checked AST
- Action: Convert AST to higher-level intermediate representation
- Caching: Fingerprint-based memoization
- Output: High-level IR (HIR)

### Stage 7: MIR Building (Mid-Level IR / SSA)
- Input: High-level IR
- Action: Build mid-level IR in Static Single Assignment (SSA) form
- Caching: Fingerprint-based memoization
- Output: Mid-level IR (MIR)

### Stage 8: Code Generation (LLVM IR)
- Input: Mid-level IR
- Action: Generate LLVM intermediate representation
- Caching: Fingerprint-based memoization
- Output: LLVM IR

### Stage 9: Backend Code Generation
- Input: LLVM IR
- Action: Compile to object code or executable (LLVM or Cranelift backend)
- Caching: Object file cache
- Output: Object files or executable

## Query Caching System

All compilation stages SHALL use the `QueryContext` caching system for incremental compilation:

### Requirements
- All stages SHALL be memoized via `QueryContext::force<R>()`
- Fingerprints SHALL be 128-bit CRC32C hashes computed from source content
- Fingerprints SHALL persist to `.incr-cache/` directory
- Dependency graph SHALL track which queries depend on which other queries
- GREEN path (no source changes) SHALL skip entire pipeline, loading cached LLVM IR from disk

### Fingerprint Invalidation
- When source file changes, its fingerprint differs
- Downstream stages recompute only for changed files
- If downstream results identical to cached version, further downstream stages skip
- If downstream changes detected, all further downstream stages recompute

### Cache Location
- Location: `build/debug/.incr-cache/` (debug build) or `build/release/.incr-cache/` (release build)
- Format: Binary metadata file (`incr.bin`) + per-file fingerprints
- Persistence: Survives across build invocations unless `--no-cache` is used

## Command-Specific Variations

All commands SHALL use the unified pipeline as described above. Command-specific behavior is limited to **output generation** and **execution**.

### build Command

**Purpose**: Compile source file(s) to executable, static library, dynamic library, or TML library

**Input**: Single `.tml` file (entry point)

**Pipeline**: Unified query-based pipeline

**Output**: One of the following (configured via --crate-type):
- Executable (`.exe` on Windows, no extension on Unix)
- Static library (`.lib` on Windows, `.a` on Unix)
- Dynamic library (`.dll` on Windows, `.so` on Unix)
- TML library format (`.rlib`)

**Execution**: None (build only, produces output file)

**Caching**:
- Query cache: `.incr-cache/`
- Object file cache: `build/debug/obj_cache/` or `build/release/obj_cache/`
- Incremental linking when available

**Supported Flags**:
- `--no-cache` - Disable incremental compilation
- `--coverage` - Enable LLVM profiling instrumentation
- `--backend [llvm|cranelift]` - Select code generation backend
- `--emit-ir` - Emit LLVM IR for debugging
- `--emit-mir` - Emit MIR for debugging
- `--emit-pipeline` - Dump all intermediate representations
- `--out-dir <path>` - Output directory for artifacts
- `--release` - Enable O3 optimizations
- `-O[0-3]` - Optimization level
- `--debug` / `-g` - Include debug information

### run Command

**Purpose**: Compile source file and execute immediately in subprocess

**Input**: Single `.tml` file (entry point)

**Pipeline**: Unified query-based pipeline

**Output**: None (temporary executable, deleted after execution)

**Execution**: Subprocess execution with stdout/stderr forwarding to terminal

**Caching**:
- Query cache: `.incr-cache/`
- Subprocess cache: `build/debug/.run-cache/` (speeds up repeated runs)

**Supported Flags**:
- Same as build command
- `--args <args>` - Arguments to pass to compiled program

### test Command (UNIFIED IN PHASE 2)

**Purpose**: Discover and execute `@test` functions from `.test.tml` files

**Input**: Multiple `.test.tml` files (discovered from directory or file list)

**Pipeline**: Unified query-based pipeline (Phase 2 change - currently uses custom pipeline)

**Output**:
- Terminal logs showing test results (pass/fail/skip)
- JSON test report (optional)
- Coverage HTML report (with `--coverage`)

**Execution**:
- Tests compiled to shared libraries (`.dll` on Windows)
- In-process execution via `tml_test_entry()` function pointers
- Multiple tests grouped into suites (configurable)
- Parallel test execution (configurable thread count)

**Caching**:
- Query cache: `.incr-cache/` (Phase 2 change)
- File hash-based test result cache: `.test-cache.json`
- Compiled test DLL cache: `build/debug/.run-cache/`

**Suite Mode**:
- Default: 8 tests per DLL (suite mode)
- `--no-suite`: 1 test per DLL (individual mode)
- `--suite`: Explicitly enable suite mode
- Compiler tests automatically use individual mode (due to Phase 3 workaround) until Phase 3.1 fixed

**Supported Flags**:
- `--no-cache` - Disable incremental compilation
- `--coverage` - Enable LLVM profiling instrumentation and coverage report generation
- `--backend [llvm|cranelift]` - Select code generation backend (Phase 1)
- `--emit-ir` - Emit LLVM IR for debugging (Phase 1)
- `--emit-mir` - Emit MIR for debugging (Phase 1)
- `--emit-pipeline` - Dump all intermediate representations (Phase 1)
- `--out-dir <path>` - Output directory for artifacts (Phase 1)
- `--release` - Enable O3 optimizations
- `-O[0-3]` - Optimization level
- `--no-suite` - Force individual test compilation (1 test per DLL)
- `--suite` - Explicitly enable suite mode (default)
- `--test-threads=N` - Number of threads for test compilation
- `--profile` - Show per-test timing information
- `--verbose` - Show detailed output
- `--filter <pattern>` - Run only tests matching pattern

## Coverage Support

All three commands (build, run, test) SHALL support the `--coverage` flag:

### Coverage Behavior
- During compilation: LLVM profiling instrumentation is added to IR
- During execution: Coverage data is collected in process
- After execution: Coverage data merged and report generated
- Output: HTML coverage report at `build/debug/coverage/coverage.html`

### Coverage Implementation
- LLVM IR instrumentation: Added by LLVM backend during CodegenUnit stage
- Runtime data collection: Happens during test execution
- Report generation: Uses LLVM coverage tools to create HTML output

## Configuration Consistency

All three commands SHALL support the following configuration options:

### Caching and Incremental Compilation
- `--no-cache` - Disables incremental compilation, forces full rebuild

### Coverage and Profiling
- `--coverage` - Enables LLVM profiling instrumentation and coverage reporting

### Backend Selection
- `--backend [llvm|cranelift]` - Selects code generation backend
  - Default: `llvm` (production-ready)
  - Alternative: `cranelift` (experimental, for fast debug builds)

### Diagnostics and Debugging
- `--emit-ir` - Emits LLVM IR to `.ll` files for inspection
- `--emit-mir` - Emits MIR for debugging
- `--emit-pipeline` - Dumps all intermediate representations (all stages)

### Output Control
- `--out-dir <path>` - Specifies output directory for build artifacts

### Optimization
- `--release` - Enables O3 optimization level
- `-O[0-3]` - Explicitly sets optimization level (O0=none, O1=basic, O2=most, O3=aggressive)

### Debugging Symbols
- `--debug` / `-g` - Includes debug symbols in output

### Command-Specific Options

Each command may have additional options specific to its purpose:

#### build command
- `--crate-type [bin|lib|dylib|rlib]` - Output type

#### run command
- `--args <args>` - Arguments to pass to program

#### test command
- `--no-suite` - Force individual test compilation (1 per DLL)
- `--suite` - Explicitly enable suite mode
- `--test-threads=N` - Thread count for test compilation
- `--profile` - Show timing information
- `--verbose` - Detailed output
- `--filter <pattern>` - Test name filtering

## Performance Guarantees

All commands SHALL meet these performance targets:

### Incremental Compilation (GREEN path, no source changes)
- Compilation time: < 100 milliseconds
- Reason: Skips entire pipeline, loads cached results

### Full Compilation (RED path, source changed)
- Build command (small file): < 30 seconds
- Build command (large file): < 120 seconds
- Test command (full suite): < 3 minutes (including execution)

### Test Execution
- Test discovery: < 500 milliseconds
- Per-test execution: < 5 seconds average
- Suite execution (8 tests): < 40 seconds

## Implementation Roadmap

### Phase 1: Low-Cost Improvements (Done: 0%)
- Add `--backend`, `--emit-pipeline`, `--out-dir` to test command
- Test command feature parity with build/run

### Phase 2: Core Unification (Done: 0%)
- Refactor test compilation to use QueryContext
- Consolidate coverage reporting
- Verify incremental compilation

### Phase 3: Bug Fixes and Cleanup (Done: 0%)
- Fix suite merging codegen bug
- Remove workarounds (compiler tests individual-mode forcing)

### Phase 4: Documentation (Done: 0%)
- Update architecture documentation
- Add design rationale comments

## Rationale

### Why QueryContext for All Commands?
- Single compilation pipeline means single source of truth
- Query caching improves performance across all commands
- Incremental compilation works consistently everywhere
- New features automatically available to all commands

### Why Command-Specific Output Layers?
- Compilation is the same, execution is different
- Separating concerns keeps code cleaner
- Commands can optimize execution without touching compilation

### Why Suite Mode for Tests?
- Grouping tests reduces DLL creation overhead (8x fewer DLLs)
- In-process execution is faster than subprocess execution
- Trade-off: Suite merging complexity (being fixed in Phase 3)
- Individual mode available for debugging or when merging fails

## Related Documents

- Implementation details: `specs/cli-architecture/implementation.md` (TBD)
- Cache system details: `compiler/include/query/query_incr.hpp`
- Current build implementation: `compiler/src/cli/builder/build.cpp`
- Current test implementation: `compiler/src/cli/tester/suite_execution.cpp`
