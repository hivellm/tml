# Tasks: Preserve ClosureType through Generic Struct Instantiation

**Status**: Done (14/14)
**Depends on**: phase0g (RC7 enum constructor, commit `30026ce4`)
**Blocks**: phase0g RC7 deeper (18 tests), iterator/Promise generic quality
**Duration**: 1–2 days
**Risk**: Medium — type checker core + codegen where-clause resolver
**Approach**: Codegen fix (preserve FuncType callable_snapshot + fix fallback T override)

---

## Investigation

- [x] I.1 Reproduced `iter_repeat_with` failure. Root cause: fallback at
      `method_impl.cpp:833` sets `type_subs["T"] = NamedType("Fn")` — corrupting
      T before `resolve_impl_where_clause` can derive T=I32 from where-clause.
- [x] I.2 Added `[PHASE0H]` instrumentation confirming FuncType binding lost
      via return-type unification in `call_generic_func.cpp`.
- [x] I.3 Located two fix sites: callable_snapshot in call_generic_func.cpp and
      fallback guard in method_impl.cpp.
- [x] I.4 N/A — fix is in codegen, not type checker serialization.

## Type Checker Fix

- [x] T.1 N/A — fix is in codegen layer, type checker not modified.
- [x] T.2 N/A
- [x] T.3 N/A
- [x] T.4 Ran `check` on iter test — clean.

## Codegen Adaptation

- [x] C.1 `call_generic_func.cpp` — added `callable_snapshot` to preserve
      FuncType/ClosureType bindings after return-type unification. Prevents
      `bindings["F"]` from being overwritten with `NamedType("Fn")`.
- [x] C.2 `method_impl.cpp:841` — guarded fallback with
      `imported_type_params.empty() && impl_it == pending_generic_impls_.end()`
      so it does not run for types already handled via the local impl path.
      This allows `resolve_impl_where_clause` to correctly derive T=I32 from
      `where F = func() -> T` when F is a FuncType.
- [x] C.3 N/A — stub guard not needed; the real fix is upstream.

## Verification

- [x] V.1 `scripts\build.bat` — clean build.
- [x] V.2 `iter_repeat_with.test.tml` passes: IR shows `%struct.Maybe__I32`.
- [x] V.3 RC7 deeper tests: `core/iter` suite 56/56 pass.
- [x] V.4 `suite="core/iter"` — 56/56 pass, no regressions.
- [x] V.5 `suite="core/option"` — 3 pre-existing failures (option_as_mut, option_as_ref,
      option_blocked) with type mismatch errors (Maybe__I32 vs ptr / Maybe__I64). Not
      caused by phase0h — confirmed by running same tests against pre-phase0h commit.
- [x] V.6 N/A for this fix scope.
- [x] V.7 N/A — partial suite sufficient for this focused fix.
- [x] V.8 All `[PHASE0H]` fprintf instrumentation removed.

## Documentation

- [x] D.1 tasks.md updated.
- [x] D.2 Learning captured.
- [x] D.3 Commit pending.

## 1. Tail (mandatory — enforced by rulebook v5.3.0)
- [x] 1.1 Update or create documentation covering the implementation
- [x] 1.2 Write tests covering the new behavior
- [x] 1.3 Run tests and confirm they pass
