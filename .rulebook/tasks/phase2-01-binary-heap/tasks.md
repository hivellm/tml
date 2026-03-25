# Tasks: BinaryHeap[T] — Priority Queue

**Status**: Proposed
**Priority**: HIGH
**Phase**: 2 — Stdlib Completeness

## Motivation

Priority queues are a fundamental data structure used in schedulers, Dijkstra's algorithm, timer wheels, task queues, and event processing. Rust includes `BinaryHeap<T>` in `alloc::collections`. TML has no heap-ordered collection.

## Phase 1: Core Implementation (`lib/std/src/collections/binary_heap.tml`)

- [ ] 1.1 Implement `BinaryHeap[T]` struct — max-heap backed by dynamic array
- [ ] 1.2 `BinaryHeap::new() -> BinaryHeap[T]` — empty heap
- [ ] 1.3 `BinaryHeap::with_capacity(cap: I64) -> BinaryHeap[T]`
- [ ] 1.4 `push(mut this, item: T)` — insert element, sift up. O(log n)
- [ ] 1.5 `pop(mut this) -> Maybe[T]` — remove max element, sift down. O(log n)
- [ ] 1.6 `peek(this) -> Maybe[ref T]` — view max without removing. O(1)
- [ ] 1.7 `len(this) -> I64` and `is_empty(this) -> Bool`
- [ ] 1.8 `clear(mut this)` — remove all elements
- [ ] 1.9 Internal: `sift_up(mut this, index: I64)` and `sift_down(mut this, index: I64)`
- [ ] 1.10 Write tests: push/pop ordering, empty heap, single element

## Phase 2: Advanced Operations

- [ ] 2.1 `BinaryHeap::from_list(items: List[T]) -> BinaryHeap[T]` — heapify in O(n)
- [ ] 2.2 `into_sorted_list(this) -> List[T]` — heap sort, O(n log n)
- [ ] 2.3 `drain(mut this) -> BinaryHeapDrain[T]` — iterator that drains elements in order
- [ ] 2.4 `retain(mut this, f: func(ref T) -> Bool)` — keep elements matching predicate
- [ ] 2.5 `capacity(this) -> I64` and `reserve(mut this, additional: I64)`
- [ ] 2.6 Implement `Iterator` for `BinaryHeap[T]` (unordered iteration)
- [ ] 2.7 Implement `IntoIterator` for `BinaryHeap[T]`
- [ ] 2.8 Implement `Display`, `Debug`, `Clone`, `Default`, `Drop`
- [ ] 2.9 Write tests: heapify, sorted output, drain, retain, large dataset (10K elements)

## Phase 3: MinHeap Variant

- [ ] 3.1 Implement `MinHeap[T]` — convenience wrapper that reverses comparison
- [ ] 3.2 OR: implement `Reverse[T]` wrapper type (like Rust's `std::cmp::Reverse`) that inverts Ord
- [ ] 3.3 Write tests: min-heap ordering, Dijkstra-style usage pattern
- [ ] 3.4 Update `collections/mod.tml` to export BinaryHeap and MinHeap/Reverse
- [ ] 3.5 Run full collections test suite
