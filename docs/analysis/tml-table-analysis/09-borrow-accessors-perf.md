# 09 — Borrow Accessors: Rust-Parity Evidence (phase26e, F-021 closed)

Date: 2026-07-19. Companion to `08-memory-copy-audit.md` (F-021: "collections
have NO working borrowing accessor — every read of a handle-bearing element
deep-clones"). This document records the Rust-as-Reference comparison that
closes F-021: TML's new borrow API emits the same zero-heap-traffic read
shapes as Rust's `&T` returns.

## Method

Rust side: three probe programs (`.sandbox/rustbench/{vec_get_ref,map_get_ref,
iter_ref}.rs`, rustc 1.96.0, `--emit=llvm-ir` at O0 and O1) over
`struct Row { payload: Rc<i64>, tag: i64 }` — the `Rc` field is the probe:
any accidental clone on a read shows up as a refcount increment in the IR.
Extracted snippets: `.sandbox/rustbench/*.snippet.ll`.

TML side: equivalent probes over `Row { payload: Shared[I64], tag: I64 }`
compiled through the AST-legacy path (what real programs run), IR inspected
per cluster during phase26e; runtime refcount probes assert the same
invariant dynamically (nested `Shared.strong_count` unchanged across borrow
reads).

## The invariant table (Rust baseline → TML result)

| Operation | Rust IR shape (O1) | TML borrow API | TML IR shape | Verdict |
|---|---|---|---|---|
| Indexed borrow read | `load len` + bounds-check + `load data` + 1 GEP (`base+i*stride`); field read = 1 GEP + 1 load; 0 alloc/clone/refcount | `List::get_ref(i) -> ref T` | bounds-panic guard + GEP into data + `ptr` return; **no aggregate alloca, no memcpy, no `duplicate`/`ptr_read_clone`** | **PARITY** |
| Map borrow read | hash + SwissTable probe (allowed) → value ptr; then 1 GEP + 1 load per field; only alloca = the key | `HashMap::get_ref(k) -> Maybe[ref V]` | probe loop → `Just(ref (*val_ptr))`; value read = GEP + loads; **0 value clone, 0 refcount** | **PARITY** |
| By-ref iteration body | pointer step (stride) + 1 GEP + 1 load per touched field; 0 clone/alloc/refcount in loop | `List::iter_ref()` (`Item = ref T`), `HashMapIter::value_ref()` | same pointer-step as the by-value iterator, read = `ref (*ptr)`; **no clone in the loop** | **PARITY** |
| Refcount probe | reading an `Rc` field through `&Row` = `load ptr` + `load i64` (a READ, never an increment) | all of the above | runtime probes: nested `Shared.strong_count` **unchanged** across `get_ref`/`iter_ref`/`value_ref`; bumps only via by-value `get`/`iter` (documented clone-read) | **PARITY** |
| Hot internal sites | n/a (Rust stdlib compares via `&T` natively) | `contains`/`index_of`/`binary_search`/`dedup`/`_quicksort` compare, `find_index`, `HashMap::duplicate` | compare loops read via `get_ref` → `load`/`strcmp` → `icmp`; **no `ptr_read_clone`/`duplicate` in the loop**; swaps/shifts intentionally remain value moves; sort keeps exactly 1 pivot clone; `duplicate` = 1 clone per entry (was 2) | **CLOSED** |

## What stays a clone (by design, documented)

- By-value `get`/`iter()`/`value()` remain **clone-reads** — the sound default
  (v0.3.52/0.3.58): the container keeps ownership, the returned value carries
  fresh refcounts. The borrow API is the zero-cost opt-in. Docstrings on both
  sides state the contract.
- Element **moves** (List `swap`, dedup compaction, BTreeMap shift loops) are
  genuine relocations, not reads — kept as value copies.
- `BTreeMapIter` keeps snapshot semantics (documented); its lookups now use
  borrowed key comparisons (`find_index` via `keys.get_ref(mid).cmp` — was
  O(log n) key clones per lookup).
- SSO/inline `Text::as_str_ref` falls back to the allocating `as_str` (the
  23-byte inline case has no in-place NUL); heap-backed `as_str_ref` is a
  zero-copy borrow of the internal buffer (storage proven NUL-terminated).

## Safety net

- Interior-ref borrow checking landed with the API (phase26e Cluster D):
  conflicting interior refs (two `mut ref`s; `mut ref` while a shared ref is
  live) are B009 compile errors, zero false positives on the corpus.
  Push-class invalidation detection awaits the mutator unique-access decision
  (`phase26g_collection-mutator-mut-this`).
- Runtime: the adversarial determinism corpus (poison + quarantine, ×100)
  covers handle-bearing List/HashMap operations at 100% floors.

## Caveats

- Rust baseline measured on x86_64-pc-windows-msvc with `-C panic=abort`;
  read-shape invariants are target-independent.
- TML IR inspected at O0 — scalar i64 allocas present at O0 are register
  slots, not element copies (no aggregate alloca/memcpy anywhere in the read
  paths); Rust O0 shows the same pattern.
- A pre-existing Str-ordering lowering quirk (`<=` on `Str` lowers to pointer
  `icmp` inside `_quicksort`) predates this work and is unrelated to the
  borrow migration (`==` correctly uses `strcmp`).
