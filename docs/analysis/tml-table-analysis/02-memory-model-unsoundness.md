# 02 — Memory-Model Unsoundness (THE core defect)

This is the single problem class that, left unfixed, makes every other investment in TML
moot. It is what killed UzDB's viability and what keeps the self-hosted compiler stuck.

## F-001 — RAII `Drop` is inserted by lexical scope, not by ownership/move analysis, over raw-pointer smart pointers

**Confidence: High. Impact: Critical.**

`Heap[T]` and `Shared[T]` are `type X { ptr: *T }` with a manual `impl Drop` that calls
`mem_free` (`lib/core/src/alloc/heap.tml:263-270`, `lib/core/src/alloc/shared.tml:489-493`).
The codegen drop pass (`compiler/src/codegen/llvm/core/drop.cpp`) tracks variables per
lexical scope and emits `drop()` at scope exit in LIFO order.

The ownership decisions that make Rust's equivalent sound — "this value was **moved out**,
so do not drop the source" and "this is a **borrow**, so never drop it" — are not reliably
applied to values that flow through raw pointers. Because the owned handle lives behind a raw
`*T` and is read via `lowlevel { *this.ptr }`, the drop pass cannot see that two distinct SSA
values alias the same allocation. Both get dropped → `mem_free` runs twice.

## F-002 — `.get()` on `Shared`/`Heap`/`HashMap`/`List`/`BTreeMap` returns a bitwise copy that aliases nested owned handles without bumping refcounts

**Confidence: High. Impact: Critical.**

`Shared::get` is literally `return (*this.ptr).value` (`shared.tml:145-150`); `Heap::get` is
`return lowlevel { *this.ptr }` (`heap.tml:113-118`). The library's own docstring
(`shared.tml:118-138`) states the hazard verbatim:

> "`get` is a **bitwise copy** … If `T` contains nested `Shared[U]`, `Heap[U]`, or other
> refcounted handles as fields, those handles are aliased into the returned value WITHOUT
> bumping their refcounts. When the returned value drops, its drop-glue decrements those
> nested refcounts — potentially freeing storage that the original `Shared`'s allocation
> still expects to own. This is the `Shared.get` aliasing class of bug (phase24k diagnosis)."

`HashMap.get`/`List.get` had the identical defect; the "fix" was a new codegen intrinsic
`ptr_read_clone[T]` (CHANGELOG 0.3.52) rather than a model change.

**Why this is fatal for applications:** every row read out of a `BTreeMap[K, Row]` where
`Row` contains a `Str`/`Buffer`/`List` is a potential premature free. A database does this
on every single query.

## F-003 — The bug class is not localizable; ~14 phases of band-aids + a "structural fix" have failed to close it

**Confidence: High. Impact: Critical.**

The remediation history (phases 24a→24n, versions 0.3.39–0.3.52) is a catalogue of
workarounds, each fixing one call site and revealing the next:

- `into_raw()`-to-null-the-pointer hacks (CHANGELOG 0.3.41/0.3.42).
- Deep-clone `impl Duplicate for CType` (0.3.43).
- `declarator_name_value_leak` — a **deliberate memory leak** shipped as a workaround (0.3.44).
- Structural `Heap[T]` → `Shared[T]` migration (0.3.45, phase24g — described in `PLANS.md` as
  the fix that "closes the entire bug class in one shot"; it did not).
- ~70 manual `.duplicate()` insertions at partial-move sites (0.3.51).
- New `get_clone`/`get_ref` methods (`shared.tml:177-213`) and the `ptr_read_clone` intrinsic
  (0.3.52).

The decisive evidence is `.rulebook/tasks/phase24l_shared-get-aliasing-deep-fix/tasks.md`,
"Attempt log":

- **Attempt 1** (add `Shared.get_clone`, migrate only the typedef arm): matches baseline on
  the minimal repro; the `essential.c` gate was **not met**.
- **Attempt 2** (migrate ~40 sites to `.get_clone()`): minimal repro **regressed** 30/30 →
  25/30 — "Each additional site … introduces refcount-bump imbalance that mirrors phase24k's
  regression class." Reverted.
- **Attempt 3** (make `.get()` itself deep-clone at the language level): **regressed
  lib/core** — "at least 4 unrelated test files (`cache_aligned_box`, `cache`,
  `cache_soavec_set`, `future_fuse`) start failing K001 codegen errors." Reverted.

A defect that (a) cannot be fixed at the call sites without introducing refcount imbalance and
(b) cannot be fixed at the type without breaking codegen monomorphization is, by definition,
a **foundational language/codegen defect, not a bug**. Per-site patching is proven
non-convergent.

## F-004 — A borrow checker exists (NLL + Polonius) but is blind to this class

**Confidence: Medium-High. Impact: High.**

`compiler/src/borrow/` contains a real borrow checker (`checker_nll.cpp`,
`polonius_checker.cpp`, `polonius_solver.cpp`, `polonius_facts.cpp`). It operates on TML's
safe reference/move layer. The double-free class lives **below** that layer: the aliasing is
created inside `lowlevel { *this.ptr }` through raw `*T`, which the borrow checker treats as
opaque.

So the safety net that would catch exactly this in Rust (you cannot move out of `*map.get()`
and also keep the map) is bypassed by the very abstractions (`Heap`/`Shared`/`HashMap`) that
every program depends on. **The borrow checker protects user code but not the stdlib's own
unsafe internals — and those internals are on every hot path.**

## F-013 — `Shared::increment_count`/`decrement_count` bitwise-copy the whole inner value (latent aliasing)

**Confidence: Medium. Impact: Medium-High.**

Both helpers do `let inner: SharedInner[T] = *this.ptr` (`shared.tml:320-333`) purely to read
a counter — this bitwise-copies `value: T`. If drop-glue runs on the `inner` local at scope
exit and `T` contains nested handles, those handles are decremented/freed even though the
operation was meant to be side-effect-free on the value. This is the same aliasing shape as
F-002, sitting **inside the refcount machinery itself**.

*Verification needed:* confirm via emitted MIR whether the `inner` local gets
drop-elaborated. If it does, this alone would explain the refcount imbalance observed in
phase24l Attempt 2.

## Root-cause synthesis

TML committed to Rust-grade RAII semantics but implemented ownership tracking only for the
safe-reference layer, then built every smart pointer and collection on raw pointers + manual
`Drop`. The compiler cannot reason about aliasing through those raw pointers, so drop
insertion is unsound wherever a value is read-by-copy out of an owning container.

There are only two real exits:

- **(A) Full move/init-state + drop-flag elaboration in MIR** — track initialization/moved-out
  state per local through the CFG so moved-from/aliased values are never dropped; make
  read-out-of-container either an explicit clone or a true borrow.
- **(B) ARC/Swift-style compiler-inserted refcounting** — make the fundamental owning types
  refcounted end-to-end, with codegen auto-inserting retain on every copy and release on
  every drop.

The current path — per-site `.duplicate()` and per-method `get_clone` — is neither, and
phase24l proves it does not converge. The decision between (A) and (B) is the single most
important pending decision in the project (see file 06, Phase B).
