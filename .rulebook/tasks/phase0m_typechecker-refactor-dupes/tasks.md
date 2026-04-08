# Tasks: Deduplicate ParsedModuleFile and resolve_imported_symbol Call

**Status**: In Progress (10/10)
**Depends on**: None
**Blocks**: Nothing critical — cleanup task
**Duration**: 1 day
**Risk**: Low — pure refactor
**Related bugs**: B-09, B-10 in `docs/specs/typechecker-invariants.md` Appendix B

---

## B-09 — Deduplicate ParsedModuleFile

- [x] B9.1 Read both definitions side-by-side: `env_module_load.cpp:46-235` and `env_module_load_decls.cpp:13-204`. Diff them. Document any differences.
- [x] B9.2 If they have diverged, reconcile first — pick the correct version and note the reconciliation in the commit message.
- [x] B9.3 Create `compiler/include/types/parsed_module_file.hpp` with the struct + helper function declarations.
- [x] B9.4 Create `compiler/src/types/parsed_module_file.cpp` with the helper function definitions (non-template, non-static).
- [x] B9.5 Include the new header in both `env_module_load.cpp` and `env_module_load_decls.cpp`; remove the duplicated static definitions.
- [x] B9.6 Update `compiler/CMakeLists.txt` if needed to add the new source file.

## B-10 — Cache resolve_imported_symbol Result

- [x] B10.1 Read `compiler/src/types/checker/expr.cpp:457, 506` to confirm the duplicate call.
- [x] B10.2 Introduce a local cache variable in `check_ident` that stores the first call's result (including negative).
- [x] B10.3 Replace the second call with the cached value. Ensure side effects (e.g. usage tracking) still fire exactly the right number of times per the invariant audit.

## Verification

- [x] V.1 Build via `scripts\build.bat`.
- [x] V.2 Full test suite via `mcp__tml__test` `structured=true`. Confirm zero regressions — pure refactor.

## Documentation

- [x] D.1 Update `docs/specs/typechecker-invariants.md` Appendix B: remove B-09, B-10 after commit lands.
- [x] D.2 Commit with conventional message: `refactor(types): deduplicate ParsedModuleFile and resolve_imported_symbol call (phase0m)`.

## 1. Tail (mandatory — enforced by rulebook v5.3.0)
- [x] 1.1 Update or create documentation covering the implementation
- [x] 1.2 Write tests covering the new behavior
- [x] 1.3 Run tests and confirm they pass
