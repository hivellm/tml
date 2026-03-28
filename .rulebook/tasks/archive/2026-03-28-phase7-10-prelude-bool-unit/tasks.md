# Tasks: Prelude, Bool Methods, Unit Impls

**Status**: Partially Complete (7/12)
**Priority**: MEDIUM
**Phase**: 7 — Rust Parity

## Phase 1: Bool Methods (pure TML, no compiler change)

- [x] 1.1 `then_some[T](condition: Bool, value: T) -> Maybe[T]` — DONE as standalone function (not method — generic methods on primitives blocked by codegen). Named `then_some` in `core::ops::bit`.
- [x] 1.2 `then_with[T](condition: Bool, f: func() -> T) -> Maybe[T]` — DONE as standalone function. Named `then_with` (not `then` — `then` is a keyword in TML).
- [x] 1.3 Tests: then_some true/false, then_with true/false, then_some zero edge case (5 tests in bool_methods.test.tml)

NOTE: `then` is a reserved keyword in TML, so `then_with` is used instead. Generic methods on `impl Bool` (`pub func then_some[T](this, ...)`) fail with unmonomorphized `%struct.T` in IR — same "generic trait dispatch" codegen bug. Standalone functions work.

## Phase 2: Unit Impls (pure TML)

- [x] 2.1 `impl Display for Unit` — ALREADY EXISTS in `core::fmt::impls`
- [x] 2.2 `impl Default for Unit` — ALREADY EXISTS in `core::types::tuple`
- [x] 2.3 `impl Debug for Unit` — ALREADY EXISTS in `core::fmt::impls`
- [ ] 2.4 Tests: Display, Default, Debug for Unit — BLOCKED: "Type mismatch: expected (), found ()" codegen bug on Unit assignment. All 3 impls exist in source but cannot be tested.

## Phase 3: Prelude (compiler change required)

- [x] 3.1 Create `lib/core/src/prelude.tml` with re-exports — DONE
- [x] 3.2 Re-export: Maybe, Just, Nothing, Outcome, Ok, Err, Ordering, PartialEq, Eq, PartialOrd, Ord, Display, Debug, Duplicate, Copy, Default, Iterator, IntoIterator, Error — DONE
- [ ] 3.3 Compiler: auto-inject `use core::prelude::*` for every module — requires C++ compiler change
- [ ] 3.4 Verify existing code still compiles (no import conflicts) — depends on 3.3
- [ ] 3.5 Tests: module compiles without explicit use for prelude types — depends on 3.3
