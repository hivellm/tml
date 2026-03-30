---
name: ListIter case-sensitive module resolution fix
description: Windows NTFS case-insensitive fs::exists caused std::collections::List to match list.tml, skipping parent module load and leaving ListIter unregistered
type: project
---

## Bug: %struct.ListIter__I32 never defined — "Cannot allocate unsized type"

**Root cause**: On Windows (NTFS), `std::filesystem::exists()` is case-insensitive. When processing `use std::collections::List`, the module resolver tried `std::collections::List` as a module path, which resolved to `list.tml` (case-insensitive match). This made the compiler think `List` was a standalone module file, so it never loaded the parent `std::collections` module (from `mod.tml` + all sibling files). As a result, `behaviors.tml` — which defines `ListIter[T]` — was never loaded, and the struct was never registered in the module registry.

**Symptoms**:
1. `%struct.ListIter__I32` used but never defined → "Cannot allocate unsized type"
2. All GEP field indices were 0 (get_field_index fallback) → wrong field access
3. All store types were i32 (default) instead of ptr/i64

**Fix**: Added `exists_case_sensitive()` helper that verifies filename matches case-sensitively by iterating the parent directory. Applied to:
- `compiler/src/types/env_module_loading.cpp` — module file resolution (resolve_lib_module_path + all fallback search paths)
- `compiler/src/types/module_binary_read.cpp` — binary meta cache (.tml.meta) lookup

**Files changed**:
- `compiler/src/types/env_module_loading.cpp` (exists_case_sensitive helper + all fs::exists calls for module files)
- `compiler/src/types/module_binary_read.cpp` (exists_case_sensitive helper + load_module_from_cache)

**Why**: This is Windows-specific. On Linux/macOS, `List.tml` ≠ `list.tml`, so the resolver naturally falls back to loading the parent `std::collections` module which includes all sibling files.

**Test**: `lib/std/tests/collections/behaviors.test.tml` — was failing, now passes. `std/collections` suite 94/94.
