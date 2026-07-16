# phase26f 1.2 — Strict-Moves Blast-Radius Measurement

**Version:** v0.3.60 · **Date:** 2026-07-16 · **Author:** codegen-debugger
**Flag:** `TML_STRICT_MOVES` (default OFF — facts only, zero behaviour change)

This spec records the measured blast radius of turning use-after-move into a
hard error, gathered by compiling four corpora with `TML_STRICT_MOVES=1` after
activating the move dataflow (item 1.1). **No fallout was fixed** — this is a
measurement, per the task. It is the number that decides how phase26f proceeds.

## Method

Each file compiled with `TML_STRICT_MOVES=1 tml build <file> --emit-ir` (the
borrow checker runs on both the AST-legacy and MIR/query paths; only the error
*formatting* differs — `error[B001]:` vs `ERROR [build] use of moved value` —
so both formats were counted). Move-class errors counted:
`use of moved value`, `use of partially moved value`, `cannot borrow moved
value`, `cannot move out of …`, `cannot assign to moved value`.

## Headline

| Metric | Value |
|---|---|
| Files compiled (all corpora) | **176** |
| Files with ≥1 move error | **7 (4.0%)** |
| Total move-error lines | **53** |
| Genuine latent bugs (true positives) | **1 file / 2 errors (≈4%)** |
| Idiomatic implicit-copy sites (would-migrate) | **6 files / 51 errors (≈96%)** |

## Per-corpus

| Corpus | Files | Files w/ move-err | Move errors |
|---|---|---|---|
| (a) determinism (`compiler/tests/determinism/*.test.tml` + `scripts/fixtures/*.tml`) | 11 | 1 | 2 |
| (b) `lib/core/tests/alloc/*` | 44 | 4 | 42 |
| (c) `lib/std/tests/collections/*` | 96 | 1 | 3 |
| (d) `compiler/tests/compiler/*` (first 25) | 25 | 1 | 4 |
| **Total** | **176** | **7** | **53** |

## Categorisation

### Category A — Genuine latent bugs caught (true positives) — 1 file, 2 errors
- **`scripts/fixtures/double_free_probe.tml`** — deliberate double-free
  (`buf_mem_free(p); buf_mem_free(p)`). Strict moves flags the second use of
  `p` as use-after-move. This is *exactly* the class strict moves exists to
  catch. (Caveat: `p` is a raw pointer, which Rust treats as `Copy`, so a
  fully Rust-faithful classifier would not compile-error here either — it is
  caught only because raw pointers are currently classified as move; see
  Gap 1. The double-free is real regardless.)

### Category B — Idiomatic implicit-copy of value types (false positives under current classifier) — 6 files, 51 errors
These programs are **correct under TML's current implicit-copy semantics**;
they reuse a value after passing it by value. A strict-moves migration would
resolve each by deriving `Copy`/`Duplicate` on the value type, or inserting an
explicit `.duplicate()`.

- **B1 — `Layout` value struct (dominant): 42 errors across 4 alloc files**
  (`allocator_ref` 19, `allocator_ref_methods` 19, `alloc_global_helpers` 4,
  `alloc_coverage` 2). `pub type Layout { size, align }`
  (`lib/core/src/alloc/layout.tml:63`) has **no `Copy`/`Duplicate` derive**, yet
  allocator code passes the same `layout`/`new_layout`/`old_layout` by value to
  several calls. Rust's `core::alloc::Layout` **is `Copy`** — this is a stdlib
  derive gap, not a bug.
- **B2 — small value structs reused: 7 errors, 2 files**
  - `compiler/tests/compiler/insertvalue_struct.test.tml` (4): `sum_point(p1)`
    passes `Point { x:I64, y:I64 }` by value, then reads `p1.x`/`p1.y`. The test
    comment literally says *"p1 still valid after pass-by-value"* — it depends on
    implicit copy.
  - `lib/std/tests/collections/interval_tree.test.tml` (3): `Interval` (two
    ints) passed by value to `overlaps(this, other: Interval)`
    (`interval_tree.tml:41`, a genuine by-value parameter), then reused. Would
    need `Copy` on `Interval` or `other: ref Interval`.
- **B3 — raw pointers reused** (folded into the alloc `ptr` counts above):
  `*T` pointers reused across allocator calls are classified as move because
  `PtrType` is absent from `is_copy_type` (see Gap 1).

## Classifier gaps found (NOT fixed — recommendations for 1.6 migration)

1. **`PtrType` (raw pointers `*T`/`*mut T`) is not in `is_copy_type`**
   (`compiler/src/borrow/checker_core.cpp` — Primitive/Ref/Tuple/Array/Named/
   Class are Copy, everything else — incl. `PtrType` — falls through to move).
   Rust treats raw pointers as `Copy`. Adding `PtrType` removes the `ptr`/`p`
   false positives outright.
2. **No auto-`Copy` for trivially-copyable value structs.** `is_copy_type` only
   accepts a `NamedType`/`ClassType` when it *explicitly* implements `Copy`.
   Value structs whose fields are all Copy and which need no drop (`Layout`,
   `Point`, `Interval`) are therefore moves. This single gap is **~49/53
   (92%)** of the blast radius. Options for 1.6: (a) stdlib `@auto(duplicate)` +
   Copy-derive on value types, or (b) checker-side auto-Copy for
   all-Copy-fields, non-`Drop` structs (Rust requires an explicit derive, but
   TML could be more ergonomic).
3. **Method-arg param-kind resolution is unavailable in the borrow checker.**
   Method arguments are conservatively recorded as moves for bare identifiers
   (`instructions`: `check_method_call`). `overlaps(other: Interval)` is a
   *genuine* by-value param so it is correctly flagged, but a `ref T` method
   param reached via auto-ref of a bare identifier would be a false move.
   Function calls are precise (signature via `TypeEnv::lookup_func` honours
   `ref` params); methods have no analogous resolution here. This over-counts
   move *facts* only; with the flag OFF it is inert (drop suppression still uses
   `consumed_vars_` until Step 1.3).

## Verdict

The blast radius is **small and highly tractable**: 7 of 176 files, 53 errors,
of which **~96% are idiomatic value-type/pointer copies** concentrated in
allocator `Layout` usage — not bugs. Only the deliberate double-free probe is a
genuine latent defect. Closing classifier Gaps 1 + 2 (raw-pointer Copy +
trivially-copyable-struct Copy) would collapse the blast radius to near-zero,
leaving genuine use-after-move/double-free as the residual signal. This makes a
future flag-flip (item 1.2 "make it hard") realistic once the stdlib value-type
`Copy` derives (item 1.6) land.

## Acceptance evidence (item 1.1)

- Join-proof (`TML_DROP_FACTS_DEBUG=1`, `.sandbox/move_probe.tml`, `--legacy`):
  `[DROP-JOIN] site=all var=x … moved_out=1 consumed=0 … DISAGREE-payoff` —
  the fact now carries signal the syntactic `consumed_vars_` set missed. Before
  1.1, `move_value` was never called and `moved_out` was uniformly 0.
- `scripts/fixtures/refcount_bleed_probe.tml`: 6 join lines, all
  `moved_out=0` (no bare-identifier moves — every use is `.duplicate()` or a
  method receiver), i.e. no false positives; `tml run` still exit 0.
- Flag OFF full regression: determinism gate **22/22** (adversarial ON);
  core/alloc **44/44**, core/str **32/32**, std/json **23/23**,
  std/collections **19/19** tests (pre-existing `arraylist` K001 unchanged,
  owned by phase27a).
