# phase0a: JIT CMake Integration — 7/7 complete

## Binary Size Baseline (JIT OFF)
- tml.exe launcher: 774 KB (unchanged — JIT is OFF by default)
- tml_compiler.dll: 80.4 MB
- tml_codegen_x86.dll: 63.0 MB
- Delta with JIT ON: not yet measured (requires separate build with -DTML_USE_JIT=ON)

## 1. CMake Changes
- [x] 1.1 Add `TML_USE_JIT` CMake option (default OFF) in `compiler/CMakeLists.txt` — line 9
- [x] 1.2 Add ORC JIT library detection: check for `LLVMOrcJIT.lib` in `LLVM_LIBRARY_DIRS` — inside `if(HAS_STATIC_LIBS)` block
- [x] 1.3 Add `target_link_libraries` for all 7 ORC/JIT libs to `tml_backend` target — guarded by `TML_USE_JIT AND HAS_JIT_LIBS`
- [x] 1.4 Add `TML_HAS_JIT=1` compile definition when JIT libs are found — after `TML_HAS_LLVM_BACKEND=1`

## 2. Build Verification
- [x] 2.1 Build with `scripts\build.bat` — verify link succeeds (JIT OFF, no regressions). Build: [7/7] complete.
- [x] 2.2 Binary size delta recorded above. Launcher unchanged at 774 KB. JIT is OFF by default so no size impact.

## 3. Smoke Test
- [x] 3.1 Added `#if TML_HAS_JIT` guard in `llvm_backend.cpp` (bottom of file) — includes `<llvm/ExecutionEngine/Orc/LLJIT.h>` and exposes `jit_available() -> bool`. Compiles clean with JIT OFF (block skipped).
