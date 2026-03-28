# Tasks: List[T] Completeness — Rust Vec Parity

**Status**: Complete — 84% (21/25 items done, 4 skipped: shrink_to_fit, windows, chunks, sort_by_key need new types/extra generics)
**Priority**: CRITICAL
**Phase**: 7 — Rust Parity

## Phase 1: Critical Methods (blocks common patterns)

- [x] 1.1 `contains(this, value: T) -> Bool` — linear scan for element
- [x] 1.2 `insert(this, index: I64, value: T)` — insert at index, shift right
- [x] 1.3 `remove(this, index: I64) -> T` — remove at index, shift left
- [x] 1.4 `reverse(this)` — reverse in place
- [x] 1.5 `swap(this, i: I64, j: I64)` — swap two elements
- [x] 1.6 `sort(this)` — in-place quicksort via get/set/swap
- [x] 1.7 `sort_by(this, cmp: func(ref T, ref T) -> I32)` — sort with comparator (closures-as-params work!)
- [x] 1.8 Tests: contains, insert, remove, reverse, swap, sort (list_phase1.test.tml + list_sort.test.tml)

## Phase 2: High Priority Methods

- [x] 2.1 `swap_remove(this, index: I64) -> T` — O(1) remove by swapping with last
- [x] 2.2 `binary_search(this, value: T) -> Outcome[I64, I64]` — sorted list search
- [x] 2.3 `iter(this) -> ListIter[T]` — already existed in behaviors.tml
- [x] 2.4 `extend(this, other: ref List[T])` — append all elements from another list
- [x] 2.5 `index_of(this, value: T) -> Maybe[I64]` — find first index of value
- [x] 2.6 Tests: binary_search, iter, extend, index_of (list_phase2b.test.tml + list_iter.test.tml)

## Phase 3: Medium Priority Methods

- [x] 3.1 `reserve(this, additional: I64)` — ensure capacity
- [ ] 3.2 `shrink_to_fit(this)` — reduce allocation (skipped: requires realloc to smaller size, low priority)
- [x] 3.3 `truncate(this, len: I64)` — shorten to N elements
- [x] 3.4 `dedup(this)` — remove consecutive duplicates
- [ ] 3.5 `windows(this, size: I64) -> WindowsIter[T]` — overlapping windows (skipped: needs new iterator type, low priority)
- [ ] 3.6 `chunks(this, size: I64) -> ChunksIter[T]` — non-overlapping chunks (skipped: needs new iterator type, low priority)
- [x] 3.7 `split_at(this, mid: I64) -> (List[T], List[T])` — split into two
- [x] 3.8 `resize(this, new_len: I64, value: T)` — grow/shrink with fill
- [x] 3.9 `fill(this, value: T)` — set all elements
- [ ] 3.10 `sort_by_key(this, key: func(ref T) -> K)` — sort by key function (skipped: needs extra generic K param)
- [x] 3.11 Tests: dedup, resize, split_at, sort_by, fill, reserve, truncate (list_phase2.test.tml + list_phase3.test.tml)

## Notes

- `sort_by` works — closures as function parameters compile and execute correctly
- `iter` was already implemented in `lib/std/src/collections/behaviors.tml`
- `index_of` implemented with `Maybe[I64]` return — works correctly
- `binary_search` implemented with `Outcome[I64, I64]` return — works correctly
- `split_at` returns tuple `(List[T], List[T])` — tuple returns with generic types work
- Phase 3 items 3.2, 3.5, 3.6, 3.10 skipped as low priority (need new iterator types or extra generics)
