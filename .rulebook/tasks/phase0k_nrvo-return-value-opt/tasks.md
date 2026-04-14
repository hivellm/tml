## 1. Diagnosis
- [ ] 1.1 Emit IR for a TML function returning a struct — confirm the intermediate copy (`memcpy` or redundant `alloca+load+store`)
- [ ] 1.2 Write equivalent Rust `fn` returning struct with `rustc -O --emit=llvm-ir` — confirm no memcpy, direct `sret` construction
- [ ] 1.3 Identify the exact MIR or codegen stage where the copy is introduced

## 2. Implementation
- [ ] 2.1 In the MIR pass or codegen: detect `return local_var` where `local_var` is a struct allocated in the current function scope
- [ ] 2.2 Rewrite: replace `local_var`'s `alloca` with a reference to the `sret` parameter pointer — all writes go directly to the return slot
- [ ] 2.3 Handle multi-return functions: apply NRVO only when all paths return the same local variable; fall back otherwise
- [ ] 2.4 Verify that `sret` attribute is correctly set on the function (prerequisite from phase0j)

## 3. Benchmark Gate
- [ ] 3.1 Run function-returning-struct benchmark with `--stage=parser:cpp` — capture ns/op before and after
- [ ] 3.2 Compare vs Rust baseline from `docs/analysis/benchmark/06-functions-closures.md`
- [ ] 3.3 GATE: Struct-returning function must show ≥30% improvement. Ratio vs Rust must be <2x at O0. Do NOT proceed if gate fails.

## 4. Validation
- [ ] 4.1 Run `tml test --suite=core` — no regressions
- [ ] 4.2 Run `tml test --suite=compiler` — no regressions
- [ ] 4.3 Verify IR: no `memcpy` for simple NRVO-eligible functions

## 5. Tail (mandatory — enforced by rulebook v5.3.0)
- [ ] 5.1 Update CHANGELOG.md with `perf(codegen): apply NRVO for struct-returning functions`
- [ ] 5.2 Write regression test: function returning a struct, called in a loop, result used to prevent DCE
- [ ] 5.3 Run tests and confirm they pass
