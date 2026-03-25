# Tasks: Core FFI Types — Type-Safe Foreign Function Interface

**Status**: 90% Complete (18/20 items done, 2 remaining: `#if` in structs, migration guide)
**Priority**: HIGH
**Phase**: 1 — Foundation

## Motivation

Every `@extern("c")` call in TML uses raw `*U8` pointers for strings and untyped integers for C types. This is unsafe and error-prone. Rust's `core::ffi` provides type-safe wrappers (`CStr`, `c_void`, `c_char`, `c_int`) that catch ABI mismatches at compile time.

## Phase 1: Core FFI Primitive Types (`lib/core/src/ffi/`)

- [x] 1.1 Create `lib/core/src/ffi/mod.tml` with module exports
- [x] 1.2 Implement `c_void` — opaque type for void pointers (`*c_void` replaces `*U8` for opaque handles)
- [x] 1.3 Implement C integer type wrappers: `c_int` (I32), `c_uint` (U32), `c_long` (I64 with as_i32), `c_ulong` (U64 with as_u32), `c_longlong` (I64), `c_ulonglong` (U64)
  - NOTE: `c_char` (I8), `c_uchar` (U8), `c_short` (I16), `c_ushort` (U16) blocked by codegen bug: sub-byte struct fields cause "Expected expression" parse error when imported cross-module
- [x] 1.4 Implement `c_float` (F32), `c_double` (F64)
- [x] 1.5 Implement `c_size_t` (U64), `c_ssize_t` (I64), `c_ptrdiff_t` (I64), `c_intptr_t` (I64), `c_uintptr_t` (U64)
- [ ] 1.6 Add `#if WINDOWS` / `#if LINUX` conditional compilation for platform-dependent sizes (c_long) — BLOCKED: `#if` inside struct fields causes parse error
- [x] 1.7 Write tests: `lib/core/tests/ffi/primitive_types.test.tml` — 14 tests passing
- [x] 1.8 Update `core/mod.tml` to export `ffi` module

## Phase 2: CStr — Borrowed C String (`lib/core/src/ffi/cstr.tml`)

- [x] 2.1 Implement `CStr` struct — wrapper around `*U8` with null-terminator guarantee
- [x] 2.2 `CStr::from_ptr(ptr: *U8) -> CStr` — wrap existing C string
- [x] 2.3 `CStr::as_ptr(this) -> *U8` — get raw pointer
- [x] 2.4 `CStr::to_str(this) -> Str` — uses `tml_str_from_cstr` FFI (identity cast, borrows same memory)
- [x] 2.5 `CStr::to_owned_str(this) -> Str` — heap copy via `mem_alloc` + `copy_nonoverlapping` (replaces `to_str_lossy`)
- [x] 2.6 `CStr::len(this) -> I64` — byte length via `cstr_strlen` helper
- [x] 2.7 `CStr::is_empty(this) -> Bool`
- [x] 2.8 `CStr::byte_at(this, index: I64) -> U8` — byte accessor
- [x] 2.9 Implement `Display`, `PartialEq` for CStr — impl via `to_str()` comparison
- [x] 2.10 Write tests: `lib/core/tests/ffi/cstr.test.tml` — 10 tests passing (len, empty, byte_at, as_ptr, to_str, to_owned_str, eq, display)

## Phase 3: Integration with Existing FFI Code

- [x] 3.1 Update `core/mod.tml` to export `ffi` module
- [ ] 3.2 Create migration guide: which `*U8` patterns should become `*c_void`, etc.
- [x] 3.3 Core has only 1 `@extern("c")` (in cstr.tml itself). Std migrations tracked in phase1-04.
- [x] 3.4 Run full core/ffi test suite — 2 suites, 10+ tests, all passing

## Blockers

1. **Sub-byte struct fields** (`I8`, `U8`, `I16`, `U16`): Importing a module containing structs with these field types causes "Expected expression" parse error in the consumer. Affects `c_char`, `c_uchar`, `c_short`, `c_ushort`.
2. ~~**`str_from_raw` codegen**~~ — RESOLVED: Used `tml_str_from_cstr` FFI instead (identity cast since Str=ptr).
3. **`#if` inside structs**: Conditional compilation directives inside type definitions cause parse errors. Affects platform-dependent `c_long` definition.
