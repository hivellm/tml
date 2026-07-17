## 1. Diagnosis
- [x] 1.1 Confirmed `str_concat_opt` always allocates exact-fit buffer + full memcpy per call — O(n²) for loops
- [x] 1.2 Baseline: 3437 ns/op (Concat Loop 10K iter, 20KB result)

## 2. Runtime — `str_concat_reuse`
- [x] 2.1 `mem_realloc` already declared in runtime catalog
- [x] 2.2 Added `str_concat_reuse` inline IR to `runtime.cpp`: uses `mem_realloc` with 2x exponential growth (alloc = total*2+1), null fallback for first-call safety
- [x] 2.3 Compiler builds successfully

## 3. Codegen — select reuse vs opt
- [x] 3.1 In `binary.cpp` Assign handler: detect `result = result + expr` pattern at AST level. When `holds_heap_str` flag is true (variable was previously assigned from a heap concat), use `str_concat_reuse` (safe realloc). Otherwise use `str_concat_opt` (fresh alloc for literals).
- [x] 3.2 Added `holds_heap_str` field to `VarInfo` in `llvm_ir_gen.hpp` for compile-time heap-tracking
- [x] 3.3 Added free-on-reassign for Str vars: loads old value and calls `tml_str_free` when overwriting a known heap Str — prevents O(n) memory leak accumulation
- [x] 3.4 4 regression tests pass (loop correctness, short, chain, non-augmented)

## 4. Benchmark Gate
- [x] 4.1 After optimization: 3048 ns/op (was 3437 ns/op, ~11% improvement)
- [x] 4.2 GATE PARTIALLY MET: improvement is modest because `strlen` on the growing string is O(n) per call, making the loop O(n²) regardless of allocation strategy. True O(n) requires length tracking, which means using `Text` (already has SSO + O(1) len).
- [x] 4.3 Note: the realloc + exponential growth eliminates most reallocation overhead, but the strlen bottleneck dominates. For string building in loops, users should use `Text` (0 ns/op for ≤23 chars via SSO).

## 5. Validation
- [x] 5.1 4 str_concat_reuse regression tests pass
- [x] 5.2 No compiler test regressions

## 6. Tail (mandatory — enforced by rulebook v5.3.0)
- [x] 6.1 Regression test: `compiler/tests/compiler/str_concat_reuse.test.tml`
- [x] 6.2 Changes: `runtime.cpp` (str_concat_reuse), `binary.cpp` (augmented concat detection + free-on-reassign), `binary_ops.cpp` (restored original pattern), `llvm_ir_gen.hpp` (holds_heap_str flag)
- [x] 6.3 All tests pass
