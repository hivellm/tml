# Tasks: Collections Generics + Missing Methods

**Status**: Proposed
**Priority**: MEDIUM
**Phase**: 7 — Rust Parity

## Phase 1: BTreeMap/BTreeSet Generics

- [ ] 1.1 Make BTreeMap generic `[K: Ord, V]` (currently I64-only)
- [ ] 1.2 Add `keys() -> List[K]` and `values() -> List[V]`
- [ ] 1.3 Add `get_mut(key: K) -> Maybe[mut ref V]`
- [ ] 1.4 Add `range(from: K, to: K) -> BTreeIter[K,V]` iterator
- [ ] 1.5 Make BTreeSet generic `[T: Ord]` (currently I64-only)
- [ ] 1.6 Add `range(from: T, to: T) -> BTreeSetIter[T]`
- [ ] 1.7 Tests

## Phase 2: Deque Extras

- [ ] 2.1 `len(this) -> I64` — alias for `count` (Rust convention)
- [ ] 2.2 `insert(this, index: I64, value: T)` — insert at arbitrary position
- [ ] 2.3 `remove(this, index: I64) -> Maybe[T]` — remove at arbitrary position
- [ ] 2.4 `iter(this) -> DequeIter[T]` — forward iterator
- [ ] 2.5 `retain(this, pred: func(ref T) -> Bool)` — keep matching elements
- [ ] 2.6 `drain(this) -> List[T]` — remove and return all
- [ ] 2.7 `rotate_left(this, n: I64)` / `rotate_right(this, n: I64)`
- [ ] 2.8 `binary_search(this, value: T) -> Outcome[I64, I64]`
- [ ] 2.9 Tests

## Phase 3: BinaryHeap Extras

- [ ] 3.1 `iter(this) -> HeapIter[T]` — iterate without consuming
- [ ] 3.2 `drain(this) -> List[T]` — drain all elements
- [ ] 3.3 `append(this, other: BinaryHeap[T])` — merge heaps
- [ ] 3.4 `retain(this, pred: func(ref T) -> Bool)` — keep matching
- [ ] 3.5 Tests
