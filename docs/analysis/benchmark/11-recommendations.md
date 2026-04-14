# 11 — Recommendations

## P0 — Critical (before v1.0)

### R-001: Enable LLVM Optimization Passes

**Gaps addressed**: G-001 to G-004, G-013 (struct access 10-18x)

The single highest-impact change. TML's debug build skips LLVM `-O2` passes. Enabling `mem2reg` alone would promote alloca+store+load to register operations, closing the 10-18x struct access gap to ~1-2x.

**Action**: Add `-O2` flag to LLVM codegen in release mode. Verify with IR diff before/after.

### R-002: Fix K001 Codegen Bugs

**Gaps addressed**: G-007, G-008, G-009 (3 blocked benchmarks)

String operations, JSON parsing, and crypto can't be benchmarked due to codegen bugs. These represent the largest functional gaps — entire stdlib categories are untestable.

**Action**: Trace K001 for `core::str::len` and boolean `i32 vs i1` mismatch. Fix in `thir_mir_builder.cpp`.

### R-003: Fix Encoding Memory Leaks

**Gaps addressed**: 200K leaks in encoding benchmark

Every `base64_encode()` / `hex_encode()` call leaks 14 bytes. In a server handling 1K req/sec, this leaks 1.2 GB/day.

**Action**: Audit return-value string allocations in `lib/core/src/encoding/`. Ensure caller or runtime frees result strings.

## P1 — High Priority (next 2-3 milestones)

### R-004: Emit `switch` for Dense `when`

**Gaps addressed**: G-005 (9.5x), G-020 (3.9x)

Currently `when` with integer patterns generates if-else chains. LLVM's `switch` instruction generates jump tables for dense cases and binary search for sparse cases.

**Action**: In MIR→LLVM emission, detect integer-pattern `when` and emit `switch` instead of cascading `br`. ~100 lines of codegen change.

### R-005: Lower If-Else to CMOV/Select

**Gaps addressed**: G-010 (8.4x)

4-branch if-else chains can often be lowered to `select` instructions (conditional moves). This avoids branch misprediction overhead.

**Action**: In LLVM IR emission, detect simple if-else chains with scalar results and emit `select` instead of phi-node merges.

### R-006: Inline List.push/pop/get

**Gaps addressed**: G-015, G-016, G-021, G-027 (2-5.5x)

`List.push()`, `List.pop()`, `List.get()` are hot methods called billions of times in real code. Inlining eliminates call overhead and enables LLVM to optimize bounds checks.

**Action**: Mark these methods `@inline` or `@always_inline`. Alternatively, add an inlining pass to MIR that expands small method bodies at call sites.

### R-007: Fix Boolean Short-Circuit Codegen

**Gaps addressed**: G-018, G-019 (4x)

`and`/`or` expressions generate too many basic blocks and phi nodes. Expected pattern: chain conditions into a single conditional branch sequence.

**Action**: Review `emit_binary_op` for `and`/`or` in LLVM emission. Compare with Rust's IR for `a && b && c`.

### R-008: Reduce Plugin Load Time

**Gaps addressed**: G-006 (27x compile time, ~30% from DLL load)

Loading 145MB of DLLs on every compile adds 2-3s of constant overhead.

**Options**:
1. Lazy-load: only load `tml_codegen_x86.dll` when LLVM codegen is needed
2. Memory-map: use `LoadLibraryEx` with `DONT_RESOLVE_DLL_REFERENCES` for faster load
3. Daemon mode: keep compiler resident between compiles (tml_daemon.exe already exists)
4. Merge DLLs: combine `tml_compiler.dll` + `tml_codegen_x86.dll` into one

## P2 — Medium Priority

### R-009: Devirtualize Known Function Pointers

**Gaps addressed**: G-011 (8.2x), G-012 (8.4x)

When a function pointer's target is known at compile time, replace indirect call with direct call. This also enables inlining of the called function.

**Action**: Add a devirtualization pass to MIR that tracks function pointer assignments.

### R-010: Bounds-Check Elimination

**Gaps addressed**: G-015, G-016 (3-5.5x)

In `for i in 0 to list.len()`, the bounds check inside `list.get(i)` is provably unnecessary. Eliminate it.

**Action**: In MIR optimization, detect `for-in` patterns over collection length and mark index access as unchecked.

### R-011: Binary Size Reduction

**Gaps addressed**: G-026 (2.4x)

1. Strip debug info from release builds (`-s` flag)
2. Enable LTO to eliminate dead library code
3. Consider `--gc-sections` linker flag

### R-012: Auto-Vectorization

**Gaps addressed**: G-014 (6.7x), G-017 (4.3x), G-029 (2.2x)

Simple loops over arrays/lists with arithmetic bodies can be vectorized by LLVM if the loop structure is clean (no function calls, no aliasing).

**Action**: Ensure TML's loop codegen emits clean LLVM loops with `!llvm.loop` metadata for vectorization hints.

## P3 — Nice to Have

### R-013: Tail Call Optimization
Convert tail-recursive functions to loops. Saves stack frames and enables LLVM to optimize further.

### R-014: Profile-Guided Optimization (PGO)
Collect runtime profiles and use them to guide LLVM optimization decisions.

### R-015: Parallel Compilation
Compile independent functions in parallel using LLVM's thread-safe module building.

### R-016: Implement Drop/RAII
Automatic resource cleanup via scope-based deallocation. Critical for production use but large language change.

### R-017: String Interning / SSO
Small String Optimization for strings < 24 bytes. Eliminates heap allocation for most short strings (variable names, keys, etc.).

## Summary: Expected Impact After P0+P1

| Category | Current Ratio | Expected After Fixes |
|----------|--------------|---------------------|
| Struct access | 10-18x | 1-2x (with -O2) |
| When/match | 3-9.5x | 1-2x (switch inst) |
| Short-circuit bool | 4x | 1-1.5x |
| List operations | 2-5.5x | 1.5-2x (inline + bounds) |
| Function pointer | 8x | 1-2x (devirt) |
| Compile time | 27x | 8-10x (plugin + release) |
| Binary size | 2.4x | 1.5-1.8x (LTO + strip) |
| Memory leaks | Present | Fixed |
