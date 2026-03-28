# Tasks: Iterator Extra Methods

**Status**: Partially Complete (3/8)
**Priority**: MEDIUM
**Phase**: 7 — Rust Parity

## Phase 1: Core Missing Adapters

- [ ] 1.1 `max(this) -> Maybe[T]` where T: Ord — BLOCKED: `.cmp()` on generic `This::Item` in default behavior method causes infinite loop/crash in legacy codegen. Compiler crashes when emitting IR.
- [ ] 1.2 `min(this) -> Maybe[T]` where T: Ord — BLOCKED: same codegen bug as max
- [ ] 1.3 `max_by_key(this, f: func(ref T) -> K) -> Maybe[T]` — BLOCKED: extra generic param `[K: Ord]` on behavior default method fails monomorphization ("undefined value @tml_s0_Counter_max_by_key")
- [ ] 1.4 `min_by_key(this, f: func(ref T) -> K) -> Maybe[T]` — BLOCKED: same as max_by_key
- [ ] 1.5 `partition(this, pred: func(ref T) -> Bool) -> (List[T], List[T])` — BLOCKED: core→std dependency (List is in std), needs cross-module generic return
- [ ] 1.6 `unzip[A,B](this) -> (List[A], List[B])` where T = (A,B) — BLOCKED: same as partition + tuple iteration codegen
- [x] 1.7 `is_sorted(this) -> Bool` where T: Ord — DONE: uses `.cmp()` via `when` pattern, works because returns scalar Bool
- [x] 1.8 Tests: is_sorted (5 tests in iter_extras.test.tml — ascending, descending, empty, single, equal)

## Blocker Summary

The root cause for items 1.1-1.4 is a legacy codegen limitation: calling `.cmp()` on generic associated types (`This::Item`) inside Iterator behavior default methods that return `Maybe[This::Item]` either causes infinite loops (timeout) or crashes the compiler. Methods returning scalar types (Bool) work fine. The `max_by`/`min_by` methods (which take a `compare: func(...)` parameter) work because the comparison is done by a concrete closure, not by generic method dispatch.

Items 1.5-1.6 require `List[T]` from `std::collections` as return type in `core::iter`, creating a circular dependency.
