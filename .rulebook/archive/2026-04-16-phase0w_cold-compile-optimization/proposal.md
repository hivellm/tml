# Proposal: phase0w_cold-compile-optimization

## Why

TML cold compile (first run, no daemon) takes ~7-10s vs Rust's ~0.3-1s (10-30x
gap). The daemon solves incremental (22ms), but cold start remains slow. The
breakdown:

- **Plugin DLL loading: ~2s** — `tml_compiler.dll` (80MB) + `tml_codegen_x86.dll`
  (58MB) loaded via `LoadLibrary` on every cold invocation. The DLL handle cache
  (phase0l) helps on repeated runs, but first-ever load is unavoidable.
- **Type-checker: ~3s** — see phase0v for parallelization
- **IR generation + LLVM: ~2s** — partially parallelized via CGUs (phase0o)

This task focuses on reducing the DLL load + process startup overhead:

1. **Precompiled meta cache**: generate .tml.meta files at install time so
   first-run doesn't need to parse 418 library modules from source
2. **Delay-load codegen DLL**: load `tml_codegen_x86.dll` only when codegen
   is actually needed (not for `tml check` which only type-checks)
3. **Release-mode compiler**: distribute release-built DLLs (3x faster C++ execution)
4. **Static linking option**: single `tml.exe` with no DLL loading overhead

## What Changes

1. Delay-load `tml_codegen_x86.dll` — load on first `build`/`run`, not on startup
2. Pre-generate .tml.meta cache during `scripts/build.bat`
3. Profile and optimize the DLL initialization path (static constructors, LLVM init)

## Impact

- Affected specs: compiler/plugin/loader, compiler/cli/builder
- Affected code: `compiler/src/plugin/loader.cpp`, `compiler/src/cli/builder/build.cpp`
- Breaking change: NO
- User benefit: Cold `tml check` drops from ~5s to ~2s by avoiding codegen DLL load.
  Cold `tml build` remains ~7s but benefits from pre-cached meta.
