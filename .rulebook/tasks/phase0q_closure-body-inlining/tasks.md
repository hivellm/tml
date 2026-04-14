## 1. Diagnosis
- [ ] 1.1 Emit IR for a TML `list.map(do(x) x * 2)` — confirm heap allocation + indirect call pattern
- [ ] 1.2 Write equivalent Rust `vec.iter().map(|x| x * 2).collect()` with `rustc -O --emit=llvm-ir` — confirm LLVM loop with inlined body, no heap alloc
- [ ] 1.3 Identify the MIR nodes for closure construction (`ClosureInst`?) and the call site (`CallInst` via fn ptr)

## 2. Implementation
- [ ] 2.1 Implement escape analysis: track whether a `ClosureLiteral` escapes the current basic block — if not, mark as inlineable
- [ ] 2.2 In the MIR builder: when a non-escaping closure is passed directly to a HOF call, substitute the closure's body instructions at the call site (inline the MIR body)
- [ ] 2.3 For captured variables: if all captures are immutable locals, pass them as extra arguments instead of allocating a capture struct
- [ ] 2.4 Mark `List.map`, `List.filter`, `List.fold`, `List.for_each` in `list.tml` with `@inline` on the closure parameter
- [ ] 2.5 Retain heap-allocation path for closures that DO escape (stored in struct, returned, etc.)

## 3. Benchmark Gate
- [ ] 3.1 Run `benchmarks/profile_tml/function_bench.tml --stage=parser:cpp` — capture closure map result
- [ ] 3.2 Compare vs Rust baseline from `docs/analysis/benchmark/06-functions-closures.md`
- [ ] 3.3 GATE: Closure map must be <2 ns/op (from 3-5 ns/op). Ratio vs Rust must be <2x. Do NOT proceed if gate fails.

## 4. Validation
- [ ] 4.1 Run `tml test --suite=core` — no regressions (List.map/filter used widely)
- [ ] 4.2 Run `tml test --suite=compiler` — no regressions
- [ ] 4.3 Verify escaping closures still work: closure stored in a struct and invoked in a subsequent call

## 5. Tail (mandatory — enforced by rulebook v5.3.0)
- [ ] 5.1 Update CHANGELOG.md with `perf(codegen): inline non-escaping closure bodies at HOF call sites`
- [ ] 5.2 Write regression tests: inlined closure (map, filter), and escaping closure (stored + invoked via struct field)
- [ ] 5.3 Run tests and confirm they pass
