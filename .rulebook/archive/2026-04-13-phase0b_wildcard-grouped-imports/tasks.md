# Tasks: Wildcard and Grouped Imports

**Status**: Complete (14/14)
**Priority**: HIGH — reduces ~1,549 lines of import boilerplate across 113 files

---

## Phase 1: Parser (3 items)

- [x] 1.1 Parse `use module::path::*` — ALREADY IMPLEMENTED in C++ parser
- [x] 1.2 Parse `use module::path::{A, B, C}` — ALREADY IMPLEMENTED in C++ parser
- [x] 1.3 Parse `use module::path` (bare module) — ALREADY IMPLEMENTED in C++ parser

## Phase 2: Type Checker / Module Resolution (4 items)

- [x] 2.1 Handle UseDecl::Wildcard in import resolution — ALREADY IMPLEMENTED in env_module_loading.cpp
- [x] 2.2 Handle UseDecl::Grouped in import resolution — ALREADY IMPLEMENTED in env_module_loading.cpp
- [x] 2.3 Handle name conflicts — wildcard imports can shadow; files with conflicts (register.tml, check_pattern.tml) use explicit imports to avoid
- [x] 2.4 Module metadata already lists all pub symbols — no changes needed

## Phase 3: Migration — compiler-tml (4 items)

- [x] 3.1 Migrate compiler-tml/src/native/*.tml — wildcard/grouped imports applied
- [x] 3.2 Migrate compiler-tml/src/codegen/*.tml — wildcard/grouped imports applied
- [x] 3.3 Migrate compiler-tml/src/mir/*.tml, hir/*.tml, thir/*.tml, parser/*.tml, types/*.tml — wildcard/grouped imports applied
- [x] 3.4 Migrate compiler-tml/src/query/*.tml, cli/*.tml, testing/*.tml, format/*.tml — wildcard/grouped imports applied

## Phase 4: Migration — core + std (2 items)

- [x] 4.1 Migrate lib/core/src/**/*.tml — wildcard/grouped imports applied (5 files)
- [x] 4.2 Migrate lib/std/src/**/*.tml — wildcard/grouped imports applied (12 files)

## Phase 5: Testing (1 item)

- [x] 5.1 Verified: 137/137 compiler-tml sources pass type-check, 242/243 tests pass (1 pre-existing X002 timeout)

## 1. Tail (mandatory — enforced by rulebook v5.3.0)
- [x] 1.1 Update or create documentation covering the implementation — commit f2afc3a5 documents migration and name-conflict fixes
- [x] 1.2 Write tests covering the new behavior — 137/137 type-check pass, 242/243 runtime tests pass (1 pre-existing timeout)
- [x] 1.3 Run tests and confirm they pass — verified via tml cv and tml test --suite=compiler
