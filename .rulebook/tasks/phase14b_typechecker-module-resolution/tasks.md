# Tasks: Type Checker — Module Resolution (Sub-phase 2b)

**Status**: Planned (0/20)
**Depends on**: phase14a (TypeEnv populated with declarations)
**Blocks**: phase14c (inference needs resolved imports)
**Duration**: 4–6 weeks
**Risk**: Medium — mechanical graph traversal, well-defined algorithm
**C++ reference**: ~7,211 LOC → ~4,700 TML

---

## Phase 1: Module Representation (4 items)

- [ ] 1.1 Create `compiler-tml/src/types/module.tml` — `Module` struct: name, path, declarations, imports, visibility map, metadata
- [ ] 1.2 Implement `ModulePath` type: `List[Str]` with display as `"std::collections::HashMap"`
- [ ] 1.3 Implement `Visibility` checking: pub, pub(crate), private — resolve against module tree
- [ ] 1.4 Implement `ModuleMetadata`: file path, last modified, fingerprint for incremental

## Phase 2: Import Resolution (5 items)

- [ ] 2.1 Create `compiler-tml/src/types/imports.tml` — `use` statement resolver
- [ ] 2.2 Implement single import: `use std::collections::HashMap` → resolve path, register alias in scope
- [ ] 2.3 Implement glob import: `use std::collections::*` → resolve all pub items, register each
- [ ] 2.4 Implement renamed import: `use std::collections::HashMap as Map` → register under alias
- [ ] 2.5 Implement re-export: `pub use inner::Type` → make visible to importers of this module

## Phase 3: Module Loading (5 items)

- [ ] 3.1 Create `compiler-tml/src/types/module_loader.tml` — load module from file path, parse, register
- [ ] 3.2 Implement module search: given `use std::json`, find `lib/std/src/json/mod.tml`
- [ ] 3.3 Implement circular import detection: track loading stack, error on cycle
- [ ] 3.4 Implement declaration loading: walk loaded module AST, register types/functions into TypeEnv
- [ ] 3.5 Implement transitive import resolution: if A imports B imports C, A sees B's pub re-exports

## Phase 4: Module Binary Cache (4 items)

- [ ] 4.1 Create `compiler-tml/src/types/module_binary.tml` — serialize resolved module to binary cache
- [ ] 4.2 Implement module fingerprinting: hash(source content) for cache invalidation
- [ ] 4.3 Implement cache read: load previously resolved module from binary, skip re-parsing
- [ ] 4.4 Implement cache invalidation: source changed or dependency changed → re-resolve

## Phase 5: Differential Testing (2 items)

- [ ] 5.1 Resolve imports for 20 stdlib modules → compare resolved TypeEnv with C++ output
- [ ] 5.2 Resolve imports for full test suite → verify zero diffs against C++ module resolution
