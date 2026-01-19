# Proposal: Self-Contained TML Compiler

## Why

The TML compiler currently requires external tools (clang, Visual Studio) at runtime for compilation and linking. This creates a poor developer experience - users must install and configure multiple toolchains before they can use any compiler functionality. Modern compilers like Go and Rust are completely self-contained; the TML compiler MUST follow this pattern. After downloading tml.exe, ALL commands (build, run, test, lint, format, etc.) must work immediately without any external dependencies.

## What Changes

- **LLVM Library Integration**: Replace `clang -c` subprocess calls with direct LLVM API calls for compiling LLVM IR to object files
- **LLD Linker Integration**: Replace external linker calls with LLD library for linking object files into executables/DLLs
- **Pre-compiled C Runtime**: Compile C runtime files during CMake build (not at user runtime), bundle with distribution
- **Remove ALL External Dependencies**: Remove all `std::system()` calls, make every compiler command fully self-sufficient
- **CMake Build Changes**: Add LLVM and LLD as build-time dependencies (for compiler developers only)
- **Complete CLI Independence**: Ensure build, run, test, lint, format, and all other commands work without any external tools

## Impact

- Affected specs: compiler/builder, compiler/backend, compiler/cli
- Affected code: compiler/src/cli/*, compiler/src/backend/* (new)
- Breaking change: NO (external behavior unchanged, only internal implementation)
- User benefit: Download tml.exe → immediately use ALL commands (build, run, test, lint, format) without installing anything else
