# Proposal: phase0t_escape-analysis-stack-promo

## Why
TML currently allocates all `Heap::new(x)` calls on the heap via `malloc`, even for values that never escape the current function scope (e.g., a `Heap[T]` created, used, and dropped within the same function). Each `malloc` call costs ~20-50 ns + TLB/cache pressure, whereas a stack allocation (`alloca`) costs ~0 ns. Benchmarks show TML allocation-heavy code at 3-5x Rust due to unnecessary heap allocation. Rust's borrow checker guarantees that stack-allocated values don't escape, allowing it to optimize aggressively. TML cannot use borrow-checking today, but a conservative escape analysis can identify `Heap::new` calls whose result never leaves the current stack frame — and promote those to stack allocation. See `docs/analysis/benchmark/09-memory-management.md`.

## What Changes
A new MIR pass — Escape Analysis + Stack Promotion — that:
1. Scans each function for `HeapAllocInst` (the MIR instruction for `Heap::new`).
2. Traces all uses of the returned pointer: if no use stores it into a struct field, List, HashMap, or returns it from the function, the allocation does not escape.
3. For non-escaping `HeapAllocInst`s, replaces `malloc` with an `alloca` of the same size — the pointer is still used the same way, but lives on the stack and is freed automatically at function exit.
4. Escaping allocations are untouched.

## Impact
- Affected specs: codegen/heap-allocation, codegen/escape-analysis
- Affected code: new MIR pass in `compiler/src/mir/`, hooked into the MIR optimization pipeline
- Breaking change: NO (observable behavior is identical; only allocation site changes)
- User benefit: Eliminates malloc overhead for short-lived heap objects — critical for parser nodes, type inference variables, and intermediate AST nodes that are created and consumed in one function.
