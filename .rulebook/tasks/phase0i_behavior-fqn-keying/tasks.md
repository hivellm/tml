# Tasks: Key Behaviors/Traits/Functions by FQN

**Status**: In Progress (targeted codegen fix done — io_write_basic unblocked)
**Depends on**: phase0g (RC1-RC6 done)
**Blocks**: `io_write_basic` and any multi-trait-with-same-short-name program
**Duration**: 1–2 days
**Risk**: High — core type checker + codegen + cache format

---

## Investigation

- [x] I.1 Reproduced `io_write_basic` failure. Root cause: `trait_decls_["Write"]`
      returns `core::fmt::Write` (last-write-wins collision), so default methods
      `write_char` and `write_fmt` with `%struct.File` GEP are generated for FakeWriter.
- [x] I.2 Identified insertion/collision sites in `trait_decls_` (9 sites). 
      Root cause: short-name keying in generate_first_pass.cpp (unguarded), 
      generate_function_bodies.cpp, dyn.cpp, and several runtime_modules* files.
- [x] I.3 Identified lookup sites in generate_function_bodies.cpp and dyn.cpp.
- [ ] I.4 Survey other trait short-name collisions in the standard library.
- [ ] I.5 Check meta cache serialization format.

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
- [x] C.2 `compiler/src/codegen/llvm/core/generate_first_pass.cpp` — register
      by FQN (current_module_name_::name) + first-write-wins short name.
- [x] C.3 `compiler/src/codegen/llvm/core/generate_function_bodies.cpp` — 
      FQN-first lookup via resolve_imported_symbol + method overlap guard 
      to prevent wrong TraitDecl from generating defaults.
- [ ] C.4 `compiler/src/codegen/llvm/core/generic_instantiate_impl.cpp:1186` — same.
- [ ] C.5 `compiler/src/codegen/llvm/core/generic_instantiate_impl.cpp:1271` — same.
- [ ] C.6 `compiler/src/codegen/llvm/core/generic_instantiate_impl.cpp:1521` — same.
- [ ] C.7 `compiler/src/codegen/llvm/core/runtime_modules.cpp:846` — same.
- [ ] C.8 `compiler/src/codegen/llvm/core/runtime_modules_tml.cpp:796` — same.
- [x] C.9 `compiler/src/codegen/llvm/core/dyn.cpp` — applied resolve_trait_it
      lambda (FQN-first via resolve_imported_symbol) at both trait lookup sites.

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

- [x] V.1 Build: `scripts\build.bat` — clean build.
- [x] V.2 `io_write_basic.test.tml` passes (1/1).
- [x] V.3 core/io 3/3, core/fmt 46/46, core/iter 56/56 — no regressions.
- [ ] V.4 Full suite regression check (need broad run to confirm baseline).

## Documentation

- [ ] D.1 Update `.rulebook/tasks/phase0g_fix-214-compile-failures/tasks.md`:
      remove `io_write_basic` from remaining failures, update counts.
- [ ] D.2 Save learning to agent memory: "Behavior/trait/function maps must
      be keyed by FQN — short-name keying causes silent last-write-wins
      collisions when two modules define the same trait name."
- [ ] D.3 Commit with conventional message:
      `fix(types,codegen): key behaviors/traits/functions by FQN (phase0i)`

## 1. Tail (mandatory — enforced by rulebook v5.3.0)
- [ ] 1.1 Update or create documentation covering the implementation
- [ ] 1.2 Write tests covering the new behavior
- [ ] 1.3 Run tests and confirm they pass
