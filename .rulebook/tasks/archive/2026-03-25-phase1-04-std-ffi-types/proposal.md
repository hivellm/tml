# Proposal: Std FFI Types — Heap-Owning FFI Strings

## Status: PROPOSED

## Summary

`CStr` (from phase1-03) is a borrowed view into an existing C string — it cannot allocate or own memory. `CString` is the owned counterpart: it heap-allocates a null-terminated byte string that can be passed to C APIs that take ownership or require a stable address. `OsStr`/`OsString` extend this to platform-native paths (UTF-16 on Windows, raw bytes on POSIX), which cannot be represented as plain UTF-8 `Str`.

## Motivation

Many C APIs expect an owned, null-terminated string: `open(2)`, `dlopen`, `exec`, and most path-related syscalls. Without `CString`, TML code must manually allocate a buffer, copy the string, append a null byte, and remember to free it — a four-step process that routinely leaks or double-frees. `CString::new` does this safely, and `Drop` frees it automatically.

`OsString` is required for correct path handling on Windows, where `CreateFile` takes a `LPCWSTR` (UTF-16) not a `char*`. A `File::open(path: Str)` silently breaks on paths containing non-ASCII characters on Windows without an `OsString` layer.

## Design

`CString` is a newtype over `Buffer` (owned bytes). It validates on construction (no interior nulls), appends the null byte, and frees via `Drop`. `into_raw`/`from_raw` enable handoff to C code that takes ownership.

`OsStr` and `OsString` use conditional compilation: on Windows they store UTF-16 (`List[U16]`); on POSIX they store raw bytes (`Buffer`). Conversion to `Str` is always checked (returns `Maybe[Str]`) because the platform representation may not be valid UTF-8/UTF-16.

Both types live in `lib/std/src/ffi/` because they require heap allocation (unavailable in core).

## What Changes

- New: `lib/std/src/ffi/mod.tml` — module root with exports
- New: `lib/std/src/ffi/cstring.tml` — `CString` with new, from_raw, into_raw, as_cstr, Drop
- New: `lib/std/src/ffi/os_str.tml` — `OsStr`, `OsString` with platform-conditional internals
- Modified: `lib/std/src/mod.tml` — add `ffi` module export
- Modified: `lib/std/src/file/path.tml` — use `OsStr` internally for path components
- New: `lib/std/tests/ffi/cstring.test.tml`
- New: `lib/std/tests/ffi/os_str.test.tml`

## Dependencies

- Depends on: `phase1-03-core-ffi-types` (CStr, c_char)
- Enables: correct path handling on Windows in `std/file`
- Enables: safe ownership transfer in any `@extern("c")` call that allocates

## Risks

- `OsString` UTF-16 on Windows adds conversion overhead on every path operation; the design must make the fast (pre-converted) path cheap
- `CString::from_raw` is inherently unsafe — the caller must guarantee the pointer came from a C allocator compatible with TML's `mem_free`; documentation must be explicit
