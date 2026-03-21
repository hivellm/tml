# Path selection logic: AST vs MIR determined by imports and generics
**Source**: manual
**Date**: 2026-03-15
**Related Task**: codegen-structural-fixes
**Tags**: codegen, path-selection, mir, legacy, architecture
At query_core.cpp:616-620, the compiler decides which codegen path to use: files with 'use' imports or local generics go through the AST/legacy path; standalone files without these use the MIR path. This means most real-world code (which has imports) still goes through legacy. The MIR path primarily handles simple standalone files. This is a critical architectural detail — MIR path coverage is narrower than expected.