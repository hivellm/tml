---
name: stdlib-essentials Phase 2.6 Random
description: What was implemented and what is blocked for stdlib random Phase 2.6
type: project
---

## Phase 2.6 Implementation (2026-03-15)

Added to `lib/std/src/random.tml`:

1. `behavior Distribution[T]` with `func sample(this, rng: mut ref Rng) -> T`
   - `impl Distribution[I64] for Uniform`
   - `impl Distribution[Bool] for Bernoulli`

2. `behavior Random` with `func random(rng: mut ref Rng) -> Self`
   - `impl Random for I64/I32/Bool/F64`
   - Direct calls like `I64::random(mut ref rng)` work fine

3. `pub func random[T: Random]() -> T` — ADDED but blocked
   - Compiler generates `@tml_s0_T_random` (unresolved generic) instead of monomorphizing
   - Same "generic T::method dispatch" codegen bug as ~140 other functions

4. `pub func choose_i64/choose_i32/choose_str(list, rng) -> Maybe[T]`
   - Works for concrete types
   - Generic `choose[T]` not implemented — same generic dispatch limitation

Tests added (4 new files): `distribution_behavior.test.tml`, `random_behavior.test.tml`,
`choose.test.tml`, `random_generic.test.tml` (placeholder for blocked `random[T]()`)

**Why:** `random[T: Random]()` uses `T::random(mut ref rng)` inside a generic function — this
is blocked by the codegen bug where `T::method()` dispatch in a generic function body
emits the unresolved generic symbol name instead of monomorphizing per call site.

**How to apply:** When implementing generic functions that call behavior methods via `T::method()`,
this codegen path is blocked. Use concrete-type functions as workaround.
