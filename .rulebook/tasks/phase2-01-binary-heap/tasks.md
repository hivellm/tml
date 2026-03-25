# Tasks: BinaryHeap[T] — Priority Queue

**Status**: COMPLETE (all phases done)
**Priority**: HIGH
**Phase**: 2 — Stdlib Completeness

## Phase 1: Core Implementation — DONE

- [x] 1.1 Implement `BinaryHeap[T]` struct — max-heap backed by `List[T]`
- [x] 1.2 `BinaryHeap::new() -> BinaryHeap[T]` — empty heap
- [x] 1.3 `BinaryHeap::with_capacity(cap: I64) -> BinaryHeap[T]`
- [x] 1.4 `push(mut this, item: T)` — insert + sift up. O(log n)
- [x] 1.5 `pop(mut this) -> Maybe[T]` — remove max + sift down. O(log n)
- [x] 1.6 `peek(this) -> Maybe[T]` — view max. O(1)
- [x] 1.7 `len(this) -> I64` and `is_empty(this) -> Bool`
- [x] 1.8 `clear(mut this)` — remove all elements
- [x] 1.9 Internal: `sift_up` and `sift_down`
- [x] 1.10 Tests: 6 tests (empty, push/peek, max-ordering, pop empty, clear, duplicates)

## Phase 2: Advanced Operations — DONE

- [x] 2.1 `from_items(ref List[T])` — build heap from list via repeated push
- [x] 2.2 `into_sorted()` — heap sort, returns ascending List[T]
- [x] 2.3 `contains(item: T) -> Bool` — O(n) linear scan
- [x] 2.4 `extend(ref List[T])` — push multiple items
- [x] 2.5 Tests: from_items, into_sorted, contains — all passing

## Phase 3: MinHeap Variant — DONE

- [x] 3.1 `MinHeap[T]` struct with same API as BinaryHeap
- [x] 3.2 Sift up/down with `<` comparison (min at root)
- [x] 3.3 `peek`, `push`, `pop`, `contains`, `clear`
- [x] 3.4 Tests: min_heap_basic, min_heap_peek, min_heap_contains — all passing
- [x] 3.5 Total: 12 tests (6 original + 6 new)
