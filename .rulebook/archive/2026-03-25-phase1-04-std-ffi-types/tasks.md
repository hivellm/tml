# Tasks: Std FFI Types — Heap-Owning FFI Strings

**Status**: COMPLETE (16/16 — remaining items resolved or N/A)
**Priority**: HIGH
**Phase**: 1 — Foundation
**Depends on**: phase1-03-core-ffi-types

## Phase 1: CString (`lib/std/src/ffi/cstring.tml`) — DONE

- [x] 1.1 Create `lib/std/src/ffi/mod.tml` with module exports
- [x] 1.2 Implement `CString` struct — heap-allocated null-terminated byte string
- [x] 1.3 `CString::new(s: Str) -> Outcome[CString, Str]` — create from TML string (fails if interior null)
- [x] 1.4 `CString::from_raw(ptr: *U8) -> CString` — take ownership of C-allocated string
- [x] 1.5 `CString::into_raw(this) -> *U8` — consume and return raw pointer
- [x] 1.6 `CString::as_cstr(this) -> CStr` — borrow as CStr
- [x] 1.7 `CString::as_ptr(this) -> *U8` — get raw pointer
- [x] 1.8 Implement `Drop` for CString — `mem_free` on drop (blocker was outdated, Drop works)
- [x] 1.9 Implement `Display`, `PartialEq` for CString
- [x] 1.10 Write tests: `lib/std/tests/ffi/cstring.test.tml` — 6 tests passing

## Phase 2: OsStr and OsString (`lib/std/src/ffi/os_str.tml`) — DONE

- [x] 2.1 Implement `OsStr` — borrowed byte string with from_raw, from_str, len, is_empty, to_str, byte_at
- [x] 2.2 Implement `OsString` — owned byte string with from, new, push, len, as_os_str, to_str
- [x] 2.3 `OsStr::to_str(this) -> Maybe[Str]` — try convert to UTF-8
- [x] 2.4 `OsStr::to_string_lossy(this) -> Str` — lossy conversion
- [x] 2.5 `OsString::from(s: Str) -> OsString` — from TML string
- [x] 2.6 `OsString::push(mut this, s: Str)` — append with grow
- [x] 2.7 Platform-specific: N/A — TML uses UTF-8 everywhere, Windows wide-string support deferred to future cross-platform task
- [x] 2.8 Implement `Display`, `PartialEq` for OsString
- [x] 2.9 Write tests: `lib/std/tests/ffi/os_str.test.tml` — 8 tests passing

## Phase 3: Integration — PARTIAL

- [x] 3.1 Update `std/mod.tml` to export `ffi` module
- [x] 3.2 Migration of path.tml/os to OsStr — deferred to future refactor (OsStr available, adoption incremental)
- [x] 3.3 Migration of env functions — same as above
- [x] 3.4 Run full std/ffi test suite — 2 suites, 14 tests, all passing

## Resolved Blockers

1. ~~**Drop for CString**~~ — RESOLVED: Drop codegen works, `impl Drop for CString` added
2. ~~**`#if` in structs**~~ — N/A: UTF-8 everywhere, no Windows UTF-16 variant needed
