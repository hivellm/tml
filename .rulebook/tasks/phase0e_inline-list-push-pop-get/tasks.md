## 1. Diagnosis
- [ ] 1.1 Emit IR for list_bench.tml — confirm List.push/get generate `call` instructions instead of inline code
- [ ] 1.2 Check if `@inline` attribute exists in the compiler — search for "inline" in `compiler/src/`
- [ ] 1.3 Read Rust reference: Rust's Vec::push is `#[inline]` — verify with `rustc --emit=llvm-ir`

## 2. Implementation
- [ ] 2.1 If `@inline` works: add `@inline` to List.push(), List.pop(), List.get(), List.set(), List.len()
- [ ] 2.2 If `@inline` doesn't work: implement `@inline` attribute support in the compiler (mark function for LLVM AlwaysInline)
- [ ] 2.3 Verify inlined IR: push should be a bounds check + store, not a function call

## 3. Benchmark Gate
- [ ] 3.1 Run `benchmarks/profile_tml/list_bench.tml --stage=parser:cpp` — capture all results
- [ ] 3.2 Run `.sandbox/rust_list.exe` — capture all results
- [ ] 3.3 GATE: List Push (reserved) must be <3 ns/op. List Access must be <2 ns/op. Ratio vs Rust must be <2.5x. Do NOT proceed if gate fails.

## 4. Validation
- [ ] 4.1 Run `tml test --suite=core` — no regressions
- [ ] 4.2 Run `tml test --suite=std` — no regressions

## 5. Tail (mandatory — enforced by rulebook v5.3.0)
- [ ] 5.1 Update CHANGELOG.md
- [ ] 5.2 Write benchmark regression test: List push 1M elements timing
- [ ] 5.3 Run tests and confirm they pass
