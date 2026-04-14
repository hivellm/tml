## 1. Diagnosis
- [ ] 1.1 Emit IR for `benchmarks/profile_tml/string_bench.tml` — confirm `bench_concat_naive` generates `str_concat_opt` + `tml_str_free` per iteration
- [ ] 1.2 Record baseline: `string_bench --stage=parser:cpp` — Concat Loop Str ns/op, Log Building Str ns/op

## 2. Runtime — `str_concat_reuse`
- [ ] 2.1 Add `realloc` declaration to `compiler/src/codegen/mir_codegen.cpp` `emit_preamble()` and `compiler/src/codegen/llvm/core/generate_support.cpp`
- [ ] 2.2 Add `str_concat_reuse` inline IR to `compiler/src/codegen/llvm/core/runtime.cpp`: `realloc(%a, needed+1)` + `memcpy(%b)` + NUL terminator
- [ ] 2.3 Rebuild compiler: `scripts\build.bat`

## 3. Codegen — select reuse vs opt
- [ ] 3.1 In `compiler/src/codegen/llvm/expr/binary_ops.cpp` string `BinOp::Add` path: when `is_heap_str_producer(*bin.left)` is true, call `str_concat_reuse` instead of `str_concat_opt` — omit the left-operand `tml_str_free` (realloc consumed it)
- [ ] 3.2 Rebuild compiler
- [ ] 3.3 `tml check benchmarks/profile_tml/string_bench.tml` — zero errors
- [ ] 3.4 Emit IR — confirm `bench_concat_naive` now calls `str_concat_reuse` instead of `str_concat_opt` + `tml_str_free`

## 4. Benchmark Gate
- [ ] 4.1 Run `benchmarks/profile_tml/string_bench.tml --stage=parser:cpp --release` — record Concat Loop Str and Log Building Str ns/op
- [ ] 4.2 Run `.sandbox/rust_string_bench.exe` — record Rust ns/op for comparison
- [ ] 4.3 GATE: Concat Loop Str ≤ 10 ns/op (vs 3437 ns baseline). Ratio vs Rust <5x. Do NOT proceed to tail if gate fails.

## 5. Validation
- [ ] 5.1 `tml test --suite=compiler` — no regressions
- [ ] 5.2 `tml test --suite=core` — no regressions

## 6. Tail (mandatory — enforced by rulebook v5.3.0)
- [ ] 6.1 Update CHANGELOG.md — version bump, perf entry
- [ ] 6.2 Write regression test: `compiler/tests/compiler/str_concat_reuse.test.tml` — verify `result = result + x` in loop produces correct string and does not leak
- [ ] 6.3 Run regression test — confirm passes
- [ ] Update or create documentation covering the implementation
- [ ] Write tests covering the new functionality
- [ ] Verify all tests pass
