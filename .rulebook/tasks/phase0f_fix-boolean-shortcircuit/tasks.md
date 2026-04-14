## 1. Diagnosis
- [ ] 1.1 Emit IR for control_flow_bench.tml — find Short-Circuit AND/OR codegen, count basic blocks per `and`/`or` expression
- [ ] 1.2 Compare with Rust IR: `rustc -O --emit=llvm-ir` — document how Rust chains short-circuit conditions
- [ ] 1.3 Identify if the extra blocks come from MIR lowering or LLVM emission

## 2. Implementation
- [ ] 2.1 For `a and b` where both are simple comparisons: emit `%r = and i1 %a, %b` (no short-circuit needed for side-effect-free expressions)
- [ ] 2.2 For `a and b` with side effects: emit minimal 2-block short-circuit (entry → eval_b → merge), not 3+ blocks
- [ ] 2.3 Same for `or`: `%r = or i1 %a, %b` or minimal short-circuit

## 3. Benchmark Gate
- [ ] 3.1 Run `benchmarks/profile_tml/control_flow_bench.tml --stage=parser:cpp` — capture Short-Circuit AND/OR
- [ ] 3.2 Run `.sandbox/rust_control_flow_bench.exe` — capture equivalents
- [ ] 3.3 GATE: Short-Circuit AND must be <2 ns/op (>500M ops/sec). Ratio vs Rust must be <2x. Do NOT proceed if gate fails.

## 4. Validation
- [ ] 4.1 Run `tml test --suite=core` — no regressions
- [ ] 4.2 Run `tml test --suite=compiler` — no regressions (boolean logic is everywhere)

## 5. Tail (mandatory — enforced by rulebook v5.3.0)
- [ ] 5.1 Update CHANGELOG.md
- [ ] 5.2 Write test: nested and/or expressions with side effects verify short-circuit order
- [ ] 5.3 Run tests and confirm they pass
