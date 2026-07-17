# Tasks: Table-Driven Intrinsics — Replace 1,357 LOC of if/else

**Status**: Complete. 100% (18/18). **Priority**: MEDIUM
**Reference**: `docs/analyses/codegen/05-INTRINSICS-DISPATCH.md`

## 1. Intrinsic Registry

- [x] 1.1 Create `compiler/include/codegen/intrinsic_table.hpp`
- [x] 1.2 Add `IntrinsicKind` enum (PtrRead, PtrWrite, PtrOffset, MemFree, CopyNonOverlapping, Memcpy, Memmove, Memset, Sqrt, Sin, Cos, ... ~35 entries)
- [x] 1.3 Add `IntrinsicInfo` struct: `{kind, min_args, has_result}`
- [x] 1.4 Create `compiler/src/codegen/intrinsic_table.cpp`
- [x] 1.5 Implement `lookup_intrinsic(func_name)` — O(1) unordered_map, handles aliases (mem_copy→Memcpy, volatile_read→PtrReadVolatile, etc.)
- [x] 1.6 Build — verify compiles

## 2. Shared Helpers

- [x] 2.1 Extract `resolve_read_element_type()` from ptr_read/ptr_read_volatile/ptr_read_unaligned — single implementation
- [x] 2.2 Extract `resolve_write_element_type()` from ptr_write — single implementation
- [x] 2.3 Extract `ensure_ptr_reg()` — replaces 12 duplicated lambdas with single method on MirCodegen
- [x] 2.4 Extract `promote_to_i64()` — replaces 4 duplicated i32→i64 sext patterns
- [x] 2.5 Build — verify compiles

## 3. Dispatch Refactor

- [x] 3.1 In `emit_call_inst()`: replace initial if/else chain with `lookup_intrinsic()` + switch (~120 LOC replaces ~700 LOC)
- [x] 3.2 Move ptr_read/ptr_write/ptr_offset/volatile variants/unaligned variants to `emit_intrinsic_ptr_*()` methods
- [x] 3.3 Move memcpy/memmove/memset/copy_nonoverlapping to `emit_intrinsic_memcpy()` etc.
- [x] 3.4 Move math intrinsics through existing `emit_llvm_intrinsic_call()` (unchanged)
- [x] 3.5 Move to_string/debug_string to `emit_intrinsic_char_to_string()`, `emit_intrinsic_str_to_string()`, `emit_intrinsic_bare_to_string()`
- [x] 3.6 Move black_box/store_byte to `emit_intrinsic_black_box()`, `emit_intrinsic_store_byte()`
- [x] 3.7 Fix: qualified names (Char::to_string, Str::to_string) must be checked BEFORE table lookup to avoid base_name collision
- [x] 3.8 Run test suites — zero regressions: core/str 25/25, core/fmt 46/46, core/ops 47/47, core/num 53/53
