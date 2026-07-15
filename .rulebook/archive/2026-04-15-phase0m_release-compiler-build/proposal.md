# Proposal: phase0m_release-compiler-build

## Why
The TML compiler DLLs are currently built in Debug mode (CMake `Debug` configuration). Debug binaries include MSVC/GCC runtime checks, iterator validation, and disabled inlining — all of which slow down the compiler itself during every compilation. Rust's `rustc` is always built in release mode. A release build of `tml_compiler.dll` + `tml_codegen_x86.dll` would reduce the compiler's own execution overhead during LLVM IR generation and optimization passes. Combined with phase0l (DLL caching), a release compiler build targeting a 2-3x throughput improvement in the codegen phase itself, reducing the 27x compile gap further. See `docs/analysis/benchmark/08-compilation.md`.

## What Changes
1. `scripts/build.bat` will be extended to support a `--release` flag that passes `-DCMAKE_BUILD_TYPE=Release` to CMake.
2. The `CMakeLists.txt` Release configuration will ensure `/O2 /GL` (MSVC) or `-O2 -flto` (Zig CC/clang) is used.
3. The debug build (`scripts/build.bat`) remains unchanged — debug is still the default for development.
4. A CI step will be added to produce `build/release/` alongside `build/debug/`.
5. `tml.exe` will load from `build/release/` when invoked as `tml --compiler=release` (or when a release install is present).

## Impact
- Affected specs: compiler/build-system, compiler/dll-loading
- Affected code: `scripts/build.bat`, `CMakeLists.txt`, `compiler/src/plugin/plugin_loader.cpp`
- Breaking change: NO (debug build unchanged)
- User benefit: Faster compiler execution throughput — the compiler itself runs 2-3x faster in release mode, reducing codegen phase from ~50ms to ~15ms for small programs.
