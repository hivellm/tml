# Changelog — TML Core Library (`lib/core`)

All notable changes to the TML core library will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [0.2.4] — 2026-03-28

### Added
- **Reflection**: compile-time field index validation (R001), impl_count/impl_name intrinsics, dynamic vtable dispatch tests, benchmark

### Changed
- **Major reorganization** — 30 loose files → 6 thematic directories:
  - `data/` (arena, bitset, cache, pool, ringbuf, soo, collections)
  - `types/` (option, result, tuple, range, any)
  - `traits/` (clone, cmp, convert, default, borrow, hash, marker)
  - `async/` (async_iter, task)
  - `runtime/` (error, panic, hint, mem, pin, intrinsics, profiler)
  - `reflect/` (reflect → reflect/mod.tml)
  - `encoding/` (bstr moved here)
- 553 import references updated across the codebase

### Fixed
- `async_iter.tml` — removed dangling doc comments causing P001 parser error
- `task.tml` — commented out Waker/Context type redefinitions (builtin clash T038), rewrote PartialEq for Poll[T] with nested when

## [0.2.3] — 2026-03-25

### Added

- **Panic Recovery** (`panic/mod.tml`) — catch panics and recover
  - `PanicInfo` struct: message, file, line, column
  - `set_hook` / `clear_hook` — custom panic handlers
  - `catch_unwind_fn` — catch panics via setjmp/longjmp, returns `CatchResult`
  - `resume_unwind` — re-panic after catching
- **Compiler Hints** (`hint.tml`) — optimization directives
  - `unreachable_unchecked`, `black_box_i64/bool/f64`, `spin_loop_hint`
  - `likely` / `unlikely` — branch prediction hints
  - `assume` — optimizer assertions
- **Core FFI types** (`ffi/mod.tml`) — Type-safe C interop wrappers
  - `c_void`, `c_int`, `c_uint`, `c_long`, `c_ulong`, `c_longlong`, `c_ulonglong`
  - `c_float`, `c_double`, `c_size_t`, `c_ssize_t`, `c_ptrdiff_t`, `c_intptr_t`, `c_uintptr_t`
  - `c_long_bits()` / `c_long_max()` — platform-aware (32 on Windows, 64 on Unix)
- **CStr** (`ffi/cstr.tml`) — Borrowed C string with null-terminator guarantee
  - `from_ptr`, `to_str`, `to_owned_str`, `len`, `is_empty`, `byte_at`, `as_ptr`
  - `impl PartialEq for CStr`, `impl Display for CStr`

### Tests

- 7 panic recovery tests (catch, no-panic, continue, resume, PanicInfo, location)
- 9 hint tests (likely, unlikely, black_box, spin_loop, assume)
- 14 FFI primitive type tests, 10 CStr tests

## [0.2.2] — 2026-03-22

### Added

- **Tracy profiler zones** — Instrumented option, cell, os, glob, math modules with profiler zones

## [0.2.1] — 2026-03-21

### Added

- **Maybe::take()** — consumes self, returns the option (like Rust's Option::take)
- **pub use in core::future** — Context, Poll, Ready, Pending now publicly re-exported

### Tests

- 13 new Buffer tests (core ops, slice/str, endian roundtrips) — collections suite 73/73

## [0.2.0] — 2026-03-19

### Added

- **Smart Pointers** (2026-02-06) — Complete Rust-style smart pointer implementations
  - `Heap[T]` — Unique pointer with ownership (like Rust's `Box[T]`)
  - `Shared[T]` — Non-atomic reference-counted pointer (like Rust's `Rc[T]`)
  - `Sync[T]` — Atomic reference-counted pointer (like Rust's `Arc[T]`)
  - `Weak[T]` — Weak reference to `Shared[T]`
  - All implement Drop, Display, Debug behaviors
  - 15 tests in `lib/core/tests/alloc/smart_pointers.test.tml`

- **Atomic Operations** (2026-02-06) — Cross-platform atomic primitives
  - I32/I64: fetch_add, fetch_sub, load, store, compare_exchange, swap
  - Memory fences: acquire, release, seq_cst barriers
  - Windows (InterlockedX) and Unix (__sync_fetch_and_X) support
  - Generic atomics via inline LLVM instructions in `core::sync`
  - Typed atomic FFI via `@extern` declarations

- **Drop Trait Enabled** (2026-02-06) — Automatic RAII cleanup for smart pointers
  - LIFO drop order, move semantics support

- **@derive Macros** (2026-02-05) — Automatic trait implementation for structs and enums
  - `@derive(PartialEq)` — Field-by-field equality
  - `@derive(Duplicate)` — Field-by-field copy
  - `@derive(Hash)` — FNV-1a hash algorithm
  - `@derive(Default)` — Zero-initialized values
  - `@derive(PartialOrd)` — Partial ordering (lexicographic)
  - `@derive(Ord)` — Total ordering
  - `@derive(Debug)` — String representation
  - 40 tests across 7 derive test files

- **Reflection** (2026-02-04) — Runtime type introspection
  - `@derive(Reflect)` for enum `variant_name()` and `variant_tag()`
  - Type info via reflect module

- **Iterator Enhancements** (2026-03-03)
  - DoubleEndedIterator protocol implementation methods
  - `nth_back` return type fix
  - Iterator adapters: Map, Filter, Take, Skip, Chain, Zip, Enumerate, Peekable, TakeWhile, SkipWhile, Flatten, FlatMap, Cycle, Fuse, Rev, Cloned, Copied, Chunks, Windows, StepBy

- **Arena Allocation** (2026-02-10) — Bump allocator for fast allocation with same-lifetime objects

- **Object Pooling** (2026-02-10) — `Pool[T]` for reusable object pools

- **Cache Module** — `Cache[K, V]` LRU cache implementation

- **Small Object Optimization** — `SmallBox[T, N]` stack-allocated box with heap fallback

- **Ring Buffer** — Lock-free ring buffer for concurrent producers/consumers

- **Bitset** — Fixed-size and dynamic bit set operations

- **SIMD Module** — Native SSE2 intrinsics (`sse2_cmpeq_epi8`, `simd_splat`, `simd_load_ptr`)

### Changed

- **Sync/Threading → @extern FFI** (2026-02-19) — Core sync primitives now use `@extern` FFI exclusively
  - 9 typed atomic declarations added to `core::sync.tml`
  - No compiler codegen mediation needed

- **Char-to-String Migration** (2026-02-19) — 4 char C calls migrated to pure TML
  - `char_to_string`, `utf8_char_len`, `string_from_char`, `char_to_utf8_bytes`

### Fixed

- **Shared[T] Memory Leak** (2026-02-17) — Broken increment/decrement for library-imported generics

- **Reflect Behavior Syntax** (2026-02-04) — Fixed invalid `ref this` syntax

- **Maybe::default() and Maybe::eq()** (2026-03-02) — Generic builtin enum methods unblocked

- **Array Mutable Method Dispatch** (2026-03-02) — `mut this` dispatch for `get_mut`, `first_mut`, etc.

- **Assertion Coverage Tracking** (2026-02-16) — Builtin assertions now tracked in coverage reports

- **`@allocates` Annotation Sweep** (2026-02-24) — Added to 50+ functions in core::fmt, core::error, core::clone

### Test Coverage

- **Comprehensive Expansion** (2026-02-10 through 2026-03-07) — Systematic coverage campaign
  - core::alloc — 6 tests (Layout constructors, from_type, array)
  - core::borrow — 9 tests (Borrow/BorrowMut, ToOwned)
  - core::cell — 15 tests (Cell, RefCell, OnceCell, UnsafeCell)
  - core::char — 18 tests (case conversion, classification, validation)
  - core::cmp — 15 tests (clamp, Ordering, partial_cmp)
  - core::convert — 12 tests (identity, Into, TryFrom)
  - core::fmt — 9 tests (FormatSpec, Argument, Placeholder)
  - core::hash — 30 tests (DefaultHasher, combine, Maybe, Outcome, primitives)
  - core::iter — 9 tests (sources, range inclusive, Step)
  - core::mem — 15 tests (ManuallyDrop, MaybeUninit, size/align, swap)
  - core::num — 24 tests (checked arithmetic, endian, rotate, NonZero, saturating)
  - core::ops — 18 tests (arithmetic, bit assign, try_trait)
  - core::option — 12 tests (basics, convert, extract)
  - core::pin — 3 tests (Pin basics)
  - core::ptr — 21 tests (alignment, RawMutPtr, RawPtr)
  - core::range — 6 tests (Range, RangeInclusive iter)
  - core::reflect — 3 tests (type reflection)
  - core::sync — 6 tests (atomic basics, spinlock)
  - core::time — 15 tests (Duration accessors, arithmetic, conversion)
  - core::types — 27 tests (array methods, char decode)

## [0.1.0] — 2025-12-22

### Added
- Initial release with fundamental behaviors and types
- clone, cmp, ops, default, fmt, convert modules
- Memory: alloc, mem, ptr, cell, borrow, pin, marker
- Iteration: iter, slice, array
- Error handling: error, option (Maybe), result (Outcome)
- Strings: str, ascii, char
- Low-level: intrinsics, any
