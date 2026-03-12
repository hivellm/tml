## 1. Toolchain Setup
- [ ] 1.1 Create zig-cc.bat and zig-cxx.bat wrapper scripts
- [ ] 1.2 Create cmake/toolchains/zig.cmake toolchain file
- [ ] 1.3 Add --zig flag to scripts/build.bat

## 2. CMake Integration
- [ ] 2.1 Update CMakeLists.txt compiler flags for Clang/Zig compatibility
- [ ] 2.2 Update C runtime compilation flags for Zig CC
- [ ] 2.3 Ensure all targets build with Zig CC

## 3. TML Object Compiler
- [ ] 3.1 Add zig cc detection in compiler_setup.cpp
- [ ] 3.2 Update object_compiler.cpp to use zig cc for .ll compilation

## 4. Verification
- [ ] 4.1 Full compiler build with zig cc succeeds
- [ ] 4.2 Full test suite passes with zig-built compiler
- [ ] 4.3 MSVC fallback still works (no regression)
