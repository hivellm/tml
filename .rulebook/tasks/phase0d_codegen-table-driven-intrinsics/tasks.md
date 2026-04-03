# Tasks: Table-Driven Intrinsics — Replace 1,357 LOC of if/else

**Status**: New. 0% (0/18). **Priority**: MEDIUM
**Reference**: `docs/analyses/codegen/05-INTRINSICS-DISPATCH.md`

## 1. Intrinsic Registry

- [ ] 1.1 Create `compiler/include/codegen/intrinsic_table.hpp`
- [ ] 1.2 Add `IntrinsicKind` enum (PtrRead, PtrWrite, PtrOffset, MemFree, CopyNonOverlapping, Memcpy, Memmove, Memset, Sqrt, Sin, Cos, ... ~30 entries)
- [ ] 1.3 Add `IntrinsicInfo` struct: `{kind, min_args, has_result}`
- [ ] 1.4 Create `compiler/src/codegen/intrinsic_table.cpp`
- [ ] 1.5 Implement `lookup_intrinsic(func_name)` — O(1) unordered_map, handles aliases (mem_copy→Memcpy, volatile_read→PtrReadVolatile, etc.)
- [ ] 1.6 Build — verify compiles

## 2. Shared Helpers

- [ ] 2.1 Extract `resolve_element_type()` from ptr_read/ptr_write — single implementation
- [ ] 2.2 Extract `ensure_ptr_value()` from 12 duplicated lambdas — single method on MirCodegen
- [ ] 2.3 Extract `emit_memcpy()`, `emit_memset()`, `emit_memmove()` helpers
- [ ] 2.4 Build — verify compiles

## 3. Dispatch Refactor

- [ ] 3.1 In `emit_call_inst()`: replace initial if/else chain with `lookup_intrinsic()` + switch
- [ ] 3.2 Move ptr_read/ptr_write/ptr_offset to `emit_intrinsic_ptr_read()` etc. (separate methods)
- [ ] 3.3 Move memcpy/memmove/memset to `emit_intrinsic_memcpy()` etc.
- [ ] 3.4 Move math intrinsics to `emit_intrinsic_math()` (already mostly separated)
- [ ] 3.5 Move to_string/debug_string to `emit_intrinsic_to_string()`
- [ ] 3.6 Run full test suite — verify zero regressions
