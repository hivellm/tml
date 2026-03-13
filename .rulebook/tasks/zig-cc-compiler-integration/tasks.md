## 1. Toolchain Setup
- [x] 1.1 Create zig-cc.bat and zig-cxx.bat wrapper scripts (commit 78ddf039)
- [x] 1.2 Create cmake/toolchains/zig.cmake toolchain file (commit 78ddf039)
- [x] 1.3 Add --zig flag to scripts/build.bat (commit 78ddf039)

## 2. CMake Integration
- [x] 2.1 Update CMakeLists.txt compiler flags for Clang/Zig compatibility (commit 78ddf039)
- [x] 2.2 Update C runtime compilation flags for Zig CC
- [x] 2.3 Ensure all targets build with Zig CC — full 423-target build passes

## 3. TML Object Compiler
- [ ] 3.1 Add zig cc detection in compiler_setup.cpp
- [ ] 3.2 Update object_compiler.cpp to use zig cc for .ll compilation

## 4. Verification
- [x] 4.1 Full compiler build with zig cc succeeds (Clang 20.1.2, Ninja)
- [ ] 4.2 Full test suite passes with zig-built compiler
- [x] 4.3 MSVC fallback still works (no regression)

## 5. Known Issues
- [ ] 5.1 Zig not auto-detected — requires manual PATH setup or `--zig` flag with zig in PATH
- [ ] 5.2 LNK4217 warnings: locally defined symbol imported (ucrt functions via loader.cpp)
- [ ] 5.3 Auto-detection (`USE_ZIG_CC=auto`) uses `where zig.exe` — fails if zig installed via winget without PATH
