# Proposal: phase0l_lazy-plugin-dll-loading

## Why
Every TML compilation loads `tml_compiler.dll` (~104 MB) and `tml_codegen_x86.dll` (~78 MB) from disk on each invocation. Cold DLL loading accounts for ~145ms of the measured 153ms average compile time — 95% of total latency for small files. Rust's `rustc` keeps its compiler binary resident; TML pays the DLL load cost every time. This is the dominant factor in TML's 27x compile-time gap vs Rust (see `docs/analysis/benchmark/08-compilation.md`). Lazy loading (loading symbols only when first called) and session caching (keeping the DLL mapped for the duration of a project build) can eliminate this penalty entirely.

## What Changes
1. **Lazy symbol resolution**: Use `RTLD_LAZY` (Linux/macOS) / `LOAD_LIBRARY_AS_DATAFILE` (Windows) for non-critical plugin exports — only resolve symbols actually called, not the entire export table on load.
2. **DLL handle caching**: Store loaded `HMODULE`/`void*` handles in a process-level singleton (static map keyed by DLL path + mtime hash). Subsequent loads return the cached handle with zero disk I/O.
3. **Warmup preloading**: `tml.exe` entrypoint preloads both DLLs in a background thread while parsing the command line — by the time the first compilation starts, the DLLs are already mapped.
4. **Build-time**: no change to DLL content; only the loader (`compiler/src/plugin/plugin_loader.cpp`) changes.

## Impact
- Affected specs: compiler/plugin-loading, build/compilation-latency
- Affected code: `compiler/src/plugin/plugin_loader.cpp` (or equivalent DLL loader)
- Breaking change: NO
- User benefit: Target: reduce cold compile time from 153ms to <20ms for small files, closing the 27x gap to <3x vs Rust.
