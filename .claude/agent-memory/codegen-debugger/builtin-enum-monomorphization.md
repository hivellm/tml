# Built-in Enum Monomorphization Bug

## Bug: Maybe[T]::default() and similar generic behavior impls on built-in enums

### Symptom
`Maybe[I32]::default()` emits `call @tml_N4core10Maybe__I327defaultE()` but the function body is never generated, causing LLVM "use of undefined value" error.

### Root Causes (Two bugs combine)

#### Bug 1: is_library_type=false for built-in enums
- File: `method_static_dispatch.cpp:747-764`
- `is_imported` checks `mod.enums.find("Maybe")` across all modules
- `Maybe` is a BUILT-IN enum created at `generate.cpp:263-282`, NOT in any module's enums map
- So `is_imported` stays false, `pim.is_library_type = false`

#### Bug 2: Module search skips all modules when is_library_type=false
- File: `generic.cpp:569-574`
- Filter: `if (!has_struct && !pim.is_library_type) continue;`
- Only checks `mod.structs` and `mod.internal_structs`, NOT `mod.enums`
- Since Maybe is not a struct in any module AND is_library_type=false, ALL modules are skipped
- `core::default` (which has the impl) is never parsed

#### Secondary: pending_generic_impls_ overwrite
- File: `runtime_modules.cpp:999-1001`
- `pending_generic_impls_["Maybe"] = &impl;` -- single-valued map overwrites
- 10+ `impl[T] Maybe[T]` blocks in core::option plus `impl[T] Default for Maybe[T]` in core::default
- Only the LAST one processed survives; system relies on fallback module search which fails

### Fix Locations
1. `method_static_dispatch.cpp:751-764` -- also check `pending_generic_enums_` for is_imported
2. `generic.cpp:569-572` -- also check `mod.enums` in has_struct computation
3. `runtime_modules.cpp:1001` -- use multimap or vector to store multiple impls per type (larger refactor)

### Affected Types
All built-in enums: Maybe, Outcome, Ordering, Poll, ControlFlow (see `generate.cpp:263-340`)
Any behavior impl defined in a DIFFERENT module than where the type's other impls live.

### Debug Trace Command
```bash
TML_LOG=debug build/debug/bin/tml.exe build file.tml --emit-ir 2>&1 | grep -E "IMPL_INST|STATIC_METHOD"
```
