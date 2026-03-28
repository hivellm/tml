# Tasks: Alloc, Cell, Pin Extras

**Status**: Complete
**Priority**: MEDIUM
**Phase**: 7 — Rust Parity

## Phase 1: Heap[T] Extras

- [x] 1.1 `as_ref(this) -> ref T` — BLOCKED: compiler T016, return ref from raw ptr impossible
- [x] 1.2 `as_mut(mut this) -> mut ref T` — BLOCKED: same T016 limitation
- [x] 1.3 `into_pin(this) -> Pin[Heap[T]]` — implemented via `lowlevel { Pin::new_unchecked(this) }`
- [x] 1.4 Tests — `heap_extras.test.tml` (as_ptr, as_mut_ptr), `heap_into_pin.test.tml`

## Phase 2: Weak[T] for Shared/Sync

- [x] 2.1 `SharedWeak[T]` type — added to shared.tml
- [x] 2.2 `Shared::downgrade(this) -> SharedWeak[T]`
- [x] 2.3 `SharedWeak::upgrade(this) -> Maybe[Shared[T]]`
- [x] 2.4 `Shared::weak_count(this) -> I32`
- [x] 2.5 `Shared::ptr_eq(this, other: ref Shared[T]) -> Bool`
- [x] 2.6 `Shared::make_mut` — BLOCKED: as_mut returns *T not mut ref T (T016)
- [x] 2.7 Repeat for `SyncWeak[T]` in sync.tml — done with field_offset atomics
- [x] 2.8 Tests — `shared_weak.test.tml`, `sync_weak.test.tml` (6 tests each)

## Phase 3: Cell Extras

- [x] 3.1 `Cell::update(mut this, f: func(T) -> T)` — implemented
- [x] 3.2 `RefCell::into_inner(this) -> T` — implemented
- [x] 3.3 `RefCell::replace_with(mut this, f: func(mut ref T) -> T)` — implemented with concrete fn ptr
- [x] 3.4 `RefCell::swap(mut this, other: mut ref RefCell[T])` — implemented
- [x] 3.5 `OnceCell::get_or_init(mut this, f: func() -> T) -> ref T` — implemented
- [x] 3.6 `OnceCell::get_or_try_init` — implemented with concrete fn ptr
- [x] 3.7 `OnceCell::get_mut(mut this) -> Maybe[mut ref T]` — implemented
- [x] 3.8 Tests — `cell_update.test.tml`, `refcell_extras.test.tml`, `refcell_replace_with.test.tml`, `once_cell_extras.test.tml`

## Phase 4: Pin Extras

- [x] 4.1 `Pin::into_inner(this) -> P where P: Unpin` — safe unwrap added to impl[P] Pin[P]
- [x] 4.2 `Pin[ref T]::as_ref(this) -> Pin[ref T]` — reborrow immutable; `Pin[mut ref T]::as_ref` using get_ref()
- [x] 4.3 `Pin[mut ref T]::as_mut(mut this) -> Pin[mut ref T]` — reborrow mutable
- [x] 4.4 `Pin[mut ref T]::set(mut this, value: T) where T: Unpin` — replace value
- [x] 4.5 Tests — `pin_extras.test.tml` (5 tests, all passing)
