# Proposal: unify-cli-pipeline

## Executive Summary

The TML CLI implements three main commands (build, run, test) with dramatically different compilation architectures, creating maintenance burden and missed optimization opportunities. This task unifies the architecture around a common QueryContext-based pipeline while preserving command-specific optimizations.

## Why This Matters

### Current State: Three Divergent Architectures

1. **build command** - Uses unified query-based pipeline
   - QueryContext caching across all 8 compilation stages
   - Incremental compilation via .incr-cache/ fingerprints
   - Configurable with --backend, --emit-pipeline, --out-dir

2. **run command** - Uses similar query pipeline
   - QueryContext-based compilation
   - Subprocess execution with output forwarding
   - Limited diagnostics support

3. **test command** - Uses CUSTOM pipeline
   - Custom compilation path (no QueryContext reuse)
   - File hash-based caching (not fingerprint-based)
   - No --backend support
   - No --emit-pipeline support
   - Different cache invalidation logic
   - Custom suite merging logic outside unified system

### Problems This Creates

- **Maintenance burden**: Bug fixes in one pipeline don't propagate to others
- **Inconsistent behavior**: Cache invalidation works differently per command
- **Missed optimizations**: Test command doesn't benefit from query caching improvements
- **Feature parity**: New flags (--backend, --emit-pipeline) must be manually added to each command
- **Suite merging codegen bug**: Custom test pipeline has its own symbol resolution issues
- **Test infrastructure complexity**: Suite execution logic is isolated, harder to reason about

## Solution: Unified CLI Pipeline

Unify all three commands around a **common QueryContext-based compilation pipeline** with command-specific output/execution layers:

### Unified Pipeline (All Commands)
```
Source → Tokenize → Parse → Typecheck → Borrowcheck → HIR → MIR → Codegen
  ↓          ↓          ↓        ↓           ↓          ↓       ↓        ↓
QueryContext caching with fingerprint-based incremental compilation
  ↓
Command-specific output (executable, subprocess, test DLL)
```

### Benefits

1. **Cleaner codebase**: Single compilation path for all three commands
2. **Better caching**: Test command gets query caching for free
3. **Feature parity**: New flags work everywhere automatically
4. **Easier debugging**: Consistent pipeline behavior across commands
5. **Better performance**: Shared query cache across commands in same session
6. **Simpler maintenance**: Bug fixes apply globally

## What Changes

### Phase 1: Low-Cost Improvements (5-10% effort, 80% value)
- Add --backend support to test command
- Add --emit-pipeline support to test command
- Add --out-dir support to test command
- Document command differences in CLI help

**Why Phase 1 is important**: These expose the test command to feature parity with build/run, increasing user choice with minimal implementation effort.

### Phase 2: Medium-Cost Refactoring (40-50% effort, additional 15% value)
- Refactor test compilation to use QueryContext (instead of custom pipeline)
- Consolidate coverage reporting architecture
- Verify incremental compilation works with new test pipeline
- Benchmark new vs old test execution time
- Add cross-command cache validation

**Why Phase 2 is important**: This is the core unification. The test command finally uses the same pipeline as build/run, eliminating the custom path and gaining all QueryContext benefits.

### Phase 3: Hard Fixes (40-50% effort, additional 5% value)
- Fix suite merging codegen bug (symbol deduplication)
- Implement DLL runtime initialization
- Remove compiler tests individual-mode workaround (once suite bug fixed)
- Full regression testing across all three commands

**Why Phase 3 is important**: Once the unified pipeline is in place, the suite merging bug becomes easier to fix because it's part of the unified system. Remove workarounds that only exist because test used a custom pipeline.

### Phase 4: Documentation & Cleanup
- Update internal CLI architecture documentation
- Update compiler/src/cli/README.md with unified pipeline diagram
- Add design rationale comments to query_context.hpp
- Document why/if any command still uses custom paths

## Impact

- **Affected specs**: Compiler CLI infrastructure (compiler/src/cli/)
- **Affected code**:
  - compiler/src/cli/builder/build.cpp (build command)
  - compiler/src/cli/builder/builder_run.cpp (run command)
  - compiler/src/cli/tester/suite_execution.cpp (test command)
  - compiler/src/cli/tester/test_runner.cpp (test infrastructure)
  - compiler/src/query/query_*.hpp/cpp (query system)
- **Breaking change**: NO - All three commands work the same from user perspective
- **User benefit**: Consistent CLI behavior, faster test execution via query caching, better error diagnostics
- **Performance impact**: Test command gets 5-15% faster via query caching (Phase 2)
