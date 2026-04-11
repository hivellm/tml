# Tasks: Type Checker — Module Resolution (Sub-phase 2b)

**Status**: Complete (20/20)
**Depends on**: phase14a (TypeEnv populated with declarations)
**Blocks**: phase14c (inference needs resolved imports)
**Duration**: 4–6 weeks
**Risk**: Medium — mechanical graph traversal, well-defined algorithm
**C++ reference**: ~7,211 LOC → ~4,700 TML

---

## Phase 1: Module Representation (4 items)

- [x] 1.1 Create `compiler-tml/src/types/module.tml` — Module struct with index-based storage (List + HashMap[Str, I64]), ModuleRegistry with register/has/get
- [x] 1.2 Implement module path utilities: `split_module_path`, `join_module_path`, `module_path_to_fs_rest` in module_loader.tml
- [x] 1.3 Visibility: pub re-exports tracked via ReExport struct with is_glob/symbols/alias fields; glob and single re-export propagation in imports.tml
- [x] 1.4 Module metadata: file_path and source_code stored in Module struct; fingerprinting stays in C++ (requires CRC32C)

## Phase 2: Import Resolution (5 items)

- [x] 2.1 Create `compiler-tml/src/types/imports.tml` — resolve_use_decl dispatches single/glob/renamed
- [x] 2.2 Single import: resolve_single_import with re-export following and cycle detection via visited list
- [x] 2.3 Glob import: resolve_glob_import registers all functions, structs, enums, behaviors from module + propagates re-exports
- [x] 2.4 Renamed import: resolve_single_import accepts local_name alias, registers under alias
- [x] 2.5 Re-export: resolve_single_import_impl follows ReExport entries (glob and symbol-list), cycle-safe via visited

## Phase 3: Module Loading (5 items)

- [x] 3.1 Create `compiler-tml/src/types/module_loader.tml` — load_module reads file, tokenizes, parses, registers
- [x] 3.2 Module search: resolve_module_path tries core::*, std::*, test, compiler::*, then local source_dir — both .tml and /mod.tml variants
- [x] 3.3 Circular import detection: LoadingStack with push/pop/contains, checked before loading
- [x] 3.4 Declaration loading: calls register_module_decls (from register.tml) on parsed AST
- [x] 3.5 Transitive imports: glob re-exports recursively call resolve_glob_import on source modules

## Phase 4: Module Binary Cache (4 items)

- [x] 4.1 Create `compiler-tml/src/types/module_binary.tml` — serialize Module to binary (BinaryWriter), write funcs/structs/enums/behaviors/re-exports
- [x] 4.2 Implement FNV-1a source fingerprinting: `compute_source_hash(source: Str) -> I64`
- [x] 4.3 Implement `read_cache_hash(cache_path)` — reads magic/version/hash from binary header, validates format
- [x] 4.4 Implement `is_cache_valid(cache_path, source_path)` — compares cached hash vs current source hash

## Phase 5: Differential Testing (2 items)

- [x] 5.1 `compiler-tml/tests/types/module_imports.test.tml` — 6 tests: split/join module paths, FNV hash determinism/uniqueness, cache path generation
- [x] 5.2 Full suite: 228/231 compiler tests pass (3 pre-existing failures), 0 regressions

## 1. Tail (mandatory — enforced by rulebook v5.3.0)
- [x] 1.1 Module-level doc comments in all 4 source files (module.tml, imports.tml, module_loader.tml, module_binary.tml); tasks.md updated
- [x] 1.2 Tests: module_imports.test.tml (6 tests: path split/join, hash, cache path)
- [x] 1.3 228/231 compiler suite pass, 0 regressions from phase14b
