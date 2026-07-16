# 08 — Memory-Copy Audit: The Move-Semantics Gap Across the Stdlib

**Date:** 2026-07-15 · **TML:** 0.3.55 · **Branch:** `fix/era0-stabilization`
**Motivation:** after the F-013 point-fix (v0.3.55), the question was whether the
copy-instead-of-move class is isolated or systemic. It is **systemic**. This audit
(4 parallel read-only passes over `lib/core`, `lib/std`, and both codegen paths)
maps the full extent. Findings continue the global numbering: F-015..F-022.

This matters more than any single bug: TML's entire value proposition — Rust-like
memory management, no GC, zero-cost — rests on values being **moved or borrowed**,
not heap-copied. Today they are copied, and the compiler chooses per-site between
"leak" and "double-free" to survive it.

---

## Closure status (updated 2026-07-16, v0.3.62)

Every finding below has an owning task; the concrete bugs are **closed**, the model
gaps are milestone work in flight. Statuses:

| Finding | Status | Closed by |
|---|---|---|
| F-013 (refcount bleed, from doc 01) | **CLOSED** | v0.3.55 direct field reads + `tml_refcount_bleed_userpath` canary 100/100 |
| F-015 no move semantics (ROOT) | **IN PROGRESS** — dataflow live | phase26f: `move_value` activated (v0.3.60), Copy classification blast-radius 53→0 (v0.3.61), fact-driven drop suppression (v0.3.62); remaining: leak special-case removal (1.4), drop flags (1.5) |
| F-016 13 double-free/UAF read-out sites | **CLOSED** | v0.3.58 `ptr_read_clone` in iterators + `List::retain` rebuild + canaries `f016_*` |
| F-017 broken move-outs (unconditional double-free) | **CLOSED** | v0.3.56 (stdlib sweep wave 1) |
| F-018 `Sync::get` copying, no safe alternative | **CLOSED** | v0.3.56–58: `get` balanced-clone via field-0 ptr; `get_ref`/`get_clone` added |
| F-019 read/iterate asymmetry | **MITIGATED** | safe path everywhere (F-016 fix); the *fast* borrow default is F-021/phase26e |
| F-020 pass-by-value MUST-BORROW class | **CLOSED** (concrete sites) | v0.3.59 ref-migration wave (bigint 14 ops, h2 handlers); model fix = phase26f |
| F-021 no borrow accessor (language gap) | **OPEN — owned** | phase26e (borrow accessors, the zero-cost enabler) |
| F-022 `destroy` leaks elements | **CLOSED** | v0.3.58 per-element drop in `List/HashMap::destroy` + `f022_destroy_releases` canary |
| F-023 `try_unwrap` weak-ref UAF | **CLOSED** | v0.3.56 weak-aware `try_unwrap` |

Plus one bug the audit did NOT catch, found by the activated facts: plain
`let b = a` **double-dropped** (no `mark_var_consumed` at the let handler); fixed by
the v0.3.62 union suppression, canary `tml_let_move_double_drop`.

---

## Executive verdict

**TML has no real move semantics at the codegen level (F-015).** Every read of an
owned value out of a container or smart pointer is a whole-aggregate **copy**;
destructors are inserted **lexically**, not by ownership dataflow. The result is a
three-way lose-lose on the read-out path — the single most frequent operation in any
program:

- **Correctness:** the copy aliases the container's owned handles → double-free /
  use-after-free (13 confirmed sites, F-016; 2 more that double-free unconditionally,
  F-017).
- **Performance:** where the compiler avoids the double-free by deep-cloning
  (`ptr_read_clone`), every read of a handle-bearing element is
  `alloc + memcpy + refcount-bump` per owned field, immediately undone by the paired
  drop — **2N heap ops to look at one row**, cascading recursively through nested
  containers (F-021). Rust emits **zero** for the borrowing default.
- **Leaks:** where the compiler avoids the double-free by *skipping* the drop
  (`drop.cpp:460-471` for Heap/List/HashMap/Buffer/BinaryWriter/BinaryReader),
  the container's heap block is never freed (F-022).

The one bright spot: **primitives and POD are genuinely zero-cost** (bitwise load,
no drop-glue), and the safe accessors `Shared::get_ref`/`get_clone` are sound
(IR-verified) — the model exists, it's just not the default and not enforced.

---

## F-015 — No real move semantics at the codegen level (ROOT)

**Confidence: High. Impact: Critical.**

The `drop.cpp:460-464` comment — *"TML doesn't have move semantics, so these types
are copied (not moved) when assigned to another variable"* — is a true statement
about the **whole compiler**, not a local quirk. Evidence:

- **MIR path (default):** the move-suppression scaffolding exists but is **dead
  code**. `BuildContext::mark_moved` + `DropInfo::is_moved`
  (`compiler/include/mir/mir_builder.hpp:68,92-101`) are honored by the drop
  collectors — but `mark_moved` is **never called anywhere** in the compiler. So
  `is_moved` is always false → every registered drop fires; nothing is a move. MIR
  also has no first-class `DropInst` (drops lower eagerly to `CallInst("Type::drop")`,
  `thir_mir_builder_expr.cpp:1085-1155`).
- **AST-legacy path (fires for every stdlib import + generics, `build.cpp:413`):**
  move tracking is `consumed_vars_`, a **name-based syntactic set**
  (`drop.cpp:97-99`), gated at all ~30 call sites on the moved value being a *bare
  identifier*. A value-returning read (`map.get(k)`, `shared.get()`) has **no named
  source** to consume, so it structurally cannot be tracked.
- **The borrow checker computes the right lattice and it is thrown away.**
  `OwnershipState{Owned,Moved,Borrowed,MutBorrowed,Dropped}` + init-state dataflow
  exist (`checker.hpp:330-336,799-816`) but `provide_mir_build` never consumes
  borrowcheck facts (`query_core.cpp:575-593`) — and the checker is blind below
  `lowlevel { *this.ptr }` anyway (F-004).

**The two paths paper over the same hole with opposite hacks:** AST = *bitwise-copy
then skip container drops (leak)*; MIR = *deep-clone at the read site then full drop
(2N heap ops, balanced only when the clone fires)*. Neither is a move.

## F-016 — 13 confirmed double-free / use-after-free sites (unfixed F-013/F-002 siblings)

**Confidence: High. Impact: Critical.**

Exact-shape twins of the bug we just fixed, still live:

| # | file:line | site | shape |
|---|-----------|------|-------|
| 1 | `lib/core/src/alloc/sync.tml:166` | `Sync[T]::get` = `(*this.ptr).value` | F-013 copy, **no `get_ref`/`get_clone` escape** (F-018) |
| 2 | `lib/std/src/collections/hashmap.tml:636` | `HashMapIter::value` = `ptr_read[V]` | `get` was fixed, iterator was not; used by `duplicate`/`to_string` |
| 3 | `lib/std/src/collections/behaviors.tml:349` | `ListIter::next` = `ptr_read[T]` | hottest read; `iter()` borrows, yields aliasing copy |
| 4 | `lib/std/src/collections/behaviors.tml:533` | `HashSetIter::next` | same |
| 5 | `lib/std/src/collections/class_collections.tml:315` | `HashSetIter::current` | same |
| 6 | `lib/std/src/collections/class_collections.tml:334` | `HashSetIter::next_item` | same |
| 7 | `lib/std/src/collections/list.tml:282` | `List::retain` local `elem` | kept elements' copy drops → decrements live handle |
| 8 | `lib/core/src/alloc/heap.tml:117` | `Heap[T]::get` = `lowlevel { *this.ptr }` | whole-T copy out of box |
| 9 | `lib/core/src/alloc/heap.tml:283` | `Heap[T]::duplicate` | `this.get()` temp drops after `.duplicate()` |
| 10 | `lib/std/src/sync/arc.tml:381` | `Arc[T]::make_mut` | CoW bitwise-copies `data` (not `T::duplicate`); doc lies |
| 11 | `lib/core/src/alloc/shared.tml:149` | `Shared[T]::get` | documented; has safe alternatives (mitigated, not fixed) |

Plus the two by-value clusters below counted separately (F-020).

## F-017 — 2 broken move-outs that double-free unconditionally (library bugs, fixable now)

**Confidence: High. Impact: Critical. INDEPENDENT of the model fix.**

- `lib/std/src/sync/arc.tml:283-318` — `Arc::try_unwrap` copies `data` out, deallocs
  the inner, then returns while `this: Arc[T]` is **still live and not forgotten** →
  `Arc::drop` runs on the freed allocation (second `fetch_sub` + `dealloc`). The
  code's own comment admits "we'd need `mem::forget` or similar" — never applied.
- `lib/core/src/types/any.tml:400-412` — `AnyValue::into_inner` reads the value,
  `dealloc(this.data)`, but **never nulls `this.data`** → `AnyValue::drop` deallocs
  the same pointer again. Both the match and mismatch paths.

These are straight library bugs (null/forget the source) — they double-free
regardless of move semantics and can be fixed immediately.

## F-018 — `Sync[T]` has a copying `get` with NO safe alternative

**Confidence: High. Impact: High.**

`Sync[T]` (the thread-safe Arc-equivalent, `core/alloc/sync.tml`) exposes `get` as a
raw `(*this.ptr).value` copy and — unlike `Shared` (which got `get_ref`/`get_clone`
in the F-013 work), `Heap` (`as_ref`/`deref`), and `Arc` (ref-returning `get`) — has
**neither `get_ref` nor `get_clone`**. A caller reading a `Sync[Shared[…]]` by value
has no sound option. Fix: port the `get_ref`/`get_clone` triad from `Shared`.

## F-019 — Read/iterate asymmetry: the safe path is slow, the fast path is unsound

**Confidence: High. Impact: Medium-High.**

`List::get`/`HashMap::get` were retrofitted to deep-clone (`ptr_read_clone`), but
iteration (`ListIter::next`, `HashMapIter::value`, `HashSetIter`) still hands out a
bitwise **alias**. So `for x in list` is cheap-but-unsound while `list.get(i)` is
sound-but-a-deep-clone — inconsistent semantics for the same logical read. A correct
model must make both do the same sound thing.

## F-020 — Pass-by-value MUST-BORROW: the phase24b class is alive across the stdlib

**Confidence: High. Impact: High (perf) + Critical (a few UAF). Mostly fixable now.**

phase24b (CHANGELOG 0.3.40) fixed ONE instance of "owning aggregate passed by value
to a read-only entry point, whose drop-glue frees storage the caller still owns."
The pattern is **live and unfixed** in the stdlib. Note: bare `this` receivers are
**borrows** (interior-mutability collections), so those are NOT hazards — the danger
is plain-by-value **parameters** of owning `Drop` types that the body only reads.

Worst clusters:
- **BigInt operators (~14 sites, `lib/std/src/bigint.tml:265-862`)** — every `a.op(b)`
  takes `other: BigInt` by value; `BigInt` embeds `digits: List[I64]`, so each op
  copies the struct AND aliases+frees the caller's inner `List` handle. The bodies
  already borrow internally (`ref this.digits, ref other.digits`) — the author knew,
  but left the param by value. Hot in `mod_pow`/RSA loops (both perf AND UAF).
- **`str::join` / `concat_all` (`core/str/convert.tml:130,200`)** — ubiquitous;
  transfers ownership of the whole `List[Str]` where a borrow suffices.
- **HTTP/2 `Buffer` family (~25 sites, `lib/std/src/http/h2/`)** — `h2_buf_append`
  / `h2_conn_append_buf` take the accumulator `dst: Buffer` by value → callee's
  drop-glue frees the caller's buffer on return → UAF on the next append.
- Others: `HashMap::extend_from` (frees both arg lists), `console::table`,
  `File::write_bytes`/`write_all_bytes`, events/reactive `List` params.

Each fix is a **one-token `ref` migration** matching the codebase's own idiom
(`List::extend(other: ref List[T])`, `Buffer::compare(other: ref Buffer)`). `core/alloc`,
`json`, and `msgpack` are clean.

## F-021 — Collections have no borrow accessor; `get` returns an owned deep clone (language gap)

**Confidence: High. Impact: Critical (perf thesis).**

The zero-cost violation is concentrated in one design decision cascaded everywhere:
`List/HashMap/BTreeMap/Deque.get` return **by value**, deep-cloning through
`ptr_read_clone` for handle-bearing elements. There is **no `get_ref`/`get_mut`/
`iter_ref` anywhere** in `lib/std/src/collections` (the sole attempt, `IndexMut::index_mut`
→ intrinsic `list_get_mut`, has **no codegen implementation** — it exists only in
`behaviors.tml`, absent from the entire `compiler/` tree).

Cost, per one logical read (`N` owned fields, nested inner length `m`):
- `List[Str]::get(i)` → 1 alloc + memcpy per string.
- `List[List[Str]]::get(i)` → **1 + 2m allocations** (inner list rebuilt, each `Str`
  cloned twice because `List::duplicate` calls `get(i).duplicate()`, `behaviors.tml:80-91`).
- `HashMap::set(k,v)` deep-clones **both** key and value on every insert
  (`hashmap.tml:196,209,214`) → `HashMap[Str,Str]` insert = 2 allocs + 2 memcpy.
- `BTreeMap` is arrays-with-binary-search: `insert`/`remove` shift via
  `get`(clone)+`set`(leaking store) → O(n) allocations AND O(n) handle leaks per op.

Already Rust-parity (do not touch): `List::push`/`pop`, doubling growth, HashMap
rehash (byte memcpy), `str::join`, `Str`-append `s = s + x`, `Text` push/grow, all
primitive/POD `get`s.

**This is a language + codegen gap, not a missing method:** the collections are
type-erased `*Unit` byte stores that hand out owned values *by design* to keep the
drop model sound; a borrowing accessor needs (1) interior-pointer codegen into the
erased buffer, and (2) borrow-checker lifetimes binding the reference to the
container. ADR-009 flagged this as the "largest blast radius, deferred" alternative.

## F-023 — `try_unwrap` frees the allocation ignoring outstanding weak refs → UAF (distinct class, fixable now)

**Confidence: High. Impact: High. INDEPENDENT of the model fix.**

`Shared::try_unwrap` (`shared.tml:285-295`) and `Sync::try_unwrap` (`sync.tml:260`)
gate on `is_unique()`, which is `strong_count() == 1` and **does not check
`weak_count`** (`shared.tml:244-246`). When unique-by-strong but weak refs exist
(from `downgrade`, `shared.tml:399`), `try_unwrap` `mem_free`s the whole
`SharedInner` — the `SharedWeak`/`SyncWeak` still point at it, so their `upgrade`
(`:444`) and `drop` (`:497`) read/write freed memory → use-after-free / heap
corruption. Rust's `Rc::try_unwrap` drops only the value and keeps the box alive
until weak reaches 0.

This is a **different class from F-013** (free-ignoring-weak, not copy-not-move) and
is library-fixable: on `strong == 1`, move the value out + decrement strong, and free
the allocation only when `weak_count` also hits 0 (mirror the `decrement_count`
logic). Same for `Sync`.

## F-022 — `List`/`HashMap` `destroy` don't run per-element Drop → leak (mirror of the copy hazard)

**Confidence: High. Impact: High.**

`List::destroy` (`list.tml:236-250`) and `HashMap::destroy` (`hashmap.tml:389+`) only
`mem_free` the backing buffers — they never run per-element `Drop`. So
`List[Shared[…]]` / `HashMap[K, Heap[…]]` **leak every element handle** on drop. This
is the exact mirror of the read hazard: reads *over*-decrement (F-016), destroy
*under*-decrements. Any real fix must make both sides consistent, which only a real
drop-elaboration model can guarantee.

---

## What is already sound (do not "fix")

- **Primitives / POD:** bitwise `get`, no drop-glue — genuinely zero-cost.
- **`Shared::get_ref` (`shared.tml:207`) and `get_clone` (`:177`)** — IR-verified
  2026-07-15: `get_clone` calls `T::duplicate` directly on the inner storage pointer
  and returns a fresh value with no dropping temp (the vestigial bitwise load is dead
  and not drop-elaborated); `get_ref` returns a true borrow. These are the canonical
  safe patterns the rest of the stdlib should adopt.
- **`this` receivers** on interior-mutability collections — borrows, not copies.
- **Correct move-outs** that null the source: `Shared/Sync::try_unwrap`,
  `Heap::into_inner`, `List::pop`/`drain`.
- **`core/alloc`, `json`, `msgpack`** — clean of by-value owning params.

---

## Implications for the roadmap

The findings split cleanly by dependency on the ADR-009 model fix:

**Fixable NOW, independent of the model (library-level, safe wins):**
- F-017 broken move-outs (`Arc::try_unwrap`, `AnyValue::into_inner`) — add null/forget.
- F-018 `Sync` — port `get_ref`/`get_clone`.
- F-020 pass-by-value → one-token `ref` migration (BigInt, str::join, HTTP/2, etc.).
- F-023 `Shared`/`Sync` `try_unwrap` — free only when weak_count also 0.
→ **New task `phase26d_stdlib-copy-hazards-sweep`** (created; runs in parallel with 26b).

Minor / lower-priority items surfaced by the audit, folded in rather than tasked
separately:
- **RefCell `replace`/`replace_with`/`swap`** (`core/cell/ref_cell.tml:105-166`) do
  `let old: T = this.value; this.value = new` — instances of F-015 (copy-not-move
  aliasing), closed generically by the model fix; added to phase26b step 4.5 as
  verification sites, not a separate task.
- **`Text.concat` double-copies for inline results** and **`Text.as_str` can't
  borrow** (`std/text.tml`) — perf only; folded into phase26e's borrow-accessor
  family (a `Text` that can hand out a `&str` view).

**Requires the model fix (ADR-009 B3, phase26b step 4 = drop-flag elaboration):**
- F-015 (root), F-016 read-out copies, F-019 iterator asymmetry, F-022 destroy leaks —
  all become sound once moves/drops are dataflow-tracked and the read-out is a move or
  a properly-paired clone. **Scope note added to phase26b step 4:** the 13 F-016 sites
  and F-022 are the acceptance surface, not just `essential.c`.

**Requires new language + codegen surface (beyond phase26b):**
- F-021 borrowing collection accessors (`get_ref`/`get_mut` + interior-pointer codegen
  + lifetimes). This is the difference between "correct but still copies" and "truly
  zero-cost." **Recommended as a dedicated task after the model lands** (candidate:
  a phase26e or a native-backend-era item), since it is the piece that actually
  delivers the Rust-parity performance goal.

**Bottom line for the performance goal:** closing F-015..F-020/F-022 makes TML
*correct and leak-free*. Only F-021 (borrow accessors) makes it *zero-cost*. Both are
needed to meet the stated thesis; the model fix (26b) is the prerequisite for both.
