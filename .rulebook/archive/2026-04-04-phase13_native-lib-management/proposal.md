# Proposal: Centralized Native Library Management System

## Context

TML packages that depend on native C/C++ libraries (libpq, OpenSSL, zlib, zstd, brotli, sqlite3) currently use ad-hoc approaches to locate, link, and distribute these libraries at runtime. There is no unified system -- each command (`build`, `run`, `test`) independently discovers native libraries through hardcoded paths, vcpkg directory scanning, and `build.tml` script execution.

**Current pain points:**

1. **Hardcoded paths everywhere.** `builder_run.cpp` has literal paths like `"F:/Node/hivellm/tml/vcpkg_installed/x64-windows/bin"`. This fails on any machine other than the developer's.

2. **DLL discovery is duplicated.** `build.cpp`, `builder_run.cpp`, `testing_compile.cpp`, `testing_compile_parallel.cpp`, and `builder_helpers_runtime.cpp` all independently search for DLLs using different strategies.

3. **No user-level cache.** Native binaries are stored inside the project tree (`lib/postgresql/native/win-x64/`) and committed to git. This bloats the repository and fails when a user clones without the binaries.

4. **No transitive native dependency resolution.** If package A depends on package B which needs `libssl.dll`, there is no mechanism for A's build to discover that it also needs `libssl.dll` in its output directory.

5. **`build.tml` artifacts are ephemeral.** The `copy-artifact` directive copies DLLs to `build/debug/` but test executables run from `build/debug/cache/tests/<suite>/`, requiring additional copies.

6. **No distribution story.** There is no `tml build --release --bundle` that collects all needed DLLs into a distributable directory.

7. **Cross-platform library naming is fragile.** Each `build.tml` must handle Windows/Linux/macOS naming manually via `#if WINDOWS/LINUX/MACOS`.

## What Exists Today

### Infrastructure Already Built

| Component | File | Status |
|-----------|------|--------|
| Build script system | `build_script.hpp/cpp` | Working -- compiles+runs `build.tml`, parses 6 directive types |
| Build script integration | `build.cpp` lines 85, 908-974 | Working -- merges link flags from build scripts |
| Build script caching | `testing_compile.cpp` lines 519-575 | Working -- mtime-based cache for test builds |
| Package detection | `build_script.cpp::detect_package_dir()` | Working -- walks up to find `package.toml` |
| Manifest system | `build_config.hpp/cpp` | Working -- parses `tml.toml` with TOML subset parser |
| Dependency resolver | `dependency_resolver.hpp/cpp` | Partial -- path deps work, version/git deps are stubs |
| Lockfile support | `dependency_resolver.hpp` | Defined -- `Lockfile` struct exists, load/save/verify |
| CLI commands | `cmd_pkg.cpp` | Working -- `tml deps`, `tml add`, `tml remove`, `tml update` |
| CLI init | `cmd_init.cpp` | Working -- `tml init` creates project structure |
| Package metadata | `package.toml` | Working -- has `[native-deps]` section (postgresql) |
| OpenSSL discovery | `builder_helpers.cpp::find_openssl()` | Working -- checks vcpkg, then system paths |
| SQLite3 discovery | `builder_helpers.cpp::find_sqlite3()` | Working -- checks vcpkg, then system paths |
| vcpkg DLL copying | `builder_run.cpp::ensure_runtime_dlls()` | Working -- hardcoded list, hardcoded paths |
| LinkOptions | `object_compiler.hpp` | Working -- has `library_search_paths` and `link_flags` |

### Native Libraries Currently in Use

| Library | Source | Used By | Size (Win x64) |
|---------|--------|---------|-----------------|
| libpq.dll + libpq.lib | `lib/postgresql/native/win-x64/` | postgresql package | 351KB + 40KB |
| libcrypto-3-x64.dll | `lib/postgresql/native/win-x64/` AND `vcpkg_installed/` | postgresql, std::crypto | 4.7MB |
| libssl-3-x64.dll | `lib/postgresql/native/win-x64/` AND `vcpkg_installed/` | postgresql, std::crypto | 780KB |
| libiconv-2.dll | `lib/postgresql/native/win-x64/` | postgresql (transitive) | 1.8MB |
| libintl-9.dll | `lib/postgresql/native/win-x64/` | postgresql (transitive) | 476KB |
| zlib1.dll | `vcpkg_installed/x64-windows/bin/` | std::zlib | 90KB |
| zstd.dll | `vcpkg_installed/x64-windows/bin/` | std::zlib | 652KB |
| brotlicommon.dll | `vcpkg_installed/x64-windows/bin/` | std::zlib | 138KB |
| brotlidec.dll | `vcpkg_installed/x64-windows/bin/` | std::zlib | 51KB |
| brotlienc.dll | `vcpkg_installed/x64-windows/bin/` | std::zlib | 3.3MB |
| sqlite3.dll | `vcpkg_installed/x64-windows/bin/` | std::db (sqlite) | 1.1MB |

**Total native library footprint (Win x64): ~13.4 MB**

### Duplication Problem

`libcrypto-3-x64.dll` and `libssl-3-x64.dll` are stored in TWO places:
- `vcpkg_installed/x64-windows/bin/` (4.7MB + 823KB)
- `lib/postgresql/native/win-x64/` (4.7MB + 780KB) -- slightly different build

This wastes ~5.5MB of git storage and creates version mismatch risk.

## Architecture Decision: Three-Tier Native Library Resolution

### Decision

Implement a three-tier resolution system:

1. **Compiler-bundled libs** (Tier 0): OpenSSL, zlib, zstd, brotli -- shipped with the compiler installation, stored in `<compiler_dir>/lib/native/<platform>/`. The compiler always knows where these are.

2. **User-level cache** (Tier 1): `~/.tml/native-cache/<package>/<version>/<platform>/`. Downloaded once, shared across all projects. Never committed to git.

3. **Project-local overrides** (Tier 2): `<project>/native/<package>/<platform>/`. For custom builds, patched versions, or offline development. Optional, takes precedence over cache.

### Rationale

- **Tier 0** eliminates the vcpkg dependency for the core language. Users get OpenSSL/zlib/sqlite3 out of the box, just like Rust ships with libc.
- **Tier 1** follows the npm/cargo model: install once, reuse everywhere. Eliminates git bloat from `lib/postgresql/native/`.
- **Tier 2** preserves the current escape hatch for packages that bundle custom-built natives (the postgresql package pattern).

### Rejected Alternative: Always bundle in project

Why rejected: Bloats every project with ~13MB of binaries. Multiple projects on the same machine duplicate the same DLLs. Git repositories balloon in size.

### Rejected Alternative: System-level only (Go model)

Why rejected: The Go model (`#cgo LDFLAGS: -lpq`) requires users to install native libraries system-wide. This is hostile to new users and breaks reproducibility. Cargo's approach (build.rs + vendored sources OR prebuilt binaries) is strictly better for developer experience.

### Rejected Alternative: Always compile from source (Cargo model)

Why rejected: Compiling OpenSSL from source takes 5-15 minutes and requires a C toolchain. TML's target audience (LLM-generated code, rapid prototyping) values instant setup over build-from-source purity.

### Trade-offs

| Given up | Gained |
|----------|--------|
| Simplicity of "just copy DLLs" | Reproducible builds across machines |
| Single source of truth (vcpkg) | Works without vcpkg installed |
| Repository self-containment | Smaller repos, no binary bloat in git |
| Manual DLL management | Automatic transitive dependency resolution |

### Consequences

- **Operational**: The compiler installer must bundle ~15MB of native libs. CI/CD pipelines need `tml install` step.
- **Development**: Every command that links or runs executables must query the NativeLibResolver instead of hardcoding paths.
- **Migration**: Existing `vcpkg_installed/` and `lib/*/native/` patterns continue to work during transition.

### Review Date

2026-07-01 -- After the package registry (phase11) ships, reassess whether Tier 1 downloads should come from the registry or remain manual.

---

## Directory Structure

### Compiler Installation Layout

```
<install_dir>/
  bin/
    tml.exe                         # Compiler binary
  lib/
    native/                         # Tier 0: Compiler-bundled native libs
      win-x64/
        libcrypto-3-x64.dll
        libcrypto-3-x64.lib         # Import lib (link-time)
        libssl-3-x64.dll
        libssl-3-x64.lib
        zlib1.dll
        zlib1.lib
        zstd.dll
        zstd.lib
        brotlicommon.dll
        brotlidec.dll
        brotlienc.dll
        sqlite3.dll
        sqlite3.lib
      linux-x64/
        libcrypto.so.3
        libssl.so.3
        libz.so.1
        libzstd.so.1
        libbrotlicommon.so.1
        libbrotlidec.so.1
        libbrotlienc.so.1
        libsqlite3.so.0
      macos-arm64/
        libcrypto.3.dylib
        libssl.3.dylib
        libz.1.dylib
        ...
    runtime/                        # Compiled C runtime objects
      tml_runtime.lib               # (existing location)
```

### User-Level Cache Layout

```
~/.tml/                             # Unix: $HOME/.tml/
%LOCALAPPDATA%\tml\                 # Windows: C:\Users\<user>\AppData\Local\tml\

  native-cache/                     # Tier 1: Downloaded native libs
    libpq/
      16.8.0/                       # Version directory
        win-x64/
          libpq.dll
          libpq.lib
          libiconv-2.dll            # Transitive deps bundled together
          libintl-9.dll
        linux-x64/
          libpq.so.5.16
          libpq.so -> libpq.so.5.16
        macos-arm64/
          libpq.5.dylib
          libpq.dylib -> libpq.5.dylib
          libpq.a
      manifest.toml                 # Metadata: version, hash, download URL, deps
    
    mongodb-c-driver/
      1.27.0/
        win-x64/
          ...
  
  credentials.toml                  # Registry auth (future, phase11)
  config.toml                       # User settings (native lib paths, etc.)
```

### Project Layout (After This Change)

```
myproject/
  tml.toml                          # Project manifest (existing)
  tml.lock                          # Lockfile (existing, extended)
  src/
    main.tml
  lib/                              # Package sources (existing pattern)
    postgresql/
      package.toml                  # Extended with [native-deps]
      build.tml                     # Existing build script (unchanged)
      src/
        mod.tml
      native/                       # Tier 2: Project-local overrides (OPTIONAL)
        win-x64/                    # Only present if user needs custom builds
          libpq.dll
  build/
    debug/
      myproject.exe
      libpq.dll                     # Copied here by artifact resolver
      libcrypto-3-x64.dll           # Copied from Tier 0 (compiler-bundled)
      libssl-3-x64.dll
    release/
      myproject.exe
      libpq.dll
      bundle/                       # Created by --bundle flag
        myproject.exe
        libpq.dll
        libcrypto-3-x64.dll
        libssl-3-x64.dll
```

---

## Detailed Design

### 1. NativeLibResolver (New C++ Class)

Central component that replaces all ad-hoc DLL discovery logic.

**File:** `compiler/src/cli/builder/native_lib_resolver.hpp/cpp`

```
class NativeLibResolver {
    // Resolve a native library by name, returning paths for link-time and run-time
    struct ResolvedNativeLib {
        string name;
        string version;
        fs::path link_lib;          // .lib/.a for linker
        fs::path runtime_lib;       // .dll/.so/.dylib for runtime
        vector<fs::path> transitive_runtime_libs;  // deps of deps
        NativeLibTier tier;         // Which tier resolved it
    };
    
    enum class NativeLibTier { CompilerBundled, UserCache, ProjectLocal };
    
    // Search order: ProjectLocal -> UserCache -> CompilerBundled -> vcpkg (compat)
    optional<ResolvedNativeLib> resolve(string name, string platform);
    
    // Resolve all native deps for a package (reads package.toml [native-deps])
    vector<ResolvedNativeLib> resolve_package(fs::path package_dir, string platform);
    
    // Resolve all native deps transitively for a full build
    vector<ResolvedNativeLib> resolve_all(Manifest manifest, fs::path project_root, string platform);
    
    // Copy all resolved runtime libs to a target directory
    void copy_runtime_libs(vector<ResolvedNativeLib> libs, fs::path target_dir);
};
```

**Resolution order per library:**

1. Check `<project>/native/<lib>/<platform>/` (Tier 2, project-local override)
2. Check `~/.tml/native-cache/<lib>/<version>/<platform>/` (Tier 1, user cache)
3. Check `<compiler_dir>/lib/native/<platform>/` (Tier 0, compiler-bundled)
4. Check `vcpkg_installed/<platform>/` (compatibility fallback during migration)
5. Check system paths via pkg-config (Linux/macOS fallback)

### 2. Package Manifest Extension (`package.toml`)

Extend the existing `[native-deps]` section (already present in postgresql's `package.toml`):

```toml
[native-deps]
libpq = { lib = "libpq", version = ">=16.0", headers = ["libpq-fe.h"] }

[native-deps.libpq.platform.win-x64]
download = "https://ftp.postgresql.org/pub/source/v16.8/..."
sha256 = "abc123..."
runtime = ["libpq.dll", "libiconv-2.dll", "libintl-9.dll", "libcrypto-3-x64.dll", "libssl-3-x64.dll"]
link = ["libpq.lib"]

[native-deps.libpq.platform.linux-x64]
download = "..."
runtime = ["libpq.so.5.16"]
link = ["libpq.so"]

[native-deps.libpq.platform.macos-arm64]
download = "..."
runtime = ["libpq.5.dylib"]
link = ["libpq.dylib"]
```

The `[native-deps]` section is **declarative** -- it tells the resolver what the package needs. The `build.tml` script is **imperative** -- it runs at build time and emits directives. Both coexist: `[native-deps]` is for automated resolution and `build.tml` is for custom build logic.

### 3. `tml install` Command (New)

**File:** `compiler/src/cli/commands/cmd_install.hpp/cpp`

```
tml install                    # Install all deps + native libs for current project
tml install --native-only      # Only install native libs (skip TML package fetch)
tml install --global <package> # Install a TML tool globally
```

Workflow:
1. Read `tml.toml` for dependencies
2. Resolve each dependency (path, git, registry)
3. For each dependency with `[native-deps]` in its `package.toml`:
   a. Check if native libs exist in user cache
   b. If not, download from URL specified in `package.toml`
   c. Verify SHA256 hash
   d. Extract to `~/.tml/native-cache/<lib>/<version>/<platform>/`
4. Write `tml.lock` with exact versions and hashes

### 4. Build Script Simplification

After NativeLibResolver exists, `build.tml` files become simpler. The resolver handles path discovery; `build.tml` only needs to emit `tml:link-lib=pq`. No more `#if WINDOWS/LINUX/MACOS` blocks for search paths.

**Before (current postgresql/build.tml):**
```tml
#if WINDOWS
func emit_link() {
    println("tml:link-search=native/win-x64")
    println("tml:link-lib=libpq")
    println("tml:copy-artifact=native/win-x64/libpq.dll")
}
#elif LINUX
func emit_link() {
    println("tml:link-search=native/linux-x64")
    println("tml:link-lib=pq")
}
#elif MACOS
func emit_link() {
    println("tml:link-search=native/macos-arm64")
    println("tml:link-lib=pq")
}
#endif
```

**After (build.tml becomes optional, package.toml drives resolution):**
```toml
# package.toml [native-deps] handles everything
# build.tml only needed for custom build logic (compiling C sources, etc.)
```

### 5. Command Changes

#### `tml build`

**Before:** Calls `detect_and_run_build_script()`, merges results into `LinkOptions`. Calls `find_openssl()` / `find_sqlite3()` / `ensure_runtime_dlls()` with hardcoded paths.

**After:** 
1. Calls `NativeLibResolver::resolve_all()` to get all native libs needed
2. Adds link-time paths to `LinkOptions.library_search_paths`
3. Adds link-time libs to `LinkOptions.link_flags`
4. After linking, calls `NativeLibResolver::copy_runtime_libs()` to copy DLLs to output dir
5. Still runs `build.tml` for custom build logic (additively)
6. New flag: `--bundle` creates a self-contained directory with all needed DLLs

**Files modified:**
- `compiler/src/cli/builder/build.cpp` -- Replace hardcoded OpenSSL/SQLite/vcpkg logic with `NativeLibResolver` calls
- `compiler/src/cli/builder/builder_internal.hpp` -- Remove `find_openssl()`, `find_sqlite3()`, `ensure_runtime_dlls()` declarations (moved to resolver)

#### `tml run`

**Before:** Calls `ensure_runtime_dlls()` with hardcoded DLL list and paths. Calls `find_openssl()`.

**After:**
1. Calls `NativeLibResolver::resolve_all()` + `copy_runtime_libs()` to ensure DLLs next to executable
2. On Windows: sets `PATH` to include native lib directories before launching subprocess (no copy needed in some cases)

**Files modified:**
- `compiler/src/cli/builder/builder_run.cpp` -- Replace `ensure_runtime_dlls()` with `NativeLibResolver`

#### `tml test`

**Before:** `testing_compile.cpp` and `testing_compile_parallel.cpp` independently discover build scripts, cache results, and copy artifacts.

**After:**
1. Test coordinator calls `NativeLibResolver::resolve_all()` once for the entire test session
2. Passes resolved native lib paths to each test suite compilation
3. Copies runtime DLLs to each test suite's execution directory

**Files modified:**
- `compiler/src/testing/testing_compile.cpp` -- Use `NativeLibResolver` instead of inline build script caching
- `compiler/src/testing/testing_compile_parallel.cpp` -- Same

#### `tml check`

No changes. Check does not link or run, so no native lib resolution needed.

#### `tml init` (Extended)

Add option to generate native-dep-aware package:
```
tml init --ffi           # Generates package.toml with [native-deps] section and build.tml template
```

**Files modified:**
- `compiler/src/cli/commands/cmd_init.cpp` -- Add `--ffi` template

#### `tml install` (New)

Brand new command for dependency + native lib installation.

**Files created:**
- `compiler/src/cli/commands/cmd_install.hpp`
- `compiler/src/cli/commands/cmd_install.cpp`

### 6. Distribution / Bundling

```
tml build --release --bundle         # Build + collect all DLLs
tml build --release --bundle=dir     # Specify output directory for bundle
```

The `--bundle` flag triggers post-link collection:
1. Scan the linked executable for all native library dependencies (Windows: PE import table, Linux: ldd, macOS: otool)
2. Copy all required runtime libraries to the bundle directory
3. On Linux: set rpath to `$ORIGIN` so the executable finds libs in its directory
4. On macOS: use `install_name_tool` to fix dylib paths to `@executable_path/`
5. On Windows: DLLs in same directory are found automatically

### 7. Platform Detection

**File:** `compiler/src/cli/builder/platform.hpp/cpp` (new)

```cpp
struct Platform {
    string os;       // "windows", "linux", "macos"
    string arch;     // "x64", "arm64"
    string triple;   // "x86_64-pc-windows-msvc", etc.
    
    string native_dir_name();  // "win-x64", "linux-x64", "macos-arm64"
    string shared_lib_ext();   // ".dll", ".so", ".dylib"
    string static_lib_ext();   // ".lib", ".a"
    string import_lib_ext();   // ".lib" (Windows only), "" (others)
    
    static Platform detect();  // Detect current platform
};
```

---

## Migration Path

### Phase A: Non-Breaking (Backward Compatible)

1. Build `NativeLibResolver` alongside existing code
2. New code calls resolver first, falls back to old logic if resolver returns nothing
3. Existing `vcpkg_installed/` and `lib/*/native/` patterns continue to work
4. `build.tml` scripts continue to work unchanged

### Phase B: Deprecation

1. Log warnings when DLLs are found via hardcoded vcpkg paths
2. Log warnings when `build.tml` emits `link-search` for `native/` directories (suggest using `[native-deps]`)
3. Add `tml migrate-native` command that moves `lib/*/native/` to user cache

### Phase C: Cleanup

1. Remove hardcoded paths from `builder_run.cpp`, `builder_helpers.cpp`, `builder_helpers_runtime.cpp`
2. Remove `find_openssl()`, `find_sqlite3()` helpers (replaced by resolver)
3. Remove `ensure_runtime_dlls()` (replaced by resolver)
4. Remove `vcpkg_installed/` from git (native libs come from compiler installation or user cache)

---

## Comparable Systems Reference

| Feature | Cargo (Rust) | npm (Node.js) | pip (Python) | TML (Proposed) |
|---------|-------------|---------------|-------------|----------------|
| Build script | `build.rs` | `binding.gyp` | `setup.py` | `build.tml` |
| Native lib declaration | `links = "pq"` in Cargo.toml | N/A | N/A | `[native-deps]` in package.toml |
| Native lib search | pkg-config, vcpkg, env vars | node-pre-gyp | auditwheel | NativeLibResolver (3-tier) |
| Prebuilt binaries | Via `-sys` crates | node-pre-gyp downloads | wheels with bundled .so | User cache downloads |
| Distribution | Not built-in (use `cargo-bundle`) | N/A (native runtime) | wheels contain binaries | `--bundle` flag |
| User cache | `~/.cargo/registry/` | `~/.npm/` | `~/.cache/pip/` | `~/.tml/native-cache/` |

## Impact

- **Affected specs**: New spec needed for native library resolution protocol
- **Affected code**: `build.cpp`, `builder_run.cpp`, `testing_compile.cpp`, `testing_compile_parallel.cpp`, `builder_helpers.cpp`, `builder_helpers_runtime.cpp`, `object_compiler.hpp`, `build_config.hpp`, `cmd_init.cpp`, `cmd_pkg.cpp`
- **New code**: `native_lib_resolver.hpp/cpp`, `platform.hpp/cpp`, `cmd_install.hpp/cpp`
- **Breaking change**: NO in Phases A-B. Phase C removes vcpkg fallback (breaking for projects relying on vcpkg layout).
- **User benefit**: Native FFI packages "just work" across machines without manual DLL management.
