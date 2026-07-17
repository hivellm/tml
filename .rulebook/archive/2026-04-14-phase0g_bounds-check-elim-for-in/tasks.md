## 1. Diagnosis
- [x] 1.1 Emit IR for list_bench.tml — find the List.get() call inside the iteration loop, confirm bounds check is present
- [x] 1.2 Read Rust reference: Rust eliminates bounds checks for `for i in 0..vec.len() { vec[i] }` patterns via range analysis
- [x] 1.3 Determine approach: (a) MIR pass that removes check, (b) unchecked_get intrinsic, or (c) LLVM assumes/range metadata

## 2. Implementation
- [x] 2.1 Implement bounds-check elimination for `for i in 0 to list.len() { list.get(i) }` pattern
- [x] 2.2 Ensure elimination ONLY happens when index provably < length (for-in bounds, not arbitrary access)
- [x] 2.3 Keep bounds check for all other access patterns (safety first)

## 3. Benchmark Gate
- [x] 3.1 Run `benchmarks/profile_tml/list_bench.tml --stage=parser:cpp` — capture List Iteration and List Random Access
- [x] 3.2 Run `.sandbox/rust_list.exe` — capture equivalents
- [x] 3.3 GATE: List Iteration must be <2 ns/op. Ratio vs Rust must be <3x. Do NOT proceed if gate fails.

## 4. Validation
- [x] 4.1 Run `tml test --suite=core` — no regressions
- [x] 4.2 Run `tml test --suite=std` — no regressions
- [x] 4.3 Verify out-of-bounds access still panics (safety preserved)

## 5. Tail (mandatory — enforced by rulebook v5.3.0)
- [x] 5.1 Update CHANGELOG.md and create docs/patches/v0.3.9.md — documentation covering the implementation
- [x] 5.2 Write test: for-in loop over list verifies no bounds panic + correct results
- [x] 5.3 Run tests and confirm they pass
