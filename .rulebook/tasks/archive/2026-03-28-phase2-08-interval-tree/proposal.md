# Proposal: IntervalTree[T, V] — Range Query Data Structure

## Status: PROPOSED

## Summary

An augmented binary search tree that stores intervals `[start, end]` and efficiently answers "which stored intervals overlap a given point or range?" queries in O(log n + k) where k is the number of results. Each node augments the BST key with the maximum endpoint in its subtree, enabling early branch pruning. A companion `Interval[T]` struct is also provided.

## Motivation

Interval overlap queries cannot be answered efficiently with a plain BST or HashMap. A naive scan is O(n) per query. Interval trees reduce this to O(log n + k), which matters when n is large (thousands of rules, millions of genomic regions, large event calendars).

Applications: memory allocators tracking free blocks, calendar systems detecting conflicting appointments, database query planners with range indices, network packet classifiers matching IP ranges, genomics tools finding overlapping gene annotations, and game engines detecting overlapping hitboxes.

TML has no range-query data structure. Any TML code performing interval overlap detection today does O(n) scans.

## Design

`IntervalTree[T, V]` is an augmented BST where:
- The BST key is `interval.start`
- Each node stores the maximum `interval.end` in its entire subtree (the "augmentation")
- On insert, the max-end augmentation is updated up the insertion path

This augmentation enables overlap queries: when searching for intervals overlapping `[query_start, query_end]`, a subtree can be skipped entirely if its max-end < query_start (no interval in the subtree can overlap the query).

The tree is self-balancing (red-black or AVL) to maintain O(log n) insert/delete. Red-black is preferred because it has fewer rotations on average.

`Interval[T]` is a simple struct `{ start: T, end: T }` where `T: Ord`. The invariant `start <= end` is enforced in the constructor.

`query_point(point)` returns all intervals where `start <= point <= end`. `query_range(range)` returns all intervals that overlap `range` (i.e., `start <= range.end and end >= range.start`).

## What Changes

- New: `lib/std/src/collections/interval_tree.tml` — Interval[T], IntervalTree[T,V], IntervalIter
- Modified: `lib/std/src/collections/mod.tml` — export IntervalTree and Interval
- New: `lib/std/tests/collections/interval_tree_basic.test.tml`
- New: `lib/std/tests/collections/interval_tree_query.test.tml`

## Dependencies

- Depends on: `List[T]`, `Maybe[T]`, `Ord` from core; `HashMap` for iterator support
- Enables: efficient scheduling and range-query applications in user code

## Risks

- Red-black tree rebalancing is complex to implement correctly; the test suite must include stress tests (insert/delete/query 10K intervals) to catch subtle rotation bugs
- `remove` on an interval tree requires re-computing the max-end augmentation on the path from the deleted node to the root; this must be done correctly or queries will return wrong results
- Intervals with equal `start` values must be handled: the BST must allow multiple nodes with the same key (use a `List[V]` per BST node, or accept duplicate keys in the tree)
