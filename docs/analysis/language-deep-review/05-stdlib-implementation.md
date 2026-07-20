# 05 — Standard Library Implementation (lib/core + lib/std)

**Findings:** L-080..L-088 · **Method:** code audit + API-surface survey · **Builds on:** F-015..F-017 (`../architecture-performance-review/04-memory-model-foundation.md`), phase44b/44c task specs.

> File refs: collection files live in `lib/std/src/collections/` (`hashmap.tml`, `list.tml`, `deque.tml`, `btreemap.tml`, `buffer.tml`, `class_collections.tml`); `text.tml` is `lib/std/src/text.tml`; `str/basic.tml` is `lib/core/src/str/basic.tml`.

## Summary

The TML stdlib's data-structure layer is **built on type-erased `*Unit` handles with hand-computed byte offsets and literal header sizes**, not on the type system — every `List`, `HashMap`, `Buffer`, and iterator is a `mem_alloc(literal)` blob poked with `lowlevel { ptr_read/ptr_write }`. This is the phase44c drift bug-class *systematized across the whole library* (headers of 32/48/16 bytes as bare literals), it forces value type-erasure (HashMap keys are always 8 bytes), and it hides the data from the optimizer behind opaque pointers. On top of that foundation sit several algorithmic downgrades that directly contradict the Rust-class goal: `HashMap` has Swiss-Table control bytes but probes them **one byte at a time** (SIMD group-scan is disabled by a generic-codegen bug, by the project's own measurement 9ns vs 4ns); `List::sort` is a naive last-pivot quicksort with an O(n) recursion stack and O(n²)/stack-overflow cliff on sorted input; and `BTreeMap` is a sorted parallel-array with O(n) insertion, not a B-tree. The string layer is a clever-but-costly C-ABI compromise: `Str` is a bare `char*` with a hidden sidecar length header, so `len` is a runtime call with a magic-check rather than a fat-pointer field read — and `Text` bypasses even that cache by re-declaring raw `strlen`. Iterators are eager clone-snapshots (Deque/BTreeMap/HashMap materialize full copy-Lists before yielding), confirming and generalizing F-016. The pure-TML migration itself is nearly complete and largely sound — the remaining C runtime under lib is thin and correctly scoped to FFI/OS — so the debt is algorithmic and representational, not "still in C."

---

### L-080 — Collections are type-erased `*Unit` blobs with literal header sizes: phase44c drift, systematized

**Impact:** High · **Confidence:** High · **Layer:** design

Every core collection stores its metadata as a raw allocation with manually-computed offsets, not a struct: `List` header = `mem_alloc(32)` with fields at offsets 0/8/16/24 (`list.tml:61`, layout comment `list.tml:35-40`); `HashMap` header = `mem_alloc(48)` with six fields at 0/8/16/24/32/40 (`hashmap.tml:75`, `:39-46`) plus a `mem_alloc(16)` iterator (`:520`); `Buffer` = `mem_alloc(32)` (`buffer.tml:247`). Consequences:

- **Drift bug-class:** these literals are exactly the phase44c specimen (`mem_alloc(8)` for a struct that grew to 12 bytes). Adding one field to any header silently requires editing the literal *and* every downstream offset; the type system checks none of it. The phase44c survey step (count the blast radius) is still unchecked — in production `lib/` the fragile literal-size sites are the four collection headers plus `text.tml:426,491` (`mem_alloc(25)`) and ~11 sites in `postgresql/connection.tml`.
- **Forced type-erasure:** HashMap keys are stored as a fixed 8 bytes (`hashmap.tml:44-50`, `key.duplicate() as I64` at `:209`), so struct/tuple keys wider than a pointer **cannot be used as keys at all** — a hard API limitation vs Rust's `HashMap<K,V>` for any `K: Hash+Eq`.
- **Optimizer-opaque:** the backing store is `*Unit`; loads/stores go through `as I64` casts, defeating TBAA/vectorization through the handle.

**Why it conflicts:** this is the F-015/F-016/F-017 "raw pointers, no move-tracking" foundation re-appearing one layer up. It's almost certainly a *workaround* for unreliable generic struct/Drop codegen (the F-006 K001 treadmill), but it trades the type system's guarantees for hand-rolled unsafety that is both slower (opaque) and the single largest surface for the heap-corruption class.

**Recommendation:** once generic struct layout/Drop is trustworthy, migrate headers to real generic structs (`type ListHeader[T] { data: *T, len: I64, cap: I64 }`) so `size_of` replaces every literal and offsets vanish. Until then, land the phase44c lint (require `size_of[T]()` over literal sizes in `lowlevel` allocs) and replace the four collection-header literals with computed sizes.

---

### L-081 — HashMap has Swiss-Table control bytes but scalar probing, and never rehashes tombstones

**Impact:** High · **Confidence:** High · **Layer:** implementation

The map allocates a dense control-byte array and computes h2 fingerprints (`hashmap.tml:52-55, :659-661`) — the entire memory cost of a Swiss Table — but the probe loops scan **one control byte at a time** (`get` at `:248-260`, `set` at `:177-206`, `has`, `remove`). The code's own comment (`:223-225`) states the SIMD group-scan version works and is measured at **4ns/op vs 9ns scalar** but is disabled because "generic instantiation has codegen bugs with SIMD intrinsics inside lowlevel blocks." Two compounding problems:

1. Paying the ctrl-byte memory/cache cost without the ~2.25× speed it exists to provide.
2. **No tombstone reclamation:** `remove` writes a `DELETED` tombstone (`:446`) and decrements `len`, but the resize trigger tests only `cur_len * 10 > cur_cap * 7` (`:118`) — live count, ignoring tombstones. A churn workload (insert/remove cycling) never grows, tombstones fill the table, and `get`/`has` for absent keys degrade to O(capacity) because probing only stops at `EMPTY` (`:250`). Rust's hashbrown rehashes in place when tombstones dominate; this never does.

**Why it conflicts:** the headline collection is ~2.25× slower than the team already knows how to make it, and has an unbounded-probe cliff under churn — the opposite of Rust-class.

**Recommendation:** fix the generic+SIMD-in-`lowlevel` codegen bug (highest leverage — it also blocks SIMD in other generic code), then re-enable group scan. Independently, count tombstones and trigger an in-place rehash when `len + tombstones` crosses the load factor.

---

### L-082 — `List::sort` is a naive last-pivot quicksort: O(n²) time and O(n) recursion on sorted input

**Impact:** High · **Confidence:** High · **Layer:** implementation

`_quicksort` (`list.tml:703-721`) and `_quicksort_by` (`:680-700`) pick `pivot = get(hi)` (last element), partition Lomuto-style, then recurse on **both** partitions with no smaller-half-first / tail-call bound. On already-sorted or reverse-sorted input the pivot is always the extremum, so partitions are size n-1 → **O(n²) comparisons and O(n) recursion depth** (stack overflow for large sorted lists — a hard crash, not just slow). There is no median-of-three, no introsort fallback to heapsort, and the sort is not stable. Rust ships driftsort (stable) / pdqsort (`sort_unstable`, introsort-class) precisely to defeat these patterns.

**Why it conflicts:** sorting an already-sorted `List` — the single most common real-world case — is the worst case here, degrading to quadratic and risking a stack overflow. Below a naive-textbook bar, not near Rust's.

**Recommendation:** replace with pattern-defeating quicksort (median-of-three or ninther pivot + insertion-sort for small n + heapsort fallback past a depth limit) and recurse smaller-half-first to bound stack at O(log n). A self-contained rewrite of ~40 lines.

---

### L-083 — `BTreeMap` is a sorted parallel-array with O(n) insertion, not a B-tree (misnomer + algorithmic downgrade)

**Impact:** High · **Confidence:** High · **Layer:** design

`BTreeMap[K,V]` is backed by `keys: List[K]` + `values: List[V]` kept sorted (`btreemap.tml:27-28`), and its own module doc admits "**O(n) insertion (due to shifting)**" (`:3-4`). It has none of a B-tree's node structure or cache behavior — lookup is binary search (O(log n)) but every insert/delete shifts an array (O(n)), and `iter()` materializes fresh full copies of both Lists (`:339`, `:323`). Naming it `BTreeMap` sets a Rust expectation (O(log n) insert, cache-friendly nodes) the implementation cannot meet.

**Why it conflicts:** the ordered-map workhorse is O(n)-insert; building one from n unsorted entries is O(n²). A Rust `BTreeMap` is O(n log n). For any insert-heavy ordered workload this is orders of magnitude off.

**Recommendation:** either (a) rename to `SortedVecMap` and document the O(n)-insert trade honestly (a legitimate structure for read-mostly small maps), and/or (b) implement a real B-tree with node arrays once generic struct codegen is solid. At minimum, stop the per-`iter()` full-copy (L-084).

---

### L-084 — Iterators are eager clone-snapshots; `get()` clone-reads by default (F-016 generalized, root cause identified)

**Impact:** High · **Confidence:** High · **Layer:** design

F-016 flagged Deque::iter clone-reading handle-bearing structs; this is a library-wide pattern:

- `Deque::iter` allocates a fresh `List` and **deep-copies every element** before yielding anything (`deque.tml:344-355`). The explanatory comment (`:257-266`) names the root cause: `List` is move-only (has `Drop`, no `Duplicate`), so a borrowing iterator would double-free — the copy is a *correctness workaround for a missing capability*, not a design choice.
- `BTreeMapIter` copies both key/value Lists (`btreemap.tml:339, :377-378`); `HashMap::keys`/`values` materialize whole `List`s (`hashmap.tml:551-575`).
- The default element accessor clone-reads everywhere: `List::get` → `ptr_read_clone[T]` (`list.tml:172`), `HashMap::get`/`get_opt`/iter `value()` → `ptr_read_clone[V]` (`hashmap.tml:255, :291, :751`). Zero-copy siblings exist (`get_ref`/`get_mut`/`value_ref`) but are opt-in and secondary.

**Why it conflicts:** iterating a Deque or BTreeMap is O(n) allocation + n clones *before the first element*, versus Rust's zero-alloc `&T`-yielding iterators. For handle-bearing element types every clone is a refcount bump + eventual drop. This is the "band-aids add copies" theme from F-015-F-017, surfacing as the default ergonomic API.

**Recommendation:** give `List` a `Duplicate` impl (or a borrowing cursor that doesn't require it) so Deque/BTreeMap can iterate by borrow; make the borrowing iterator the default and clone opt-in. Deletes an entire class of hidden allocations. (Same conclusion as L-028/L-029 from the memory-model dive, reached independently.)

---

### L-085 — `Str` is a bare `char*` + hidden sidecar length header: length is a runtime call, not a fat-pointer field; `Text` bypasses even the cache

**Impact:** Medium-High · **Confidence:** High · **Layer:** design + implementation

`Str` is a raw `*U8` to a NUL-terminated buffer. TML-allocated strings carry a hidden `[magic | cap | len | data | NUL]` header behind the pointer, and `core::str::len` binds to `tml_str_len`, which reads that cached length but must first validate a magic sentinel + image-range check and fall back to `strlen` for `.rdata` literals (`str/basic.tml:10-15, :40-58`). So `len(s)` is a **runtime function call with a branch**, not a register-cheap field read like Rust's `&str` fat pointer `{ptr, len}`. It is also not fully binary-safe (literal-derived strings still depend on NUL termination). Worse, `Text` — the recommended string-builder — re-declares its own `@extern("strlen")` (`text.tml:60-69`) and uses raw `strlen` for **every** `Str` length in `from`/`push_str`/`concat_str`/`index_of`/etc., so it never benefits from the phase1k O(1) cache even when handed a TML-owned string.

**Why it conflicts:** on parsing/hashing/formatting hot paths, length queries are calls-with-branches instead of free field reads, and `Text` pays a full O(n) `strlen` per `Str` argument. A defensible C-ABI-compat choice, but the cost is real and the `Text` regression is a pure oversight.

**Recommendation:** point `Text` at `core::str::len` (`tml_str_len`) instead of raw `strlen` — a one-line-per-callsite fix that restores O(1) length. Longer term, evaluate a fat-pointer `Str` (`{ptr, len}`) with a thin C-ABI shim at FFI boundaries; the length-as-a-call cost is foundational. (See L-025 for the ownership half of the same representation decision.)

---

### L-086 — `Text` transforms double-allocate, and its substring search is naive scalar (inconsistent with Buffer, which uses libc SIMD)

**Impact:** Medium · **Confidence:** High · **Layer:** implementation

Every heap-sized `Text` transform allocates a scratch buffer, fills it, then calls `text_from_raw`, which **allocates again and copies again**, then frees the scratch — 2 mallocs + 2 copies + 1 free per result. The pattern repeats in `to_upper_case` (`text.tml:876-896`), `to_lower_case` (`:899-919`), `reverse` (`:1098-1113`), `repeat` (`:999-1016`), `pad_start`/`pad_end` (`:1116-1151`), and `replace` (`:1019-1047`). (`concat` was fixed to a single allocation — comment at `:1181-1183` — but the transforms were not.) Separately, `index_of`/`last_index_of`/`starts_with`/`ends_with`/`replace_all` are hand-rolled scalar byte loops with O(n·m) substring search (`:757-818, :1050-1095`), while `Buffer` searches with libc `memchr`/`memcmp` and SSE helpers (`buffer.tml:36-59, :910-935`). Same project, two opposite strategies for byte search. `to_upper/lower` are also ASCII-only (`:887, :910`), silently wrong for non-ASCII.

**Why it conflicts:** string transforms — a hot path in any text/web workload — do twice the memory traffic they need, and substring search forgoes the SIMD the project already links.

**Recommendation:** add a `text_from_raw_owned(buf, len)` that adopts the buffer without re-copying (or write transforms directly into the result), and route `Text` search through the same `memchr`/`memmem` path `Buffer` uses.

---

### L-087 — `Buffer` mallocs+frees a scratch block per float read/write, and stringifies byte-by-byte via a function call per byte

**Impact:** Medium-High · **Confidence:** High · **Layer:** implementation

Every float accessor type-puns through the heap: `write_f32_le`/`read_f32_le`/`_be` do `buf_mem_alloc(4)` … `buf_mem_free` (`buffer.tml:683-728`), and the f64 variants `buf_mem_alloc(8)`…free (`:731-774`) — a **malloc + free pair for a single 4/8-byte bit-cast**. Serializing 1000 floats = 1000 mallocs + 1000 frees purely to reinterpret bits. Likewise `to_string`, `from_string`, `to_hex`, `from_hex` copy with `buf_read_byte_at`/`buf_write_byte_at` **one byte per function call** in a loop (`:970-1045, :946-968`) instead of a single `copy_nonoverlapping`, and the big-endian integer paths use scalar `/`/`%` byte extraction (`:555-563, :651-665`) rather than a bswap.

**Why it conflicts:** `Buffer` is the serialization / network-protocol hot path. A heap round-trip per float and a call per byte are precisely the overheads a Rust-class buffer avoids (stack `transmute`, `copy_from_slice`).

**Recommendation:** replace the float type-pun with a bitcast/`transmute` intrinsic (or a stack local, not a heap block); replace the byte-loop stringifiers with `copy_nonoverlapping`; use the existing `buf_bswap*` helpers for BE integers.

---

### L-088 — Inconsistent bounds/miss policy across collections; unsound zero-value sentinel; Deque pre-fills capacity with sentinel clones

**Impact:** Medium-High · **Confidence:** High · **Layer:** design

The same conceptual `get` has four different failure behaviors: `List::get` does **no bounds check → UB** on out-of-range (`list.tml:164-173`, while `get_ref` *does* check and panic — an odd split); `HashMap::get` returns `0 as V` on miss (`hashmap.tml:234, :250, :259`); `Deque::get` **panics** (`deque.tml:163-165`); `Buffer::get` returns `0` (`buffer.tml:429-434`). The `0 as V` sentinel (5 sites in hashmap) is **unsound for non-integer `V`** — casting integer 0 to a struct/`Str` value type yields a null/garbage value indistinguishable from a real zero, which is why `get_opt`/`get_ref` had to be bolted on. Separately, `Deque` mis-uses `List` as a fixed array: to make `set(idx)` legal it **pre-fills the entire capacity with clones of the first pushed element** (`fill_backing` `deque.tml:81-88`, and again in `grow` `:206-230`) — 16 handle-clones on first push — and keeps its own `length`/`capacity` fields separate from the backing `List`'s len/cap (two sources of truth that can desync; `ArrayList` has the same double-counter, `class_collections.tml:46-49, :81-84`).

**Why it conflicts:** the zero-sentinel forces callers into double-lookups (`has` then `get`) or silently returns garbage; the Deque pre-fill turns an empty deque into 16 eager clones. Both are correctness-adjacent and add work Rust's API (`Option<&T>`, uninitialized ring slots) never does.

**Recommendation:** standardize on `get(i) -> Maybe[ref T]` (or a checked panic) across all collections; delete the `0 as V` path. Back `Deque` with a raw uninitialized buffer (or `Maybe[T]` slots) instead of pre-filling a `List` with sentinel clones, and let it own a single length.

---

## Verdict

**Tier today: solid-but-below-Rust-class — roughly "careful hand-written C in TML clothing," ~2 tiers below hashbrown/alloc.** The library *works*, is broadly tested, and the C→TML migration is genuinely near-complete (the remaining `collections.c` is 65 lines of FFI glue for crypto/zlib interop only). But the data-structure core is built on type-erased `*Unit` + literal sizes rather than the type system, and several flagship algorithms are textbook-naive or misnamed.

- **Need rewrite (algorithmic):** `List::sort` (introsort), `BTreeMap` (real B-tree or honest rename), `HashMap` probe (SIMD group-scan + tombstone rehash), Deque/BTreeMap/HashMap iterators (borrow instead of snapshot).
- **Need polish (mechanical, high-ROI):** `Text` → use `tml_str_len` not `strlen`; `Text` transforms → single-allocation; `Buffer` floats → bitcast not heap; `Buffer` stringifiers → memcpy; unify collection `get` semantics.
- **Foundational (cross-cutting, shared with dives 02/04):** the `*Unit`/literal-size header pattern and the `Str`-is-a-`char*` representation are the two decisions gating everything above; both trace to unreliable generic struct codegen and C-ABI compat.

## Keep

- **`Text` SSO** (24-byte struct, ≤23 bytes inline) is genuinely *better than Rust std `String`* for short strings, and `concat`'s single-allocation path is well done (`text.tml:48-52, :1181-1189`).
- **`Buffer`** is the best-built module: clean 32-byte layout, correct use of libc `memcmp`/`memchr` + SSE `bswap`/reverse-search FFI, complete LE/BE integer API (float I/O and byte-loop stringifiers excepted).
- **`HashMap` design intent** is right — open addressing, power-of-two mask, FNV h1/h2 split, 0.7 load factor, and the zero-copy `get_ref`/`get_mut`/`value_ref` accessors already exist; it's the disabled SIMD and tombstone handling that hold it back, not the shape.
- **Str phase1k** is a clever way to get O(1) length while staying `char*`-ABI-compatible — the idea is sound; it just isn't free and `Text` forgot to use it.
- **C-runtime scoping is disciplined:** what remains under `compiler/runtime` + `lib/std/runtime` is correctly limited to OS/crypto/net/zlib FFI and SIMD helpers — no hot pure-algorithm code left in C.

## Top 3 highest-leverage recommendations

1. **Fix generic+SIMD-inside-`lowlevel` codegen, then re-enable HashMap group-scan.** By the project's own numbers a ~2.25× win on the most-used collection (`hashmap.tml:223-225`), and it unblocks SIMD in *all* generic library code — the single highest-ROI item in this area.
2. **Give `List` a borrowing cursor / `Duplicate` so iterators stop deep-copying.** Deletes the eager clone-snapshots in Deque, BTreeMap, and HashMap (L-084) — a whole class of hidden O(n) allocations — and resolves F-016 at the root rather than per-collection.
3. **Land the phase44c literal-size lint and migrate collection headers to `size_of[T]()`-computed sizes.** Closes the systematized drift bug-class (L-080) that already burned months as "floating flaky," and is the first concrete step toward replacing the `*Unit`-blob headers with real typed structs.
