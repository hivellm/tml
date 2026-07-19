# phase27a — K001 Root-Cause Sweep (Stabilization ERA 0, Phase C1)

> Analysis: `docs/analysis/tml-table-analysis/03-codegen-stability.md` (F-005, F-007).
> Fix the MECHANISM (mangling/monomorphization mismatches), not the instance —
> the release history shows one-off K001 fixes (0.3.38/0.3.46) keep being followed
> by new instances of the same shape. Requires phase25b (verifier) landed so
> fixes are protected.
>
> **Fresh specimens (found 2026-07-15 while authoring the phase25a corpus —
> all pass `tml check` and fail compilation; details in
> `compiler/tests/determinism/f013_refcount_cycles.test.tml` header):**
> 1. `outer.get_ref().payload.get()` on `Shared[Row]` → "Cannot resolve field
>    access object" (chained field access through ref-returning method).
> 2. Same expression two-stepped (`let b = outer.get_ref()` then
>    `b.payload.get()`) → K001 `%tN defined with type 'i32' but expected 'ptr'`.
> 3. Module-composition-sensitive mono queueing: a test file that compiles
>    alone gets K001 `use of undefined value @...Shared__I64.duplicate` when
>    other tests coexist in the module.
> 4. `@derive(Duplicate)` glue referencing nested `Shared[I64]::duplicate`
>    without queueing its instantiation → same undefined-value K001 even in a
>    single-file module (when an explicit `.duplicate()` loop coexists).
> 5. (2026-07-16, phase26b step 3) `HashMap[I64, Shared[I64]]` — a smart
>    pointer as the map VALUE type directly — K001 `'%tN' defined with type
>    'ptr' but expected '%struct.HashMap__I64__Shared__I64'`. The
>    struct-wrapped shape (`HashMap[I64, ValRow{payload: Shared[I64]}]`)
>    compiles fine; see the workaround note in
>    `compiler/tests/determinism/f022_destroy_releases.test.tml`.

## 1. Implementation
- [ ] 1.1 Root-cause `std/collections` K001 (btreeset / btreemap / arraylist) — these are the flagship container suites; trace the exact verifier error to the emitting code path (MIR vs legacy)
- [ ] 1.2 Root-cause `hir_types` + `infer_differential` K001
- [ ] 1.3 Root-cause `c_preprocessor` K001 and `c_frontend` `Maybe[Heap[CBlockItem]]` K001
- [ ] 1.4 Root-cause the `future_fuse` / `cache_aligned_box` / `cache` / `cache_soavec_set` K001 class (surfaced by phase24l Attempt 3 — generic monomorphization of `Shared` users; likely same mechanism as 1.1)
- [ ] 1.5 Audit the impl-level type-param re-inference path (`method_impl.cpp` heuristics fixed in 0.3.46) and the struct-by-value ABI fixups (0.3.39) for remaining holes; replace heuristics with authoritative signature data where possible
- [ ] 1.6 Gate: zero K001 across compiler + core + std suites with the verifier (phase25b) as hard error

## 2. Tail (docs + tests — check or waive with tailWaiver)
- [ ] 2.1 Update or create documentation covering the implementation
- [ ] 2.2 Write tests covering the new behavior
- [ ] 2.3 Run tests and confirm they pass

## Findings (2026-07-19) — specimen #3/#4 class: `ptr_read_clone` on handle-bearing aggregate

`lib/core/tests/alloc/shared_get_sound.test.tml` `test_get_struct_nested_shared_bumps_and_restores`
was NOT a Class-1 dedup/composition divergence. The codegen is **uniformly** wrong
in both standalone and suite modes; the module IR is byte-identical (normalised)
between the two. The apparent "standalone passes / suite fails" split was a
**test-harness false-pass**: the standalone dispatcher emits `test_pass` even when
the test body panics on an assertion (verified by running `core_alloc.exe --run-all`
directly — it panics on `:39` yet reports `passed:1`). The suite's crash detection
correctly surfaced the same panic (misattributed to a co-packed file).

Root cause (AST codegen path, used by `tml test` for import-bearing files):
- `ptr_read_clone[T]` (intrinsics.cpp) gated auto-clone on `has_duplicate_impl`
  (explicit `impl Duplicate` / `@auto` / `@derive` only). A local
  `type BoxedCounter { inner: Shared[I64], tag: I64 }` has none, so it hit the
  POD bitwise fallback — the copy aliased the nested `Shared` handle, no refcount
  bump. The detection never checked that `T` *transitively* owns handles.
- Symmetric other half: `llvm_ir_gen_stmt_let.cpp` `suppress_field_drops` skipped
  field-level drops for any method/call-initialised `let` lacking a DIRECT Drop
  impl — so even once cloned, the copy was never dropped and the count stayed
  inflated.

Fix (mode-independent, symmetric with drop-glue):
- New `LLVMIRGen::gen_structural_duplicate` + `struct_has_droppable_field`
  (derive/duplicate.cpp) synthesise field-wise clone glue for a needs-drop
  aggregate with no explicit Duplicate — deep-cloning each droppable field
  (calling its `duplicate`, recursing for nested locals, Str via `Str::duplicate`)
  and bitwise-copying genuine PODs.
- `ptr_read_clone` now clones when `env_.type_needs_drop(T) ||
  struct_has_droppable_field(T)` (falls back to bitwise for genuinely handle-free
  PODs — conservative fallback preserved).
- `suppress_field_drops` now still registers field-level drops when the read-out
  type transitively owns handles, restoring the counts the clone bumped.

Verification: standalone shared_get_sound passes (genuinely, no panic); `--suite=core/alloc`
44/45 (shared_get_sound green — remaining failure is the pre-existing floating
X003 heap-corruption flaky that changes victim every run: shared_get_sound /
shared_getmut / sync_refcount, all green standalone); determinism-gate 10 → 28/28
at floor; core/hash and compiler/borrow canaries clean.

NOTE (separate latent bug, out of scope here): the standalone test dispatcher
reports `test_pass` for a body that panics mid-run. This masks real failures and
should be fixed in the test-dispatcher codegen (`testing_dispatcher_gen.cpp`).
