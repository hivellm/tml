## 1. Diagnosis
- [x] 1.1 Emit IR — confirmed `!llvm.loop` metadata with `vectorize.enable=true` ALREADY emitted on for-in back-edge (create_loop_metadata(true, 4) in loop.cpp:338)
- [x] 1.2 LLVM remark: "loop not vectorized: Control flow cannot be substituted for a select" — vectorization metadata exists but LLVM cannot vectorize
- [x] 1.3 Root cause: `list.get(i)` inline body has multiple pointer indirections (handle→data_addr→element) via `inttoptr`+`load` that LLVM cannot hoist out of the loop or prove alias-free

## 2. Implementation
- [x] 2.1 Vectorization metadata already emitted — no changes needed
- [x] 2.2 BLOCKED: LLVM cannot vectorize `list.get(i)` pattern because:
  - The handle pointer loads (stride, data_addr) are not marked `!invariant.load`
  - `inttoptr` + `ptrtoint` pattern prevents alias analysis
  - The `alwaysinline` get() generates many allocas that mem2reg resolves, but after inlining the loads are still not provably loop-invariant
- [ ] 2.3 FUTURE: Implement pointer-based ListIter for for-in (pre-hoist data/stride, iterate via ptr+=stride) to match Rust's Vec::iter()
- [ ] 2.4 FUTURE: Add `@vectorize` attribute for user-controlled vectorization hints

## 3. Benchmark Gate
- [x] 3.1 Current: TML 4.0B ops/s vs Rust 17.7B ops/s (4.4× gap) — Rust auto-vectorizes, TML scalar
- [ ] 3.2 GATE NOT MET: requires pointer-based iteration to enable vectorization

## 4. Tail
- [x] 4.1 Documented root cause and future fix path
- [x] 4.2 No code changes — metadata was already correct
- [x] 4.3 No regressions
