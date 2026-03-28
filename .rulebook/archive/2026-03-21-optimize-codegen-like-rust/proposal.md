# Proposal: Optimize TML Codegen Using Rust as Reference

## Why

The TML compiler's LLVM IR generation has significant inefficiencies compared to Rust's codegen for equivalent code. A side-by-side IR comparison of a simple iterator (`SimpleRange` with `count`/`last`/`nth`) revealed:

1. **`Maybe[T]` layout waste**: `Maybe[I32]` uses 16 bytes (`{ i32, [1 x i64] }`) vs Rust's `Option<i32>` at 8 bytes (`{ i32, i32 }`). This doubles memory usage for every enum return value in the entire iterator pipeline.

2. **Redundant alloca/store/load**: Struct constructors emit `alloca` + `store` + `load` for every parameter instead of using SSA `insertvalue` directly. The `SimpleRange::new()` function uses 10 instructions where Rust uses 3.

3. **500+ unused declarations**: Every `.ll` file emits declarations for the entire runtime (channels, mutexes, TLS, sockets, hashmap, etc.) even when none are used. Rust emits only what's referenced.

4. **No checked arithmetic**: TML uses `add nsw` (poison on overflow) while Rust uses `@llvm.sadd.with.overflow` with explicit panic. TML silently produces undefined behavior on integer overflow.

5. **No exception handling**: TML uses bare `call` everywhere while Rust uses `invoke` + `cleanuppad` for proper stack unwinding with cleanup (drop) on panic. TML cannot run destructors during panic propagation.

6. **Redundant memory loads**: The codegen reloads struct fields multiple times within the same basic block instead of caching values in SSA registers.

These issues compound: in a tight iterator loop, TML's debug IR runs 3-5x more instructions than necessary, and even in release, the LLVM optimizer must work harder to compensate.

## What Changes

### Methodology: Rust-as-Reference IR Comparison

For every codegen bug or optimization target, the workflow is:

1. Write equivalent code in `.sandbox/temp_*.rs` (Rust) and `.sandbox/temp_*.tml` (TML)
2. Generate LLVM IR from both (`rustc --emit=llvm-ir`, `tml build --emit-ir`)
3. Compare function-by-function to identify where TML diverges from Rust's codegen quality
4. Fix the TML compiler's codegen to match or exceed Rust's IR quality
5. Verify with tests that behavior is preserved

### Components Affected

- `compiler/src/codegen/llvm/` — AST-based LLVM IR generation (primary target)
- `compiler/src/codegen/cranelift/` — Cranelift backend (apply same fixes if applicable)
- `compiler/src/codegen/mir_codegen.cpp` — MIR-based codegen (same fixes)
- `compiler/include/codegen/llvm/llvm_ir_gen.hpp` — Codegen state and helpers

### Breaking Changes

None. All changes are internal to IR generation. The observable behavior of compiled programs remains identical.

## Impact

- Affected specs: None (internal optimization only)
- Affected code: `compiler/src/codegen/`, `compiler/include/codegen/`
- Breaking change: NO
- User benefit: Faster debug-mode executables, smaller IR files, better diagnostics, foundation for future checked-arithmetic and exception-handling support

## Dependencies

- LLVM backend must be functional (already is)
- Rust toolchain installed for reference IR generation (`rustc` available)

## Success Criteria

1. `Maybe[I32]` layout is `{ i32, i32 }` (8 bytes), matching Rust's `Option<i32>`
2. Struct constructors use `insertvalue` directly, no intermediate `alloca`
3. `.ll` files only contain declarations for actually-used runtime functions
4. `--checked-math` flag available in debug mode using `@llvm.sadd.with.overflow.*`
5. Loop IR for `count()`/`last()`/`nth()` has same instruction count as equivalent Rust code
6. No regressions in test suite

## Risks

- Layout changes to `Maybe[T]` could break existing code that relies on the current ABI — mitigated by running full test suite after each change
- `insertvalue` optimization might not work for all struct patterns — mitigated by keeping `alloca` path as fallback for complex structs
- Checked arithmetic adds runtime cost — mitigated by making it opt-in via flag
