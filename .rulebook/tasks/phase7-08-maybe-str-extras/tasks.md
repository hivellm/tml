# Tasks: Maybe[T] Extra Methods

**Status**: Proposed
**Priority**: MEDIUM
**Phase**: 7 — Rust Parity

## Phase 1: Maybe methods
- [ ] 1.1 `is_just_and(this, pred: func(ref T) -> Bool) -> Bool`
- [ ] 1.2 `get_or_insert(mut this, value: T) -> ref T`
- [ ] 1.3 `get_or_insert_with(mut this, f: func() -> T) -> ref T`
- [ ] 1.4 `replace(mut this, value: T) -> Maybe[T]`
- [ ] 1.5 `unzip[A,B](this) -> (Maybe[A], Maybe[B])` where T = (A,B)
- [ ] 1.6 Tests
