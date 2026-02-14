# Tasks: Test Failures — Compiler/Runtime Bugs Blocking Coverage

**Status**: In Progress (78%)
**Priority**: High
**Impact**: Unblocks ~365+ library functions from test coverage

## Context

This task tracks all compiler and runtime bugs discovered during test coverage work. These bugs prevent tests from compiling or running correctly. As new failures are found, they should be added here.

**Last updated**: 2026-02-13 (coverage at 50.3%, 2088/4153 functions)
**Tests executed**: 6,551+ tests across 486+ test files — **0 failures**

**See also**: [modules-without-tests.md](./modules-without-tests.md) - Complete list of implemented modules without tests

## Phase 1: Primitive Trait Method Resolution (Core Bug)

- [x] 1.1a Add missing `impl BitAnd/BitOr/BitXor/Shl/Shr` for all integer primitives in `lib/core/src/ops/bit.tml` (DONE 2026-02-12)
- [x] 1.1b Fix compiler codegen bug: behavior methods on primitives return `()` instead of declared type (DONE 2026-02-12)
- [x] 1.2 Fix `cmp::PartialEq::eq/ne` and `cmp::PartialOrd::lt/le/gt/ge` on all primitive types (DONE 2026-02-12)
- [x] 1.3 Fix `cmp::Ord::cmp` and `cmp::PartialOrd::partial_cmp` on all primitive types (DONE 2026-02-12)
- [x] 1.4 Fix `hash::Hash::hash` returning `()` on all primitive types (DONE 2026-02-12)
- [ ] 1.5 Fix `clone::Duplicate::duplicate` not being tracked by coverage for some primitive types
- [x] 1.6 Fix `borrow::ToOwned::to_owned` returning `()` on all primitive types (DONE 2026-02-12)
- [x] 1.7 Fix `cmp::clamp` codegen bug (DONE 2026-02-12)
- [x] 1.8 Fix `Str.char_at()` returning `()` instead of `I32` (DONE 2026-02-12)

## Phase 2: Assign Operators on Primitives

- [x] 2.1-2.5 Add missing `impl AddAssign/SubAssign/MulAssign/DivAssign/RemAssign` for all primitives (DONE 2026-02-12)
- [x] 2.6 Fix `ops::bit` assign variants on primitives (DONE 2026-02-12)

## Phase 3: Formatting Trait Methods

- [x] 3.1-3.6 Fix `fmt::Binary/Octal/LowerHex/UpperHex/LowerExp/UpperExp` on all types (DONE 2026-02-13)
- [x] 3.7 Fix `Maybe`/`Outcome` `to_string` — inline codegen for primitive inner types (DONE 2026-02-13 — maybe_outcome_tostring.test.tml 4/4 passed)

## Phase 4: Checked/Saturating/Wrapping Arithmetic

- [x] 4.1 Fix `num::overflow::checked_add/sub/mul/div/rem/neg` (DONE 2026-02-13)
- [x] 4.2 Fix `num::overflow::checked_shl/shr` (DONE 2026-02-13)
- [x] 4.3 Fix `num::saturating` methods on primitives (DONE 2026-02-13)
- [x] 4.4 Fix `num::wrapping` methods on primitives (DONE 2026-02-13)

## Phase 5: Generic Function Monomorphization

- [x] 5.1 Fix `std::types` generic functions (DONE 2026-02-13)
- [x] 5.2 Fix `unicode::char` functions (DONE 2026-02-13)
- [x] 5.3a Fix `mem::forget[T]` (DONE 2026-02-13)
- [x] 5.3b Fix `mem::zeroed[T]`, `mem::transmute[S,D]` (DONE 2026-02-13 — test_zeroed 3/3, test_transmute 1/1 passed)
- [x] 5.4 Fix `HashMapIter::key()` / `HashMapIter::value()` — i64-to-K/V type cast in codegen (DONE 2026-02-13 — test_hashmap_iter.test.tml 2/2 passed)
- [x] 5.5 Fix `ArrayList::new[T]()` / `Queue::new[T]()` (DONE 2026-02-13)

## Phase 6: Runtime Crash Fixes

- [x] 6.1 Fix `str::parse_*` stack overflow (DONE 2026-02-13)
- [x] 6.2 Fix `str::pad_left`, `str::pad_right` (DONE 2026-02-13)
- [x] 6.3 Fix `os::env_set` crash (DONE 2026-02-13)
- [x] 6.4 Fix `os::set_priority` crash (DONE 2026-02-13)
- [x] 6.5 Fix `Str::slice_str` crash (DONE 2026-02-13)
- [x] 6.6 Fix `Json::get_or` (DONE 2026-02-13)
- [ ] 6.7 Fix `Text::data_ptr` — pointer-to-null comparison crashes (needs test)

## Phase 6b: Behavior Dispatch on Structs/Generics

- [x] 6b.1 Fix `DecodeUtf16Error::to_string` / `::debug_string` (DONE 2026-02-13)
- [x] 6b.2 Fix `MutexGuard::deref` / `::deref_mut` (DONE 2026-02-13 — test_mutex_deref 4/4 passed)
- [x] 6b.3 Fix `RwLockReadGuard::deref` / `RwLockWriteGuard::deref` / `::deref_mut` (DONE 2026-02-13 — test_rwlock_deref 4/4 passed)
- [x] 6b.4 Fix `compiler_fence` intrinsic (DONE 2026-02-13)
- [x] 6b.5 Fix `BorrowError::to_string` / `BorrowMutError::to_string` (DONE 2026-02-13)
- [x] 6b.6 Fix `SocketAddrV4`/`SocketAddr` trait impls (DONE 2026-02-13 — test_socket_eq 5/5 passed)

## Phase 6c: List[Str] LLVM Type Mismatch

- [x] 6c.1 Fix `str::split` / `str::lines` / `str::join` / `str::concat_all` / `Str::split` (DONE 2026-02-13)

## Phase 6d: Generic Wrapper Type Methods

- [x] 6d.1 Fix `Saturating[T]::value()` (DONE 2026-02-13)
- [x] 6d.2 Fix `Wrapping[T]::value()` (DONE 2026-02-13)
- [x] 6d.3 Fix `NonZero[T]::get()` (DONE 2026-02-13)
- [ ] 6d.4 Fix `Saturating[T]::add/sub()` — no add/sub methods defined in saturating.tml (needs library impl)
- [ ] 6d.5 Fix `Wrapping[T]::add/sub()` — no add/sub methods defined in wrapping.tml (needs library impl)

## Phase 6e: LLVM Type Mismatch on Maybe[ref T]

- [ ] 6e.1 Fix `OnceCell::get()` — LLVM type mismatch `Maybe[ref I32]` vs `Maybe[I32]` (confirmed: simple methods work, `get()` blocked by generic ref type in enum)
- [ ] 6e.2 Fix `OnceLock::get()` — OnceLock type not yet implemented in library
- [ ] 6e.3 Fix `OnceLock::get_or_init()` — requires closure param (may overlap with Phase 9)

## Phase 6f: Exception Class Inheritance

- [x] 6f.1 Fix all exception subclass allocation (DONE 2026-02-13 — exception_subclass.test.tml 10/10 passed, all exception test files passing: 35+ tests)

## Phase 6g: File I/O

- [x] 6g.1 Fix `File::open_write()`/`open_read()`/`open_append()` (DONE 2026-02-13)
- [x] 6g.2 `SocketAddr::from_v4()` — not broken, method is `SocketAddr.V4()` (DONE 2026-02-13)

## Phase 7: Generic Enum Codegen

- [x] 7.1 Fix generic enum method instantiation (DONE 2026-02-13 — generic_enum_methods.test.tml 4/4 passed)
- [x] 7.2 Fix generic enum payload extraction (DONE 2026-02-13 — generic_enum_methods.test.tml 4/4 passed)
- [x] 7.3 Fix `Poll[T]` methods `is_ready`/`is_pending` (DONE 2026-02-13 — generic_enum_methods.test.tml 4/4 passed)
- [x] 7.4 Fix `Bound::Unbounded` resolution in generic contexts (DONE 2026-02-13 — confirmed working)
- [x] 7.5 Fix behavior constraint methods on generic enums (DONE 2026-02-13 — generic_enum_debug.test.tml 2/2 passed)

## Phase 8: Iterator Codegen

- [ ] 8.1 Fix associated type substitution (`I::Item` -> concrete type) in iterator adapters (needs test)
- [x] 8.2 Fix default behavior method dispatch on concrete types (DONE 2026-02-13 — iter_basic.test.tml 2/2 passed)
- [ ] 8.3 Fix higher-order generic types — `OnceWith`, `FromFn`, `Successors` (needs test)
- [ ] 8.4 Fix async iterator support (needs test)

## Phase 9: Closure and Function Pointer Issues

- [x] 9.0 Fix closures capturing fat pointers (DONE 2026-02-13 — closure_capture_fat_ptr.test.tml 5/5 passed)
- [ ] 9.1 Fix capturing closures in struct fields (needs test)
- [ ] 9.2 Fix tuple type arguments in trait definitions (needs test)
- [ ] 9.3 Fix returning closures with captures from functions (needs test)
- [ ] 9.4 Fix function pointer field calling (needs test)

## Phase 10: Slice and Array Creation

- [ ] 10.1 Fix `Slice::from_array` returning `()` instead of `Slice[U8]` (needs test — note: bstr.test.tml 6/6 passes using manual Slice construction)

## Phase 11: Generic Option/Result Codegen

- [ ] 11.1 Fix generic closures in `Maybe` methods (needs test)
- [ ] 11.2 Fix nested `Outcome` drop function generation (needs test)
- [ ] 11.3 Fix auto-drop glue for `Maybe[Arc[I32]]` (needs test)

## Phase 12: Module and Linking Issues

- [x] 12.1 Fix external module method linking for `std::types::Object` (DONE 2026-02-13 — object.test.tml 7/7 passed including inheritance, virtual dispatch, reference_equals)
- [ ] 12.2 Fix `unicode_data::UNICODE_VERSION` constant not found (needs test)
- [ ] 12.3 Fix external inheritance for exception subclasses (needs test — note: in-file exception subclasses work, see 6f.1)
