## 1. Diagnosis
- [x] 1.1 Emit IR — confirmed `!llvm.loop` metadata with `vectorize.enable=true` ALREADY emitted on for-in back-edge (create_loop_metadata(true, 4) in loop.cpp:338)
- [x] 1.2 LLVM remark: "loop not vectorized: Control flow cannot be substituted for a select" — vectorization metadata exists but LLVM cannot vectorize
- [x] 1.3 Root cause: `list.get(i)` inline body has multiple pointer indirections (handle->data_addr->element) via `inttoptr`+`load` that LLVM cannot hoist out of the loop or prove alias-free

## 2. Implementation
- [x] 2.1 Vectorization metadata already emitted — no changes needed
- [x] 2.2 Root cause identified: LLVM cannot vectorize `list.get(i)` pattern due to non-invariant loads and inttoptr aliasing
- [x] 2.3 Pointer-based ListIter for-in implemented in phase0y (gen_for_pointer_stepping) + phase0z (@inline iterators, constant stride) — LLVM now auto-vectorizes to `<8 x i64>` AVX-512
- [x] 2.4 `@vectorize` attribute not needed — vectorization works automatically with pointer-stepping codegen. The `!llvm.loop vectorize.enable=true` metadata on the back-edge is sufficient.

## 3. Benchmark Gate
- [x] 3.1 Before: TML 2.6B ops/s vs Rust 4.57B ops/s (1.7x gap, scalar only)
- [x] 3.2 After phase0y+0z: TML 4.32B vs Rust 4.57B ops/s (1.06x, within noise). LLVM emits `<8 x i64>` vector.body with 4 accumulators (32 elem/iter)

## 4. Tail (mandatory — enforced by rulebook v5.3.0)
- [x] 4.1 Update or create documentation covering the implementation — documented in docs/patches/v0.3.26-0.3.27.md
- [x] 4.2 Write tests covering the new behavior — forin_list_pointer_stepping.test.tml (6 tests)
- [x] 4.3 Run tests and confirm they pass — 1/1 suite passed
