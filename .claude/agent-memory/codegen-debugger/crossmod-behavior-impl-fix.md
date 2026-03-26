---
name: Cross-Module Behavior Impl has_pure_tml_functions Fix
description: Behavior impl methods without explicit `pub` caused modules to be skipped by codegen (source_code empty, has_pure_tml_functions=false)
type: project
---

## Cross-Module Behavior Impl Dispatch Fix (2026-03-25, FIXED)

### Bug
When a behavior impl is defined in module A for a type from module B (e.g., `impl ToJson for I64` in `std::json::serialize`), calling the method from module C generates a reference to the correct mangled name but the function definition is never emitted.

### Root Cause
NOT a mangling bug. The mangling was correct (`tml_N4core3I647to_jsonE`). The real issue:

1. `has_pure_tml_functions` check in `env_module_support.cpp:1641` required `method.vis == parser::Visibility::Public`
2. Behavior impl methods like `func to_json(this) -> Json` don't have explicit `pub` keyword in TML
3. The parser defaults method visibility to `Private` when no `pub` keyword is present
4. So `has_pure_tml_functions` stayed `false` for modules containing ONLY behavior impls without `pub`
5. When `has_pure_tml_functions=false`, `source_code` is not stored (line 1688)
6. Binary meta cache serialized `has_pure_tml_functions=false` and `source_code=""`
7. Codegen's `emit_module_pure_tml_functions` skips modules with `!has_pure_tml_functions || source_code.empty()`

### Fix
- `compiler/src/types/env_module_support.cpp:1641`: Removed `method.vis == parser::Visibility::Public` requirement from the `has_pure_tml_functions` check for impl blocks. Now ANY impl method with a body (not unsafe) triggers `has_pure_tml_functions=true`.
- `compiler/include/types/module_binary.hpp:54`: Bumped MODULE_META_VERSION_MAJOR from 7 to 8 to force regeneration of all stale binary meta caches.

### Key Insight
The codegen's method generation filter at `runtime_modules.cpp:1169-1174` already handles non-public methods correctly in lazy mode (bypasses the `Visibility::Public` check). The only place that was broken was `has_pure_tml_functions` which gates whether the module's source code is stored and whether the module is processed at all.

### Affected Modules
Any module containing behavior impls for foreign types WITHOUT explicit `pub` on methods. Primary examples:
- `std::json::serialize` (ToJson/FromJson for primitives)
- Any user module with `impl SomeBehavior for ForeignType { func method(this)... }`

### Files Changed
- `compiler/src/types/env_module_support.cpp` (has_pure_tml_functions check)
- `compiler/include/types/module_binary.hpp` (meta version bump v7 -> v8)
- `lib/std/tests/json/json_serialize_impls.test.tml` (new test: 5 tests for ToJson on primitives)
