# Tasks: IntervalTree[T, V] — Range Query Data Structure

**Status**: Proposed
**Priority**: LOW
**Phase**: 2 — Stdlib Completeness

## Motivation

Interval trees efficiently answer "which intervals overlap this point/range?" queries in O(log n + k). Used in: memory allocators (free block tracking), schedulers (overlapping events), database query planners (range index), genomics (overlapping gene regions), and calendar systems (conflicting appointments).

## Phase 1: Core Implementation (`lib/std/src/collections/interval_tree.tml`)

- [ ] 1.1 Define `Interval[T]` struct — `{ start: T, end: T }` where T: Ord
- [ ] 1.2 Implement `IntervalTree[T, V]` — augmented BST with max-endpoint tracking
- [ ] 1.3 `IntervalTree::new() -> IntervalTree[T, V]`
- [ ] 1.4 `insert(mut this, interval: Interval[T], value: V)` — insert interval with associated value
- [ ] 1.5 `query_point(this, point: T) -> List[(Interval[T], ref V)]` — all intervals containing point
- [ ] 1.6 `query_range(this, range: Interval[T]) -> List[(Interval[T], ref V)]` — all intervals overlapping range
- [ ] 1.7 `remove(mut this, interval: Interval[T]) -> Maybe[V]` — remove interval
- [ ] 1.8 `len(this) -> I64` and `is_empty(this) -> Bool`
- [ ] 1.9 `contains(this, interval: Interval[T]) -> Bool`
- [ ] 1.10 Internal: self-balancing (red-black or AVL) to maintain O(log n) operations
- [ ] 1.11 Implement `Iterator`, `Display`, `Debug`, `Clone`, `Default`, `Drop`
- [ ] 1.12 Write tests: insert, point query, range query, remove, overlapping intervals
- [ ] 1.13 Update `collections/mod.tml` to export IntervalTree and Interval
- [ ] 1.14 Run collections test suite
