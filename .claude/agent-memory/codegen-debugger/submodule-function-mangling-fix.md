---
name: Submodule Function Mangling Mismatch Fix
description: Free functions in submodules (e.g., core::fmt::helpers::i16_to_str) get wrong mangled names at call sites because module registry aggregates them into parent module
type: project
---

## Bug: Submodule Function Name Mangling Mismatch (2026-03-25, FIXED)

**Symptom**: `@tml_N4core3fmt10i16_to_strE_s` called but never declared/defined. Blocked 40+ compiler tests.

**Root Cause**:
- Module registry aggregates functions from submodule files into parent directory module. When `core::fmt::helpers::i16_to_str` is defined in `helpers.tml`, the module registry stores it under `core::fmt` (the parent module).
- `call_user.cpp` line 213-297 searches all modules and finds `i16_to_str` in `core::fmt`, setting `found_mod_name = "core::fmt"`.
- The mangled name becomes `@tml_N4core3fmt10i16_to_strE_s` (using parent path).
- But `gen_func_decl` (func.cpp) and `pre_register_func` use `current_module_name_ = "core::fmt::helpers"` (the submodule path), producing `@tml_N4core3fmt7helpers10i16_to_strE_s`.
- The mismatch means the call references a symbol that doesn't exist.

**Fix**: In `call_user.cpp` line 408-416 (module-qualified free function path), before constructing mangled name from `found_mod_name`, try to find the function in `functions_` by:
1. Exact qualified lookup: `found_mod_name + "::" + bare_name`
2. Suffix search: any key ending with `"::" + bare_name` that starts with `found_mod_name`

If found in `functions_`, use the registered `llvm_name` (which has the correct submodule path).

**Files Changed**: `compiler/src/codegen/llvm/expr/call_user.cpp` (line ~408)

**Why**: `pre_register_func` registers with keys like `core::fmt::helpers::i16_to_str` and `helpers::i16_to_str`, but call resolution tries `i16_to_str` (bare) and `core::fmt::impls::i16_to_str` (caller's module). The fix bridges this gap by looking up the actual registered entry.

**How to apply**: This same pattern affects ANY free function in a submodule called from another submodule within the same parent directory module. The fix is generic — it works for all such cases.

**Affected functions**: All `*_to_str` helpers in `core::fmt::helpers`, and potentially functions in other submodule structures (e.g., `core::reflect::*`, `std::zlib::*`).
