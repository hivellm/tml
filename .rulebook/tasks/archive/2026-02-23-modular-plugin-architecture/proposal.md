# Proposal: Modular Plugin Architecture

## Why

The TML compiler (`tml.exe`) is 100MB in debug / ~40MB in release because it statically links ~85MB of LLVM/LLD libraries plus 25 internal static libraries. Commands like `tml check`, `tml fmt`, and `tml lint` load the full 100MB binary even though they never need the LLVM backend. This results in:

- Slow startup for simple commands (loading 100MB to format a file)
- Inability to expand coverage (CUDA, OpenCL, new backends) without bloating the main executable
- Heavy distribution — a single monolithic 40MB+ binary in release
- No way to do partial installation (e.g., only x86_64 without ARM64)

The solution is a plugin architecture where the compiler is split into modules loaded on demand, with zstd compression to reduce total size. All packaging tooling is written in pure TML.

## What Changes

### New Components

1. **Plugin ABI** (`compiler/include/tml_plugin.h`) — Pure C interface that every plugin DLL exports. Defines `TmlPluginInfo` (metadata: name, version, capabilities, dependencies) and 3 exported functions (`tml_plugin_query`, `tml_plugin_init`, `tml_plugin_shutdown`).

2. **Module Macro** (`compiler/include/tml_module.hpp`) — `TML_MODULE("name")` macro that each `.cpp` uses to declare which plugin it belongs to. No-op at runtime, used by CMake to group sources.

3. **Plugin Loader** (`compiler/src/plugin_loader.cpp`) — ~150 lines of C++ that handles: zstd decompression of `.dll.zst` to cache, `LoadLibrary`/`dlopen`, ABI verification, initialization.

4. **Thin Launcher** (`compiler/src/launcher/main_launcher.cpp`) — ~200 lines of C++. New entry point that: parses argv → determines required plugins → loads via plugin loader → delegates.

5. **CMake Module Collection** (`compiler/cmake/collect_modules.cmake`) — CMake script that scans `TML_MODULE("xxx")` in `.cpp` files and generates per-module source lists.

6. **Plugin Pack Tool** (`tools/plugin_pack.tml`) — 100% TML tool that compresses DLLs with zstd and generates `manifest.json`.

7. **Plugin Verify Tool** (`tools/plugin_verify.tml`) — 100% TML tool that verifies plugin integrity via SHA256 hashing.

### Modified Files

- `compiler/CMakeLists.txt` — Adds `TML_BUILD_MODULAR` option with conditional SHARED targets
- `compiler/src/codegen/codegen_backend.cpp` — `create_backend()` uses plugin loader when modular
- `compiler/src/cli/builder/object_compiler.cpp` — Routes to plugin LLVM C API when modular
- `compiler/src/cli/dispatcher.cpp` — On-demand loading logic
- `scripts/build.bat` — `--modular` and `--pack` flags
- ~369 `.cpp` files — Add `TML_MODULE("xxx")` (1 line at the top of each)

### Resulting Plugin DLLs

| Plugin | Contents | Est. Size (release+zstd) |
|--------|----------|--------------------------|
| `tml_compiler.dll` | lexer, parser, types, borrow, MIR, codegen IR, query | ~4MB |
| `tml_codegen_x86.dll` | LLVM x86_64 backend + LLD | ~8MB |
| `tml_codegen_arm64.dll` | LLVM AArch64 backend + LLD | ~7MB |
| `tml_codegen_cranelift.dll` | Cranelift backend | ~2MB |
| `tml_tools.dll` | formatter, linter, doc gen, search | ~1MB |
| `tml_test.dll` | test runner, coverage, benchmark | ~1MB |
| `tml_mcp.dll` | MCP server protocol | ~1MB |

### Integration Points

- **CodegenBackend abstraction** (`codegen_backend.hpp`) — Already the natural seam. `create_backend()` switches from `make_unique` to dlopen.
- **object_compiler.cpp** — Already has `#ifdef TML_HAS_LLVM_BACKEND` guards. The modular path adds routing via plugin C API.
- **Build scripts** — `scripts/build.bat` gains flags without breaking the existing flow.
- **Monolithic build remains default** — No behavior change for anyone not using `--modular`.

## Impact

- **Affected specs**: 09-CLI.md (new `--modular` flag), 16-COMPILER-ARCHITECTURE.md (plugin system)
- **Affected code**: `compiler/src/codegen/`, `compiler/src/cli/`, `compiler/src/backend/`, `compiler/CMakeLists.txt`, `scripts/build.bat`
- **Breaking change**: NO — monolithic build remains default and unchanged
- **User benefit**: 10-100x faster startup for simple commands (`tml fmt`: 1MB vs 100MB), compact distribution (~14MB vs 40MB), extensibility for CUDA/OpenCL/new backends without bloating the main executable

## Success Criteria

1. Monolithic build (`scripts\build.bat`) works identically to today
2. Modular build (`scripts\build.bat --modular`) produces tml.exe (~500KB) + plugin DLLs
3. Packed build (`scripts\build.bat --modular --pack`) generates compressed plugins + manifest.json
4. `tml --help` works without loading any plugin
5. `tml check file.tml` loads only `tml_compiler.dll` (no LLVM)
6. `tml build file.tml` loads compiler + codegen_x86 and compiles normally
7. Full test suite passes in both modes (monolithic and modular)
8. `tools/plugin_pack.tml` compiles and runs with TML itself

## Out of Scope

- Compiler self-hosting (roadmap Phase 6)
- Plugin marketplace or remote plugin download
- Hot-reload of plugins during compilation
- C runtime migration to TML (separate task)
