# Tasks: Wildcard and Grouped Imports

**Status**: Planned (0/14)
**Priority**: HIGH — reduces ~3,000 lines of import boilerplate across 135 files

---

## Phase 1: Parser (3 items)

- [ ] 1.1 Parse `use module::path::*` — wildcard import: extend parse_use_decl() to recognize `::*` at end of path; produce a new AST node UseDecl::Wildcard(module_path)
- [ ] 1.2 Parse `use module::path::{A, B, C}` — grouped import: extend parse_use_decl() to recognize `::{ ident, ident, ... }`; produce UseDecl::Grouped(module_path, names: List[Str])
- [ ] 1.3 Parse `use module::path` (bare module) — module-level import: when the path has no trailing `::Symbol`, treat as wildcard import of all pub symbols; produce UseDecl::Wildcard(module_path)

## Phase 2: Type Checker / Module Resolution (4 items)

- [ ] 2.1 Handle UseDecl::Wildcard in import resolution: call load_native_module(module_path), iterate all pub symbols (functions, structs, enums, behaviors, type aliases), register each in current scope
- [ ] 2.2 Handle UseDecl::Grouped in import resolution: call load_native_module(module_path), filter to only the listed names, error if a listed name is not found in the module
- [ ] 2.3 Handle name conflicts: if a wildcard import introduces a name that already exists in scope, produce a clear error with both sources ("ambiguous import: X defined in both module_a and module_b")
- [ ] 2.4 Update module metadata (.tml.meta) to list all pub symbols so wildcard import doesn't require parsing the full source

## Phase 3: Migration — compiler-tml (4 items)

- [ ] 3.1 Migrate compiler-tml/src/native/*.tml — replace single-symbol import chains with grouped/wildcard imports
- [ ] 3.2 Migrate compiler-tml/src/codegen/*.tml — replace single-symbol import chains
- [ ] 3.3 Migrate compiler-tml/src/mir/*.tml, hir/*.tml, thir/*.tml, parser/*.tml, types/*.tml — replace single-symbol import chains
- [ ] 3.4 Migrate compiler-tml/src/query/*.tml, cli/*.tml, testing/*.tml, format/*.tml — replace single-symbol import chains

## Phase 4: Migration — core + std (2 items)

- [ ] 4.1 Migrate lib/core/src/**/*.tml — replace single-symbol import chains where applicable
- [ ] 4.2 Migrate lib/std/src/**/*.tml — replace single-symbol import chains where applicable

## Phase 5: Testing (1 item)

- [ ] 5.1 Write tests: wildcard import (`use std::collections::list`), grouped import (`use std::collections::{List, HashMap}`), bare module import, name conflict detection, error on nonexistent name in group

## 1. Tail (mandatory — enforced by rulebook v5.3.0)
- [ ] 1.1 Update or create documentation covering the implementation
- [ ] 1.2 Write tests covering the new behavior
- [ ] 1.3 Run tests and confirm they pass
