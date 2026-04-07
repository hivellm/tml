# Tasks: Key Behaviors/Traits/Functions by FQN

**Status**: Planned (0/24)
**Depends on**: phase0g (RC1-RC6 done)
**Blocks**: `io_write_basic` and any multi-trait-with-same-short-name program
**Duration**: 1–2 days
**Risk**: High — core type checker + codegen + cache format

---

## Investigation

- [ ] I.1 Reproduce `io_write_basic` failure: `mcp__tml__test` with
      `path="lib/core/tests/io/io_write_basic.test.tml"` `debug_layers=true`.
      Confirm IR contains `%struct.File` GEP against `FakeWriter`.
- [ ] I.2 Grep for all insertion sites into `behaviors_`, `trait_decls_`,
      `functions_` maps. Build a complete list (proposal lists known 9 sites,
      verify none are missed):
      `grep -rn "behaviors_\[" compiler/src/`
      `grep -rn "trait_decls_\[" compiler/src/`
      `grep -rn "functions_\[" compiler/src/`
- [ ] I.3 Identify all lookup sites:
      `grep -rn "lookup_behavior\|lookup_function" compiler/`
- [ ] I.4 Survey other trait short-name collisions in the standard library:
      Display, Debug, Clone, Eq, Hash, Ord — are any defined in 2+ modules?
      Document findings in `.sandbox/phase0i_collisions.md`.
- [ ] I.5 Check meta cache serialization format
      (`compiler/src/types/module_binary_write.cpp` and `_read.cpp`) — does it
      emit short name or FQN? Will format bump be required?

## Type Checker Refactor

- [ ] T.1 `compiler/include/types/env.hpp` — change `behaviors_` map key
      comment to "FQN (module::Name)". Add helper
      `TypeEnv::lookup_behavior_by_short_name(const std::string&)` for legacy
      callers, returning `Maybe<FQN>` (or logging ambiguity).
- [ ] T.2 `compiler/src/types/env_definitions.cpp:36` — insert by FQN. Compute
      FQN from `def.module_path + "::" + def.name`.
- [ ] T.3 Same for `functions_` map insertions in env_definitions.cpp.
- [ ] T.4 Update all `lookup_behavior(short_name)` call sites in
      `compiler/src/types/checker/` to pass FQN when known (resolved via
      module scope), or fall back to `lookup_behavior_by_short_name` with
      ambiguity diagnostics.
- [ ] T.5 Run `mcp__tml__check` on 5 small test files (one-trait, two-trait,
      multi-module). Verify no T-errors introduced.

## Codegen Refactor

- [ ] C.1 `compiler/src/codegen/llvm/core/generate.cpp:770` — insert
      `trait_decls_` by FQN.
- [ ] C.2 `compiler/src/codegen/llvm/core/generate_first_pass.cpp:216` — same.
- [ ] C.3 `compiler/src/codegen/llvm/core/generate_function_bodies.cpp:531` — same.
- [ ] C.4 `compiler/src/codegen/llvm/core/generic_instantiate_impl.cpp:1186` — same.
- [ ] C.5 `compiler/src/codegen/llvm/core/generic_instantiate_impl.cpp:1271` — same.
- [ ] C.6 `compiler/src/codegen/llvm/core/generic_instantiate_impl.cpp:1521` — same.
- [ ] C.7 `compiler/src/codegen/llvm/core/runtime_modules.cpp:846` — same.
- [ ] C.8 `compiler/src/codegen/llvm/core/runtime_modules_tml.cpp:796` — same.
- [ ] C.9 `compiler/src/codegen/llvm/core/dyn.cpp:147` — update
      `emit_vtables` to resolve `impl.trait_type` to FQN (via
      type checker's module resolution), then call `lookup_behavior(fqn)`.

## Cache Format

- [ ] F.1 `compiler/src/types/module_binary_write.cpp` — verify behavior/
      function entries serialize FQN or can be reconstructed on read.
- [ ] F.2 `compiler/src/types/module_binary_read.cpp` — same, read path.
- [ ] F.3 Bump `META_CACHE_VERSION` if format changed. Invalidate existing
      caches via `mcp__tml__cache_invalidate` or rely on version check.

## Regression Test

- [ ] R.1 Create `compiler/tests/regression/behavior_fqn_collision.test.tml`:
      imports both `core::io::Write` and `core::fmt::Write`, defines a type
      that impls one of them, asserts correct method dispatch.
- [ ] R.2 Run the new test via `mcp__tml__test` — must pass.

## Verification

- [ ] V.1 Build: `scripts\build.bat`.
- [ ] V.2 Run `io_write_basic.test.tml` — must pass.
- [ ] V.3 Run full suite via `mcp__tml__test` with `structured=true`. Confirm
      no regressions vs baseline 1791/1874. Target: ≥1792 (one more passing).
- [ ] V.4 Check other tests flagged in I.4 — any that were previously failing
      due to short-name collisions should now pass. Bonus reduction.

## Documentation

- [ ] D.1 Update `.rulebook/tasks/phase0g_fix-214-compile-failures/tasks.md`:
      remove `io_write_basic` from remaining failures, update counts.
- [ ] D.2 Save learning to agent memory: "Behavior/trait/function maps must
      be keyed by FQN — short-name keying causes silent last-write-wins
      collisions when two modules define the same trait name."
- [ ] D.3 Commit with conventional message:
      `fix(types,codegen): key behaviors/traits/functions by FQN (phase0i)`
