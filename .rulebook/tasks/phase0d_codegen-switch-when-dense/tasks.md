## 1. Diagnosis
- [ ] 1.1 Emit IR for control_flow_bench.tml, find the when-dense codegen — confirm it generates cascading `icmp`+`br` instead of `switch`
- [ ] 1.2 Read Rust reference IR: `rustc -O --emit=llvm-ir .sandbox/rust_control_flow_bench.rs` — document how Rust emits `switch` for `match`

## 2. Implementation
- [ ] 2.1 In the MIR→LLVM emission for WhenInst/MatchInst, detect integer patterns and emit LLVM `switch` instruction
- [ ] 2.2 For sparse patterns (non-consecutive), emit `switch` with default — LLVM will optimize to binary search
- [ ] 2.3 Keep if-else fallback for non-integer patterns (string, struct, etc.)

## 3. Benchmark Gate
- [ ] 3.1 Run `benchmarks/profile_tml/control_flow_bench.tml --stage=parser:cpp` — capture When Dense result
- [ ] 3.2 Run `.sandbox/rust_control_flow_bench.exe` — capture Match Dense result
- [ ] 3.3 GATE: When Dense must be <1.5 ns/op (>1.5B ops/sec). Ratio vs Rust must be <2x. Do NOT proceed to next task if gate fails.

## 4. Validation
- [ ] 4.1 Run `tml test --suite=core` — no regressions
- [ ] 4.2 Run `tml test --suite=compiler` — no regressions

## 5. Tail (mandatory — enforced by rulebook v5.3.0)
- [ ] 5.1 Update CHANGELOG.md
- [ ] 5.2 Write regression test: when with 10 dense integer cases
- [ ] 5.3 Run tests and confirm they pass
