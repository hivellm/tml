# Tasks: Prelude, Bool Methods, Unit Impls

**Status**: Proposed
**Priority**: MEDIUM
**Phase**: 7 — Rust Parity

## Phase 1: Bool Methods (pure TML, no compiler change)

- [ ] 1.1 `Bool::then[T](this, f: func() -> T) -> Maybe[T]` — if true, return Just(f()), else Nothing
- [ ] 1.2 `Bool::then_some[T](this, value: T) -> Maybe[T]` — if true, return Just(value), else Nothing
- [ ] 1.3 Tests: then, then_some with true/false

## Phase 2: Unit Impls (pure TML)

- [ ] 2.1 `impl Display for Unit` — displays "()"
- [ ] 2.2 `impl Default for Unit` — returns ()
- [ ] 2.3 `impl Debug for Unit` — displays "()"
- [ ] 2.4 Tests: Display, Default, Debug for Unit

## Phase 3: Prelude (compiler change required)

- [ ] 3.1 Create `lib/core/src/prelude.tml` with re-exports
- [ ] 3.2 Re-export: Maybe, Just, Nothing, Outcome, Ok, Err, Str, Bool, I32, I64, F64
- [ ] 3.3 Compiler: auto-inject `use core::prelude::*` for every module
- [ ] 3.4 Verify existing code still compiles (no import conflicts)
- [ ] 3.5 Tests: module compiles without explicit use for prelude types
