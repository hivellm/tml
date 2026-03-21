---
name: cached-library-state-issues
description: Library state caching for test suite speedup — obj_cache hash fix, tracked blockers for full enablement
type: project
---

## Cached Library State for Test Suites

### Background
`build_stdlib_object()` in `testing_compile.cpp` pre-compiles library modules once, then workers restore the cached state to skip `emit_module_pure_tml_functions()` per suite. This provides ~3x speedup (48s vs 143s per batch of 40 suites).

### What Was Fixed (2026-03-21)
1. **obj_cache hash collision**: `ir_fp.to_hex().substr(0, 16)` truncated CRC32C fingerprint to 64 bits. With cached state, library IR preamble is identical across test files, making CRC32C of first half collide. Fixed: use full 32-char hex hash.
   - File: `testing_compile.cpp` lines 687 and 1406 (two obj_cache sites)

2. **Infrastructure added for future supplemental pass**:
   - `processed_module_paths_` member in LLVMIRGen (tracks which modules were processed)
   - `processed_module_paths` field in CodegenLibraryState (persists across workers)
   - `skip_modules` parameter on `emit_module_pure_tml_functions()` (skip already-processed modules)
   - Files: `llvm_ir_gen.hpp`, `generate_support.cpp`, `runtime_modules.cpp`

### Remaining Blockers (feature still disabled)
The cached state optimization is disabled in `compile_suites_parallel()` because restoring cached state causes:

1. **I32::duplicate redefinition**: Test files with local `impl Duplicate for I32` conflict with the cached library's version. Both produce `@tml_N4core3I329duplicateE`. `generated_functions_` should deduplicate but doesn't in the cached state path (root cause unclear).
   - Affected: `iter_cycle.test.tml`, `iter_size_hints.test.tml`

2. **i64/i32 type mismatch in range iterators**: `%t97 defined with type 'i64' but expected 'i32'` in range iteration codegen.
   - Affected: `iter_range.test.tml`, `iter_range_from.test.tml`, `iter_range_inclusive.test.tml`

3. **assert_str_empty undefined**: Pre-existing bug where `test::assertions` module functions aren't compiled. NOT caused by cached state.
   - Affected: `assertions_coverage.test.tml`

### Why Supplemental Pass Was Removed
A supplemental `emit_module_pure_tml_functions(skip_modules)` pass was attempted but caused the redefinition/type-mismatch errors. The `emit_module_pure_tml_functions()` function has complex multi-phase processing with extensive side effects (modifying `struct_types_`, `enum_variants_`, `functions_`, etc.). Running it twice (once for cached state restoration, once for supplemental modules) causes state conflicts.

### What Would Fix It
- Need to make `emit_module_pure_tml_functions()` truly incremental: track which types/functions/impls are already registered and skip them cleanly in all phases
- OR: make cached state capture ALL modules (but this takes ~5min for 355 modules — too slow for one-time cost)
- OR: selectively process only the MISSING modules in a lightweight way that doesn't re-register existing types
