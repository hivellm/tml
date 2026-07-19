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
- [x] 1.2 Borrow-checker lifetimes (2026-07-19, Cluster D — mechanism delivered; push-class residual re-scoped to phase26g, see LIMITATION below): interior-reference lifetime binding WIRED in the NLL checker (borrow/checker_expr.cpp check_method_call + checker_stmt.cpp check_let). Narrow, confident-only method resolution (receiver type via place/get_expr_type → `Type::method` FuncSig): a ref-returning method over a place records a borrow of the receiver bound to the LHS ref (create_borrow_with_projection, ref_place=lhs); NLL extends it to the ref's last use. Conflicts fire ONLY against live interior-ref borrows THIS wiring created (ref_place != 0) — two `mut ref`s, or `mut ref` while a shared interior ref lives → B009. VERIFIED: negatives reject (compiler/tests/borrow/interior_ref/ + cli/borrow_interior_ref.sh), positives compile+run (lib/std/tests/collections/list_interior_ref.test.tml), blast-radius sweep ZERO new borrow errors (collections/alloc/str/hash/json/borrow + stdlib files), determinism 28/28. LIMITATION: `list.push(x)` invalidation is NOT caught — `List.push` is `this` (not `mut this`; handle-based), and making the whole mutator surface `mut this` has a massive type-level blast radius (immutable-collection mutation is pervasive: proven — 20+ std tests break). Catching push-class invalidation requires a separate coordinated mutator-signature change; the wiring already handles it the instant those methods become `mut this`. FILED as `phase26g_collection-mutator-mut-this` (2026-07-19) with the measured blast radius and the decision framing (`mut this` migration vs `@invalidates_refs` attribute — user decision, same class as moves decision #12). Not extended below `lowlevel { *ptr }` (callee bodies stay trusted — call-site rule only, per spec point 4).
- [x] 1.3 Read accessors (2026-07-19, Cluster B): `List::get_ref(i) -> ref T` (bounds-panic + elem-ptr `ref (*ep)`), `HashMap::get_ref(k) -> Maybe[ref V]` (probe loop, `Just(ref (*val_ptr))`), `BTreeMap::get_ref(k) -> Maybe[ref V]` (find_index → `values.get_ref`), `Deque::get_ref(i) -> ref T` (bounds-panic + ring-slot math → `data.get_ref`). All zero-alloc, no clone (IR-verified on `List[Row].get_ref`: GEP + ptr return, no alloca-aggregate/memcpy/duplicate). Maybe[ref V] verified working end-to-end (concrete + generic).
- [x] 1.4 Mut accessors (Cluster A + Cluster C, 2026-07-19): `list_get_mut` intrinsic + `IndexMut` sugar (Cluster A). `HashMap::get_mut(k) -> Maybe[mut ref V]` via `ptr_as_mut[V]`, `BTreeMap::get_mut -> Maybe[mut ref V]`, `Deque::get_mut(i) -> mut ref T`, `List::get_mut(i) -> mut ref T` — all write-through verified.
- [x] 1.5 Borrowing iterators (2026-07-19, Cluster E): `List::iter_ref() -> ListRefIter[T]` (`impl Iterator` with `type Item = ref T`, `next()` = pointer-step + `ref (*ptr)`, no clone) and `HashMapIter::value_ref() -> ref V` (zero-copy sibling of `value()`). F-019 resolved pragmatically: by-value `iter()`/`value()` STAY documented clone-read (semantics unchanged, sound); `iter_ref`/`value_ref` are the zero-cost opt-in — docstrings on both state which is which. **Deque.iter_ref SKIPPED** (ring wraparound `(head+i)%cap` is a bespoke iterator, not a natural List delegation; zero-copy indexed borrow already covered by `Deque::get_ref(i)`). **BTreeMapIter** left as snapshot semantics (its iter double-copies by design) — noted, not converted. Tests: iter_ref no-bump vs iter by-value bump, value_ref no-bump — all pass.
- [x] 1.6 Migrate hot call sites (2026-07-19, Cluster F): List `contains`/`index_of`/`binary_search`/`dedup`(compare)/`_quicksort`(compare) → `*get_ref` in-place compare; `_quicksort_by` → `cmp(get_ref(j), ref pivot)` (cmp param is already `ref T`); BTreeMap `find_index` → `keys.get_ref(mid).cmp(key)`; HashMap `duplicate` → `value_ref().duplicate()` (2 clones → 1). **NOTE: the task's premise that `ref` operands auto-deref in binary ops is FALSE** (`get_ref(i) == v` → K001 `ptr` vs `i64`); migration uses explicit `*get_ref(i)`. Element MOVES (swap, dedup compaction-set) kept as value copies. Soundness for handle-bearing T PROVEN via a `Shared`-bearing `PartialEq`/`PartialOrd` probe (contains/sort: stored refcount unchanged, no double-free) + determinism corpus 28/28. IR-verified: `List[Str]::_quicksort`/`contains` loop reads via `get_ref` (borrow), no `ptr_read_clone`/`duplicate`, swaps preserved, 1 intentional pivot clone.
- [x] 1.7 `Text` borrow view (2026-07-19, Cluster G): G1 — `Text::as_str_ref(this) -> Str`: heap-backed storage is NUL-terminated in place (verified: `text_push_str_ptr`:396, `text_from_raw`:233, grow:279, clear:587), so heap Text returns the internal buffer directly (zero-copy); inline/SSO falls back to allocating `as_str()` (bytes live in the value, 23-byte case has no in-place NUL). G2 — killed `concat`/`concat_str` double-copy: heap path now single-allocs the result buffer and copies both sources directly (was scratch-alloc + `text_from_raw` re-alloc + copy + free); inline path packs both sources into the SSO words via new `text_concat_inline` (zero alloc). Byte-correct across the inline↔heap (23-byte) boundary. **Compiler gap found + worked around:** `Str→I64→*U8` double-cast miscompiles in the shared-stdlib-state batch (K001 `i64` vs `ptr`); fixed by the proven single cast `s as *U8`.
- [x] 1.8 Benchmark vs Rust (2026-07-19): Rust baseline captured via rust-reference probes (rustc 1.96.0, Rc-field-as-probe over `Row{payload,tag}`, O0+O1 IR in `.sandbox/rustbench/`); invariants: indexed borrow read = (bounds-check)+1 GEP+N loads, map read = probe+GEP+loads, iter body = pointer step+GEP+load — 0 alloc/clone/refcount in every read path. TML borrow API verified at PARITY per operation (IR + runtime refcount probes); hot internal sites (contains/sort/find_index/duplicate) closed. Recorded in `docs/analysis/tml-table-analysis/09-borrow-accessors-perf.md` (incl. what stays a clone by design and caveats).

## 2. Tail (docs + tests — check or waive with tailWaiver)
- [x] 2.1 Documentation: zero-copy + invalidation docstrings on every new accessor/iterator (List/HashMap/BTreeMap/Deque get_ref/get_mut, ListRefIter, value_ref, as_str_ref); F-019 by-value-vs-ref coherence note on iter()/value(); module-level as_str_ref contract.
- [x] 2.2 Tests (2026-07-19): `iter_ref.test.tml` (E), `text/text_as_str_ref.test.tml` (G1), `text/text_concat_nocopy.test.tml` (G2) added; plus Cluster B/C accessor tests from the prior session. Migration soundness covered by handle-bearing probes (folded into verification).
- [x] 2.3 Run tests: std/collections 102/102, core/str 33/33, std/text 7 pass + 1 pre-existing skip / 0 fail, core/alloc 44/45 (the 1 is the pre-existing floating flaky — victim rotates shared_weak/shared_getmut/shared_refcount in-batch, all green standalone). Determinism gate ×10 adversarial: 28/28 at/above floor.

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

## Cluster D — struct-layout collision + borrow-lifetime wiring (2026-07-19)

### D0 — bare-name struct-layout collision under shared-stdlib state (FIXED)
A test-local `type Row { payload: Shared[I64], tag: I64 }` collided with
`std::sqlite::Row { stmt_handle: ptr, num_columns: I32 }` by BARE NAME under the
shared-stdlib codegen-state path. Root cause: `CodegenLibraryState.struct_types/
struct_fields` are keyed by bare name; the restore in generate.cpp injects
`sqlite::Row`'s layout, then gen_struct_decl's `if (struct_types_.find(name) !=
end()) return;` guard makes the local `Row` INHERIT the library layout →
`insertvalue %struct.Shared__I64` into a `ptr` slot (K001), or silent field
corruption on the GEP path. (Proven with an import-sqlite + local-Row probe:
`%struct.Row = type {ptr,i32}` used for the local Row.)
Fix (generate.cpp): pre-scan the CURRENT module's struct/enum decl names; at
restore, SKIP any library `struct_types/struct_fields/enum_variants/instantiation-
guard` entry whose bare name the module redefines (local shadows library, mirrors
the existing `local_generic_struct_names_` handling for generics), AND strip the
shadowed `%struct./%union./%class./%enum.<Name> = type ...` line from the restored
`imported_type_defs` text so the locally-emitted def does not collide
("redefinition of type"). Regression test: lib/std/tests/collections/d0_row_collision.test.tml
(un-renamed local `Row`, passes in the full collections suite where sqlite::Row is
captured by test_bootstrap.tml).

### D1 — see item 1.2 above.
Diagnostic text produced (existing BorrowError messages, reused): "cannot borrow
`c` as mutable more than once at a time" (two mut refs); "cannot borrow `c` as
mutable because it is also borrowed as immutable" (mut ref while shared ref live).
