## 1. Diagnosis
- [ ] 1.1 Emit LLVM IR for json_bench.tml and locate the `icmp eq i1 %t4892, 1` instruction at IR line ~13243
- [ ] 1.2 Trace backwards from `%t4892` to find the instruction that produces `i32` — identify the TML source expression (likely a boolean comparison, `when` match, or struct field access)
- [ ] 1.3 Check if the issue is: (a) boolean stored in struct as I32 then loaded without truncation, (b) comparison operator returning I32 discriminant instead of i1, or (c) enum variant check producing wrong type

## 2. Fix
- [ ] 2.1 Fix the codegen to emit `i1` for boolean values in the identified path — add `trunc i32 %v to i1` or fix the producer to emit `i1` directly
- [ ] 2.2 Audit all `icmp` emission sites in the codegen for similar i32/i1 mismatches
- [ ] 2.3 Compile and run `benchmarks/profile_tml/json_bench.tml --stage=parser:cpp` successfully

## 3. Validation
- [ ] 3.1 Run `tml test --suite=core` — no regressions
- [ ] 3.2 Run `tml test --suite=std` — no regressions
- [ ] 3.3 Run `tml test --suite=compiler` — no regressions

## 4. Tail (mandatory — enforced by rulebook v5.3.0)
- [ ] 4.1 Update CHANGELOG.md with the fix
- [ ] 4.2 Write a regression test with boolean struct field + comparison codegen
- [ ] 4.3 Run tests and confirm they pass
