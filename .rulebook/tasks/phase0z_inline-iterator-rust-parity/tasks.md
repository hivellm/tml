## 1. Inline iterator hot path
- [ ] 1.1 Add `@inline` to `into_iter()` on `impl IntoIterator for List[T]` in `behaviors.tml`
- [ ] 1.2 Add `@inline` to `iter()` on `impl List[T]` in `behaviors.tml`
- [ ] 1.3 Add `@inline` to `next()` on `impl Iterator for ListIter[T]` in `behaviors.tml`
- [ ] 1.4 Verify `alwaysinline` appears on all three functions in emitted IR
- [ ] 1.5 Verify stride becomes constant `8` (for I64) after LLVM optimization

## 2. SSA accumulator in pointer-stepping loop
- [ ] 2.1 In `gen_for_pointer_stepping` (loop.cpp): emit accumulator as phi node instead of alloca+store+load
- [ ] 2.2 Only fall back to alloca if loop body takes address of accumulator variable
- [ ] 2.3 Verify mem2reg promotes remaining allocas after inlining

## 3. Eliminate element alloca
- [ ] 3.1 In `gen_for_pointer_stepping`: bind loop variable directly to loaded SSA value (no store+reload)
- [ ] 3.2 If body takes address of loop variable (`&x`), emit alloca only then
- [ ] 3.3 Verify 2 fewer memory ops per iteration in emitted IR

## 4. Load metadata
- [ ] 4.1 Add `!noundef` metadata to pointer-stepping element loads
- [ ] 4.2 Add `align` based on element type size (not hardcoded 8)

## 5. Release-mode wrapping arithmetic
- [ ] 5.1 In `gen_binary_ops` (binary_ops.cpp): when optimization level >= 2, emit `add nsw` instead of `@llvm.sadd.with.overflow`
- [ ] 5.2 Same for sub, mul (emit `sub nsw`, `mul nsw`)
- [ ] 5.3 Verify no overflow branch in loop body at release mode
- [ ] 5.4 Verify LLVM produces `vector.body` with `<2 x i64>` at O2

## 6. Benchmark gate
- [ ] 6.1 `for x in list` sum 10M I64 with wrapping_add must reach at least 40B ops/s (from 4.4B)
- [ ] 6.2 Ratio vs Rust must be under 2×
- [ ] 6.3 Confirm `vector.body` with `<2 x i64>` in optimized IR

## 7. Validation
- [ ] 7.1 `tml test` on forin_list_pointer_stepping.test.tml — all 6 pass
- [ ] 7.2 Correct results: empty list, single element, 1M elements, break, continue
- [ ] 7.3 `for entry in btreemap.iter()` still works (Iterator path unchanged)

## 8. Tail (mandatory — enforced by rulebook v5.3.0)
- [ ] 8.1 Update CHANGELOG.md and docs/patches
- [ ] 8.2 Write additional regression tests if needed
- [ ] 8.3 Run tests and confirm they pass
