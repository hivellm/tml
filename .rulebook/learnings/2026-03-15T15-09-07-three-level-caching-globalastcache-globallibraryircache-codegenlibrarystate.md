# Three-level caching: GlobalASTCache + GlobalLibraryIRCache + CodegenLibraryState
**Source**: manual
**Date**: 2026-03-15
**Tags**: codegen, caching, library-ir, performance
Library IR caching uses three layers: (1) GlobalASTCache stores pre-parsed ASTs to avoid re-parsing library sources. (2) GlobalLibraryIRCache stores pre-generated LLVM IR text for library functions. (3) CodegenLibraryState captures the full registry snapshot (type info, function signatures, generic instances) so test suite compilations can restore state without re-running library codegen. This is critical for test performance — without it, each of 200+ test suites would regenerate all library IR.