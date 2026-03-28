# Tasks: List[T] Completeness — Rust Vec Parity

**Status**: Proposed
**Priority**: CRITICAL
**Phase**: 7 — Rust Parity

## Phase 1: Critical Methods (blocks common patterns)

- [ ] 1.1 `contains(this, value: T) -> Bool` — linear scan for element
- [ ] 1.2 `insert(this, index: I64, value: T)` — insert at index, shift right
- [ ] 1.3 `remove(this, index: I64) -> T` — remove at index, shift left
- [ ] 1.4 `reverse(this)` — reverse in place
- [ ] 1.5 `swap(this, i: I64, j: I64)` — swap two elements
- [ ] 1.6 `sort(this)` — in-place sort (quicksort or mergesort via get/set/swap)
- [ ] 1.7 `sort_by(this, cmp: func(ref T, ref T) -> I32)` — sort with comparator
- [ ] 1.8 Tests: contains, insert, remove, reverse, swap, sort, sort_by

## Phase 2: High Priority Methods

- [ ] 2.1 `swap_remove(this, index: I64) -> T` — O(1) remove by swapping with last
- [ ] 2.2 `binary_search(this, value: T) -> Outcome[I64, I64]` — sorted list search
- [ ] 2.3 `iter(this) -> ListIter[T]` — iterator over elements
- [ ] 2.4 `extend(this, other: List[T])` — append all elements from another list
- [ ] 2.5 `index_of(this, value: T) -> Maybe[I64]` — find first index of value
- [ ] 2.6 Tests: swap_remove, binary_search, iter, extend, index_of

## Phase 3: Medium Priority Methods

- [ ] 3.1 `reserve(this, additional: I64)` — ensure capacity
- [ ] 3.2 `shrink_to_fit(this)` — reduce allocation
- [ ] 3.3 `truncate(this, len: I64)` — shorten to N elements
- [ ] 3.4 `dedup(this)` — remove consecutive duplicates
- [ ] 3.5 `windows(this, size: I64) -> WindowsIter[T]` — overlapping windows
- [ ] 3.6 `chunks(this, size: I64) -> ChunksIter[T]` — non-overlapping chunks
- [ ] 3.7 `split_at(this, mid: I64) -> (List[T], List[T])` — split into two
- [ ] 3.8 `resize(this, new_len: I64, value: T)` — grow/shrink with fill
- [ ] 3.9 `fill(this, value: T)` — set all elements
- [ ] 3.10 `sort_by_key(this, key: func(ref T) -> K)` — sort by key function
- [ ] 3.11 Tests: all Phase 3 methods
