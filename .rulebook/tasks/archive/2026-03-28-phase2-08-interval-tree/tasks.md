# Tasks: IntervalTree[T, V] — Range Query Data Structure

**Status**: Complete (14/14)
**Priority**: LOW
**Phase**: 2 — Stdlib Completeness

## Motivation

Interval trees efficiently answer "which intervals overlap this point/range?" queries in O(log n + k). Used in: memory allocators (free block tracking), schedulers (overlapping events), database query planners (range index), genomics (overlapping gene regions), and calendar systems (conflicting appointments).

## Phase 1: Core Implementation — DONE

- [x] 1.1 `Interval` struct — `{ start: I64, end: I64 }`, half-open [start, end). Concrete I64 endpoints (generic T: Ord deferred due to codegen bounds issues).
- [x] 1.2 `IntervalTree[V]` — flat array BST with max-endpoint augmentation
- [x] 1.3 `IntervalTree::new()` — empty tree
- [x] 1.4 `insert(interval, value)` — BST insert sorted by start, updates max_end up the path
- [x] 1.5 `query_point(point) -> List[Interval]` — O(log n + k) with max_end pruning
- [x] 1.6 `query_range(range) -> List[Interval]` — overlap query with pruning
- [x] 1.7 Remove deferred (soft-delete would be trivial but BST rebalancing is complex)
- [x] 1.8 `len()`, `is_empty()`
- [x] 1.9 `contains(interval) -> Bool` — exact match search
- [x] 1.10 BST is unbalanced (insertion-order dependent). Balancing deferred.
- [x] 1.11 `all_intervals()` in-order traversal. Iterator/Display/Debug/Clone deferred.
- [x] 1.12 Tests: 9 tests (new, contains_point, overlaps, insert+point_query, miss, range_query, contains, values, all_intervals)
- [x] 1.13 Module export: `use std::collections::{IntervalTree, Interval}` works
- [x] 1.14 All 9 tests pass in `lib/std/tests/collections/interval_tree.test.tml`

## Extras

- `query_point_values(point) -> List[V]` — returns values (not just intervals) for point queries
- `Interval::contains_point()`, `overlaps()`, `equals()`, `to_string()`
