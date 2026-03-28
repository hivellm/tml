# Tasks: Iterator Extra Methods

**Status**: Proposed
**Priority**: MEDIUM
**Phase**: 7 — Rust Parity

## Phase 1: Core Missing Adapters

- [ ] 1.1 `max(this) -> Maybe[T]` where T: Ord — find maximum element
- [ ] 1.2 `min(this) -> Maybe[T]` where T: Ord — find minimum element
- [ ] 1.3 `max_by_key(this, f: func(ref T) -> K) -> Maybe[T]` — max by key function
- [ ] 1.4 `min_by_key(this, f: func(ref T) -> K) -> Maybe[T]` — min by key function
- [ ] 1.5 `partition(this, pred: func(ref T) -> Bool) -> (List[T], List[T])` — split by predicate
- [ ] 1.6 `unzip[A,B](this) -> (List[A], List[B])` where T = (A,B)
- [ ] 1.7 `is_sorted(this) -> Bool` where T: Ord — check sorted order
- [ ] 1.8 Tests: max, min, max_by_key, min_by_key, partition, unzip, is_sorted
