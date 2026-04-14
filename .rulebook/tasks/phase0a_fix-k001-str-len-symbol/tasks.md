## 1. Diagnosis
- [ ] 1.1 Emit LLVM IR for a minimal file that calls `str.len()` — inspect IR for presence/absence of `@tml_N4core3str3lenE_S`
- [ ] 1.2 Trace codegen path: check if `core::str::len` is in the MIR module, the symbol table, and the LLVM emission queue
- [ ] 1.3 Determine if the issue is dead-code elimination, missing generic instantiation, or symbol mangling mismatch

## 2. Fix
- [ ] 2.1 Fix the root cause so `core::str::len()` emits a valid LLVM function definition
- [ ] 2.2 Verify all other `core::str` methods (`contains`, `find`, `split`, `trim`, `starts_with`, `ends_with`) also emit correctly
- [ ] 2.3 Compile and run `benchmarks/profile_tml/string_bench.tml --stage=parser:cpp` successfully
- [ ] 2.4 Compile and run `benchmarks/profile_tml/text_bench.tml --stage=parser:cpp` successfully

## 3. Validation
- [ ] 3.1 Run `tml test --suite=core` — no regressions
- [ ] 3.2 Run `tml test --suite=std` — no regressions
- [ ] 3.3 Run `tml test --suite=compiler` — no regressions

## 4. Tail (mandatory — enforced by rulebook v5.3.0)
- [ ] 4.1 Update CHANGELOG.md with the fix
- [ ] 4.2 Write a regression test for `core::str::len` codegen (`.test.tml` file)
- [ ] 4.3 Run tests and confirm they pass
