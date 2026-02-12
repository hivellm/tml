# Tasks: Test Failures — Compiler/Runtime Bugs Blocking Coverage

**Status**: Not Started (0%)
**Priority**: High
**Impact**: Unblocks ~365+ library functions from test coverage (currently at ~50.92%)

## Context

This task tracks all compiler and runtime bugs discovered during test coverage work. These bugs prevent tests from compiling or running correctly. As new failures are found, they should be added here.

**Last updated**: 2026-02-12 (coverage at 50.92%, 2070/4065 functions)

## Phase 1: Primitive Trait Method Resolution (Core Bug)

- [x] 1.1a Add missing `impl BitAnd/BitOr/BitXor/Shl/Shr` for all integer primitives in `lib/core/src/ops/bit.tml` (DONE 2026-02-12)
- [ ] 1.1b Fix compiler codegen bug: behavior methods on primitives return `()` instead of declared type
- [ ] 1.2 Fix `cmp::PartialEq::eq` returning `()` instead of `Bool` on all primitive types (I8-I64, U8-U64, F32, F64, Bool)
- [ ] 1.3 Fix `cmp::Ord::cmp` and `cmp::PartialOrd::partial_cmp` returning `()` on all primitive types
- [ ] 1.4 Fix `hash::Hash::hash` returning `()` on all primitive types (I8-I64, U8-U64, Bool, Str)
- [ ] 1.5 Fix `clone::Duplicate::duplicate` not being tracked by coverage for some primitive types
- [ ] 1.6 Fix `borrow::ToOwned::to_owned` returning `()` on all primitive types
- [ ] 1.7 Fix `cmp::clamp` codegen bug — Self type not substituted (blocks `cmp.test.tml`, `cmp_partial_cmp.test.tml`)
- [ ] 1.8 Fix `Str.char_at()` returning `()` instead of `I32` — behavior dispatch bug (blocks `str_methods.test.tml`)

## Phase 2: Assign Operators on Primitives

- [ ] 2.1 Fix `ops::arith::AddAssign::add_assign` on primitives
- [ ] 2.2 Fix `ops::arith::SubAssign::sub_assign` on primitives
- [ ] 2.3 Fix `ops::arith::MulAssign::mul_assign` on primitives
- [ ] 2.4 Fix `ops::arith::DivAssign::div_assign` on primitives
- [ ] 2.5 Fix `ops::arith::RemAssign::rem_assign` on primitives
- [ ] 2.6 Fix `ops::bit` assign variants (`bitand_assign`, `bitor_assign`, `bitxor_assign`, `shl_assign`, `shr_assign`) on primitives

## Phase 3: Formatting Trait Methods

- [ ] 3.1 Fix `fmt::Binary::fmt_binary` on integer types (I8-I64, U8-U64) — undefined symbol error
- [ ] 3.2 Fix `fmt::Octal::fmt_octal` on integer types
- [ ] 3.3 Fix `fmt::LowerHex::fmt_lower_hex` on integer types
- [ ] 3.4 Fix `fmt::UpperHex::fmt_upper_hex` on integer types
- [ ] 3.5 Fix `fmt::LowerExp::fmt_lower_exp` on float types (F32, F64)
- [ ] 3.6 Fix `fmt::UpperExp::fmt_upper_exp` on float types
- [ ] 3.7 Fix `Maybe`/`Outcome` `to_string` — type mismatch in inner type codegen (blocks `fmt_format_methods.test.tml`)

## Phase 4: Checked/Saturating/Wrapping Arithmetic

- [ ] 4.1 Fix `num::overflow::checked_add/sub/mul/div/rem/neg` returning `()` instead of `Maybe[T]` on primitives
- [ ] 4.2 Fix `num::overflow::checked_shl/shr` returning `()` instead of `Maybe[T]` on primitives
- [ ] 4.3 Fix `num::saturating` methods on primitives
- [ ] 4.4 Fix `num::wrapping` methods on primitives

## Phase 5: Generic Function Monomorphization

- [ ] 5.1 Fix `std::types` generic functions (`unwrap[T]`, `expect[T]`, `ok_or[T,E]`, etc.) emitting `Maybe__T` instead of `Maybe__I32` in LLVM IR
- [ ] 5.2 Fix `unicode::char` functions returning `()` — Char param codegen issue (i32 vs i1 type mismatch in branch)
- [ ] 5.3 Fix `mem::forget[T]`, `mem::zeroed[T]`, `mem::transmute[S,D]`, `MaybeUninit::uninit[T]` — emit `%struct.T` instead of concrete type in LLVM IR
- [ ] 5.4 Fix `HashMapIter::key()` / `HashMapIter::value()` — return generic `K`/`V` instead of substituted concrete type
- [ ] 5.5 Fix `ArrayList::new[T]()` / `Queue::new[T]()` — return `()` instead of constructed value (workaround: use `::create()`)

## Phase 6: Runtime Crash Fixes

- [ ] 6.1 Fix `str::parse_i32`, `str::parse_i64`, `str::parse_f64`, `str::parse_bool` causing stack overflow at runtime
- [ ] 6.2 Fix `str::pad_left`, `str::pad_right` — Char literal passed as `ptr 48` instead of integer type
- [ ] 6.3 Fix `os::env_set` crashing at runtime
- [ ] 6.4 Fix `os::set_priority` crashing at runtime
- [ ] 6.5 Fix `Str::slice_str` — runtime crash (STATUS_STACK_BUFFER_OVERRUN)
- [ ] 6.6 Fix `Json::get_or` — runtime panic
- [ ] 6.7 Fix `Text::data_ptr` — pointer-to-null comparison crashes (workaround: don't compare result)

## Phase 6b: Behavior Dispatch on Structs/Generics

- [ ] 6b.1 Fix `DecodeUtf16Error::to_string` / `::debug_string` — return `()` instead of `Str` (behavior dispatch on struct)
- [ ] 6b.2 Fix `MutexGuard::deref` / `::deref_mut` — return `()` instead of `ref T` (behavior dispatch on generic struct)
- [ ] 6b.3 Fix `RwLockReadGuard::deref` / `RwLockWriteGuard::deref` / `::deref_mut` — same issue as 6b.2
- [ ] 6b.4 Fix `compiler_fence` intrinsic — emits `@tml_compiler_fence` instead of LLVM intrinsic
- [ ] 6b.5 Fix `BorrowError::to_string` / `BorrowMutError::to_string` — behavior dispatch on struct returns `()`
- [ ] 6b.6 Fix `SocketAddrV4`/`SocketAddr` trait impls (`eq`, `cmp`, `partial_cmp`, `duplicate`, `hash`, `fmt`) — "Unknown method" at codegen for all behavior dispatch on these structs

## Phase 6c: List[Str] LLVM Type Mismatch

- [ ] 6c.1 Fix `str::split` / `str::lines` / `str::join` / `str::concat_all` / `Str::split` — `List[Str]` return type has LLVM IR mismatch (`ptr` vs `%struct.List__Str`)

## Phase 6d: Generic Wrapper Type Methods

- [ ] 6d.1 Fix `Saturating[T]::get()` — "Unknown method: get" at codegen (generic struct method dispatch)
- [ ] 6d.2 Fix `Wrapping[T]::get()` — "Unknown method: get" at codegen (same root cause as 6d.1)
- [ ] 6d.3 Fix `NonZero[T]::new_unchecked()` — returns `()` instead of `NonZero[T]` (generic monomorphization)
- [ ] 6d.4 Fix `Saturating[T]::add/sub()` — returns `()` instead of `Saturating[T]` (generic monomorphization)
- [ ] 6d.5 Fix `Wrapping[T]::add/sub()` — returns `()` instead of `Wrapping[T]` (generic monomorphization)

## Phase 6e: LLVM Type Mismatch on Maybe[ref T]

- [ ] 6e.1 Fix `OnceCell::get()` — LLVM type mismatch `Maybe[ref I32]` vs `Maybe[I32]` (codegen emits wrong Maybe variant)
- [ ] 6e.2 Fix `OnceLock::get()` — same `Maybe[ref T]` vs `Maybe[T]` LLVM type mismatch
- [ ] 6e.3 Fix `OnceLock::get_or_init()` — requires closure param (may overlap with Phase 9)

## Phase 6f: Exception Class Inheritance

- [ ] 6f.1 Fix all exception subclass allocation — "Cannot allocate unsized type %struct.Exception" in LLVM IR (affects ArgumentNullException, ArgumentOutOfRangeException, IndexOutOfRangeException, FileNotFoundException, ArgumentException)

## Phase 6g: File I/O

- [ ] 6g.1 Fix `File::create()` — returns `()` instead of `File` (blocks file write tests)
- [ ] 6g.2 Fix `SocketAddr::from_v4()` — returns `()` instead of `SocketAddr`

## Phase 7: Generic Enum Codegen

- [ ] 7.1 Fix generic enum method instantiation — `CoroutineState[T]` methods not working (blocks `ops_coroutine.test.tml`)
- [ ] 7.2 Fix generic enum payload extraction — type parameters not substituted (blocks `ops_coroutine.test.tml`)
- [ ] 7.3 Fix `Poll[T]` methods `is_ready`/`is_pending` — generic enum variant resolution (blocks `ops_async.test.tml`)
- [ ] 7.4 Fix `Bound::Unbounded` resolution in generic contexts (blocks `ops_range.test.tml`)
- [ ] 7.5 Fix behavior constraint methods (`debug_string`, `to_string`) on generic enums

## Phase 8: Iterator Codegen

- [ ] 8.1 Fix associated type substitution (`I::Item` → concrete type) in iterator adapters (blocks `iter_adapters.test.tml`)
- [ ] 8.2 Fix default behavior method dispatch returning `()` on concrete types — `iter.count()`, `iter.last()`, etc. (blocks `iter_consumers.test.tml`)
- [ ] 8.3 Fix higher-order generic types — `OnceWith`, `FromFn`, `Successors` emit `Maybe__Fn` vs `Maybe__T` mismatch (blocks `iter_sources.test.tml`)
- [ ] 8.4 Fix async iterator support — depends on `task::Waker` function pointer field calling (blocks `async_iter.test.tml`)

## Phase 9: Closure and Function Pointer Issues

- [ ] 9.1 Fix capturing closures in struct fields — closure signature includes captures but struct field loses capture info (blocks `fn_traits.test.tml`)
- [ ] 9.2 Fix tuple type arguments in trait definitions — `Fn[(I32,)]` not parseable (blocks `fn_traits.test.tml`)
- [ ] 9.3 Fix returning closures with captures from functions (blocks `std::types::closure.test.tml`)
- [ ] 9.4 Fix function pointer field calling — calling function stored in struct field (blocks `async_iter.test.tml`, `arc_methods.test.tml`)

## Phase 10: Slice and Array Creation

- [ ] 10.1 Fix `Slice::from_array` returning `()` instead of `Slice[U8]` (blocks `bstr.test.tml`)

## Phase 11: Generic Option/Result Codegen

- [ ] 11.1 Fix generic closures in `Maybe` methods — `option.test.tml` tests commented out
- [ ] 11.2 Fix nested `Outcome` drop function generation (blocks `result.test.tml`)
- [ ] 11.3 Fix auto-drop glue for `Maybe[Arc[I32]]` — enum with non-trivial payload (blocks `arc_methods.test.tml`)

## Phase 12: Module and Linking Issues

- [ ] 12.1 Fix external module method linking for `std::types::Object` (blocks `object.test.tml`)
- [ ] 12.2 Fix `unicode_data::UNICODE_VERSION` constant not found + i16/i32 type mismatch in char functions (blocks `unicode.test.tml`)
- [ ] 12.3 Fix external inheritance for exception subclasses (blocks `exception_subclasses.test.tml`)

## Affected Modules and Estimated Function Count

| Module | Blocked Functions | Error Type |
|--------|------------------|------------|
| `cmp` | 44 | `eq`/`cmp`/`partial_cmp`/`clamp` return `()` |
| `ops/bit` | 41 | "Unknown method" at codegen |
| `ops/arith` (assign) | 5 | Assign operators fail |
| `ops/coroutine` | 8 | Generic enum method instantiation |
| `ops/async` | 10 | `Poll[T]` generic enum codegen |
| `ops/range` | 4 | `Bound` generic enum resolution |
| `fmt/impls` | 42 | `fmt_binary`/`octal`/`hex`/`exp` undefined symbol |
| `hash` | 10 | `hash` method returns `()` |
| `borrow` | 14 | `to_owned` returns `()` |
| `num/overflow` | 19 | `checked_*` returns `()` |
| `num/saturating` | 5 | Methods return `()` |
| `num/wrapping` | 5 | Methods return `()` |
| `iter` (adapters) | 16 | Associated type substitution |
| `iter` (consumers) | 12 | Default method dispatch returns `()` |
| `iter` (sources) | 6 | Higher-order generic types |
| `iter` (async) | 8 | Function pointer fields + async |
| `closure/fn_traits` | 6 | Capture semantics + tuple args |
| `unicode/char` | 14 | Char param codegen bug |
| `std::types` | 8 | Generic monomorphization |
| `str` (parse/pad) | 7 | Runtime crashes |
| `str` (split/lines/join) | 5 | List[Str] LLVM type mismatch |
| `os` (env_set/priority) | 4 | Runtime crashes |
| `bstr` (slice) | 5 | `Slice::from_array` returns `()` |
| `option/result` | 8 | Generic closures + drop glue |
| `std::types::Object` | 4 | External module linking |
| `unicode_data` | 3 | Missing constants + type mismatch |
| `mem` | 4 | Generic monomorphization (`%struct.T`) |
| `char/decode` | 2 | Behavior dispatch on struct |
| `sync/mutex` (guard) | 2 | Behavior dispatch on generic struct |
| `sync/rwlock` (guard) | 3 | Behavior dispatch on generic struct |
| `sync/ordering` | 1 | `compiler_fence` intrinsic codegen |
| `collections/hashmap` (iter) | 2 | Generic `K`/`V` not substituted |
| `json/types` | 1 | `get_or` runtime panic |
| `Str::slice_str` | 1 | Runtime crash |
| `cell/once` (BorrowError) | 2 | `to_string` behavior dispatch on struct |
| `net/socket` (SocketAddr*) | 10+ | All trait impls "Unknown method" at codegen |
| `num/nonzero` (new_unchecked) | 1 | Generic monomorphization returns `()` |
| `num/saturating` (get/add/sub) | 5 | "Unknown method: get" + returns `()` |
| `num/wrapping` (get/add/sub) | 5 | "Unknown method: get" + returns `()` |
| `cell/once` (OnceCell::get) | 1 | `Maybe[ref T]` vs `Maybe[T]` LLVM mismatch |
| `sync/once` (OnceLock::get) | 1 | `Maybe[ref T]` vs `Maybe[T]` LLVM mismatch |
| `exception` (all subclasses) | 4 | "Cannot allocate unsized type %struct.Exception" |
| `file/file` (File::create) | 1 | Returns `()` instead of `File` |
| `net/socket` (from_v4) | 1 | Returns `()` instead of `SocketAddr` |
| **Total** | **~365+** | |
