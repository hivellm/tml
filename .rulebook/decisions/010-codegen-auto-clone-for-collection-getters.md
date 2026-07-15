# 10. Codegen-level auto-clone in HashMap.get / List.get via ptr_read_clone[T] intrinsic

**Status**: proposed
**Date**: 2026-05-05
**Related Tasks**: phase24n_cc-preproc-aliasing-sweep, phase24m_essential-c-residual-segv, phase24l_shared-get-aliasing-deep-fix, phase24k_essential-cleanup-segv, phase24g_heap-rc-or-borrow-language-fix, phase0z_cc-driver-essential-c-residual

## Context

Phase24k diagnosed the `Shared.get / HashMap.get / List.get` bitwise-alias double-free bug class. Phase24l (ADR-009) added an opt-in `Shared.get_clone(this) -> T where T: Duplicate` and migrated one proven site (the typedef arm of `base_to_ctype`). Phase24m landed 15+ surgical `.duplicate()` calls across `compiler-tml/src/cc/preproc/` to address the most prominent aliasing sites. Both whittled the symptom but did not close it: the bug class is broader than any one set of patches can cover, and reactive site-by-site sweeps invite regression (phase24l attempt 2 regressed minimal_repro from 29/30 to 25/30 by over-applying `.get_clone()`).

The user-facing requirement is that `HashMap[K, V]::get(k)` and `List[V]::get(i)` produce a value whose drop is safe regardless of how many concurrent gets happen on the same slot. The natural lib-level fix — make these getters call `value.duplicate()` — requires `where V: Duplicate` constraints, which would force every value type stored in a HashMap or List to have a Duplicate impl. That is a global breaking change and was explicitly rejected as option (a) in phase24m.

## Decision

Move the deep-clone responsibility into the TML codegen layer. Add a new memory intrinsic `ptr_read_clone[T](ptr) -> T` that:

1. Performs a bitwise load (same as `ptr_read[T]`).
2. Detects whether `T` has a non-trivial `Duplicate` impl by inspecting the codegen's struct/enum/impl registries (`struct_decls_`, `pending_generic_structs_`, `pending_generic_enums_`, `pending_generic_impls_`, `pending_generic_impls_all_`).
3. If non-trivial Duplicate is available, spills the bitwise value to an alloca and emits an explicit `T::duplicate(spill)` call. The spill alloca is NOT registered for drop — its inner refcounted handles still belong to the source slot, while the duplicated result owns its own bumped refcounts.
4. If `T` is primitive (i1/i8/i16/i32/i64/i128/f32/f64/ptr) or a pure-POD aggregate without a Duplicate impl, falls through to bitwise read.

Switch the user-facing canonical container getters in lib/std to use the new intrinsic:
- `HashMap[K, V]::get(this, key) -> V` — `lib/std/src/collections/hashmap.tml`
- `HashMap[K, V]::get_opt(this, key) -> Maybe[V]`
- `List[T]::get(this, index) -> T` — `lib/std/src/collections/list.tml`

Internal `ptr_read[I64]` / `ptr_read[I8]` reads of header fields and control bytes stay unchanged — those types are primitive and `ptr_read_clone` would emit identical IR.

Implement the intrinsic in BOTH codegen pipelines:
- AST path: `compiler/src/codegen/llvm/builtins/intrinsics.cpp`.
- MIR path: `compiler/src/codegen/mir/instructions_call.cpp::emit_intrinsic_ptr_read_clone` with a static `mangle_itanium_path` helper and a `canonical_module_for_type` table for the well-known refcount libraries (`Shared`, `Heap`, `Box`, `Rc`, `Arc`, `Cell`, `RefCell`, `List`, `HashMap`, `HashSet`, `Buffer`, `Maybe`, `Outcome`, `Str`).

To support library struct/enum types whose `@derive(Duplicate)` / `@auto(duplicate)` was skipped by the early-return in `gen_struct_decl` (the type was pre-registered by `runtime_modules.cpp` before the derive emitter pass), the intrinsic also drives `gen_derive_duplicate_struct` directly when emitting a clone for a non-generic local/library struct, gated by an idempotency check on `generated_functions_`.

## Alternatives Considered

- **Pure option (a) — modify `Shared.get` / `HashMap.get` / `List.get` to require `where V: Duplicate` and call `value.duplicate()`**: REGRESSED lib/core test suite (`cache_aligned_box`, `cache`, `cache_soavec_set`, `future_fuse` K001) and would force every Hash/List value type to have a Duplicate impl. Rejected in phase24l attempt 3.
- **Broad option (a) — migrate all ~40 `.get()` sites in `compiler-tml/src/cc/` to `.get_clone()`**: phase24m landed 15 such migrations and got `c_essential_repro.c` from 25/30 to 28/30, but did NOT close `essential.c` × 5. Each additional surgical site introduces regression risk; the bug class is broader than the canonical sweep can cover.
- **Option (c) — change `ptr_read[T]` semantics to auto-clone for non-primitive T**: REJECTED because `ptr_read` is a documented bitwise-read intrinsic with legitimate users in `option.tml::replace`/`get_or_insert` who explicitly want raw bitwise semantics. Auto-cloning there would leak refcounts.
- **Option (d) — borrow-checker enforcement (NLL refinement) that rejects use-after-move on Shared/Heap returns**: largest blast radius, requires every TML program to refactor; explicitly deferred per ADR-009 alternatives.

## Consequences

**PROS**:
- Eliminates the entire `HashMap.get` / `List.get` aliasing bug class structurally — zero per-site work for downstream consumers.
- All current and future TML code using `HashMap[K, Shared[T]]` or `List[Shared[T]]` (or any container with non-trivial-Duplicate value types) is now safe by default.
- Conservative detection means the intrinsic falls back to bitwise read for pure-POD types — no behaviour change for the 99% case.
- Zero source-level breaking changes for existing users.
- Preserves the surgical phase24l/24m fixes (they are now redundant but not harmful — the duplicate becomes a no-op refcount round-trip).

**CONS**:
- Increases codegen complexity (the AST-path emitter is ~170 lines, the MIR-path emitter is ~140 lines).
- Adds ~20% codegen-time overhead on cc_driver builds (48s → 58s) due to the per-call detection logic and the explicit spill+call sequences emitted into IR.
- The MIR `canonical_module_for_type` table is a static mapping that must be extended by hand when new refcounted libraries are introduced. Long-term, threading the type registry through to MIR codegen would let us look up modules dynamically.
- `essential.c` × 5 = 0/5 gate is NOT closed by this fix. The residual SIGSEGV is in `pp_sweep_in_file` recursion (per phase24m diagnostic) — a parser-level state aliasing class that does not flow through `HashMap.get` / `List.get`. A separate scope is required to address it.
- `Shared.get` itself is unchanged because its body uses `(*this.ptr).value` (struct-field deref), not `ptr_read`. Callers needing deep-clone of a Shared still use the explicit `Shared.get_clone()` from ADR-009. Adding auto-clone to `Shared.get` requires a different codegen hook.

## Implementation Notes

The intrinsic is registered in three places:

1. `compiler/include/codegen/intrinsic_table.hpp` — `IntrinsicKind::PtrReadClone`.
2. `compiler/src/codegen/intrinsic_table.cpp` — `{"ptr_read_clone", {IntrinsicKind::PtrReadClone, 1, true}}` lookup entry.
3. `compiler/src/codegen/llvm/builtins/intrinsics.cpp` — added to the `intrinsics` set so the AST dispatcher routes name resolution.

The MIR dispatcher case in `instructions_call.cpp::emit_call_inst` was extended to call `emit_intrinsic_ptr_read_clone`. Both code paths emit identical LLVM IR shapes for the same input.

Detection of "T has Duplicate impl" is conservative but covers the cases that matter:
- `@derive(Duplicate)` / `@auto(duplicate)` on a struct or enum decl (lookup via `struct_decls_` / `pending_generic_structs_` / `pending_generic_enums_`).
- Manual `impl[T] Duplicate for X[T]` in `pending_generic_impls_` or `pending_generic_impls_all_` — found when iterating impl methods looking for one named `duplicate`.
- Pure POD aggregate without any of the above → bitwise fallback.

The fix queues `T::duplicate` for instantiation via `pending_impl_method_instantiations_` so the lazy library generator emits the function body. For non-generic local types the recovery loop in `runtime_modules_library.cpp` would fail to pick up Itanium-mangled names, so the intrinsic drives `gen_derive_duplicate_struct` directly when needed (idempotency-guarded by `generated_functions_`).

## Future Work

1. Close the `essential.c` × 5 gate by addressing the residual `pp_sweep_in_file` parser-level aliasing class.
2. Replace the MIR `canonical_module_for_type` static table with a dynamic registry lookup so new refcounted libraries don't need codegen changes.
3. Move `gen_derive_duplicate_struct` invocation out of `gen_struct_decl`'s early-return gate so library struct decorators are emitted uniformly without needing the intrinsic to drive them.
4. Investigate auto-clone for `Shared.get` itself — currently requires a different codegen hook because the body uses a struct-field deref pattern.
