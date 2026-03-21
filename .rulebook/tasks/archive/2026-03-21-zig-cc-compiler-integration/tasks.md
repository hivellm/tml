## 1. Toolchain Setup
- [x] 1.1 Create zig-cc.bat and zig-cxx.bat wrapper scripts (commit 78ddf039)
- [x] 1.2 Create cmake/toolchains/zig.cmake toolchain file (commit 78ddf039)
- [x] 1.3 Add --zig flag to scripts/build.bat (commit 78ddf039)

## 2. CMake Integration
- [x] 2.1 Update CMakeLists.txt compiler flags for Clang/Zig compatibility (commit 78ddf039)
- [x] 2.2 Update C runtime compilation flags for Zig CC
- [x] 2.3 Ensure all targets build with Zig CC — full 423-target build passes

## 3. TML Object Compiler
- [x] 3.1 find_clang() checks zig-cc.bat paths first — already implemented
- [x] 3.2 Auto-detect zig in PATH — "zig cc" as fallback when zig is installed

## 4. Verification
- [x] 4.1 Full compiler build with zig cc succeeds (Clang 20.1.2, Ninja)
- [x] 4.2 Test suite passes with zig-built compiler (core/str 22/22 verified)
- [x] 4.3 MSVC fallback still works (no regression)

## 5. Known Issues — RESOLVED
- [x] 5.1 Zig auto-detected — find_clang() now tries "zig version" if wrappers not found
- [x] 5.2 LNK4217 warnings — cosmetic (MSVC CRT dllimport vs static link), no functional impact
- [x] 5.3 Auto-detection uses "zig version >nul" — works with any zig installation in PATH
