## 1. Diagnosis
- [x] 1.1 Emit IR for list_bench.tml — confirm List.push/get generate `call` instructions instead of inline code
- [x] 1.2 Check if `@inline` attribute exists in the compiler — search for "inline" in `compiler/src/`
- [x] 1.3 Read Rust reference: Rust's Vec::push is `#[inline]` — verify with `rustc --emit=llvm-ir`

## 2. Implementation
- [x] 2.1 If `@inline` works: add `@inline` to List.push(), List.pop(), List.get(), List.set(), List.len()
- [x] 2.2 If `@inline` doesn't work: implement `@inline` attribute support in the compiler (mark function for LLVM AlwaysInline)
- [x] 2.3 Verify inlined IR: push should be a bounds check + store, not a function call

## 3. Benchmark Gate
- [x] 3.1 Run `benchmarks/profile_tml/list_bench.tml --stage=parser:cpp` — capture all results
- [x] 3.2 Run `.sandbox/rust_list.exe` — capture all results
- [x] 3.3 GATE: List Push (reserved) must be <3 ns/op. List Access must be <2 ns/op. Ratio vs Rust must be <2.5x. Do NOT proceed if gate fails.

## 4. Validation
- [x] 4.1 Run `tml test --suite=core` — no regressions (4 pre-existing core/any T056 failures only)
- [x] 4.2 Run `tml test --suite=std` — no regressions (2 pre-existing std/collections K001 failures only)

## 5. Tail (mandatory — enforced by rulebook v5.3.0)
- [x] 5.1 Update CHANGELOG.md
- [x] 5.2 Write benchmark regression test: List push 1M elements timing
- [x] 5.3 Run tests and confirm they pass
- [x] Update or create documentation covering the implementation (docs/patches/v0.3.7.md)
- [x] Write tests covering the new functionality (compiler/tests/compiler/list_inline.test.tml — 6 tests)
- [x] Verify all tests pass (6/6 list_inline pass, core/std failures all pre-existing)
