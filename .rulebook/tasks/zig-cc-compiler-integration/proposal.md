# Proposal: zig-cc-compiler-integration

## Why
Replace MSVC (cl.exe) with zig cc/c++ as primary C/C++ compiler. User testing shows significant build performance gains. Zig CC wraps Clang with better defaults, faster compilation, no Visual Studio dependency, and trivial cross-compilation support. CMake remains the build system.

## What Changes
1. CMake toolchain file for Zig CC (`cmake/toolchains/zig.cmake`)
2. Wrapper scripts for CMake compatibility (`scripts/zig-cc.bat`, `scripts/zig-cxx.bat`)
3. `build.bat` accepts `--zig` flag, passes toolchain file to CMake
4. `CMakeLists.txt` handles Clang-compatible flags (Zig reports as Clang)
5. `compiler_setup.cpp` detects and prefers Zig CC for `.ll` → `.obj`
6. `object_compiler.cpp` routes to Zig CC when available
7. C runtime compilation uses Zig CC

## Impact
- Affected specs: none (internal build tooling only)
- Affected code: scripts/build.bat, CMakeLists.txt, compiler_setup.cpp, object_compiler.cpp
- Breaking change: NO (MSVC remains as fallback)
- User benefit: faster builds, no Visual Studio requirement, cross-compilation ready
