# Proposal: Key Behaviors/Traits/Functions by FQN (Fix Name Collisions)

**Task**: phase0i_behavior-fqn-keying
**Status**: Planned
**Priority**: P1 — blocks `io_write_basic` and any program using multiple traits with the same short name
**Estimated effort**: 1–2 days (single specialist, touches ~10 sites)
**Risk**: High — core type checker + codegen + cache serialization format
**Depends on**: phase0g (RC1-RC6 completed)
**Blocks**: Any codebase that imports `core::io::Write` and `core::fmt::Write` together

## Problem

Two different behaviors share the short name `Write`:

- `core::io::Write` (lib/core/src/io.tml line 75) — methods: `write_bytes`, `flush`
- `core::fmt::Write` (lib/core/src/fmt/traits.tml line 158) — methods: `write_str`, `write_char` (default), `write_fmt` (default)

Internal compiler maps store them keyed by **short name**, causing last-write-wins
collisions:

- `TypeEnv::behaviors_` (compiler/include/types/env.hpp:760)
- `LLVMIRGen::trait_decls_` (compiler/src/codegen/llvm/core/generate.cpp:770 plus 7 other insertion sites)
- `LLVMIRGen::functions_` (same pattern — short-name keyed)

Representative failing test: `lib/core/tests/io/io_write_basic.test.tml`
defines `FakeWriter` and impls `core::io::Write`. At codegen,
`dyn.cpp:emit_vtables` calls `env_.lookup_behavior("Write")` and gets the
**wrong** behavior (`core::fmt::Write`). It then iterates its method list
(`write_str/write_char/write_fmt`) and emits default-method bodies for
`FakeWriter`. The default body for `write_char` contains
`this.write_str(char_to_string(c))`, and `this.write_str` resolves to
**`File::write_str`** (the inherent method in `lib/std/src/file/file.tml`
line 151) because `functions_` is also short-name keyed.

The inlined body emits `%struct.File` GEPs against `FakeWriter`'s `this`,
producing the LLVM error `base element of getelementptr must be sized`.

## Root Cause

All behavior/trait/function maps in the type checker and codegen layers are
keyed by unqualified short name. This is a latent soundness bug: any two
items with the same short name from different modules collide. The first file
to be processed "wins" for some maps, the last for others, giving
non-deterministic dispatch based on compilation order.

This is the single bug that blocks `io_write_basic` and likely contaminates
any program that pulls in both `core::io::Write` and `core::fmt::Write`
(which is most non-trivial programs, since `core::fmt` is in prelude-like
contexts).

## Proposed Fix

Migrate all short-name-keyed maps to **fully qualified name (FQN)** keying.
Provide a short-name → FQN resolver for call sites that currently pass short
names, but prefer FQN lookups everywhere possible.

### Affected Maps

1. `TypeEnv::behaviors_` — `std::unordered_map<std::string, BehaviorDef>`
2. `TypeEnv::functions_` — same pattern
3. `TypeEnv::structs_` — verify if this collides
4. `TypeEnv::enums_` — verify if this collides
5. `LLVMIRGen::trait_decls_` — 8 insertion sites
6. `LLVMIRGen::functions_` — same pattern
7. Meta cache binary format — may need a version bump

### Key Insertion Sites (from investigation)

- `compiler/src/types/env_definitions.cpp:36`
- `compiler/src/codegen/llvm/core/generate.cpp:770`
- `compiler/src/codegen/llvm/core/generate_first_pass.cpp:216`
- `compiler/src/codegen/llvm/core/generate_function_bodies.cpp:531`
- `compiler/src/codegen/llvm/core/generic_instantiate_impl.cpp:1186`
- `compiler/src/codegen/llvm/core/generic_instantiate_impl.cpp:1271`
- `compiler/src/codegen/llvm/core/generic_instantiate_impl.cpp:1521`
- `compiler/src/codegen/llvm/core/runtime_modules.cpp:846`
- `compiler/src/codegen/llvm/core/runtime_modules_tml.cpp:796`

### Key Lookup Sites

- `compiler/src/codegen/llvm/core/dyn.cpp:147` — `lookup_behavior("Write")`
  must resolve via impl's `trait_type` FQN.
- All call sites of `TypeEnv::lookup_behavior`, `lookup_function`, etc. —
  need to pass FQN when known.

## Success Criteria

1. `io_write_basic.test.tml` compiles and passes.
2. A new regression test `compiler/tests/regression/behavior_fqn_collision.test.tml`
   exercises two behaviors with the same short name from different modules
   and verifies correct dispatch.
3. `mcp__tml__test` with `structured=true` — no regressions vs baseline.
4. `phase0g` RC category for `io_write_basic` family drops to 0.
5. Any latent short-name-collision bugs in other trait families are also
   resolved (bonus: check `Display`/`Debug`/`Clone`/`Eq`/`Hash` for multi-
   module definitions).

## Out of Scope

- RC7 closure type preservation (owned by `phase0h`)
- Test runner legacy-vs-MIR switch (separate task)
- Rewriting the meta cache format for unrelated reasons (only the minimum
  needed to support FQN keying)
