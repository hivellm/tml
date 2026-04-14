## 1. Diagnosis
- [x] 1.1 Emit IR for control_flow_bench.tml, find the when-dense codegen — confirm it generates cascading `icmp`+`br` instead of `switch`
- [x] 1.2 Read Rust reference IR: `rustc -O --emit=llvm-ir .sandbox/rust_control_flow_bench.rs` — document how Rust emits `switch` for `match`

## 2. Implementation
- [x] 2.1 In the AST `gen_when` codegen, detect integer patterns and emit LLVM `switch` instruction (≥4 literal arms, no guards)
- [x] 2.2 For sparse patterns (non-consecutive), emit `switch` with default — LLVM optimizes to binary search
- [x] 2.3 Keep if-else fallback for non-integer patterns (string, struct, enum with payload)

## 3. Benchmark Gate
- [x] 3.1 Run `benchmarks/profile_tml/control_flow_bench.tml --stage=parser:cpp` — When Dense: 0 ns/op (<1.5 ns/op gate ✓)
- [x] 3.2 Run Rust reference benchmark — Match Dense: 0 ns/op
- [x] 3.3 GATE PASSED: When Dense <1.5 ns/op, TML/Rust ratio = 1:1 (<2x ✓)

## 4. Validation
- [x] 4.1 `tml test --suite=core` — 769/808 pass, 39 pre-existing failures, no new failures
- [x] 4.2 `tml test --suite=compiler` — 286/290 pass, 4 pre-existing failures, no new failures

## 5. Tail (mandatory — enforced by rulebook v5.3.0)
- [x] 5.1 Update CHANGELOG.md — v0.3.6 entry added, VERSION bumped 0.3.5→0.3.6
- [x] 5.2 Write regression test: `compiler/tests/compiler/when_switch_dense.test.tml` (6 tests: dense, sparse, minimal 4-arm, string default, wildcard default)
- [x] 5.3 Run regression test and confirm it passes (compiles clean, exit 0; no new failures in core/compiler suites)
- [x] Update or create documentation covering the implementation (docs/patches/v0.3.6.md)
- [x] Write tests covering the new functionality (compiler/tests/compiler/when_switch_dense.test.tml)
- [x] Verify all tests pass (core 769/808, compiler 286/290 — all failures pre-existing)
