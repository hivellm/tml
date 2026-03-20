# Tasks

## Phase 1: Generic Trait Dispatch → () — PARTIALLY FIXED
- [x] Reproduce: `Array[I32, 3].hash()` — already works, not a regression (verified via standalone + test file)
- [x] Trace return type resolution: root cause is check_range() returning Slice[I64] instead of Range[T]
- [x] Fix: check_range() now returns Range[T] / RangeInclusive[T] (control.cpp)
- [x] Fix: check_for() handles Range[T] element type extraction (control.cpp)
- [x] Fix: gen_range() added to AST codegen for standalone range expressions (llvm_ir_gen_expr.cpp)
- [x] Verify: Array PartialEq, Hash, Display, Debug, Duplicate — all work (tested via standalone + test file)
- [x] Verify: Range::size_hint TYPE CHECKS correctly (was () before, now (I64, Maybe[I64]))
- [x] Run test suite — 52/52 iter, 2/2 range, 10/10 lang, 50/50 num, 7/7 types — no regressions
- [ ] Remaining: Range method CODEGEN dispatch (AST codegen needs Range struct registration for r.size_hint() to compile+run)
- [ ] Remaining: Pool::acquire, Poll::eq (separate codegen issues, not type checker)
- [ ] Remaining: F32/F64 sum/product (Sum/Product behavior dispatch, separate from Range fix)
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
