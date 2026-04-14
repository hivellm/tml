## 1. Diagnosis
- [ ] 1.1 Emit IR for `benchmarks/profile_tml/function_bench.tml --stage=parser:cpp` — locate the function-pointer call, confirm it is `call ptr %fn_ptr` (indirect)
- [ ] 1.2 Write equivalent Rust with a concrete closure passed to a function, compile with `rustc -O --emit=llvm-ir` — confirm Rust devirtualizes to a direct `call @closure_body`
- [ ] 1.3 Identify the MIR instruction type used for the indirect call — confirm it is distinct from the direct `CallInst`

## 2. Implementation
- [ ] 2.1 In `thir_mir_builder_expr.cpp`, method call path: when receiver type is concrete (not `TypeVar` or `dyn`), look up the impl in the type environment and emit `CallInst` with the impl function's symbol directly
- [ ] 2.2 In closure call path: when the callee expression is a `ClosureLiteral` defined in the same scope and not stored elsewhere, inline the closure body or emit a direct `call @closure_N`
- [ ] 2.3 Add a fallback: if static resolution fails, retain the indirect call (correctness over optimization)
- [ ] 2.4 Use `/compare-ir` skill to verify the emitted IR matches Rust's devirtualized form

## 3. Benchmark Gate
- [ ] 3.1 Run `benchmarks/profile_tml/function_bench.tml --stage=parser:cpp` — capture function pointer call result
- [ ] 3.2 Compare vs Rust baseline from `docs/analysis/benchmark/06-functions-closures.md`
- [ ] 3.3 GATE: Function pointer (concrete callee) must be <2 ns/op. Ratio vs Rust must be <3x. Do NOT proceed if gate fails.

## 4. Validation
- [ ] 4.1 Run `tml test --suite=core` — no regressions (behavior dispatch used heavily in collections)
- [ ] 4.2 Run `tml test --suite=compiler` — no regressions
- [ ] 4.3 Verify polymorphic dispatch (dyn behavior) still uses indirect call — devirtualization must not break dynamic dispatch

## 5. Tail (mandatory — enforced by rulebook v5.3.0)
- [ ] 5.1 Update CHANGELOG.md with `perf(codegen): devirtualize behavior method calls on concrete receiver types`
- [ ] 5.2 Write regression test: concrete-type method call in a loop, verifying direct call IR
- [ ] 5.3 Run tests and confirm they pass
