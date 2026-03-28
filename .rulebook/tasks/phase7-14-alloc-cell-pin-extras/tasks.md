# Tasks: Alloc, Cell, Pin Extras

**Status**: Proposed
**Priority**: MEDIUM
**Phase**: 7 — Rust Parity

## Phase 1: Heap[T] Extras

- [ ] 1.1 `as_ref(this) -> ref T` — borrow inner value
- [ ] 1.2 `as_mut(mut this) -> mut ref T` — mutably borrow inner value
- [ ] 1.3 `into_pin(this) -> Pin[Heap[T]]` — convert to pinned box
- [ ] 1.4 Tests

## Phase 2: Weak[T] for Shared/Sync

- [ ] 2.1 `Weak[T]` type for `Shared[T]` — weak reference that doesn't prevent deallocation
- [ ] 2.2 `Shared::downgrade(this) -> Weak[T]`
- [ ] 2.3 `Weak::upgrade(this) -> Maybe[Shared[T]]`
- [ ] 2.4 `Shared::weak_count(this) -> I64`
- [ ] 2.5 `Shared::ptr_eq(this, other: ref Shared[T]) -> Bool`
- [ ] 2.6 `Shared::make_mut(mut this) -> mut ref T` — clone-on-write
- [ ] 2.7 Repeat 2.1-2.6 for `Sync[T]` (atomic Weak)
- [ ] 2.8 Tests: downgrade/upgrade cycle, weak_count, expired weak

## Phase 3: Cell Extras

- [ ] 3.1 `Cell::update(mut this, f: func(T) -> T)` — apply function in-place
- [ ] 3.2 `RefCell::into_inner(this) -> T` — consume cell
- [ ] 3.3 `RefCell::replace_with(mut this, f: func(mut ref T) -> T)` — replace via closure
- [ ] 3.4 `RefCell::swap(mut this, other: mut ref RefCell[T])` — swap contents
- [ ] 3.5 `OnceCell::get_or_init(mut this, f: func() -> T) -> ref T` — lazy init
- [ ] 3.6 `OnceCell::get_or_try_init(mut this, f: func() -> Outcome[T,E]) -> Outcome[ref T, E]`
- [ ] 3.7 `OnceCell::get_mut(mut this) -> Maybe[mut ref T]`
- [ ] 3.8 Tests

## Phase 4: Pin Extras

- [ ] 4.1 `Pin::into_inner(this) -> P` where T: Unpin — safe unwrap
- [ ] 4.2 `Pin::as_ref(this) -> Pin[ref T]` — reborrow as immutable
- [ ] 4.3 `Pin::as_mut(mut this) -> Pin[mut ref T]` — reborrow as mutable
- [ ] 4.4 `Pin::set(mut this, value: T)` where T: Unpin — replace value
- [ ] 4.5 Tests
