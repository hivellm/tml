# Proposal: phase0k_borrowing-iterators-ref-reads

## Why
Iterating a collection today allocates and deep-copies the entire collection
before the first element, every `get` clone-reads, ownership of returns is
guessed from syntax, and map misses return unsound zero-values (analysis
L-084, L-028, L-088). Rust yields references for free.

## What Changes
List gains a borrowing cursor; Deque/BTreeMap/HashMap iterate by borrow; one
read policy (`Maybe[ref T]`/checked) replaces the zero-sentinel; by-value get
requires Duplicate; the initializer-syntax drop heuristic is deleted; Deque
stops pre-filling capacity with clones.

## Impact
- Affected specs: stdlib collection docs
- Affected code: lib/std/src/collections/*.tml, lib/core alloc read helpers, compiler/src/codegen/llvm/llvm_ir_gen_stmt_let.cpp
- Breaking change: YES (get miss-behavior unified; by-value get gains a bound) — each break replaces silent unsoundness
- User benefit: zero-alloc iteration, no hidden clone tax, drops symmetric by construction (F-016 class closed for good)
