---
name: Observable Module Implementation
description: Codegen workarounds for Observable/Subject module — cross-module closures, struct GEP, List[func(T)]
type: project
---

## Observable Module (2026-03-19) — IMPLEMENTED

**Files**: `lib/std/src/observable/mod.tml`, tests in `lib/std/tests/observable/`
**Status**: 7 test files, all passing. ObservableI32 fully working. Subject/BehaviorSubject/ReplaySubject state-only (callbacks managed by caller).

### Codegen Bugs Encountered

1. **Cross-module closures**: Closures defined in library functions (mod.tml) do NOT get their LLVM symbols emitted when called from test files. Error: `use of undefined value '@tml_<func>_closure_N'`. Workaround: avoid closures in module code; use explicit loops or enum dispatch.

2. **Generic static method monomorphization**: `Observable::create(...)` returns `Observable__T` instead of `Observable__I32`. Same bug as Promise module. Workaround: concrete factory functions, no generic static methods.

3. **Struct GEP bug**: Any struct containing a `List[T]` field crashes codegen when passed by value to functions. Error: `'%subj' defined with type '%struct.X' but expected 'ptr'` on GEP. Affects single-field AND multi-field structs. Workaround: pass `List[I64]` directly instead of wrapping in a struct.

4. **List[func(T)] runtime crash**: `List[func(I32)]` compiles but crashes at runtime (access violation). The List stride calculation likely gets `size_of` wrong for fat pointer types `{ptr, ptr}`. Cannot store function pointers in Lists.

5. **lowlevel ptr_read/ptr_write on local variables**: `lowlevel { ptr_read[I64](ptr_offset(ref f, 0)) }` to extract function pointer components doesn't work — returns `ptr` type but callers expect `{ptr, ptr}`.

6. **Incremental cache stale results**: `mcp__tml__cache_invalidate` does NOT invalidate the `.incr-cache/incr.bin` incremental compilation cache. When the `GREEN: reusing cached codegen result` message appears with old errors, the only fix is to run the WHOLE test directory (which forces recompilation of the suite) or somehow invalidate incr.bin.

### Architecture Decisions

- **ObservableI32**: Enum-based source (`ObsSourceI32`) with `when` dispatch in `obs_subscribe_i32`. No closures or callbacks stored in the library. Operators collect values eagerly via `obs_collect_i32` helper.

- **SubjectI32**: State-only `List[I64]` with layout `[completed, has_error, next_id]`. Callback management delegated to caller. Full multicast not possible from library code due to fn-pointer storage bugs.

- **BehaviorSubjectI32**: Extends subject layout with `[3]=current_value`. Same state-only approach.

- **ReplaySubjectI32**: Extends subject layout with `[3]=buffer_size, [4]=value_count, [5..]=value_slots`. Ring buffer with shift-left eviction.

### Why: This is the correct approach given TML's current codegen constraints. When the compiler fixes cross-module closures and List[func(T)], the module can be upgraded to fully-encapsulated Subjects with stored callbacks.
