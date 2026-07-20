# phase0k — Borrowing iterators + `ref`-returning container reads

> Filed 2026-07-20 from `docs/analysis/language-deep-review/` findings
> L-084/L-028/L-088 (05-stdlib-implementation.md, 02-memory-model-borrow.md).
> Deletes a whole class of hidden O(n) allocations and the per-read clone tax;
> resolves the F-016 asymmetry family at the root. Depends on phase0b (payload
> drops must work first); phase0a's enforced moves make the contracts checkable.

## Motivation

Iterators are eager clone-snapshots: `Deque::iter` deep-copies every element
into a fresh List before yielding anything (root cause per its own comment:
`List` is move-only, so a borrowing iterator would double-free);
`BTreeMapIter` copies both backing Lists; `HashMap::keys/values` materialize
whole Lists. The default accessor clone-reads (`ptr_read_clone`) on every
`get`. Return-value ownership is then *guessed* from initializer syntax in
codegen (`llvm_ir_gen_stmt_let.cpp:1258-1266`), and `HashMap::get` returns an
unsound `0 as V` on miss.

## 1. Implementation
- [ ] 1.1 Give `List` a sound borrowing cursor (or a `Duplicate` impl where
  deep copy is genuinely wanted) so iteration never snapshots; convert
  `Deque::iter` (`deque.tml:344-355`), `BTreeMapIter`
  (`btreemap.tml:339,377-378`) and `HashMap::keys/values`
  (`hashmap.tml:551-575`) to borrow.
- [ ] 1.2 One read policy across collections, recorded in the proposal:
  `get(i) -> Maybe[ref T]` (or checked panic) for List/Deque/HashMap/Buffer;
  delete every `0 as V` miss-sentinel path (`hashmap.tml:234,250,259` + 2
  more).
- [ ] 1.3 By-value `get` requires `T: Duplicate`; `get_ref`/`get_mut`/
  `value_ref` become the default idiom at lib call sites.
- [ ] 1.4 Delete the initializer-shape drop-suppression heuristic
  (`compiler/src/codegen/llvm/llvm_ir_gen_stmt_let.cpp:1258-1266`) once reads
  are ref-based — drops become unconditional and symmetric. Run the full
  phase44b corpus (std/collections standalone 20/20) as the regression gate.
- [ ] 1.5 `Deque` backing store without sentinel pre-fill
  (`deque.tml:81-88,206-230`): raw uninitialized buffer or `Maybe[T]` slots;
  one length owner (drop the double-counter, same for ArrayList).
- [ ] 1.6 Alloc-count gate: iterating a 10k-element Deque/BTreeMap performs
  zero allocations after the change.

## 2. Tail (docs + tests — check or waive with tailWaiver)
- [ ] 2.1 Update documentation (iteration semantics; the unified read policy;
  migration notes for by-value get)
- [ ] 2.2 Write tests covering the new behavior (borrowing-iterator lifetime
  fixtures incl. B009-style invalidation; miss-policy fixtures; Deque ring
  behavior without pre-fill)
- [ ] 2.3 Run tests and confirm they pass
