## 1. Diagnosis
- [x] 1.1 Emit IR for control_flow_bench.tml — find if-else chain codegen, confirm it generates `br`+`phi` instead of `select`
- [x] 1.2 Read Rust reference IR — confirm Rust generates `select` for simple if-else chains
- [x] 1.3 Identify the codegen path for if-else expression (IfExpr in MIR emission)

## 2. Implementation
- [x] 2.1 Detect pattern: if-else where both branches are single scalar values (no side effects)
- [x] 2.2 Emit `select i1 %cond, i64 %then_val, i64 %else_val` instead of branch+phi
- [x] 2.3 Keep branch+phi for complex if-else (multi-statement bodies, side effects)
- [x] 2.4 Handle chained if-else-if: nest `select` instructions

## 3. Benchmark Gate
- [x] 3.1 Run `benchmarks/profile_tml/control_flow_bench.tml --stage=parser:cpp` — capture If-Else Chain and Ternary Chain
- [x] 3.2 Run `.sandbox/rust_control_flow_bench.exe` — capture equivalents
- [x] 3.3 GATE: If-Else Chain (4 branches) must be <1 ns/op (>2B ops/sec). Ratio vs Rust must be <3x. Do NOT proceed if gate fails.

## 4. Validation
- [x] 4.1 Run `tml test --suite=core` — no regressions
- [x] 4.2 Run `tml test --suite=compiler` — no regressions

## 5. Tail (mandatory — enforced by rulebook v5.3.0)
- [x] 5.1 Update CHANGELOG.md
- [x] 5.2 Write test: if-else returning scalar values produces correct results
- [x] 5.3 Run tests and confirm they pass
