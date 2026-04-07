# Tasks: Preserve ClosureType through Generic Struct Instantiation

**Status**: Planned (0/14)
**Depends on**: phase0g (RC7 enum constructor, commit `30026ce4`)
**Blocks**: phase0g RC7 deeper (18 tests), iterator/Promise generic quality
**Duration**: 1–2 days
**Risk**: Medium — type checker core + codegen where-clause resolver
**Approach**: A (preserve ClosureType at upstream instantiation site)

---

## Investigation

- [ ] I.1 Reproduce `iter_repeat_with` failure: `mcp__tml__test` with
      `path="lib/core/tests/iter/iter_repeat_with.test.tml"`, `debug_layers=true`.
      Save HIR/MIR/IR dumps to `.sandbox/phase0h_repro/`.
- [ ] I.2 Add temporary `fprintf(stderr, ...)` instrumentation at
      `compiler/src/types/checker/expr_call.cpp` (or wherever closure args bind
      to generic struct fields) to log the exact `Type` recorded for `type_args[0]`.
      Confirm it is `NamedType("Fn")` and not `ClosureType`.
- [ ] I.3 Locate the site that collapses `ClosureType` → `NamedType("Fn")`.
      Likely in struct constructor argument type resolution or generic param
      binding. Grep for `"Fn"` string construction and `NamedType{"Fn"` literal.
- [ ] I.4 Verify cache compatibility: does `env_serialize.cpp` persist
      `ClosureType` correctly, or does it round-trip through a name? Check the
      binary format in `lib/std/meta/` cache entries for a test that uses
      closures.

## Type Checker Fix

- [ ] T.1 `compiler/src/types/type.hpp` — confirm `ClosureType` has `params`,
      `return_type`, `captures` fields and stable equality/hashing. Add any
      missing serialization hooks.
- [ ] T.2 `compiler/src/types/checker/expr_call.cpp` (or the correct file
      identified in I.3) — when a closure literal is passed as a generic
      struct field, store the full `ClosureType` in the struct's `type_args`
      instead of emitting `NamedType("Fn")`.
- [ ] T.3 `compiler/src/types/env_serialize.cpp` — ensure `ClosureType` round-
      trips through the meta cache. Add test if format changes.
- [ ] T.4 Run `mcp__tml__check` on 3 small test files exercising closures in
      generic structs. Verify no T-series regressions.

## Codegen Adaptation

- [ ] C.1 `compiler/src/codegen/llvm/expr/method_impl.cpp:556-557` — update
      `type_subs[F] = named.type_args[0]` to handle `ClosureType` directly.
      When the stored type is a `ClosureType`, the resolver can now extract
      return type via `closure.return_type` instead of pattern matching.
- [ ] C.2 `compiler/src/codegen/llvm/core/generic_instantiate_impl.cpp:441-468`
      — extend `match_pattern_type` to match `func() -> T` patterns against
      `ClosureType` (not just `FuncType`). Add unit tests in
      `compiler/tests/codegen/` if the helper is test-covered.
- [ ] C.3 `compiler/src/codegen/llvm/decl/impl.cpp:980-1011` — widen the stub-
      emission guard so it fires when impl-level generic params are missing
      entirely from `full_type_subs`, not only when existing entries are
      unresolved. Prevents regressions if another codepath slips through.

## Verification

- [ ] V.1 Build: `scripts\build.bat` (NEVER cmake directly). Confirm clean
      build with no clang errors.
- [ ] V.2 Run `iter_repeat_with` via `mcp__tml__test` with `debug_layers=true`.
      Confirm IR shows `%struct.Maybe__I32` in signature, not `Maybe__T`.
- [ ] V.3 Run all 18 RC7 deeper tests. Confirm each passes individually.
- [ ] V.4 Run `mcp__tml__test` with `suite="core/iter"` — full iterator suite
      regression check.
- [ ] V.5 Run `mcp__tml__test` with `suite="core/option"` — full Maybe suite.
- [ ] V.6 Run `mcp__tml__test` with `suite="std/promise"` — closures heavy.
- [ ] V.7 Full suite via `mcp__tml__test` with `structured=true`. Confirm no
      regressions vs baseline 1791/1874.
- [ ] V.8 Remove all `fprintf` instrumentation added in I.2.

## Documentation

- [ ] D.1 Update `.rulebook/tasks/phase0g_fix-214-compile-failures/tasks.md`:
      mark RC7 items 7.1/7.5 fully done, drop RC7 row to 0, update Current
      line + Progress Summary table, commit.
- [ ] D.2 Save learning to agent memory via `mcp__rulebook__rulebook_learn_capture`:
      "ClosureType must be preserved through generic struct instantiation —
      collapsing to NamedType('Fn') loses params/return and breaks monomorphization."
- [ ] D.3 Commit with conventional message:
      `fix(types): preserve ClosureType through generic struct instantiation (phase0h)`

## 1. Tail (mandatory — enforced by rulebook v5.3.0)
- [ ] 1.1 Update or create documentation covering the implementation
- [ ] 1.2 Write tests covering the new behavior
- [ ] 1.3 Run tests and confirm they pass
