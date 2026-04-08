# 34. Package Manifest & Workspace Registry

This document describes the workspace/package manifest format and the
`PackageRegistry` that resolves module paths to source directories.

## 1. Workspace Manifest (`tml.toml`)

A workspace is rooted at a directory containing a `tml.toml` with a
`[workspace]` section. Members are listed by relative path:

```toml
[package]
name = "tml"
version = "0.1.0"
edition = "2025"

[workspace]
members = [
    "lib/core",
    "lib/std",
    "lib/test",
    "compiler-tml",
]
```

Members can live anywhere relative to the workspace root — they are not
required to live under `lib/`. This is what allows `compiler-tml/` to
expose itself as the `compiler` namespace without being copied into
`lib/compiler/`.

## 2. Member Manifest

Each member has its own `tml.toml`:

```toml
[package]
name = "compiler"
version = "0.1.0"
edition = "2025"

[dependencies]
core = { path = "../lib/core" }
std  = { path = "../lib/std" }
test = { path = "../lib/test" }
```

The `[package].name` becomes the **TML import namespace** for that
package. Files under `<member>/src/` are reachable as
`<package_name>::<file_stem>` (or `<package_name>::<dir>::mod` via
`mod.tml`).

## 3. PackageRegistry (C++)

`tml::pkg::PackageRegistry` is a process-wide singleton that walks up
from the current working directory looking for the workspace root,
then loads each member manifest into a `{name → PackageInfo}` map.

```cpp
struct PackageInfo {
    std::string name;            // [package].name
    std::filesystem::path root_dir;  // <workspace>/<member>
    std::filesystem::path src_dir;   // root_dir / "src"
    std::vector<std::string> dependencies;
};
```

### Key API

- `lookup_for_module(module_path)` — splits on `::`, returns
  `{package, rest}` if the head segment matches a registered package.
- `is_package_module(module_path)` — true iff the head segment is a
  workspace package.
- `get(name)` — direct lookup by package name.

### Loading semantics

`ensure_loaded()` runs once via mutex on first use. If no workspace
root is found, the registry stays empty and callers fall back to their
default behavior — `tml check file.tml` continues to work from
arbitrary directories.

## 4. Module Resolution Flow

When the type checker encounters `use compiler::serial::writer`, it
walks the resolver chain:

1. Hardcoded fast path for `core::*`, `std::*`, `test::*` (these are
   special-cased to enable AOT-cached metadata loading).
2. **PackageRegistry path**: split off the head segment, look it up in
   the registry. If found, build `pkg.src_dir / rest.tml` (or
   `pkg.src_dir / rest / mod.tml`) and load.
3. Legacy `lib/<name>/src/` probing as a final fallback.

This means **any workspace member is importable by its package name**
without hardcoding prefixes in the compiler.

## 5. Sites that use `PackageRegistry`

- `compiler/src/types/env_module_loading.cpp` — module path resolution.
- `compiler/src/types/module_binary_read.cpp::resolve_module_source_path` — meta-cache source lookup.
- `compiler/src/types/module_metadata.cpp` — `get_metadata_path` / `get_object_path` (compiled artifact location).
- `compiler/src/types/module.cpp::GlobalModuleCache::should_cache` — caches all package modules.
- `compiler/src/codegen/llvm/core/generate_cache.cpp` — codegen-side cache key whitelist.
- `compiler/src/cli/builder/build.cpp`, `builder_run.cpp`, `run_profiled.cpp` — `build.tml` dispatch (skip workspace packages, the driver handles them directly).

## 6. Artifact Roots

TML build outputs go to `<workspace>/build/{debug,release}/` (or
`<project_root>/build/...` when no workspace is active). See
`get_tml_artifact_root()` in `compiler/src/cli/builder/builder_helpers.cpp`.

## 7. Cycle Detection

Dependencies declared in `[dependencies]` are validated by
`PackageRegistry::load_impl()` for cycles using a DFS coloring pass. A
cycle aborts loading and emits a diagnostic naming the cycle members.

## 8. Lib vs Crate Terminology

TML uses `lib` consistently (not Rust's `crate`). The relevant manifest
keys and CLI flags:

| TML | (vs. Rust) |
|-----|-----------|
| `[lib]` section | `[lib]` |
| `lib-type = ["rlib"]` | `crate-type = ["rlib"]` |
| `--lib-type=rlib` | `--crate-type=rlib` |
| `LibConfig::lib_types` | `crate_types` |

See ADR notes in `phase12g_package-registry/tasks.md` §6d.
