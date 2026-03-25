# Tasks: Core FFI Types — Type-Safe Foreign Function Interface

**Status**: Proposed
**Priority**: HIGH
**Phase**: 1 — Foundation

## Motivation

Every `@extern("c")` call in TML uses raw `*U8` pointers for strings and untyped integers for C types. This is unsafe and error-prone. Rust's `core::ffi` provides type-safe wrappers (`CStr`, `c_void`, `c_char`, `c_int`) that catch ABI mismatches at compile time.

## Phase 1: Core FFI Primitive Types (`lib/core/src/ffi/`)

- [ ] 1.1 Create `lib/core/src/ffi/mod.tml` with module exports
- [ ] 1.2 Implement `c_void` — opaque type for void pointers (`*c_void` replaces `*U8` for opaque handles)
- [ ] 1.3 Implement C integer type aliases: `c_char` (I8), `c_uchar` (U8), `c_short` (I16), `c_ushort` (U16), `c_int` (I32), `c_uint` (U32), `c_long` (I32 on Windows, I64 on Linux), `c_ulong`, `c_longlong` (I64), `c_ulonglong` (U64)
- [ ] 1.4 Implement `c_float` (F32), `c_double` (F64)
- [ ] 1.5 Implement `c_size_t` (U64), `c_ssize_t` (I64), `c_ptrdiff_t` (I64)
- [ ] 1.6 Add `#if WINDOWS` / `#if LINUX` conditional compilation for platform-dependent sizes (c_long)
- [ ] 1.7 Write tests: `lib/core/tests/ffi/primitive_types.test.tml`

## Phase 2: CStr — Borrowed C String (`lib/core/src/ffi/cstr.tml`)

- [ ] 2.1 Implement `CStr` struct — wrapper around `*c_char` with null-terminator guarantee
- [ ] 2.2 `CStr::from_ptr(ptr: *c_char) -> CStr` — wrap existing C string (unsafe, trusts null terminator)
- [ ] 2.3 `CStr::as_ptr(this) -> *c_char` — get raw pointer
- [ ] 2.4 `CStr::to_str(this) -> Outcome[Str, Str]` — convert to TML Str (validates UTF-8)
- [ ] 2.5 `CStr::to_str_lossy(this) -> Str` — convert with replacement for invalid UTF-8
- [ ] 2.6 `CStr::len(this) -> I64` — byte length (not including null)
- [ ] 2.7 `CStr::is_empty(this) -> Bool`
- [ ] 2.8 Implement `Display`, `Debug`, `PartialEq`, `Eq`, `Hash` for CStr
- [ ] 2.9 Write tests: `lib/core/tests/ffi/cstr.test.tml`

## Phase 3: Integration with Existing FFI Code

- [ ] 3.1 Update `core/mod.tml` to export `ffi` module
- [ ] 3.2 Create migration guide: which `*U8` patterns should become `*c_void`, `*c_char`, etc.
- [ ] 3.3 Migrate 3-5 representative `@extern("c")` declarations in core to use new types (as examples)
- [ ] 3.4 Run full core test suite to verify no regressions
