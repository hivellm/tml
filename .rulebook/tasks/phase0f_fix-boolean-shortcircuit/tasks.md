## 1. Diagnosis
- [x] 1.1 Emit IR for control_flow_bench.tml — find Short-Circuit AND/OR codegen, count basic blocks per `and`/`or` expression
- [x] 1.2 Compare with Rust IR: `rustc -O --emit=llvm-ir` — document how Rust chains short-circuit conditions
- [x] 1.3 Identify if the extra blocks come from MIR lowering or LLVM emission

## 2. Implementation
- [x] 2.1 For `a and b` where both are simple comparisons: emit `%r = and i1 %a, %b` (no short-circuit needed for side-effect-free expressions)
- [x] 2.2 For `a and b` with side effects: emit minimal 2-block short-circuit (entry → eval_b → merge), not 3+ blocks
- [x] 2.3 Same for `or`: `%r = or i1 %a, %b` or minimal short-circuit

## 3. Benchmark Gate
- [x] 3.1 Run `benchmarks/profile_tml/control_flow_bench.tml --stage=parser:cpp` — capture Short-Circuit AND/OR
- [x] 3.2 Run `.sandbox/rust_control_flow_bench.exe` — capture equivalents
- [x] 3.3 GATE: Short-Circuit AND must be <2 ns/op (>500M ops/sec). Ratio vs Rust must be <2x. Do NOT proceed if gate fails.

## 4. Validation
- [x] 4.1 Run `tml test --suite=core` — no regressions (pre-existing core/any T056 only)
- [x] 4.2 Run `tml test --suite=compiler` — 182/182 pass, no regressions

## 5. Tail (mandatory — enforced by rulebook v5.3.0)
- [x] 5.1 Update CHANGELOG.md (v0.3.8 entry added), VERSION bumped 0.3.7 → 0.3.8
- [x] 5.2 Write test: `compiler/tests/compiler/bool_short_circuit.test.tml` — 8 tests verifying and/or short-circuit with side-effect block expressions
- [x] 5.3 Run tests and confirm they pass — 183/183 ✓
