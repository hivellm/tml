# Coverage Blockers Verified 2026-03-08

## ptr module (19 uncovered) — ALL BLOCKED

Missing lowlevel intrinsic codegen (not declared in LLVM IR):
- `@tml_ptr_read_unaligned`, `@tml_ptr_write_unaligned` — blocks RawMutPtr/RawPtr read_unaligned/write_unaligned
- `@tml_ptr_read_volatile`, `@tml_ptr_write_volatile` — blocks RawMutPtr/RawPtr read_volatile/write_volatile
- `@tml_memcpy` — blocks copy_from_nonoverlapping (RawMutPtr, NonNull, ops)
- `@tml_memmove` — blocks copy_from (RawMutPtr, NonNull, ops), copy_mut
- `@tml_memset` — blocks write_bytes_val
- `ptr_as_ref`/`ptr_as_mut` — returns i32 instead of ptr, blocks NonNull::as_ref/as_mut

## intrinsics module (10 uncovered) — ALL BLOCKED

Generic intrinsic monomorphization bug (%struct.T instead of concrete type):
- `cast[T,U]`, `volatile_read[T]`, `volatile_write[T]`, `atomic_cmpxchg[T]`, `atomic_xor[T]`

Array intrinsics (inherently uncoverable — used via `lowlevel {}` inline, not function calls):
- `array_as_ptr`, `array_as_mut_ptr`, `array_offset_ptr`, `array_offset_mut_ptr`

Not testable (UB if reached):
- `unreachable()`

## iter module (24 uncovered) — ALL BLOCKED

- peekable (7): nested `Maybe[Maybe[T]]` type layout mismatch
- cloned (3), copied (3): `where I::Item = ref T` generic monomorphization
- flatten (2): `IntoIterator` trait dispatch ("Unknown method: into_iter")
- intersperse (2): malformed load instruction IR
- F32/F64 sum/product (4): float literal codegen ("integer constant must have integer type")
- Range/RangeInclusive/RangeFrom size_hint (3): generic trait dispatch returns ()

## User claim correction

User said peekable/cloned/copied/flatten/intersperse "now PASS after recent fixes" — this is INCORRECT.
All 5 adapters remain blocked by the same codegen bugs documented in existing placeholder tests.
