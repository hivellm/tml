# Tasks: Std FFI Types — Heap-Owning FFI Strings

**Status**: Proposed
**Priority**: HIGH
**Phase**: 1 — Foundation
**Depends on**: phase1-03-core-ffi-types

## Motivation

`CStr` is borrowed (no allocation). `CString` owns the memory and ensures null-termination. `OsStr`/`OsString` handle platform-native paths (UTF-16 on Windows, bytes on Unix). These are essential for correct OS interop.

## Phase 1: CString (`lib/std/src/ffi/cstring.tml`)

- [ ] 1.1 Create `lib/std/src/ffi/mod.tml` with module exports
- [ ] 1.2 Implement `CString` struct — heap-allocated null-terminated byte string
- [ ] 1.3 `CString::new(s: Str) -> Outcome[CString, Str]` — create from TML string (fails if interior null)
- [ ] 1.4 `CString::from_raw(ptr: *c_char) -> CString` — take ownership of C-allocated string
- [ ] 1.5 `CString::into_raw(this) -> *c_char` — consume and return raw pointer (caller must free)
- [ ] 1.6 `CString::as_cstr(this) -> CStr` — borrow as CStr
- [ ] 1.7 `CString::as_ptr(this) -> *c_char` — get raw pointer
- [ ] 1.8 Implement `Drop` for CString (free heap memory)
- [ ] 1.9 Implement `Display`, `Debug`, `Clone`, `PartialEq`, `Eq`, `Hash`
- [ ] 1.10 Write tests: `lib/std/tests/ffi/cstring.test.tml`

## Phase 2: OsStr and OsString (`lib/std/src/ffi/os_str.tml`)

- [ ] 2.1 Implement `OsStr` — borrowed platform-native string (UTF-16 on Windows, bytes on Unix)
- [ ] 2.2 Implement `OsString` — owned platform-native string
- [ ] 2.3 `OsStr::to_str(this) -> Maybe[Str]` — try convert to UTF-8
- [ ] 2.4 `OsStr::to_string_lossy(this) -> Str` — lossy conversion
- [ ] 2.5 `OsString::from(s: Str) -> OsString` — from TML string
- [ ] 2.6 `OsString::push(mut this, s: Str)` — append
- [ ] 2.7 Platform-specific: `#if WINDOWS` use UTF-16, `#if LINUX` use raw bytes
- [ ] 2.8 Implement `Display`, `Debug`, `Clone`, `PartialEq`, `Eq`, `Hash` for both
- [ ] 2.9 Write tests: `lib/std/tests/ffi/os_str.test.tml`

## Phase 3: Integration

- [ ] 3.1 Update `std/mod.tml` to export `ffi` module
- [ ] 3.2 Update `std/file/path.tml` to use `OsStr` internally for path components
- [ ] 3.3 Update `std/os/mod.tml` env functions to return `OsString` where appropriate
- [ ] 3.4 Run full std test suite
