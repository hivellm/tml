# Section 2 — Module Resolution

**Task**: phase12c_typechecker-invariants  
**Section**: 2 of 6  
**Sources**: `env_module_loading.cpp` (875 LOC), `env_module_load.cpp` (508 LOC), `env_module_load_decls.cpp` (1253 LOC)  
**Supporting**: `compiler/include/types/env.hpp` (807 LOC), `compiler/include/types/module.hpp` (282 LOC), `compiler/include/types/module_binary.hpp` (165 LOC), `compiler/src/types/checker/core.cpp` (Phase 0 analysis)

---

## Table of Contents

1. [Overview and Component Roles](#1-overview-and-component-roles)
2. [Data Structures](#2-data-structures)
3. [Module Load Sequence](#3-module-load-sequence)
4. [Filesystem Resolution Algorithm](#4-filesystem-resolution-algorithm)
5. [The Three-Layer Cache](#5-the-three-layer-cache)
6. [Import Resolution During Module Extraction](#6-import-resolution-during-module-extraction)
7. [pub use Re-Export Semantics](#7-pub-use-re-export-semantics)
8. [Circular Import Handling](#8-circular-import-handling)
9. [Visibility Rules and Enforcement Points](#9-visibility-rules-and-enforcement-points)
10. [TypeChecker Phase 0 — process_use_decl](#10-typechecker-phase-0--process_use_decl)
11. [TypeEnv State After Module Loading](#11-typeenv-state-after-module-loading)
12. [Invariant Catalogue](#12-invariant-catalogue)
13. [Surprising Findings](#13-surprising-findings)

---

## 1. Overview and Component Roles

Module resolution in TML involves three distinct files with clearly separated responsibilities:

| File | Responsibility |
|------|---------------|
| `env_module_loading.cpp` | Filesystem path resolution; cache layer decisions; dispatching to `load_module_from_file` |
| `env_module_load.cpp` | Parsing source files; directory module expansion; invoking `extract_module_declarations` |
| `env_module_load_decls.cpp` | Walking AST nodes; filling a `Module` struct with typed signatures; two-pass processing |

The entry point for all callers is `TypeEnv::load_native_module(module_path, silent)` in `env_module_loading.cpp:173`. Everything flows from there.

```
load_native_module(path)          [env_module_loading.cpp:173]
  ├── module_registry_->has_module()       # early-out if already loaded
  ├── GlobalModuleCache::get()            # in-process memory cache
  │     └── validate hash vs .tml.meta
  ├── load_module_from_cache()            # .tml.meta binary cache
  └── load_module_from_file(path, fs_path) [env_module_load.cpp:298]
        ├── loading_modules_.count()      # circular-dep guard
        ├── parse all .tml files in dir OR single file
        └── extract_module_declarations() [env_module_load_decls.cpp:352]
              ├── Pass 1: walk decls → fill Module struct
              └── Pass 2: register default behavior methods
```

---

## 2. Data Structures

### 2.1 Module struct (`module.hpp:66`)

Every loaded module is represented as a `Module` value. The struct contains:

| Field | Type | Purpose |
|-------|------|---------|
| `name` | `string` | Canonical module path (e.g., `"std::sync"`) |
| `file_path` | `string` | Filesystem path to source (for codegen re-parsing) |
| `functions` | `unordered_map<string, FuncSig>` | Public + lowlevel + generic impl methods |
| `structs` | `unordered_map<string, StructDef>` | Public structs only |
| `internal_structs` | `unordered_map<string, StructDef>` | Private structs (visible within module) |
| `enums` | `unordered_map<string, EnumDef>` | Public enums only |
| `internal_enums` | `unordered_map<string, EnumDef>` | Private enums |
| `behaviors` | `unordered_map<string, BehaviorDef>` | Public behaviors (traits) |
| `type_aliases` | `unordered_map<string, TypePtr>` | Public type aliases |
| `type_alias_generics` | `unordered_map<string, vector<string>>` | Generic params for aliases |
| `submodules` | `unordered_map<string, string>` | `pub mod` name → submodule path |
| `constants` | `unordered_map<string, ConstantInfo>` | Scalar and tuple constants |
| `classes` | `unordered_map<string, ClassDef>` | OOP class definitions |
| `interfaces` | `unordered_map<string, InterfaceDef>` | OOP interface definitions |
| `re_exports` | `vector<ReExport>` | Records of `pub use` declarations |
| `private_imports` | `vector<string>` | Module paths from private `use` declarations |
| `behavior_impls` | `unordered_map<string, vector<string>>` | type → behaviors for Drop detection |
| `source_code` | `string` | Combined preprocessed source (for modules with TML bodies) |
| `has_pure_tml_functions` | `bool` | True when module contains non-extern function bodies |

### 2.2 ModuleRegistry (`module.hpp:174`)

A per-`TypeEnv` map from module path to `Module`. Created once per compilation unit. The `TypeEnv` holds one `shared_ptr<ModuleRegistry>` (`env.hpp:782`).

### 2.3 GlobalModuleCache (`module.hpp:126`)

A process-scoped singleton holding `Module` values for library modules (`core::*`, `std::*`, `test`). Thread-safe via `shared_mutex`. Validated against source hash before use.

### 2.4 TypeEnv private fields relevant to module loading (`env.hpp:757–790`)

```cpp
std::shared_ptr<ModuleRegistry> module_registry_;    // per-TypeEnv registry
std::string current_module_path_;                    // module being compiled
std::string source_directory_;                       // for local module resolution
std::unordered_map<string, ImportedSymbol> imported_symbols_;
std::unordered_map<string, set<string>> import_conflicts_;
bool abort_on_module_error_ = true;                  // controls fatal vs silent errors
std::unordered_set<string> loading_modules_;         // cycle detection
```

---

## 3. Module Load Sequence

### 3.1 The full sequence for a cache-cold load

When a `use` statement triggers `load_native_module("std::sync", false)` with an empty caches:

1. **Duplicate check** — `module_registry_->has_module(path)` → `false`, continue.
   (`env_module_loading.cpp:179`)

2. **GlobalModuleCache check** — `GlobalModuleCache::should_cache(path)` determines whether to try the in-memory process cache. Only `core::*`, `std::*`, and `test` return true. User and local modules skip this entire block.
   (`env_module_loading.cpp:184`)

3. **Hash validation** (if GlobalModuleCache hit) — The cached `Module` is considered valid only if the source file's CRC32C hash matches the hash stored in the corresponding `.tml.meta` file. A mismatch invalidates the in-memory entry.
   (`env_module_loading.cpp:215–237`)

4. **Binary meta cache check** — `load_module_from_cache(module_path, source_path)` attempts to read a `.tml.meta` file from `build/debug/cache/meta/<module-path>.tml.meta`. The reader verifies the magic number, version, and source hash. On hit, the serialized `Module` is deserialized and placed into both `GlobalModuleCache` and the local `ModuleRegistry`.
   (`env_module_loading.cpp:310–390`)

5. **Path resolution** — `load_native_module` resolves the module path to a filesystem path. For library modules this calls `resolve_lib_module_path(lib_subdir, "src", fs_path)` which tries `<lib_root>/<lib_subdir>/src/<fs_path>.tml` and `<lib_root>/<lib_subdir>/src/<fs_path>/mod.tml` in that order.
   (`env_module_loading.cpp:146–171`)

6. **Source loading** — `load_module_from_file(module_path, fs_path)` is called.
   (`env_module_load.cpp:298`)

7. **Circular-dependency guard** — `loading_modules_.count(module_path)` prevents re-entrant loading of the same module. Returns `true` immediately if the module is already being loaded.
   (`env_module_load.cpp:309`)

8. **Parsing** — For a directory module (when `fs_path.stem() == "mod"`), all `.tml` files in the directory are parsed. For a single-file module, only that file is parsed.
   (`env_module_load.cpp:340–433`)

9. **Declaration extraction** — `extract_module_declarations(module_path, file_path, all_parsed, mod)` walks the AST and fills `mod` in two passes.
   (`env_module_load.cpp:464`, `env_module_load_decls.cpp:352`)

10. **Module registration** — `module_registry_->register_module(module_path, std::move(mod))`.
    (`env_module_load.cpp:475`)

11. **Transitive re-export loading** — For each `ReExport::source_path` recorded in the module, `load_native_module(source_path, silent=true)` is called recursively.
    (`env_module_load.cpp:482–484`)

12. **Transitive private import loading** — For each private import path, `load_native_module` is attempted. If it fails, the last `::` segment is stripped and the parent path is tried, since the path may be a symbol path (`core::option::Maybe`) rather than a module path (`core::option`).
    (`env_module_load.cpp:493–502`)

### 3.2 Declaration extraction within extract_module_declarations

`extract_module_declarations` performs two passes over `all_parsed`:

**Pass 1** (`env_module_load_decls.cpp:359–1071`) — Walks every declaration in every parsed file:

- `FuncDecl`: registered if `vis == Public`, or `extern_abi.has_value()`, or `is_unsafe`. All three conditions bypass the visibility filter to ensure codegen can emit `declare` statements. Qualified name is the bare function name.

- `StructDecl`: public structs go to `mod.structs`, private to `mod.internal_structs`. Both are populated.

- `EnumDecl`: same split as structs. Public → `mod.enums`, private → `mod.internal_enums`.

- `ImplDecl`: extracts methods under the qualified key `TypeName::method`. Additionally, for specialized impls (e.g., `impl[T] Pin[Heap[T]]`), a discriminated key `TypeName[Heap]::method` is also registered. The choice between private method inclusion and exclusion depends on `is_generic_impl || is_behavior_impl`: if either is true, all methods are included regardless of visibility.

- `ConstDecl` (module-level): only if a compile-time scalar or tuple can be extracted.

- `InterfaceDecl`, `ClassDecl`: registered only for `vis == Public`.

- `TypeAliasDecl`: registered only for `vis == Public`.

- `ModDecl`: `pub mod foo` inserts `foo → "parent::foo"` into `mod.submodules`.

- `UseDecl`: If `vis == Public`, creates a `ReExport` entry and appends to `mod.re_exports`. If private, appends the module path to `mod.private_imports`. For both visibility levels, the referenced module is eagerly loaded if it is from `mod.tml` (`is_from_mod_file == true`) or is an intra-module reference. Sibling `.tml` files (not `mod.tml`) do NOT eagerly load their external dependencies.

- `TraitDecl`: registered only for `vis == Public`.

**Pass 2** (`env_module_load_decls.cpp:1073–1158`) — For each `ImplDecl` that has a `trait_type` (i.e., is a behavior impl), looks up the `BehaviorDef` (first in `mod.behaviors`, then in all registered modules) and registers default behavior methods not overridden by the impl. This ensures types that rely on defaults show the correct method set.

After both passes, a third sweep determines `mod.has_pure_tml_functions` and collects `mod.source_code`.

---

## 4. Filesystem Resolution Algorithm

### 4.1 Module path to filesystem path translation

The `::` separator in module paths maps to `/` in filesystem paths. The translation is performed before any filesystem probe:

```
"core::str"          → "str"           → lib/core/src/str.tml or lib/core/src/str/mod.tml
"std::collections"   → "collections"   → lib/std/src/collections.tml or .../mod.tml
"std::http::server"  → "http/server"   → lib/std/src/http/server.tml or .../mod.tml
```

(`env_module_loading.cpp:626–640` for core, `694–704` for std, `460–475` for test)

### 4.2 Resolution priority order (library modules)

For `core::*`, `std::*`, `test::*`, `backtrace::*`:

1. **Path resolution cache** — `s_resolved_paths[module_path]` (process-scoped, never invalidated within a process). Hit → use cached path directly.
   (`env_module_loading.cpp:116–123`)

2. **Known-not-found cache** — `s_not_found_paths` prevents repeated filesystem probes for paths already known to not exist.
   (`env_module_loading.cpp:126–129`)

3. **Cached library root (2 probes)** — `resolve_lib_module_path(lib_subdir, "src", fs_path)` tries exactly two paths: `<lib_root>/<lib_subdir>/src/<fs_path>.tml` and `<lib_root>/<lib_subdir>/src/<fs_path>/mod.tml`.
   (`env_module_loading.cpp:146–171`)

4. **Full fallback search (10–12 probes)** — A vector of relative and absolute paths is tried in order. Paths are relative to CWD, two levels up (for `build/debug/bin`), and hardcoded to `F:/Node/hivellm/tml/lib/`.
   (`env_module_loading.cpp:644–658` for core example)

### 4.3 Local module resolution

Modules with paths that do not begin with `core::`, `std::`, `test::`, or `backtrace::` are resolved as local or external packages. The resolution sequence is:

1. **External package detection** — If `lib_root/<package_name>/src/` exists, the path is treated as an external package and resolved within that tree. (`env_module_loading.cpp:751–824`)

2. **Source-directory-relative** — `source_directory_ / (fs_path + ".tml")` and `source_directory_ / fs_path / "mod.tml"`. (`env_module_loading.cpp:840–855`)

3. **CWD-relative** — Same patterns relative to `std::filesystem::current_path()`. (`env_module_loading.cpp:858–869`)

### 4.4 Windows case-sensitivity workaround

On Windows, `std::filesystem::exists()` is case-insensitive (NTFS). This means `lib/std/src/List.tml` would match when resolving `std::collections::list` (expecting `list.tml`). The helper `exists_case_sensitive` iterates the parent directory and verifies the filename byte-for-byte against the expected name.
(`env_module_loading.cpp:55–67`)

This has a correctness consequence: `std::collections::List` resolves to `List.tml` (a symbol, not a file), which correctly fails, causing `std::collections` to be loaded as the parent directory module. Without the case check, `List.tml` would be matched and `std::collections`'s sibling files (including `behaviors.tml` defining `ListIter`) would be skipped.

---

## 5. The Three-Layer Cache

### 5.1 Layer 1: ModuleRegistry (per-TypeEnv, compile-unit scoped)

The `ModuleRegistry` held by a `TypeEnv` is checked first. It is populated exclusively by `register_module`. It never evicts entries. It is destroyed when the `TypeEnv` goes out of scope at the end of the compilation unit.

### 5.2 Layer 2: GlobalModuleCache (process-scoped singleton)

The `GlobalModuleCache` stores `Module` values for library modules across the lifetime of the compiler process. It is the primary speedup for multi-file builds and test suites: once a library module is loaded by one test file's `TypeEnv`, subsequent test files find it in the process cache without filesystem access.

The cache is read with `shared_lock` and written with `unique_lock` (`module.hpp:165–166`).

Hash validation occurs before a cached entry is used. The `Module.source_code` stored in the cache contains preprocessed source, not raw source. If the source hash changes between builds (meta file regenerated), the in-process cache is bypassed.
(`env_module_loading.cpp:193–238`)

### 5.3 Layer 3: Binary meta cache (.tml.meta files)

Binary `.tml.meta` files are written once per library module compilation and read on subsequent runs. The format is:

```
Header (24 bytes):
  [0..4)    magic: 0x544D4D54 ("TMMT")
  [4..6)    version_major: 8
  [6..8)    version_minor: 0
  [8..16)   source_hash: u64 (CRC32C of source files)
  [16..24)  timestamp: u64
```

(`module_binary.hpp:9–27`)

The current format version is **8** (incremented when Module struct layout changes). Older cache files from a version mismatch are silently rejected.
(`module_binary.hpp:55`)

Cache path computation: `"core::mem"` → `<build_root>/cache/meta/core/mem.tml.meta`. The build root is discovered by walking up from CWD looking for `build/debug/` or `build/release/` structures.
(`module_binary.hpp:74–79`)

### 5.4 Cache relationship to behavior_impls

When a module is loaded from either the binary cache or GlobalModuleCache, its `behavior_impls` map is re-registered into the current `TypeEnv` via `register_impl`. This is essential for `is_trivially_destructible()` to correctly identify `Drop` implementations on imported library types.

If the cache entry predates the addition of `behavior_impls` (old format), a fallback scans `module.functions` for names ending in `::drop` and infers `Drop` implementations from them.
(`env_module_loading.cpp:256–272`, `359–367`)

---

## 6. Import Resolution During Module Extraction

### 6.1 When imports are resolved relative to declarations

Within `extract_module_declarations`, `UseDecl` nodes are processed in Pass 1 along with all other declarations. There is no separate import-first phase within this function. This means a `use` statement appearing after a `struct` declaration in source order is processed after the struct. However, because `extract_module_declarations` only populates the `Module` struct (not a `TypeEnv`), the ordering within a single module file has no effect on symbol availability.

The module-level type checker (`checker/core.cpp:193–261`) does process `UseDecl` before all other declarations (Phase 0 precedes Phase 1).

### 6.2 Eager loading of dependencies

During Pass 1 of `extract_module_declarations`, `UseDecl` nodes trigger `load_native_module` only under specific conditions:

- **Always loaded** if the declaration is from `mod.tml` (`is_from_mod_file == true`).
- **Always loaded** if the import path is intra-module (`use_path.find(module_path + "::") == 0 || use_path == module_path`).
- **Not loaded** if the declaration is from a sibling `.tml` file and the import is external.

This optimization avoids loading `std::zlib` just because `encoding.tml` (a sibling file in `std::http`) imports it, since `encoding.tml`'s types are only needed when the file is explicitly compiled.
(`env_module_load_decls.cpp:926–955`)

### 6.3 Two-step module resolution for symbol paths

When a `use` path like `core::option::Maybe` is processed, the code first tries to load `core::option::Maybe` as a module path. When this fails (no file `option/Maybe.tml`), the last `::` segment is stripped and `core::option` is loaded as the base module.
(`env_module_load.cpp:493–501`, `env_module_load_decls.cpp:939–953`)

### 6.4 Glob imports (use_decl.is_glob)

In `extract_module_declarations`, glob imports (`use foo::*`) create a `ReExport` with `is_glob = true` and an empty `symbols` vector. No special treatment is applied during extraction; the expansion happens later when `TypeEnv::import_all_from` is called.

In the type checker's `process_use_decl`, `env_.import_all_from(module_path)` is called after loading the module, which copies every public symbol from the module into `imported_symbols_`.

---

## 7. pub use Re-Export Semantics

### 7.1 ReExport struct layout

```cpp
struct ReExport {
    string source_path;          // Resolved module path for the source
    bool is_glob;                // True for pub use foo::*
    vector<string> symbols;      // Empty for glob; specific names otherwise
    optional<string> alias;      // For pub use foo as bar
};
```
(`module.hpp:58–63`)

### 7.2 Single-symbol re-export extraction

When `pub use foo::bar::SymbolName` is encountered (no `{}` group, no `*`), the code splits off the last `::` segment:

```cpp
re_source_path = use_path without last segment;  // "foo::bar"
re_symbols = {symbol_name};                      // {"SymbolName"}
```

(`env_module_load_decls.cpp:967–974`)

This means the `ReExport.source_path` is the module containing the symbol, not the full import path.

### 7.3 Re-export chain loading (transitive)

After a module is registered, `load_module_from_file` loads every `re_export.source_path` recursively:

```cpp
for (const auto& source_path : re_export_sources) {
    load_native_module(source_path, silent=true);
}
```
(`env_module_load.cpp:482–484`)

The same pattern occurs for binary cache hits (`env_module_loading.cpp:276–280`) and GlobalModuleCache hits (`env_module_loading.cpp:278–280`). All three load paths are symmetric with respect to re-export chain loading.

### 7.4 Re-export resolution in ModuleRegistry

`ModuleRegistry::lookup_function_impl`, `lookup_struct_impl`, `lookup_enum_impl`, `lookup_behavior_impl`, and `lookup_constant_impl` all follow `re_exports` chains. Each uses an `unordered_set<string>& visited` parameter to prevent infinite recursion in diamond re-export patterns.
(`module.hpp:255–277`)

This means a module consumer can access `foo::Bar` even though `Bar` was originally defined in `foo::internal::Bar`, as long as `foo` has `pub use foo::internal::Bar`. The lookup traverses the re-export chain to find the original definition.

### 7.5 Re-export via process_use_decl (type checker level)

In `process_use_decl` (`checker/core.cpp:586–607`), when a symbol group import (`use mod::{A, B}`) is processed, the code additionally loads all re-export source modules that might contain any of the requested symbols. This is a proactive load to ensure that cross-module re-exported enums and constants are available when their types are resolved.

---

## 8. Circular Import Handling

### 8.1 The loading_modules_ guard

`loading_modules_` is an `unordered_set<string>` stored on `TypeEnv` (`env.hpp:789–790`). Before loading a module from file, `load_module_from_file` inserts the module path into this set. If a recursive `load_native_module` call encounters a path already in the set, it returns `true` immediately without loading.

```cpp
if (loading_modules_.count(module_path) > 0) {
    TML_DEBUG_LN("[MODULE] Skipping circular dependency: " << module_path);
    return true; // Return true to allow compilation to proceed
}
loading_modules_.insert(module_path);
```
(`env_module_load.cpp:309–315`)

The RAII guard `LoadingGuard` removes the path from the set on any return path before registration. After successful registration, `loading_guard.mark_completed()` prevents the guard from removing the entry, and the entry is then manually erased.
(`env_module_load.cpp:318–331`, `477–479`)

### 8.2 Behaviour during a circular detection event

When a circular dependency is detected, the module is not yet registered. The `load_native_module` call returns `true` to the caller, signaling success. This is intentional: it prevents the compilation from aborting, since the circular dependency will be resolved once the outer load completes.

This means that code which depends on a circularly-referenced module must not require that module's types to be available during the loading of the outer module. The type checker body-checking phase (Phase 3) runs after all imports complete (Phase 0), so this is safe in the two-pass architecture.

### 8.3 No cycle detection at the binary cache level

The binary meta cache and GlobalModuleCache paths do not use `loading_modules_`. Since these paths load pre-serialized `Module` structs (no recursive parsing), they cannot trigger circular dependencies. Only `load_module_from_file` can trigger re-entrant loading.

---

## 9. Visibility Rules and Enforcement Points

### 9.1 Visibility values

The parser defines:

```cpp
enum class Visibility { Public, Private };       // for module-level declarations
enum class MemberVisibility { Private, Protected, Public };  // for class members
```

In all module extraction code, the only test for public visibility is `vis == parser::Visibility::Public`.

### 9.2 Per-declaration-kind enforcement

The table below maps declaration kinds to the visibility enforcement applied in `extract_module_declarations`:

| Declaration | Visibility check | Private result |
|-------------|-----------------|----------------|
| `FuncDecl` | `vis == Public` OR `extern_abi` OR `is_unsafe` | Not registered |
| `StructDecl` | `vis == Public` | Goes to `internal_structs` |
| `EnumDecl` | `vis == Public` | Goes to `internal_enums` |
| `ImplDecl` method | `vis == Public` OR `is_behavior_impl` OR `is_generic_impl` | Not registered |
| `ImplDecl` constant | `vis == Public` | Not registered |
| `InterfaceDecl` | `vis == Public` | Not registered |
| `ClassDecl` | `vis == Public` | Not registered |
| `TypeAliasDecl` | `vis == Public` | Not registered |
| `ModDecl` | `vis == Public` | Not registered |
| `UseDecl` | `vis == Public` → re-export; private → private_imports | |
| `TraitDecl` (behavior) | `vis == Public` | Not registered |

(`env_module_load_decls.cpp:363–1069` — per-branch analysis)

### 9.3 Asymmetry: internal structs and enums are always populated

Public structs and enums go to `mod.structs` / `mod.enums`. Private structs and enums go to `mod.internal_structs` / `mod.internal_enums`. Both maps are always populated regardless of visibility.

The `internal_structs` and `internal_enums` maps are present so that impl methods within the same module can use private types without needing them in the public API surface.

### 9.4 pub(crate) is not implemented

The parser recognizes `pub(crate)` as a syntax extension, but `extract_module_declarations` only tests for `parser::Visibility::Public`. There is no separate `Crate` variant in the enum. A `pub(crate)` item is either parsed as `Public` or rejected at the parser level. The type environment has no representation of crate-scoped visibility distinct from fully public.

### 9.5 Enforcement at import time

When `TypeEnv::import_symbol` and `TypeEnv::import_all_from` are called during Phase 0 of the type checker, they consult the `Module` struct populated by `extract_module_declarations`. Since private types are absent from `Module::structs`, `Module::enums`, etc., they cannot be imported from outside the module. The enforcement is structural: private items are simply not present in the public symbol tables.

The exception is `internal_structs` / `internal_enums`: these are stored in the module but are not consulted by `lookup_struct` / `lookup_enum` externally (lookup follows `ModuleRegistry::lookup_struct_impl` which queries `module.structs`, not `module.internal_structs`).

---

## 10. TypeChecker Phase 0 — process_use_decl

### 10.1 Phase ordering in check_module

`TypeChecker::check_module` performs exactly four passes over the declarations:
(`checker/core.cpp:193–261`)

```
Pass 0: UseDecl → process_use_decl()
Pass 1: StructDecl, EnumDecl, TraitDecl, TypeAliasDecl, InterfaceDecl, ClassDecl
        → register_*_decl()
Pass 2: FuncDecl, ImplDecl, ConstDecl, ClassDecl, InterfaceDecl
        → check_func_decl(), check_impl_decl(), check_const_decl()
Pass 3: FuncDecl, ImplDecl, ClassDecl body checking
        → check_func_body(), check_impl_body(), check_class_body()
```

Pass 0 is always complete before Pass 1 begins. This means all modules referenced by `use` declarations are loaded (and their `TypeEnv` symbols imported) before any type registration for the current module occurs.

### 10.2 process_use_decl resolution algorithm

`process_use_decl` in `checker/core.cpp:472` handles three import forms:

**Glob imports** (`use mod::*`):
1. `load_native_module(module_path, silent=true)`
2. If not found, try `try_resolve_via_parent_reexports(module_path)` — walks parent module's `re_exports` for a matching source path suffix.
3. If found, `env_.import_all_from(module_path)`.
4. If still not found, emit error `T027`.

**Symbol group imports** (`use mod::{A, B, C}`):
1. `load_native_module(module_path, silent=true)`
2. If not found, try parent re-export resolution.
3. If found, load all re-export source modules for the requested symbols.
4. Call `env_.import_symbol(module_path, symbol, nullopt)` for each symbol.

**Single path imports** (`use foo::bar::Baz`):
1. Try `load_native_module(full_path)` — treats the full path as a module.
2. If not found, strip last segment and try `load_native_module(base_path)` — treats last segment as a symbol name.
3. If base not found, try grandparent re-export resolution (2-level re-export following).
4. Import the symbol (last segment) from the resolved module.

### 10.3 Re-export resolution in process_use_decl

`try_resolve_via_parent_reexports` in `checker/core.cpp:490–541` implements a fallback for paths like `std::http::chunked` that do not correspond to filesystem files but are re-exported from `std::http`. It:

1. Loads the parent module (`std::http`).
2. Scans its `re_exports` for a `source_path` ending with `::chunked`.
3. Loads and returns the actual source module.

A two-level variant (searching the grandparent) handles cases where even the parent path doesn't exist on disk but is reachable via grandparent re-exports.
(`checker/core.cpp:637–680`)

---

## 11. TypeEnv State After Module Loading Completes

After `check_module` returns (the full 4-pass sequence is complete), the following state is guaranteed:

### 11.1 Module registry

Every module path referenced by a `use` statement in the source being compiled, and all transitive re-export source paths from those modules, are registered in `module_registry_`. The registry is complete — no lazy loading occurs during type-checking phases after Phase 0.

### 11.2 TypeEnv type tables

After Pass 1 (`register_*_decl`):
- `structs_`, `enums_`, `behaviors_`, `type_aliases_` contain all declarations from the currently-compiled module.
- They do NOT contain declarations from imported modules (those remain in the `ModuleRegistry`).

After Pass 2 (`check_impl_decl`, `check_func_decl`):
- `functions_` contains signatures for all functions and methods declared in the current module.
- `behavior_impls_` contains all `(type, behavior)` pairs registered via `register_impl` during extraction of the current module AND all imported modules.

### 11.3 imported_symbols_

`imported_symbols_` maps local symbol names to `ImportedSymbol` (original name + module path). It is populated by `import_symbol` and `import_all_from` during Phase 0. After Phase 0, the set is complete.

The lookup chain for resolving a name during body type checking is:
1. Current lexical scope chain (local variables).
2. `imported_symbols_` (module-level imports).
3. `structs_` / `enums_` / `behaviors_` / `functions_` (current module's own declarations).
4. `module_registry_` (cross-module lookup for qualified paths).

### 11.4 substitutions_ (type inference)

`substitutions_` is populated incrementally during Pass 3 (body checking) via `unify()`. It contains no entries before Pass 3 begins. The `resolve()` method follows the substitution chain to ground a type variable.

### 11.5 loading_modules_

After a successful `check_module` call, `loading_modules_` should be empty. All modules loaded during Phase 0 were inserted and then removed by the RAII guard. A non-empty `loading_modules_` at the end of compilation indicates a circular import that was silently swallowed.

---

## 12. Invariant Catalogue

This section enumerates all invariants discovered during the audit, each traceable to a specific file and line range.

---

**INV-01**: A module path is registered in `ModuleRegistry` at most once per `TypeEnv`. The check `module_registry_->has_module(path)` is performed before any loading work begins and returns `true` immediately on a hit.

*Source*: `env_module_loading.cpp:179`, `env_module_load.cpp:304`

---

**INV-02**: A module path appears in `loading_modules_` for the duration of its loading and is removed (by RAII guard or manual erase) before the caller can observe its absence. A path present in `loading_modules_` is not present in `module_registry_` and vice versa.

*Source*: `env_module_load.cpp:309–331`, `477–479`

---

**INV-03**: `GlobalModuleCache::should_cache(path)` returns `true` if and only if the path begins with `"core::"`, `"std::"`, or equals `"test"` or begins with `"test::"`. User-defined and local modules are never placed in the GlobalModuleCache.

*Source*: `env_module_loading.cpp:184` (usage pattern), `module.hpp:155` (static method declaration)

---

**INV-04**: When a module is loaded from GlobalModuleCache or binary meta cache, its `behavior_impls` map is re-registered into the current `TypeEnv` before the module is registered into `ModuleRegistry`. This means `TypeEnv::type_implements` returns correct results for library types as soon as the module is accessed, not after a separate registration step.

*Source*: `env_module_loading.cpp:256–260` (GlobalModuleCache path), `352–356` (binary cache path)

---

**INV-05**: A binary `.tml.meta` cache file is accepted only when: (a) the magic number matches `0x544D4D54`, (b) the version major equals `8`, and (c) the CRC32C hash of the current source files equals the hash stored in the header. Any mismatch causes the cache file to be ignored and the module to be loaded from source.

*Source*: `module_binary.hpp:46–56`, `env_module_loading.cpp:219–233`

---

**INV-06**: The filesystem path resolution cache (`s_resolved_paths`, `s_not_found_paths`) is process-scoped and never invalidated during the lifetime of the compiler process. A path cached as "not found" will never be retried even if the file is created mid-process.

*Source*: `env_module_loading.cpp:74–141` (static storage), `606–609` (not-found cache population example for test modules)

---

**INV-07**: Directory modules (modules with `mod.tml`) parse ALL `.tml` files in the directory, not only those declared with `pub mod`. Parse errors in individual sibling files do not abort loading if `abort_on_module_error_ == false`. Declarations from all successfully-parsed files are registered.

*Source*: `env_module_load.cpp:340–392`

---

**INV-08**: Sibling `.tml` files (not `mod.tml`) do NOT eagerly load their external `use` dependencies. Only `mod.tml` and intra-module `use` paths trigger eager loading. This prevents transitive dependency bloat.

*Source*: `env_module_load_decls.cpp:926–930` (the `is_from_mod_file || is_intra_module` condition)

---

**INV-09**: Private structs and enums are stored in `Module::internal_structs` and `Module::internal_enums`. They are not present in `Module::structs` and `Module::enums`. The `ModuleRegistry::lookup_struct` chain does not consult `internal_structs`, so private types cannot be accessed from outside their module.

*Source*: `env_module_load_decls.cpp:445–453` (struct), `493–501` (enum), `module.hpp:73–76` (field declarations)

---

**INV-10**: Methods in generic impl blocks (`!impl_decl.generics.empty()`) are always registered regardless of their declared visibility. Methods in non-generic, non-behavior impl blocks are registered only if `vis == Public`.

*Source*: `env_module_load_decls.cpp:557–561`

---

**INV-11**: For specialized impls (e.g., `impl[T] Pin[Heap[T]]`), two entries are created in `Module::functions`: one under the base key (`Pin::method`) and one under the discriminated key (`Pin[Heap]::method`). The base key may be overwritten by later impl registrations for the same type.

*Source*: `env_module_load_decls.cpp:643–651`

---

**INV-12**: Default behavior methods are only registered in Pass 2 of `extract_module_declarations`, after Pass 1 has populated `mod.behaviors`. A default method is registered only if the impl block does not already provide it (name-based check in `impl_method_names`). Default methods are registered under `TypeName::method` in `mod.functions`.

*Source*: `env_module_load_decls.cpp:1073–1156`

---

**INV-13**: The `TypeEnv::check_module` (type-checker) processes `UseDecl` in Phase 0 before any type registration. All modules referenced by `use` declarations are loaded (ModuleRegistry-registered) and their symbols imported into `imported_symbols_` before any `struct`, `enum`, or `behavior` from the current module is registered.

*Source*: `checker/core.cpp:203–261`

---

**INV-14**: When `load_native_module` is called with `silent=false` and the module is not found, an error is logged. When called with `silent=true`, no error is logged. The `abort_on_module_error_` flag controls whether parse errors during directory module loading abort the load or continue with successfully-parsed files.

*Source*: `env_module_loading.cpp:502–507` (test silent), `env_module_load.cpp:396–429` (abort_on_module_error_ branch)

---

**INV-15**: After `load_module_from_file` registers a module, it loads all `re_exports[i].source_path` modules and all `private_imports[i]` modules recursively. Both are loaded with `silent=true`. This makes re-export chains and private transitive dependencies available in the local `ModuleRegistry` without requiring explicit `use` statements from the consumer.

*Source*: `env_module_load.cpp:481–503`

---

**INV-16**: Re-export source path loading is symmetric across all three load paths (GlobalModuleCache hit, binary meta cache hit, source file load). All three paths execute the same re-export and private-import loading logic after registering the module.

*Source*: `env_module_loading.cpp:244–295` (GlobalModuleCache), `343–388` (binary cache), `env_module_load.cpp:481–503` (source file)

---

**INV-17**: `Module::has_pure_tml_functions` is set to `true` if the module contains any function with a body, any behavior impl method with a body, any OOP class method with a body, or any public constant. When `false`, the module contains only `@extern` declarations and no codegen source is needed.

*Source*: `env_module_load_decls.cpp:1177–1224`

---

**INV-18**: `Module::source_code` is populated only when `has_pure_tml_functions == true`. It contains the concatenation of all parsed files' preprocessed source code (not raw source). Preprocessor directives have already been evaluated; the stored string is ready for re-lexing by the codegen.

*Source*: `env_module_load_decls.cpp:1246–1250`

---

**INV-19**: The `UseDecl` path in `extract_module_declarations` handles relative imports by prepending the current module path when the path does not start with `core::`, `std::`, or `test`. This means `use helpers` inside `myapp::utils` resolves to `myapp::utils::helpers`.

*Source*: `env_module_load_decls.cpp:909–913`

---

**INV-20**: `process_use_decl` in the type checker implements two-level re-export resolution: if a direct module path is not found on disk, it searches the parent module's `re_exports` (and for three-segment paths, also the grandparent's `re_exports`). This allows logical module paths like `std::http::chunked` to be used even when the file is physically at `std::http::encoding::chunked`.

*Source*: `checker/core.cpp:490–541` (parent), `637–680` (grandparent)

---

**INV-21**: The type checker's `process_use_decl` emits a `T027` error ("Module not found") only for glob imports and grouped symbol imports when the module cannot be resolved through any fallback. Single-path imports (`use foo::bar::Baz`) do not emit `T027` — they silently fail if the module is not found, because the last segment is assumed to be a symbol rather than a module path.

*Source*: `checker/core.cpp:554–558` (glob error), `578–582` (group error); compare with `617–693` (single path, no error emitted)

---

**INV-22**: The `s_lib_root` path is resolved once per process by examining several candidate directories. Once resolved (or determined to be unresolvable), the result is cached in `s_lib_root` behind `s_lib_root_resolved`. All subsequent calls return the cached value. A failed resolution returns an empty string, causing all resolution to fall back to the full relative-path search.

*Source*: `env_module_loading.cpp:83–113`

---

**INV-23**: `FuncSig::is_lowlevel` is set from `parser::FuncDecl::is_unsafe`. In TML, `lowlevel` blocks are represented as `is_unsafe` in the AST. The type environment stores this flag so that codegen can distinguish between safe and lowlevel functions without re-examining the source.

*Source*: `env_module_load_decls.cpp:408`, `639`

---

## 13. Surprising Findings

### 13.1 Duplicate definitions at translation unit scope

`ParsedModuleFile`, `get_tml_type_name`, `format_float_const`, `try_extract_scalar_const_value`, `try_extract_module_const_value`, and `ParseResult` are defined once in `env_module_load.cpp` and again in `env_module_load_decls.cpp`. These are static (internal linkage) or file-local struct definitions. They are binary-identical copies. The design relies on the C++ ODR exemption for `static` functions and the fact that the structs have the same layout in both translation units. This is fragile — a future change to one copy that is not mirrored in the other will introduce silent divergence.

*Source*: `env_module_load.cpp:46–235`, `env_module_load_decls.cpp:13–204`

### 13.2 Base key collision for multiple specialized impls

When multiple impl blocks exist for the same base type (e.g., `impl[T] Pin[ref T]` and `impl[T] Pin[Heap[T]]`), both register methods under `Pin::method` in `mod.functions`. The second registration overwrites the first (plain `unordered_map::operator[]`). Only the specialized keys (`Pin[ref]::method`, `Pin[Heap]::method`) survive both impls.

A callee looking up `Pin::method` by its base key will find the last-written impl's signature. If the caller is trying to select the correct impl based on the receiver type, it must use the discriminated key. This is not documented in the code — callers that use the base key for dispatch may silently pick the wrong impl.

*Source*: `env_module_load_decls.cpp:643–650`

### 13.3 abort_on_module_error_ is temporarily disabled during import loading

Within `extract_module_declarations`, when a `UseDecl` triggers `load_native_module`, the code saves and restores `abort_on_module_error_`:

```cpp
bool prev_abort_on_error = abort_on_module_error_;
abort_on_module_error_ = false;
bool loaded = load_native_module(use_path, silent=true);
abort_on_module_error_ = prev_abort_on_error;
```

This means that even in normal compilation mode (`abort_on_module_error_ == true`), a module path that fails to parse during import resolution does not trigger an error. The failure is silently swallowed. The error is only surfaced if the caller explicitly requests the symbol that failed to load.

*Source*: `env_module_load_decls.cpp:933–955`

### 13.4 source_code stores preprocessed output, not raw source

`Module::source_code` is populated from `parsed_file.source_code`, which is set to `pp_result.output` in `parse_tml_file`. The preprocessor has already evaluated all `#if` directives. When codegen re-parses this source, it receives the post-preprocessor form. This is intentional (avoids running the preprocessor twice) but means the stored source code may differ from what a developer sees in the file.

*Source*: `env_module_load.cpp:288–290` (source_code = pp_result.output), `env_module_load_decls.cpp:1248`

### 13.5 Library root hardcoded fallback

`find_lib_root` includes a hardcoded absolute path `"F:/Node/hivellm/tml/lib"` as a fallback candidate. This is a developer-machine-specific path that will fail on any other machine. If the CWD-relative paths also fail (e.g., because the binary is run from a non-standard directory), the library root resolution falls back to the full relative-path search (10–12 probes per module). The hardcoded path is harmless on other machines but it reveals an assumption about where development occurs.

*Source*: `env_module_loading.cpp:97`

### 13.6 Circular detection returns true, not false

When a circular import is detected, `load_module_from_file` returns `true` (`env_module_load.cpp:311`). The caller (`load_native_module`) propagates this `true` back to the type checker's Phase 0. The type checker receives `true` (success) for a module that is not yet registered and has no symbols. If a symbol from the circularly-referenced module is needed before the outer load completes, the lookup will fail silently with no error — the symbol simply won't be found.

This is an inherent limitation of the single-registry architecture: circular dependencies are "resolved" by pretending success, but partial module availability during loading is not tracked.

*Source*: `env_module_load.cpp:309–312`

### 13.7 Fallback type for unknown types is I32, not an error

`resolve_simple_type` returns `make_primitive(PrimitiveKind::I32)` for type expressions it cannot resolve. There is even a duplicate `return` statement suggesting a copy-paste:

```cpp
return make_primitive(PrimitiveKind::I32);
TML_DEBUG_LN("[MODULE] Warning: Could not resolve type, using I32 as fallback"); // unreachable
return make_primitive(PrimitiveKind::I32);                                        // unreachable
```

Any type that has no explicit mapping in `resolve_simple_type` silently becomes `I32`. This affects the method signatures stored in `Module::functions` for imported modules, and consequently affects type inference and codegen for those methods.

*Source*: `env_module_load_decls.cpp:344–349`

---

## Summary Statistics

| Metric | Value |
|--------|-------|
| Invariants documented | 23 |
| Files analysed | 6 (3 primary + env.hpp + module.hpp + module_binary.hpp + checker/core.cpp) |
| Surprising findings | 7 |
| Approximate page count | ~18 pages |

---

*Document written by spec-engineer agent, 2026-04-06. Read-only audit — no code changes.*
