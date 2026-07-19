# phase26e — Collection Borrow Accessors (the zero-cost enabler)

> Addresses F-021 (08-memory-copy-audit): collections have NO working borrowing
> accessor, so every `get` of a handle-bearing element deep-clones. This is the
> single piece that turns "correct + leak-free" (delivered by 26b) into
> "zero-cost" (the actual Rust-parity thesis). Depends on phase26b (real move/drop
> model) being landed first — a borrow into a container is only sound once the
> drop model can track its lifetime. New language + codegen surface, not a stdlib
> patch. NOTE (ADR-009 revised): the interior-pointer codegen targets the AST-legacy
> path (what real programs run), not MIR; the MIR flip was deferred to phases 30-33.

## 1. Implementation
- [x] 1.1 Interior-pointer codegen (2026-07-19, Cluster A): DONE via two complementary mechanisms on the AST path (MIR deferred per header note, matching the slice-intrinsic model). (a) Source-level: `ref (*typed_ptr)` is a zero-instruction pointer forward and `ref (*ptr).field` a single GEP (unary.cpp:124-132 / :185-211,531-536 — proven by Sync.get_ref + List::retain); accessors use these in pure TML. (b) Intrinsics for the type-erased buffers: `list_get_mut` (header pointer-math GEP), `ptr_as_ref[T]`/`ptr_as_mut[T]` (inttoptr) — all previously referenced-but-unimplemented (i32-default hole), now real. Caller-side chains fixed: C027 RefType unwrap (A2) + two-step verified working at HEAD (A3). IR-verified zero-copy (no alloca/memcpy).
- [ ] 1.2 Borrow-checker lifetimes: bind the returned reference's lifetime to the container borrow so use-after-invalidation (get_ref then push/rehash) is a compile error; extend below the `lowlevel { *ptr }` boundary the checker is currently blind to
- [x] 1.3 Read accessors (2026-07-19, Cluster B): `List::get_ref(i) -> ref T` (bounds-panic + elem-ptr `ref (*ep)`), `HashMap::get_ref(k) -> Maybe[ref V]` (probe loop, `Just(ref (*val_ptr))`), `BTreeMap::get_ref(k) -> Maybe[ref V]` (find_index → `values.get_ref`), `Deque::get_ref(i) -> ref T` (bounds-panic + ring-slot math → `data.get_ref`). All zero-alloc, no clone (IR-verified on `List[Row].get_ref`: GEP + ptr return, no alloca-aggregate/memcpy/duplicate). Maybe[ref V] verified working end-to-end (concrete + generic).
- [x] 1.4 Mut accessors (Cluster A + Cluster C, 2026-07-19): `list_get_mut` intrinsic + `IndexMut` sugar (Cluster A). `HashMap::get_mut(k) -> Maybe[mut ref V]` via `ptr_as_mut[V]`, `BTreeMap::get_mut -> Maybe[mut ref V]`, `Deque::get_mut(i) -> mut ref T`, `List::get_mut(i) -> mut ref T` — all write-through verified.
- [ ] 1.5 Borrowing iterators: `iter_ref`/`values_ref` yielding `ref T`; make the existing by-value iterators either move-out (consuming) or delegate to the ref form so the F-019 asymmetry is resolved coherently
- [ ] 1.6 Migrate hot stdlib call sites (List sort/dedup/contains, BTreeMap shift, HashMap duplicate/to_string) from `get`(clone) to `get_ref` — IR-verify zero alloc on `List[Str]` sort and `HashMap[Str,Str]` iteration
- [ ] 1.7 `Text` borrow view: `Text::as_str_ref -> ref str`-style borrow so an SSO `Text` can be read as a string without a fresh alloc+copy (F-08 minor: `Text.as_str` currently allocates; `Text.concat` double-copies inline results, std/text.tml)
- [ ] 1.8 Benchmark vs Rust: `HashMap::get`/`List::get`/iteration on handle-bearing elements must emit zero heap traffic (match Rust's `&T` return); record in a perf doc

## 2. Tail (docs + tests — check or waive with tailWaiver)
- [ ] 2.1 Update or create documentation covering the implementation
- [ ] 2.2 Write tests covering the new behavior
- [ ] 2.3 Run tests and confirm they pass

## Cluster A — codegen foundations (2026-07-19, DONE)

Three dead intrinsics implemented (AST-legacy path; slice/collection element
intrinsics are AST-only — no MIR `IntrinsicKind`, matching `slice_get_mut`):

- **A1** `list_get_mut(handle,idx) -> mut ref T` (intrinsics_slice_simd.cpp) —
  loads data ptr + stride from the 32-byte List header, `mul`, byte-GEP →
  element ptr. Zero-copy (IR-verified: pure load/gep/mul, no alloca/memcpy).
  `ptr_as_ref[T]`/`ptr_as_mut[T](addr:I64) -> ref/mut ref T` (intrinsics.cpp) —
  `inttoptr` (pass-through if already ptr). All three added to the name set.
- **A1 blocker (generic-trait-dispatch "index_mut returns ()")** ROOT-CAUSED +
  FIXED: it was a TYPE-checker bug — `check_index` (expr_ops.cpp) only handled
  Array/Slice and returned `()` for named collections, so `list[i] = v` saw
  `()` vs `T`. Now resolves the element type from the `index`/`index_mut` impl
  method (substituting receiver type args). Plus codegen routing:
  `gen_container_index_via_method` (collections.cpp) desugars `container[idx]` /
  `container[idx] = v` to `index`/`index_mut` method calls (reusing full
  dispatch + generic instantiation); wired into `gen_index` (read) and the
  index-assign branch in binary.cpp (write, stores through the returned ptr).
- **A2** C027 "Cannot resolve field access object" on ref-returning calls
  (`outer.get_ref().field`, phase27a Class 6a) FIXED in struct_field.cpp: the
  CallExpr/MethodCallExpr field-access branch now unwraps a `RefType`/`PtrType`
  return before the NamedType struct-resolution (the returned ptr already points
  at T; no extra load), mirroring the Deref/ptr-struct branches.
- **A3** two-step ref shape (`let b: ref Row = outer.get_ref()` then
  `b.field` / method-through-`b`) — VERDICT: already compiles + runs correctly
  at HEAD (annotated AND inferred bindings), no K001. The phase27a Class 2
  declared-signature coercion (commit d39333dd) covers it. No fix needed.

Blocked tests un-stubbed as real tests: `lib/core/tests/ptr/non_null_refs.test.tml`
(renamed from non_null_blocked; as_ref/as_mut) and
`lib/std/tests/collections/list_index_mut.test.tml` (list[i]=v, read, mixed).

## Cluster B + C — pure-TML accessors (2026-07-19, DONE, NO compiler changes)

Files: `lib/std/src/collections/{list,hashmap,btreemap,deque}.tml` (accessors +
zero-copy/invalidation docstrings). Tests (new):
`lib/std/tests/collections/{list_get_ref,hashmap_get_ref,btree_deque_get_ref}.test.tml`
(refcount-probe model: `get` bumps a nested `Shared` handle, `get_ref` must NOT;
`get_mut` write-through). All pass individually and in the full suite.

- **B2 verdict — `Maybe[ref V]` WORKS end-to-end.** Probed first (concrete
  `Maybe[ref I64]` + generic `Maybe[ref T]` via a helper over `List.get_ref`):
  construct `Just(ref)`, pattern-match, read through → correct values (not
  garbage → ref not mis-typed to i32). So HashMap/BTreeMap use the `Maybe[ref V]`
  / `Maybe[mut ref V]` form (not the panic fallback). Deque uses plain
  `ref T`/`mut ref T` to mirror `Deque::get`'s panic-on-OOB contract.
- **IR spot-check** (`List[Row].get_ref`, O0): returns `ptr`; element address via
  `getelementptr inbounds %struct.List__Row` (data field) + int arith; NO memcpy,
  NO duplicate/ptr_read_clone call. Scalar allocas are O0 i64-local slots, not a
  Row-aggregate copy.
- **Compiler gap found (NON-blocking, worked around test-side; report filed):**
  in the shared-stdlib codegen-state path a test-local `type Row { payload:
  Shared[I64], … }` collides with `std::sqlite::Row` in the by-name global
  struct-layout cache → `[K001] insertvalue … %struct.Shared__I64 instead of
  ptr`. Passes standalone; only the multi-suite state-capture path trips it.
  Fixed here by renaming the probe structs to unique names (`ListRcRow` /
  `MapRcRow` / `BtdRcRow`). Root-cause repro for the codegen agent:
  `.sandbox/phase26e_compiler_gap_struct_name_collision.md`.

Verification (Cluster B+C): std/collections 96 passed / 3 skipped / 0 failed
(baseline 96); core/alloc 44/45 (the 1 is the pre-existing floating flaky —
crashes rotate shared_weak↔shared_getmut in-batch, both green standalone ×2).
Determinism gate ×10 adversarial: 28/28 targets at or above floor.

**Regression found+fixed along the way** (in prior-session `gen_structural_duplicate`,
duplicate.cpp): for a handle-bearing aggregate read via `ptr_read_clone` whose
field duplicate wasn't yet generated, it (1) recursed mid-emission, interleaving
a nested `define` into the current body → invalid IR ("multiple definition of
'ret'"), and (2) for a LIBRARY handle field (Shared__I64) wrongly
structural-synthesized a bitwise copy (no refcount bump) instead of the real
`impl Duplicate`. Fix: build the body in a local buffer (append after recursion),
and for library-handle fields queue the real duplicate via
`pending_impl_method_instantiations_` (mirrors drop-glue field-drop queueing);
only local aggregates recurse. Surfaced by the determinism corpus
(f002_list/f002_hashmap/churn_uzdb_shape).

Verification: non_null_refs + list_index_mut pass; std/collections 96, core/str 33,
compiler/borrow 12, core/alloc 36/37 (the 1 is the pre-existing floating X002/X003
core/alloc flaky — victim rotates, all green standalone; NOT shared_get_sound).
Determinism gate: all TML targets at floor (move/drop probes are adversarial-
allocator flakies that recover on re-run; the residual `c_essential_repro` is a
pre-existing environmental exit-127 flaky on the FROZEN C frontend `tml cc
--emit=ast`, untouched by this work). shared_get_sound (prior task) still green.
core/ptr suite has a pre-existing composition-sensitive "Unknown method: unwrap"
(Maybe::unwrap in ptr.test.tml, passes standalone) — unrelated to this work.
