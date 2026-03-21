# 7. Zig CC as Preferred Compiler Toolchain

**Status**: proposed
**Date**: 2026-03-15
**Related Tasks**: zig-cc-compiler-integration

## Context

Build speed on Windows is critical. The ~240K line C++ codebase takes significant time with MSVC. Zig bundles Clang 20 and works with Ninja for parallel compilation.

## Decision

Auto-detect Zig CC when zig.exe and ninja.exe are in PATH. Fall back to MSVC. Build scripts (zig-cc.bat, zig-cxx.bat) wrap Zig as C/C++ compiler. All UBSan checks disabled (LLVM internals trigger false positives). Build guard in CMakeLists.txt prevents direct cmake usage — only build.bat passes required token.

## Alternatives Considered

- MSVC only (slower builds)
- Clang/LLVM standalone (extra dependency management)
- MinGW/GCC (ABI compatibility concerns on Windows)

## Consequences

Faster builds via Ninja parallelism. UBSan disabled means sanitizer coverage gap. MSVC fallback must remain functional for environments without Zig. Non-standard toolchain may surprise contributors.
