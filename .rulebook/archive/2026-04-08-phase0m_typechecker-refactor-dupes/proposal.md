# Proposal: Deduplicate ParsedModuleFile and resolve_imported_symbol Call

## Why

Two maintenance-grade findings from the phase12c audit that are not user-visible bugs but are fragility sources:

**B-09 — Duplicate ParsedModuleFile definition in two translation units** (`env_module_load.cpp:46-235`, `env_module_load_decls.cpp:13-204`): The struct `ParsedModuleFile` and several helper functions are defined as `static` in both translation units. A change to one copy that is not mirrored in the other introduces silent divergence. This is a ticking time bomb: the next time someone edits the helpers in one file without noticing the duplicate, we get subtle module loading bugs.

**B-10 — Duplicate resolve_imported_symbol call for constants** (`checker/expr.cpp:457, 506`): `check_ident` calls `resolve_imported_symbol` twice — once for struct/enum lookup at line 457, then again for constant lookup at line 506. Both calls record side effects. Not a functional bug but wastes cycles and fires side effects twice.

## What Changes

1. **B-09 fix**: Move `ParsedModuleFile` and its helpers into a shared header (`compiler/include/types/parsed_module_file.hpp` or similar). Both translation units include it. Remove the duplicates.

2. **B-10 fix**: Cache the result of the first `resolve_imported_symbol` call in `check_ident`. If it resolved to a struct/enum, use that. If it didn't, reuse the negative result for the constants path instead of calling again.

## Impact

- **Affected specs**: `docs/specs/typechecker-invariants.md` (remove B-09, B-10 from Appendix B)
- **Affected code**: `compiler/src/types/env_module_load.cpp`, `compiler/src/types/env_module_load_decls.cpp`, `compiler/include/types/parsed_module_file.hpp` (new), `compiler/src/types/checker/expr.cpp`
- **Breaking change**: NO — pure refactor, no user-visible behavior change.
- **User benefit**: less fragile code; slightly faster `check_ident`.
