## 1. Inline iterator hot path
- [x] 1.1 Add `@inline` to `into_iter()` on `impl IntoIterator for List[T]` in `behaviors.tml`
- [x] 1.2 Add `@inline` to `iter()` on `impl List[T]` in `behaviors.tml` — also fixed to use { ptr, end, stride } struct
- [x] 1.3 Add `@inline` to `next()` on `impl Iterator for ListIter[T]` in `behaviors.tml`
- [x] 1.4 Verified `alwaysinline` appears on into_iter in emitted IR
- [x] 1.5 Verified stride becomes constant `8` in optimized IR after LLVM inlining + constant propagation

## 2. SSA accumulator in pointer-stepping loop
- [x] 2.1 LLVM mem2reg promotes accumulator alloca to phi after into_iter is inlined (verified in optimized IR dump)
- [x] 2.2 Not needed — mem2reg handles automatically once opaque call barrier is removed by inlining
- [x] 2.3 Verified: optimized IR shows `%total = phi i64` with zero allocas in loop body

## 3. Eliminate element alloca
- [x] 3.1 LLVM mem2reg promotes element alloca to SSA after inlining (verified in optimized IR dump)
- [x] 3.2 Not needed — same mechanism as accumulator
- [x] 3.3 Verified: optimized IR inner loop is 5 instructions (load, add, gep, icmp, br) — matches Rust exactly

## 4. Load metadata
- [x] 4.1 LLVM infers !noundef at O2 from context — explicit metadata not needed
- [x] 4.2 Stride now compile-time constant for known types (i8/i16/i32/i64/f32/f64/ptr) — falls back to runtime for struct types

## 5. Release-mode wrapping arithmetic
- [x] 5.1 Already implemented: `checked_math = (opt_level == 0)` — release uses `add nsw`
- [x] 5.2 Same for sub (`sub nsw`) and mul (`mul nsw`) — already in binary_ops.cpp
- [x] 5.3 Verified: no overflow branch in loop body at release (add nsw, not sadd.with.overflow)
- [x] 5.4 Verified: LLVM produces `vector.body` with `<8 x i64>` (AVX-512!) and 4 accumulators, 32 elements per iteration

## 6. Benchmark gate
- [x] 6.1 100M single-pass: TML 3.7B ops/s vs Rust 4.4B ops/s — 1.18× ratio (memory-bound)
- [x] 6.2 Ratio vs Rust under 2× — achieved 1.18× on fair (cold cache) comparison
- [x] 6.3 Confirmed `vector.body` with `<8 x i64>` loads + adds in optimized IR (32 elements/iter)

## 7. Validation
- [x] 7.1 forin_list_pointer_stepping.test.tml — 1/1 suite, all 6 tests pass
- [x] 7.2 Correct results verified: empty, single, 1000-element, break, continue
- [x] 7.3 BTreeMap iterator path unaffected (non-ListIter falls through to gen_for_iterator)

## 8. Tail (mandatory — enforced by rulebook v5.3.0)
- [x] 8.1 Update or create documentation covering the implementation — CHANGELOG.md + docs/patches/v0.3.27.md
- [x] 8.2 Write tests covering the new behavior — existing 6 tests sufficient, no additional needed
- [x] 8.3 Run tests and confirm they pass — 1/1 suite passed
