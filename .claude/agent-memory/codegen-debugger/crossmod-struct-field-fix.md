---
name: Cross-module generic struct field access fix
description: lookup_struct didn't follow re-exports + core::future missing pub use for task types
type: project
---

# Cross-Module Generic Struct Field Access Resolves as ()

**Date**: 2026-03-21
**Status**: FIXED

## Bug
`f.value` where `f: Ready[I32]` (imported from core::future) resolved field type as `()` instead of `Maybe[I32]`.

## Root Cause (Two Issues)

### 1. ModuleRegistry::lookup_struct didn't follow re-exports
- `lookup_enum` had `lookup_enum_impl` that follows `module->re_exports` recursively
- `lookup_struct` did NOT — it only checked `module->structs.find(symbol_name)` directly
- When `Ready` was imported via `core::future` which re-exported it from `core::task`, the lookup failed

### 2. Library: core::future::mod.tml used private `use` instead of `pub use`
- `use core::task::{Context, Poll, Ready, Pending}` was private
- Changed to `pub use core::task::{Context, Poll, Ready, Pending}`
- Without `pub use`, these types aren't in `module->re_exports`, so even with the fix, they wouldn't be found

## Fix Files
- `compiler/src/types/module.cpp`: Added `lookup_struct_impl` with re-export following (matches `lookup_enum_impl` pattern)
- `compiler/include/types/module.hpp`: Added `lookup_struct_impl` private method declaration
- `lib/core/src/future/mod.tml`: Changed `use` to `pub use` for core::task types

## Key Pattern
When a type appears to be importable but field access/struct init fails, check:
1. Is the type directly defined in the module or re-exported?
2. Does `lookup_struct` follow re-exports? (Now it does, after this fix)
3. Is the re-export `pub use` not just `use`?

## Note: resolve_type_path fallback
`resolve_type_path` at line 337-339 returns `NamedType{name, "", {}}` as a FALLBACK for unknown types. This means type annotations like `Ready[I32]` "work" even when the struct isn't properly found — the type is created but struct operations (field access, struct init) fail later.
