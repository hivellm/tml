# Proposal: Core FFI Types — Type-Safe Foreign Function Interface

## Status: PROPOSED

## Summary

Every `@extern("c")` declaration in TML currently uses raw `*U8` pointers for strings and untyped integers for C primitive types. This creates a silent ABI mismatch hazard — passing the wrong integer width or using a string pointer where a void pointer is expected compiles without error but corrupts memory at runtime.

This task introduces a `core::ffi` module modeled after Rust's `core::ffi`: platform-correct integer type aliases (`c_int`, `c_long`, `c_size_t`, etc.) and a `CStr` borrowed-string wrapper that enforces null-termination at the type level.

## Motivation

Type safety at FFI boundaries eliminates an entire class of ABI bugs. `c_long` is 32 bits on Windows but 64 bits on Linux — code that uses `I64` directly is silently wrong on one platform. `CStr` prevents calling `strlen` on an uninitialized pointer and makes UTF-8 validation explicit. These are not nice-to-haves: every `@extern("c")` call in TML and std that touches strings or platform integers needs these types to be correct.

## Design

A new `lib/core/src/ffi/` module with two files:

- `mod.tml` — exports and re-exports all FFI types
- `cstr.tml` — `CStr` struct wrapping a `*c_char` with null-terminator invariant

Integer aliases use `#if WINDOWS` / `#if LINUX` conditional compilation for `c_long` (I32 on Windows, I64 on POSIX). All aliases are type aliases (`type c_int = I32`), so they participate in TML's type system without new runtime overhead.

`CStr::from_ptr` is the only unsafe entry point — it trusts that the C string is properly null-terminated. All other methods are safe wrappers that compute length by scanning for the null byte.

## What Changes

- New: `lib/core/src/ffi/mod.tml` — `c_void`, integer aliases, float aliases, size types
- New: `lib/core/src/ffi/cstr.tml` — `CStr` struct with Display, Debug, PartialEq, Eq, Hash
- Modified: `lib/core/src/mod.tml` — add `ffi` module export
- New: `lib/core/tests/ffi/primitive_types.test.tml`
- New: `lib/core/tests/ffi/cstr.test.tml`
- Updated: 3-5 representative `@extern("c")` declarations in core migrated to use new types

## Dependencies

- Depends on: nothing (core has no external dependencies)
- Enables: `phase1-04-std-ffi-types` (CString and OsString depend on CStr from core)
- Enables: broader migration of `@extern("c")` declarations in std/net, std/crypto, std/file

## Risks

- `c_long` platform divergence requires discipline — existing code may assume one size; the migration guide in phase 3.2 must be clear
- `CStr::to_str` UTF-8 validation adds a scan cost; the lossy variant must be available for performance-sensitive paths
