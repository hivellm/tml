# phase0a: JIT CMake Integration — 0/7 complete

## 1. CMake Changes
- [ ] 1.1 Add `TML_USE_JIT` CMake option (default OFF) in `compiler/CMakeLists.txt`
- [ ] 1.2 Add ORC JIT library detection: check for `LLVMOrcJIT.lib` in `LLVM_LIBRARY_DIRS`
- [ ] 1.3 Add `target_link_libraries` for all 7 ORC/JIT libs to `tml_backend` target
- [ ] 1.4 Add `TML_HAS_JIT=1` compile definition when JIT libs are found

## 2. Build Verification
- [ ] 2.1 Build with `scripts\build.bat` — verify link succeeds with ORC libs
- [ ] 2.2 Measure binary size delta (record before/after in this file)

## 3. Smoke Test
- [ ] 3.1 Add minimal `#if TML_HAS_JIT` guard in `llvm_backend.cpp` that logs "JIT available" at startup (verbose mode only)
