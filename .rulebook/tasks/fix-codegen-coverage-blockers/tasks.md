# Tasks

## Phase 1: Generic Trait Dispatch → ()
- [ ] Reproduce: `Array[I32, 3].hash()` returns () instead of I64
- [ ] Trace return type resolution in expr_call_method.cpp for constrained impls
- [ ] Fix type substitution for constrained generic impl return types
- [ ] Verify: Array PartialEq, Hash, Display, Debug, Default, Duplicate
- [ ] Verify: Pool::acquire, Range::size_hint, Poll::eq, F32/F64 sum/product
- [ ] Run test suite — no regressions
- [ ] Write/uncomment tests for newly unblocked functions

## Phase 2: Missing LLVM Intrinsic Declarations
- [ ] Add runtime declarations for ptr_read/write_unaligned, ptr_read/write_volatile
- [ ] Add runtime declarations for memcpy, memmove, memset
- [ ] Verify: RawMutPtr volatile/unaligned operations
- [ ] Verify: copy_from, copy_from_nonoverlapping, write_bytes_val
- [ ] Run test suite — no regressions

## Phase 3: Mutex[Unit] Void Zeroinitializer — RESOLVED
- [x] Fix Unit type in struct fields to use i8 or {} instead of void — FIXED in codegen-structural-fixes (commit c03d7702, void-in-data-context across 13 files)
- [x] Verify: Mutex[Unit] construction — Unit type fully fixed (3 commits)
- [x] Verify: thread/scope (Scope::new, spawn, wait_all) — passes in full suite run (2026-03-16)
- [x] Run test suite — no regressions

## Phase 4: Const Generic N Cross-Module — RESOLVED
- [x] Fix apply_type_substitutions for ArrayType size parameter — FIXED (const generic N monomorphization across 6 files)
- [x] Verify: ArrayIter[I32, 3] imported from library has [3 x i32] — working
- [x] Write tests for Array::into_iter, iter, iter_mut — array method dispatch fixed (commit 57b849e5)
- [x] Run test suite — no regressions

## Phase 5: Closure Capture Bug — RESOLVED
- [x] Reproduce: closure capturing function parameter returns 0 — FIXED (closure/fat-pointer codegen i64↔{ptr,ptr} fix)
- [x] Fix closure capture codegen for function parameters — FIXED in MIR codegen (commit a0551f9a, ThirMirBuilder::build_closure across 8 files)
- [x] Verify: local_const and other closure-heavy APIs — cell module 26/26 passing
- [x] Run test suite — no regressions

## Final
- [ ] Run full coverage — confirm 95%+ (currently at 99.38%)
- [ ] Update MEMORY.md with results
- [ ] Commit all changes
