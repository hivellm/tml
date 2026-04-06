# Proposal: Type Checker — Module Resolution (Sub-phase 2b)

## Problem

Before the type checker can check any expression, it must resolve all `use` imports and make
the declared types visible in the correct scopes. This is the second phase of checking in the
C++ compiler, spanning `env_module_loading.cpp` (875 LOC), `env_module_load.cpp` (508 LOC),
`env_module_load_decls.cpp` (1,253 LOC), `module.cpp` (549 LOC), `module_metadata.cpp` (740
LOC), `module_binary.cpp` (799 LOC), and `module_binary_read.cpp` (1,409 LOC) — ~7,211 LOC
total. Until this phase is ported to TML, the self-hosted type checker cannot process any
multi-file program.

## Solution

Port module resolution to four TML modules under `compiler-tml/src/types/`:

- `module.tml` — `Module` struct and `ModulePath`, `Visibility`, `ModuleMetadata` types
- `imports.tml` — `use` statement resolver covering single, glob, renamed, and re-export forms
- `module_loader.tml` — file-system search, load-parse-register pipeline, circular import guard
- `module_binary.tml` — binary serialization and cache read/write for incremental compilation

## Key Design Decisions

**Module path as List[Str].** `std::collections::HashMap` is represented as
`List["std", "collections", "HashMap"]`. Display joins with `"::"`. This is simpler than a
custom struct and integrates cleanly with `HashMap[ModulePath, Module]` keyed lookups.

**File-system-based module search.** Given `use std::json`, the loader searches `lib/std/src/json/mod.tml`
and `lib/std/src/json.tml` in order, matching the C++ `env_module_loading.cpp` search algorithm.
The search roots are passed in at loader construction time, making the loader testable without
touching the real filesystem.

**Circular import detection via loading stack.** The loader maintains a `List[ModulePath]` of
modules currently being loaded. Before loading a module, it checks whether the path is already
in this stack. If so, it emits a `E0201: circular import` diagnostic and returns an error. This
matches the C++ `loading_stack_` guard in `env_module_loading.cpp`.

**Binary cache for incremental compilation.** Each resolved module serializes to a compact
binary format (header + fingerprint + declaration table). On reload, the loader reads the
fingerprint, compares with the source file hash, and skips re-parsing if unchanged. The format
is identical to the C++ `module_binary.cpp` output so cached modules produced by C++ can be
read by the TML loader during the transition period.

**Re-export propagation.** `pub use inner::Type` adds the item to the current module's public
visibility map. When another module imports the current module, the resolver checks this map
and registers the re-exported item under the importer's scope. This matches the two-pass
approach in `env_module_load.cpp` (collect re-exports first, then resolve importers).

## Files Changed

| File | Purpose |
|------|---------|
| `compiler-tml/src/types/module.tml` | Module struct, ModulePath, Visibility, ModuleMetadata |
| `compiler-tml/src/types/imports.tml` | use statement resolution — single, glob, renamed, pub use |
| `compiler-tml/src/types/module_loader.tml` | File search, load-parse-register, circular import guard |
| `compiler-tml/src/types/module_binary.tml` | Binary cache serialize/deserialize, fingerprinting |

## Success Criteria

Differential testing: run import resolution on 20 stdlib modules and compare the resulting
`TypeEnv` (all registered names, their types, and visibility) against C++ output on the same
inputs. Zero differences on stdlib and the full test suite constitutes a passing port. The C++
resolver remains the reference until phase14d completes.

## Dependencies

- **Requires**: phase14a (TypeEnv must be populated with builtin types and declaration skeletons
  before imports can be resolved against it)
- **Blocks**: phase14c (Hindley-Milner inference walks import-resolved TypeEnv to infer
  expression types — unresolved imports produce spurious type errors)
- **Duration**: 4–6 weeks
- **Risk**: Medium — the algorithm is a well-defined graph traversal with no constraint solving;
  the main complexity is the binary cache format compatibility requirement during the C++/TML
  transition period
