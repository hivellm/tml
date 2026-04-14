## 1. Diagnosis
- [ ] 1.1 Emit IR for list_bench.tml — find the List.get() call inside the iteration loop, confirm bounds check is present
- [ ] 1.2 Read Rust reference: Rust eliminates bounds checks for `for i in 0..vec.len() { vec[i] }` patterns via range analysis
- [ ] 1.3 Determine approach: (a) MIR pass that removes check, (b) unchecked_get intrinsic, or (c) LLVM assumes/range metadata

## 2. Implementation
- [ ] 2.1 Implement bounds-check elimination for `for i in 0 to list.len() { list.get(i) }` pattern
- [ ] 2.2 Ensure elimination ONLY happens when index provably < length (for-in bounds, not arbitrary access)
- [ ] 2.3 Keep bounds check for all other access patterns (safety first)

## 3. Benchmark Gate
- [ ] 3.1 Run `benchmarks/profile_tml/list_bench.tml --stage=parser:cpp` — capture List Iteration and List Random Access
- [ ] 3.2 Run `.sandbox/rust_list.exe` — capture equivalents
- [ ] 3.3 GATE: List Iteration must be <2 ns/op. Ratio vs Rust must be <3x. Do NOT proceed if gate fails.

## 4. Validation
- [ ] 4.1 Run `tml test --suite=core` — no regressions
- [ ] 4.2 Run `tml test --suite=std` — no regressions
- [ ] 4.3 Verify out-of-bounds access still panics (safety preserved)

## 5. Tail (mandatory — enforced by rulebook v5.3.0)
- [ ] 5.1 Update CHANGELOG.md
- [ ] 5.2 Write test: for-in loop over list verifies no bounds panic + correct results
- [ ] 5.3 Run tests and confirm they pass
