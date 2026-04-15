## 1. Analysis
- [ ] 1.1 Emit Rust `for x in vec.iter()` optimized IR — document exact loop structure
- [ ] 1.2 Emit TML `for x in list` IR — identify every divergence from Rust pattern
- [ ] 1.3 List all blockers preventing LLVM vectorization

## 2. Core: pointer-stepping for-in (eliminate Maybe)
- [ ] 2.1 In `gen_for` (loop.cpp): detect IntoIterator + ListIter-like type, emit direct pointer-stepping loop — no `next() -> Maybe[T]` call
- [ ] 2.2 Emit phi-based loop: `%ptr = phi ptr` with `icmp eq ptr %ptr, %end` as condition
- [ ] 2.3 Direct element load: `%val = load T, ptr %ptr, align 8` — no extractvalue from Maybe
- [ ] 2.4 GEP pointer advance: `%ptr.next = getelementptr i8, ptr %ptr, i64 %stride`
- [ ] 2.5 Attach `!llvm.loop` with `vectorize.enable=true, vectorize.width=0`

## 3. Alias metadata
- [ ] 3.1 Create `!alias.scope` + `!noalias` metadata nodes for for-in loads
- [ ] 3.2 Attach to element loads in pointer-stepping loops
- [ ] 3.3 Verify LLVM sees non-aliasing loads

## 4. Remove personality from leaf functions
- [ ] 4.1 Only emit `personality ptr @__CxxFrameHandler3` when function has invoke/landingpad/throw
- [ ] 4.2 Omit for simple leaf functions (arithmetic, loads, GEP)
- [ ] 4.3 Verify try/catch still works

## 5. MIR path: same optimization
- [ ] 5.1 MIR codegen: detect ListIter for-in, emit phi+GEP+load
- [ ] 5.2 MIR loop metadata with vectorize.enable
- [ ] 5.3 Suppress lifetime markers in MIR loop bodies

## 6. Benchmark Gate
- [ ] 6.1 `for x in list` sum 10M I64 must reach at least 10B ops/s (from 2.6B)
- [ ] 6.2 Compare vs Rust `.iter()` — ratio must be under 2x
- [ ] 6.3 Confirm `vector.body` with `<2 x i64>` in optimized IR

## 7. Validation
- [ ] 7.1 `tml test --suite=compiler` — no regressions
- [ ] 7.2 `tml test --suite=core` — no regressions
- [ ] 7.3 Correct results: empty list, single element, 1M elements
- [ ] 7.4 `for entry in btreemap.iter()` still works (Iterator path unchanged)
- [ ] 7.5 try/catch functions still have personality

## 8. Tail
- [ ] 8.1 Update CHANGELOG.md
- [ ] 8.2 Write regression test: for-in list sum + correctness
- [ ] 8.3 Run tests and confirm they pass
