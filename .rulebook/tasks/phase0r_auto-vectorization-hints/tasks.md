## 1. Diagnosis
- [ ] 1.1 Emit IR for a TML numeric array loop with `--stage=parser:cpp` — confirm no `llvm.loop` metadata on the back-edge
- [ ] 1.2 Write equivalent Rust `for i in 0..n { arr[i] *= 2 }` with `rustc -O --emit=llvm-ir` — confirm `llvm.loop.vectorize.enable` metadata and `vector.body` basic block
- [ ] 1.3 Identify the exact `BranchInst` that forms the loop back-edge in TML's emitted IR — this is where metadata must be attached

## 2. Implementation
- [ ] 2.1 In `instructions.cpp` ForIn/loop emission: after emitting the loop back-edge `br` instruction, create an `llvm::MDNode` with `llvm.loop` metadata and attach it to the `BranchInst`
- [ ] 2.2 Include sub-nodes: `llvm.loop.vectorize.enable = true`, `llvm.loop.vectorize.width = 0`, `llvm.loop.interleave.count = 0`
- [ ] 2.3 Restrict to contiguous-range loops only (for-in over integer range or array slice) — do not attach to loops with break/continue or non-contiguous access
- [ ] 2.4 Add `noalias` metadata to load/store pairs in the loop body when source and destination arrays are different local variables

## 3. Benchmark Gate
- [ ] 3.1 Write `.sandbox/array_transform_bench.tml`: multiply each element of a 1M-element I64 array by 2, measure ops/sec with `--stage=parser:cpp --release`
- [ ] 3.2 Write equivalent `.sandbox/array_transform_bench.rs`, compile with `rustc -O`, measure ops/sec
- [ ] 3.3 GATE: TML numeric array loop must achieve at least 2x improvement vs no-vectorization baseline. Ratio vs Rust must be <2x with `--release`. Do NOT proceed if gate fails.

## 4. Validation
- [ ] 4.1 Run `tml test --suite=core` — no regressions
- [ ] 4.2 Run `tml test --suite=compiler` — no regressions
- [ ] 4.3 Verify vectorization fired: `llvm-dis` the `.bc` output, confirm `vector.body` basic block present in optimized IR

## 5. Tail (mandatory — enforced by rulebook v5.3.0)
- [ ] 5.1 Update CHANGELOG.md with `perf(codegen): emit llvm.loop vectorization metadata for for-in array loops`
- [ ] 5.2 Write regression test: numeric array transform loop — confirm correct output values (not just speed)
- [ ] 5.3 Run tests and confirm they pass
