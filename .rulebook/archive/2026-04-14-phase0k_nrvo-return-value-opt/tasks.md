## 1. Diagnosis
- [x] 1.1 Emit IR for a TML function returning a struct — confirm the intermediate copy (`memcpy` or redundant `alloca+load+store`)
- [x] 1.2 Write equivalent Rust `fn` returning struct with `rustc -O --emit=llvm-ir` — confirm no memcpy, direct `sret` construction
- [x] 1.3 Identify the exact MIR or codegen stage where the copy is introduced

## 2. Implementation
- [x] 2.1 In the MIR pass or codegen: detect `return local_var` where `local_var` is a struct allocated in the current function scope
- [x] 2.2 Rewrite: replace `local_var`'s `alloca` with a reference to the `sret` parameter pointer — all writes go directly to the return slot
- [x] 2.3 Handle multi-return functions: apply NRVO only when all paths return the same local variable; fall back otherwise
- [x] 2.4 Verify that `sret` attribute is correctly set on the function (prerequisite from phase0j)

## 3. Benchmark Gate
- [x] 3.1 Run function-returning-struct benchmark with `--stage=parser:cpp` — capture ns/op before and after
- [x] 3.2 Compare vs Rust baseline from `docs/analysis/benchmark/06-functions-closures.md`
- [x] 3.3 GATE: Struct-returning function must show ≥30% improvement. Ratio vs Rust must be <2x at O0. Do NOT proceed if gate fails.

## 4. Validation
- [x] 4.1 Run `tml test --suite=core` — no regressions
- [x] 4.2 Run `tml test --suite=compiler` — 188/188 pass
- [x] 4.3 Verify IR: no `memcpy` for simple NRVO-eligible functions

## 5. Tail (mandatory — enforced by rulebook v5.3.0)
- [x] 5.1 Update CHANGELOG.md with `perf(codegen): NRVO in MIR sret path`
- [x] 5.2 Write regression test: `compiler/tests/compiler/nrvo_sret.test.tml` — 10 tests covering wrapper, double-wrap, clamp, loop, edge cases
- [x] 5.3 Run tests and confirm they pass — 188/188
- [x] 5.4 Update documentation: `docs/patches/v0.3.13.md` created, CHANGELOG.md updated
