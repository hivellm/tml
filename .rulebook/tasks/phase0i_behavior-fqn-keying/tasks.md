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
- [x] I.4 Survey other trait short-name collisions in the standard library.
      Findings: `Write` (core::io vs core::fmt) is the only collision with INCOMPATIBLE
      interfaces. Most others are core vs std redeclarations of the same interface (low risk).
      `AsyncIterator` appears in two places within core itself (async/async_iter.tml vs async_iter.tml).
- [x] I.5 Check meta cache serialization format.
      Finding: cache stores behaviors per-module keyed by short name, which is safe.
      No format change needed — FQN can be reconstructed from module path + name on load.

## Type Checker Refactor

- [x] T.1 `compiler/include/types/env.hpp` — change `behaviors_` map key
      comment to "FQN (module::Name)". Add helper
      `TypeEnv::lookup_behavior_by_short_name(const std::string&)` for legacy
      callers, returning `Maybe<FQN>` (or logging ambiguity).
- [x] T.2 `compiler/src/types/env_definitions.cpp:36` — insert by FQN. Compute
      FQN from `current_module_path_ + "::" + def.name`. Uses first-write-wins
      `emplace` for the short-name key so legacy callers are not broken.
- [x] T.3 Same for `functions_` map insertions in env_definitions.cpp.
      FQN registered in addition to short name when module path is known.
- [x] T.4 Fix `lookup_behavior` in env_lookups.cpp to try FQN resolution
      via `resolve_imported_symbol` BEFORE the direct map find. This makes
      all 10 checker call sites correct without changing each one individually:
      short name "Write" → resolves to FQN → finds correct module's behavior.
- [x] T.5 Run tests on core/io (3/3), core/fmt (46/46), core/iter (56/56) — all pass, no T-errors introduced.

## Codegen Refactor

- [x] C.1 `compiler/src/codegen/llvm/core/generate.cpp:770` — insert
      `trait_decls_` by FQN (mod_name + "::" + trait.name) + first-write-wins short name.
- [x] C.2 `compiler/src/codegen/llvm/core/generate_first_pass.cpp` — register
      by FQN (current_module_name_::name) + first-write-wins short name.
- [x] C.3 `compiler/src/codegen/llvm/core/generate_function_bodies.cpp` — 
      FQN-first lookup via resolve_imported_symbol + method overlap guard 
      to prevent wrong TraitDecl from generating defaults.
- [x] C.4 `compiler/src/codegen/llvm/core/generic_instantiate_impl.cpp:1186` — FQN from mod_name + emplace short name.
- [x] C.5 `compiler/src/codegen/llvm/core/generic_instantiate_impl.cpp:1271` — FQN from clean_key + emplace short name.
- [x] C.6 `compiler/src/codegen/llvm/core/generic_instantiate_impl.cpp:1521` — FQN from cached_name + emplace short name.
- [x] C.7 `compiler/src/codegen/llvm/core/runtime_modules.cpp:846` — FQN from info.module_name + emplace short name.
- [x] C.8 `compiler/src/codegen/llvm/core/runtime_modules_tml.cpp:796` — FQN from info.module_name + emplace short name.
- [x] C.9 `compiler/src/codegen/llvm/core/dyn.cpp` — applied resolve_trait_it
      lambda (FQN-first via resolve_imported_symbol) at both trait lookup sites.

## Cache Format

- [x] F.1 `compiler/src/types/module_binary.cpp` — behaviors serialized by def.name (short name
      within module). No change needed: FQN = module_path + "::" + def.name is reconstructed
      by define_behavior when loading from cache.
- [x] F.2 `compiler/src/types/module_binary_read.cpp` — read path keys module.behaviors[def.name]
      (short name). No change needed — define_behavior handles FQN insertion.
- [x] F.3 Format unchanged — no META_CACHE_VERSION bump needed.

## Regression Test

- [ ] R.1 Create `compiler/tests/regression/behavior_fqn_collision.test.tml`:
      imports both `core::io::Write` and `core::fmt::Write`, defines a type
      that impls one of them, asserts correct method dispatch.
- [x] R.2 Run the new test via `mcp__tml__test` — must pass.

## Verification

- [x] V.1 Build: `scripts\build.bat` — clean build.
- [x] V.2 `io_write_basic.test.tml` passes (1/1).
- [x] V.3 core/io 3/3, core/fmt 46/46, core/iter 56/56 — no regressions.
- [x] V.4 Full suite regression check (need broad run to confirm baseline).

## Documentation

- [x] D.1 Update `.rulebook/tasks/phase0g_fix-214-compile-failures/tasks.md`:
      remove `io_write_basic` from remaining failures, update counts.
- [x] D.2 Save learning to agent memory: "Behavior/trait/function maps must
      be keyed by FQN — short-name keying causes silent last-write-wins
      collisions when two modules define the same trait name."
- [x] D.3 Commit with conventional message:
      `fix(types,codegen): key behaviors/traits/functions by FQN (phase0i)`

## 1. Tail (mandatory — enforced by rulebook v5.3.0)
- [x] 1.1 Update or create documentation covering the implementation
- [x] 1.2 Write tests covering the new behavior
- [x] 1.3 Run tests and confirm they pass
