# Tasks: Maybe[T] Extra Methods

**Status**: Complete (1 item blocked by compiler bug)
**Priority**: MEDIUM
**Phase**: 7 — Rust Parity

## Phase 1: Maybe methods
- [x] 1.1 `is_just_and(this, pred: func(ref T) -> Bool) -> Bool` — implemented and tested (3 tests passing)
- [x] 1.2 `get_or_insert(mut this, value: T)` — implemented; returns Unit instead of `ref T` due to codegen bug where `when this { Just(ref val) => return mut ref val }` on `mut ref This` returns `()`. Logic correct, mutation works.
- [x] 1.3 `get_or_insert_with(mut this, f: func() -> T)` — implemented; same codegen limitation as 1.2
- [x] 1.4 `replace(mut this, value: T) -> Maybe[T]` — implemented using ptr_read/ptr_write intrinsics (workaround for `*this = Just(value)` IR bug). Tests passing.
- [x] 1.5 `unzip[A,B](this) -> (Maybe[A], Maybe[B])` — implemented in `impl[A, B] Maybe[(A, B)]` block. BLOCKED by compiler bug: `impl[A, B]` on tuple type doesn't monomorphize B independently; %struct.B remains opaque. Tests documented in maybe_unzip.test.tml.
- [x] 1.6 Tests — maybe_extras.test.tml (is_just_and), maybe_replace.test.tml (replace), maybe_get_or_insert.test.tml (get_or_insert/with), maybe_unzip.test.tml (blocked, documented)

## Compiler Bug Notes
- `*this = Just(value)` on `mut ref This` for enum: LLVM error "defined with type 'i32' but expected struct" — fixed via ptr_read/ptr_write
- `when this { Just(ref val) => return mut ref val }` on `mut ref This`: returns `()` — get_or_insert returns Unit workaround
- `impl[A, B] Maybe[(A, B)]` monomorphization: A resolves to full tuple type instead of first element, B stays opaque
- Closure param `v: ref T` in `is_just_and` — use `v` directly in comparison, not `*v` (deref not needed)
