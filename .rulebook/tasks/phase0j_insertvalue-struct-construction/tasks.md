## 1. Diagnosis
- [ ] 1.1 Emit IR for `benchmarks/profile_tml/struct_bench.tml --stage=parser:cpp` — locate struct literal construction, confirm `alloca+store+load` pattern
- [ ] 1.2 Write equivalent Rust struct in `.sandbox/rust_struct_bench.rs`, compile with `rustc -O --emit=llvm-ir` — confirm `insertvalue` pattern
- [ ] 1.3 Use `/compare-ir` skill to diff TML vs Rust IR for the struct constructor function

## 2. Implementation
- [ ] 2.1 In `instructions.cpp` MIR→LLVM emission for aggregate/struct construction: replace `alloca+store+load` with `insertvalue` chain starting from `undef`
- [ ] 2.2 Verify that field order in `insertvalue` matches struct layout (field indices 0, 1, 2, ...)
- [ ] 2.3 Preserve `sret` ABI for struct-returning functions (do not change calling convention)
- [ ] 2.4 Handle nested structs: emit nested `insertvalue` for struct fields that are themselves structs

## 3. Benchmark Gate
- [ ] 3.1 Run `benchmarks/profile_tml/struct_bench.tml --stage=parser:cpp` — capture struct creation and field access results
- [ ] 3.2 Compare vs Rust baseline from `docs/analysis/benchmark/05-memory-structs.md`
- [ ] 3.3 GATE: Struct access must be <5 ns/op (improvement from 16-32 ns/op). Ratio vs Rust must be <3x at O0. Do NOT proceed to next task if gate fails.

## 4. Validation
- [ ] 4.1 Run `tml test --suite=core` — no regressions
- [ ] 4.2 Run `tml test --suite=compiler` — no regressions
- [ ] 4.3 Verify IR: `insertvalue` present, no `alloca` for struct literals that don't escape

## 5. Tail (mandatory — enforced by rulebook v5.3.0)
- [ ] 5.1 Update CHANGELOG.md with `perf(codegen): emit insertvalue chains for struct construction`
- [ ] 5.2 Write regression test: struct literal construction and field read in a tight loop
- [ ] 5.3 Run tests and confirm they pass
