# Proposal: BinaryHeap[T] — Priority Queue

## Status: PROPOSED

## Summary

A max-heap priority queue backed by a dynamic array, matching the interface of Rust's `alloc::collections::BinaryHeap<T>`. Elements are retrieved in largest-first order in O(log n). The task includes a `Reverse[T]` wrapper type that inverts the comparison order, enabling min-heaps without a separate implementation.

## Motivation

Priority queues underpin a large fraction of real-world algorithms: Dijkstra's shortest path, A* search, Huffman coding, task schedulers, timer wheels, and event-driven simulations. TML has `List`, `HashMap`, and `BTreeMap` but no heap-ordered collection. Implementing Dijkstra in TML today requires sorting the entire candidate list every iteration — O(n log n) per step instead of O(log n).

The HTTP server's work-stealing scheduler and the async executor's timer wheel are both places where a min-heap would improve performance.

## Design

`BinaryHeap[T]` is a struct wrapping a `List[T]` as the backing array. The heap invariant (parent >= children) is maintained by two private operations: `sift_up` after `push`, and `sift_down` after `pop`. Both are O(log n).

The type constraint is `T: Ord` (total order). The `Reverse[T]` wrapper type implements `Ord` by delegating to the inner type's comparison with the result flipped — `Reverse[T]` where T: Ord gives a min-heap when used with `BinaryHeap`.

`from_list` uses Floyd's bottom-up heapify algorithm (O(n), not O(n log n)) for efficient bulk construction. `into_sorted_list` performs heap sort by repeatedly popping.

`BinaryHeapDrain` is an iterator struct that pops elements in order, destructively consuming the heap.

## What Changes

- New: `lib/std/src/collections/binary_heap.tml` — BinaryHeap[T], BinaryHeapDrain[T]
- New: `lib/std/src/collections/reverse.tml` — Reverse[T] with inverted Ord
- Modified: `lib/std/src/collections/mod.tml` — export BinaryHeap and Reverse
- New: `lib/std/tests/collections/binary_heap_basic.test.tml`
- New: `lib/std/tests/collections/binary_heap_advanced.test.tml`
- New: `lib/std/tests/collections/binary_heap_minheap.test.tml`

## Dependencies

- Depends on: `List[T]`, `Ord` behavior from core
- Enables: efficient priority-based scheduling in the async executor and HTTP timer wheel
- Enables: algorithm implementations (Dijkstra, A*) in application code

## Risks

- The `Ord` constraint means `BinaryHeap[F64]` is impossible (floating-point has no total order); this matches Rust's design and must be documented
- `sift_down` during `pop` requires swapping the root with the last element then shrinking — the implementation must correctly handle the single-element edge case
