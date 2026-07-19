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
- [ ] 1.1 Interior-pointer codegen: emit a GEP into the type-erased `*Unit` backing buffer of `List`/`HashMap`/`BTreeMap`/`Deque` that yields a `ref T`/`mut ref T` without copying the element (both codegen paths, or the unified MIR path if 26b step 3 retired the AST path)
- [ ] 1.2 Borrow-checker lifetimes: bind the returned reference's lifetime to the container borrow so use-after-invalidation (get_ref then push/rehash) is a compile error; extend below the `lowlevel { *ptr }` boundary the checker is currently blind to
- [ ] 1.3 Read accessors: `List::get_ref(i) -> ref T`, `HashMap::get_ref(k) -> Maybe[ref V]`, `BTreeMap::get_ref`, `Deque::get_ref` — zero-alloc, no clone
- [~] 1.4 Mut accessors: `list_get_mut` intrinsic IMPLEMENTED + `IndexMut` sugar (`list[i] = v`) fully working end-to-end (Cluster A A1). `HashMap::get_mut`, etc. still pending.
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
