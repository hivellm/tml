## 1. Diagnosis
- [ ] 1.1 Emit IR for a function that creates a `Heap::new(42)`, uses it, and never returns it — confirm `call @malloc` present
- [ ] 1.2 Write equivalent Rust `Box::new(42)` in a local scope with `rustc -O --emit=llvm-ir` — confirm Rust promotes to stack (`alloca`) when value doesn't escape
- [ ] 1.3 Identify `HeapAllocInst` in the MIR — confirm the MIR IR node that represents `Heap::new`

## 2. Implementation
- [ ] 2.1 Create `compiler/src/mir/escape_analysis.cpp`: implement `EscapeAnalysis` pass that iterates all `HeapAllocInst`s in a function
- [ ] 2.2 For each `HeapAllocInst`, walk all users in the def-use chain — mark as escaping if any use is: `StoreInst` into a non-stack location, `ReturnInst`, `CallInst` that takes the pointer as a non-inlined argument
- [ ] 2.3 For non-escaping allocations: replace `HeapAllocInst` with `AllocaInst` of the same type — all pointer uses remain valid (same type, same operations)
- [ ] 2.4 Hook the pass into the MIR optimization pipeline before LLVM emission

## 3. Benchmark Gate
- [ ] 3.1 Write `.sandbox/heap_stack_bench.tml`: function that creates and consumes 1M `Heap[I64]` values in a loop — measure ns/op with `--stage=parser:cpp`
- [ ] 3.2 Write equivalent Rust with `Box<i64>` — compile `-O`, measure ns/op
- [ ] 3.3 GATE: After stack promotion, the non-escaping `Heap::new` loop must show ≥50% improvement. Ratio vs Rust must be <2x. Do NOT proceed if gate fails.

## 4. Validation
- [ ] 4.1 Run `tml test --suite=core` — no regressions (all heap semantics must remain correct for escaping pointers)
- [ ] 4.2 Run `tml test --suite=compiler` — no regressions
- [ ] 4.3 Verify escaping `Heap::new` still uses `malloc` (not demoted to stack)

## 5. Tail (mandatory — enforced by rulebook v5.3.0)
- [ ] 5.1 Update CHANGELOG.md with `perf(mir): escape analysis pass promotes non-escaping Heap::new to stack alloca`
- [ ] 5.2 Write regression tests: non-escaping Heap (stack-promoted), escaping Heap (stored in struct — must remain heap-allocated)
- [ ] 5.3 Run tests and confirm they pass
