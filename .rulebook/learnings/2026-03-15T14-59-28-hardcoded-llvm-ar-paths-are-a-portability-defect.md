# Hardcoded llvm-ar paths are a portability defect
**Source**: manual
**Date**: 2026-03-15
**Tags**: build, portability, tech-debt
build_runtime_archive() in testing_compile.cpp (lines 244-250) includes hardcoded developer-specific absolute paths like F:/LLVM/bin/llvm-ar.exe for the archiver search. This breaks portability for other developers. Should use PATH-based discovery or the Zig CC bundled archiver instead.