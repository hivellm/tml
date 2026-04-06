# Proposal: phase10_build-tml-package-system

## Why

TML packages that depend on native C libraries (PostgreSQL/libpq, OpenSSL, zlib, etc.) have no way to declare native dependencies, resolve library search paths, or copy runtime DLLs to the output directory. The existing `@link("lib")` decorator only emits `-llib` to the linker without any search path resolution — the library must be in the system path or linking fails.

Rust solves this with `build.rs` (a build script compiled and executed before the crate), which prints `cargo:rustc-link-search=` and `cargo:rustc-link-lib=` directives to stdout. This enables self-contained packages with bundled native binaries that work offline without system-wide library installation.

Without this infrastructure, `lib/postgresql/` (and any future FFI-heavy package) cannot be self-hosting and isolated.

## What Changes

### 1. Build Script Support (`build.tml`)

A new compilation phase: when the compiler processes a package that contains a `build.tml` file at its root, it:

1. Compiles `build.tml` using the standard TML pipeline
2. Executes the resulting binary as a subprocess
3. Parses stdout lines prefixed with `tml:` as build directives
4. Applies the directives to the subsequent compilation of the package's `src/`

This mirrors Rust's `build.rs` exactly.

### 2. Build Directive Protocol (`tml:` stdout lines)

| Directive | Effect | Rust Equivalent |
|-----------|--------|-----------------|
| `tml:link-lib=<name>` | Adds `-l<name>` to linker | `cargo:rustc-link-lib=<name>` |
| `tml:link-search=<path>` | Adds `-L <pkg_dir>/<path>` to linker | `cargo:rustc-link-search=native=<path>` |
| `tml:copy-artifact=<path>` | Copies file to output dir after link | No Rust equivalent (handled by build tools) |
| `tml:rerun-if-changed=<path>` | Invalidates build cache if path changed | `cargo:rerun-if-changed=<path>` |
| `tml:warning=<msg>` | Prints warning during build | `cargo:warning=<msg>` |
| `tml:cfg=<key>` | Defines conditional compilation symbol | `cargo:rustc-cfg=<key>` |

### 3. Linker Search Path Resolution

The linker (`lld_linker.cpp` / `builder_run.cpp` / `build.cpp`) is extended to accept search paths alongside link lib names. Currently it only handles `-l<name>` — after this change it also handles `-L <path>` from build script output.

### 4. Post-Link Artifact Copying

After successful linking, the compiler copies any files declared via `tml:copy-artifact=` to the output directory (where the `.exe` lives). This ensures runtime DLLs are next to the executable.

### 5. Package Directory Convention

```
lib/<package>/
├── package.toml       # Package metadata (existing)
├── build.tml          # Build script (NEW, optional)
├── native/            # Pre-built native libraries (NEW, optional)
│   ├── win-x64/       # Windows x64 binaries
│   │   ├── libpq.lib  # Import library (link time)
│   │   └── libpq.dll  # Runtime library (copied to output)
│   ├── linux-x64/     # Linux x64 binaries
│   │   ├── libpq.a    # Static library
│   │   └── libpq.so   # Shared library
│   └── macos-arm64/   # macOS ARM64 binaries
│       └── libpq.dylib
└── src/               # TML source (existing)
    ├── mod.tml
    └── ffi.tml         # @link("pq") @extern("c") func PQ*...
```

## Impact

- **Affected specs**: New spec — `package-build-system`
- **Affected code**:
  - `compiler/src/cli/builder/build.cpp` — build script detection + execution + directive parsing
  - `compiler/src/cli/builder/builder_run.cpp` — link search paths for `tml run`
  - `compiler/src/backend/lld_linker.cpp` — accept `-L` search paths
  - `compiler/include/codegen/codegen_backend.hpp` — add `link_search_paths` field
  - `compiler/src/testing/testing_compile_parallel.cpp` — search paths for test suite linking
  - `compiler/src/cli/builder/run_profiled.cpp` — search paths for profiled runs
- **Breaking change**: NO — purely additive; packages without `build.tml` are unaffected
- **User benefit**: Self-contained packages with native dependencies that work offline across all platforms. Enables `lib/postgresql/`, `lib/openssl/`, `lib/zlib/`, and any future FFI package.
