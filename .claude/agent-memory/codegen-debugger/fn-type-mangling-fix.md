---
name: FuncType Fn Mangling Fix
description: Fix for parse_tokens_with_pattern greedy token consumption + FuncType mangling as "Fn" causing wrong struct types and undefined methods
type: project
---

## FuncType "Fn" Mangling Fix (2026-03-16) -- FIXED

### Root Cause
Two bugs in `generic.cpp` related to FuncType mangling:

1. **Greedy token consumption in `parse_tokens_with_pattern`**: When a bare type param (e.g., `I` in `Map[I, F]`) encountered remaining tokens, it consumed ALL of them (`while (pos < tokens.size())`), stealing tokens from subsequent type params. For `Map__Counter__Fn`, `I` consumed both `Counter` and `Fn`, creating bogus `Counter[Fn]` type.

2. **FuncType → "Fn" lossy mangling**: `llvm_types.cpp:1063` mangles FuncType as just `"Fn"`. When demangled, `parse_mangled_type_string("Fn")` previously returned nothing useful. Fixed to return `NamedType("Fn")` which `llvm_type_from_semantic` correctly maps to `{ ptr, ptr }` (fat pointer).

### Fix
**File**: `compiler/src/codegen/llvm/core/generic.cpp`

1. Added `remaining_siblings` parameter to `parse_tokens_with_pattern`. When `remaining_siblings > 0`, consume exactly 1 token per bare type param. Only the LAST type param (remaining_siblings=0) can consume multiple tokens for generic types like `SliceIter__I32`.

2. Added `"Fn"` handling in `parse_mangled_type_string` to return `NamedType("Fn")`. This allows `llvm_type_from_semantic` to map it to `{ ptr, ptr }`, and closure calls default to `i32` return type (the default in call.cpp:1600).

3. Added merge protection in two places: when merging new_subs from tokenized mangled names, don't override an existing `FuncType` with a `NamedType("Fn")`.

### Key Discovery: Incremental Cache Stale IR
The `.incr-cache/ir/` directory stores generated LLVM IR. When compiler codegen is modified, the cache still returns `GREEN: reusing cached codegen result`, causing stale IR to be used. **Must delete/rename `.incr-cache/` after codegen fixes.**

**Why**: The incremental cache keys on source file content + compiler flags, NOT compiler DLL version.

### Impact
- Fixed 7 iter tests (52/52 now, was 45/52)
- iter_repeat_with and iter_sources: fixed "Unknown method: gen" (token consumption was corrupting type resolution)
- iter_map, iter_filter_map, iter_map_while, iter_scan, iter_size_hints: fixed "@tml_*__Fn_next undefined" (FuncType mangling)

### Affected Files
- `compiler/src/codegen/llvm/core/generic.cpp` (parse_tokens_with_pattern, parse_mangled_type_string, merge protection)
- `compiler/src/codegen/llvm/decl/impl.cpp` (debug cleanup only)
