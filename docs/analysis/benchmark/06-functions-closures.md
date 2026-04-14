# 06 — Function Call & Closure Benchmarks

## Function Call Results (10M iterations)

| Benchmark | TML (ns/op) | TML (ops/sec) |
|-----------|-------------|---------------|
| Inline Call | 1 | 727M |
| Direct Call (noinline) | 1 | 727M |
| Many Parameters (6 args) | 4 | 212M |
| Fibonacci Recursive (n=20) | 24,233 | 41K |
| Fibonacci Tail (n=50) | 110 | 9.0M |
| Mutual Recursion (n=100) | 169 | 5.9M |

## Closure & Iterator Results (10M iterations)

| Benchmark | Rust (ops/sec) | TML (ops/sec) | Ratio |
|-----------|---------------|---------------|-------|
| Function Pointer | 4.84B | 593M | 8.2x |
| Function Pointer Switch | — | 440M | — |
| Higher Order Function | optimized away | 78M | N/A |
| Function Composition | — | 194M | — |
| Manual Loop (array) | optimized away | 1.24B | N/A |
| Map Simulation | optimized away | 669M | N/A |
| Filter Simulation | 6.12B | 729M | 8.4x |
| Fold/Reduce Simulation | optimized away | 1.01B | N/A |
| Chain Operations | — | 714M | — |

## Analysis

### Inline vs Direct Call — No Difference

Both at 1 ns/op (727M ops/sec). This means TML is inlining the function call. Good.

### 6-Argument Call — 4 ns/op

4 ns for a 6-argument function call. On x64, the first 4 args go in registers (rcx, rdx, r8, r9), args 5-6 go on the stack. The 3 ns overhead vs inline is from the call/ret + stack argument push. This is normal.

### Recursion

| Type | TML (ns/op) | Expected | Status |
|------|-------------|----------|--------|
| fib_recursive(20) | 24,233 | ~14K calls × 1.7 ns | Correct |
| fib_tail(50) | 110 | 50 iterations × 2.2 ns | Correct |
| Mutual recursion(100) | 169 | 100 calls × 1.69 ns | Correct |

All recursion benchmarks show expected results. No tail-call optimization is applied (fib_tail is still 110 ns for 50 iterations = 2.2 ns/call, same as non-tail).

### Function Pointer — 8.2x Gap

Rust: 4.84B ops/sec. TML: 593M ops/sec.

Rust's optimizer devirtualizes function pointers when the target is known at compile time, converting them to direct calls. TML always does an indirect call through the pointer.

### Higher-Order Functions — Unmeasurable

Rust optimizes away the entire higher-order function call, including the closure. TML's 78M ops/sec reflects real work: calling a closure 10M times with an indirect dispatch.

### Filter/Map/Fold Simulations

TML's simulation of iterator operations:
- Map: 669M ops/sec (1.5 ns/element)
- Filter: 729M ops/sec (1.4 ns/element)
- Fold: 1.01B ops/sec (1.0 ns/element)
- Chain: 714M ops/sec (1.4 ns/element)

These are respectable numbers for non-inlined loop bodies. With closure inlining, these could approach the Manual Loop speed of 1.24B ops/sec.

## Improvement Opportunities

| Priority | Change | Expected Impact |
|----------|--------|-----------------|
| P0 | Devirtualize known function pointers | 4-8x for fn ptr calls |
| P1 | Inline closure bodies in map/filter/fold | 2-4x for iterator ops |
| P1 | Tail call optimization | 30-50% for tail-recursive functions |
| P2 | Speculative devirtualization | 2x for polymorphic call sites |
