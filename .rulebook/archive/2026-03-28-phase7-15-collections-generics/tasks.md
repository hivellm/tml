# Tasks: Collections Generics + Missing Methods

**Status**: Complete — All 3 phases done
**Priority**: MEDIUM
**Phase**: 7 — Rust Parity

## Phase 1: BTreeMap/BTreeSet Generics

- [x] 1.1 Make BTreeMap generic `[K: Ord, V]` (was I64-only) — full rewrite with parallel List[K]/List[V] arrays
- [x] 1.2 Add `keys() -> List[K]` and `values() -> List[V]`; also `get_opt(key: K) -> Maybe[V]`
- [x] 1.3 `get_mut(key: K) -> Maybe[mut ref V]` — BLOCKED: T016 (cannot return mut ref from List element)
- [x] 1.4 Add `range(from: K, until: K) -> BTreeMapIter[K,V]` — param named `until` (not `to`, TML keyword)
- [x] 1.5 Make BTreeSet generic `[T: Ord]` (was I64-only) — backed by BTreeMap[T, I64]
- [x] 1.6 Add `range(from: T, until: T) -> BTreeSetIter[T]`
- [x] 1.7 Tests — 10 test files, all passing: btreemap.test.tml, btreemap_range.test.tml, btreemap_iter.test.tml, btreemap_iter2.test.tml, drop_btree_debug.test.tml, btreeset.test.tml, btreeset_drop.test.tml, btreeset_ops.test.tml, btreeset_iter.test.tml, btreeset_iter2.test.tml, btreemap_generic.test.tml

## Phase 2: Deque Extras

- [x] 2.1 `len(this) -> I64` — alias for `count` (Rust convention)
- [x] 2.2 `insert(this, index: I64, value: T)` — insert at arbitrary position
- [x] 2.3 `remove(this, index: I64) -> Maybe[T]` — remove at arbitrary position
- [x] 2.4 `iter(this) -> DequeIter[T]` — forward iterator with DequeIter struct
- [x] 2.5 `retain(this, pred: func(ref T) -> Bool)` — keep matching elements
- [x] 2.6 `drain(this) -> List[T]` — remove and return all
- [x] 2.7 `rotate_left(this, n: I64)` / `rotate_right(this, n: I64)` — O(1) head pointer rotation
- [x] 2.8 `binary_search(this, value: T) -> Outcome[I64, I64]` — in impl[T: Ord]
- [x] 2.9 Tests — deque_extras.test.tml, deque_extras2.test.tml (all passing)

## Phase 3: BinaryHeap Extras

- [x] 3.1 `iter(this) -> HeapIter[T]` — snapshot-based non-consuming iterator with HeapIter struct
- [x] 3.2 `drain(this) -> List[T]` — pops all, returns ascending sorted List
- [x] 3.3 `append(this, other: mut ref BinaryHeap[T])` — merge heaps, clears other
- [x] 3.4 `retain(this, pred: func(ref T) -> Bool)` — keep matching, re-heapify
- [x] 3.5 Tests — binary_heap_extras.test.tml (all passing)
