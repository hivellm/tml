# Proposal: phase27e_generic-free-func-monomorph

## Why
Generic **module free-function** calls emit an un-monomorphized, declaration-mangled
callee with no queued definition → `use of undefined value` at link/verify time. This
is the K001 root that:
- causes the **5 pre-existing `core/str` K001 failures** (`str_coverage2`, `str_advanced`,
  `str_transform`, `str_method`, `str_methods`), and
- blocks the phase41b shared-stdlib fast-path (F-006) via the identical
  `@tml_N4core7runtime3mem7replaceE_R1T1T` dangling ref (`core::runtime::mem::replace[T]`,
  defined `lib/core/src/runtime/mem.tml:205`).

Independently root-caused by two agents (phase41b executor + codegen-debugger), saved to
`.claude/agent-memory/codegen-debugger/shared-stdlib-fastpath-dedup-gap.md`.

**Root cause (file:line):** at `compiler/src/codegen/llvm/expr/call_user.cpp:452`, the
generic-free-function fallback (reached when `gen_call_generic_func` — `call.cpp:296` →
`call_generic_func.cpp:207` — returns nullopt) emits a call to the Itanium **declaration**
mangling (`mangle_tml_symbol`) and queues nothing. The parallel generic `Type::method`
path at `call_user.cpp:595-632` DOES re-mangle to the monomorphized name and queue
`require_func_instantiation`; there is **no equivalent block for generic module free
functions**. The `free_func_type_subs` computed at `call_user.cpp:506-593` (which holds the
concrete `T`) is used only for ret/arg types, never to fix the callee mangling. The literal
`T` in the mangled name (not `__I32`/`__Unit`) proves the call never entered
`require_func_instantiation`.

This is a C++ compiler codegen bug — **in scope** for ERA-0 stabilization (the C++ compiler,
not the frozen `compiler-tml` self-hosted tree).

## What Changes
Fix B (general — covers all generic free functions): in `call_user.cpp`, after building
`free_func_type_subs`, when the callee's `func_sig->type_params` is non-empty **and all
type params resolve to concrete types**, call `require_func_instantiation(bare, type_args)`
and override the emitted `mangled` callee name with the monomorphized name (mirroring the
`Type::method` block at 595-632). Guard strictly on all-concrete to avoid emitting for
still-generic contexts. Confirm the exact caller with a temporary diagnostic before editing
(per the codegen-debugger's handoff).

Scope boundary: this fixes ONLY the generic-free-function monomorphization gap. It is a
**prerequisite** for enabling the shared-stdlib fast-path (F-006) but does NOT alone enable
it — that additionally needs the `generated_impl_methods_output_` dedup-keyspace capture and
the i32-default range-iterator width fix (tracked separately, same memory file).

## Impact
- Affected specs: none
- Affected code: `compiler/src/codegen/llvm/expr/call_user.cpp` (+ possibly `generic.cpp`/`call_generic_func.cpp` if the miss must be resolved upstream)
- Breaking change: NO (emits a correct define where there was a dangling ref)
- User benefit: fixes 5 pre-existing K001 test failures now; unblocks the biggest remaining test cold-compile lever (F-006) as a prerequisite
