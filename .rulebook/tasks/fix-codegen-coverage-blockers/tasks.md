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

## Phase 3: Mutex[Unit] Void Zeroinitializer
- [ ] Fix Unit type in struct fields to use i8 or {} instead of void
- [ ] Verify: Mutex[Unit] construction
- [ ] Verify: thread/scope (Scope::new, spawn, wait_all)
- [ ] Run test suite — no regressions

## Phase 4: Const Generic N Cross-Module
- [ ] Fix apply_type_substitutions for ArrayType size parameter
- [ ] Verify: ArrayIter[I32, 3] imported from library has [3 x i32]
- [ ] Write tests for Array::into_iter, iter, iter_mut
- [ ] Run test suite — no regressions

## Phase 5: Closure Capture Bug
- [ ] Reproduce: closure capturing function parameter returns 0
- [ ] Fix closure capture codegen for function parameters
- [ ] Verify: local_const and other closure-heavy APIs
- [ ] Run test suite — no regressions

## Final
- [ ] Run full coverage — confirm 95%+
- [ ] Update MEMORY.md with results
- [ ] Commit all changes
