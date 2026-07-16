# phase27a — K001 Root-Cause Map (execution spec)

Read-only scoping, 2026-07-16. Mechanisms (not instances) behind every catalogued
K001; all on the AST-legacy path (`generate_pending_instantiations()` loop).

## Two cross-cutting drivers

- **Queue dedup split across 3 inconsistent sets**: `generated_functions_` (keys
  with leading `@`), `generated_impl_methods_` (bare mangled keys, queue dedup),
  `generated_impl_methods_output_` (emission dedup, `generic_instantiate_impl.cpp:522`).
  A symbol can be "seen" in one set by a path that never emits a body → composition
  sensitivity.
- **`i32` is the silent default** for unresolved types; sites that re-derive from
  `last_expr_type_`/`infer_expr_type` instead of the registered signature drift.

## Class 1 — undefined `…_drop`/`…_duplicate` (mono-queueing) — size M, order 2nd

Reference sites that DON'T queue:
1. Derive-duplicate glue: `derive/duplicate.cpp:258-263` (generic) and `:174-181`
   (non-generic) emit `call @<Field>_duplicate` with NO `PendingImplMethod` push
   (the drop path DOES queue, `drop.cpp:549-561`). = specimen #4.
2. `emit_drop_call` fallback `drop.cpp:401-405`: synthesizes + calls a drop symbol
   without queueing (unlike `emit_field_level_drops`/`register_for_drop`). =
   DualMutexInner (sync_barrier/sync_isolation).
3. Split dedup sets (above) = specimen #3 composition sensitivity; `empty_subs` at
   `drop.cpp:552` compounds (inner drop generated without substitutions).

**Fix:** every reference site also queues; unify dedup keyspace.
**Resolves:** sync_barrier, sync_isolation, f013 specimens #3/#4, future_fuse/
cache_aligned_box/cache family. Confidence High.

## Class 2 — `iN` vs `iM` / struct-vs-ptr (re-inference family) — size L, order 1st

Remaining heuristic holes (0.3.46 fixed only method_impl's impl_self_type_args case):
1. `method_impl.cpp:1392` seeds `expected_type = "i32"`; only overridden when
   func_sig/functions_ has the param (`:1398-1418`).
2. Class static dispatch `method_static_dispatch.cpp:216-280` builds args from
   `last_expr_type_` (`:260-262`) and ret from unsubstituted `m.sig.return_type`
   (`:250-254`) — no reconciliation with registered FuncInfo. = arraylist
   `{i64,i64}` vs i32.
3. Hardcoded `ret_type = "ptr"` fallback `method_static_dispatch.cpp:1202-1204`. =
   specimen #5 shape.
4. Single-type-arg mangled parser `method_static_dispatch.cpp:95-105` (vs the
   multi-arg tokenizer in `generic_instantiate_impl.cpp:250-285`) — misgroups
   `HashMap__I64__Shared__I64`. = specimen #5.

**Fix:** registered `FuncInfo.param_types`/`ret_type` as source of truth in all
three dispatch paths; coerce to registered type instead of seeding i32/ptr; swap in
the multi-arg mangled parser.
**Resolves:** arraylist, btreeset, btreemap, bigint, replay_subject, specimen #5,
hir_types, infer_differential, c_preprocessor, c_frontend — plus unblocks Classes
5 and 6b. Confidence High (mechanism), Medium (per-suite attribution).

## Class 3 — "void type only allowed for function results" — size S, independent

`once.tml:325-342` `get_or_init` passes a Unit-returning closure to `call_once`;
`llvm_type_from_semantic(Unit)` = "void"; the closure-return/slot path lacks the
Unit guard the receiver path has (`method_impl.cpp:1382`) → `alloca void`.
**Fix:** apply the same guard in closure/return-slot lowering. Resolves once_lock.

## Class 4 — "Cannot allocate unsized type" — size S-M, independent

`console.tml:344` template literal interpolates a `List[Str]` element; the
interpolation lowering allocas the value type, which resolves to an opaque
(forward-declared-only) struct in the module.
**Fix:** don't alloca opaque aggregates in template-interpolation lowering; use the
canonical Str value/pointer form. Resolves grouping_assert_table.

## Class 5 — "integer constant must have integer type" — size S-M, after Class 2

The 3 failing h2 suites are the only ones exercising H2Connection + hpack
(encode_headers → hpack_encode_headers, process_frame). An integer const is emitted
with a mis-resolved aggregate expected type — the Class 2 mechanism surfacing on a
constant operand. **Fix:** verify Class 2's signature-truth fix resolves; else
localized in the hpack/connection const-emit path. Resolves h2_build_response,
h2_flow_control, h2_connection_streams.

## Class 6 — get_ref chained field access — 6a size S independent; 6b via Class 2

6a ("Cannot resolve field access object", specimen #1): `struct_field.cpp:689-770`
CallExpr branch never unwraps RefType/PtrType (`:711` requires NamedType; `:751-761`
unwraps only NamedType) → falls to `report_error` at `:773-774`. The deref path
(`:640-687`) and ptr-struct path (`:787-807`) DO unwrap — mirror them.
6b (i32-vs-ptr two-step, specimen #2): `ref T` method return typed via the i32
default = Class 2.

## Execution order

**Class 2 → Class 1 → 6a, 3, 4 (parallelizable) → Class 5 (verify).** All fixes
protected by the phase25b hard verifier. Every class has a catalogued repro in
`scripts/known-failures.txt` + `compiler/tests/determinism/f013_refcount_cycles.test.tml`
header + phase27a tasks.md specimens.
