# Content-Addressable Object Cache

**Category**: architecture
**Tags**: codegen, cache, performance, llvm

## Description

Fingerprint LLVM IR strings with CRC32C hash to create content-addressable .obj cache. Identical IR from different source files (e.g., test files importing the same library) produces the same hash and reuses the same compiled object. Avoids redundant LLVM backend invocations.

## When to Use

When multiple compilation units produce identical intermediate representations. Applied in testing_compile.cpp lines 454-488.
