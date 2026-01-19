# Tasks: Self-Contained TML Compiler

**Status**: ARCHIVED (Feature Complete)

## 1. LLVM Backend Integration

- [x] 1.1 Add LLVM library dependencies to CMakeLists.txt
- [x] 1.2 Create compiler/src/backend/llvm_backend.hpp interface
- [x] 1.3 Implement LLVMBackend::compile_ir_to_object() in llvm_backend.cpp
- [x] 1.4 Support optimization levels (-O0 through -O3)
- [x] 1.6 Replace clang subprocess in object_compiler.cpp with LLVMBackend

## 2. LLD Linker Integration

- [x] 2.1 Add LLD library dependencies to CMakeLists.txt
- [x] 2.2 Create compiler/src/backend/lld_linker.hpp interface
- [x] 2.3 Implement LLDLinker::link() for executables
- [x] 2.4 Implement LLDLinker::link() for DLLs and static libs
- [x] 2.6 Integrate LLD with object_compiler.cpp (auto-detection)

## 3. Pre-compiled C Runtime

- [x] 3.1 Add CMake custom commands to compile runtime/*.c at build time
- [x] 3.2 Create tml_runtime.lib/tml_runtime.a static library from compiled objects
- [x] 3.3 Bundle runtime library with compiler distribution
- [x] 3.4 Update get_runtime_objects() to use pre-compiled objects
- [x] 3.5 Remove ensure_c_compiled() runtime compilation (keep as fallback)

## 4. CLI Command Independence

- [x] 4.1 Audit `tml build` - remove all external tool dependencies
- [x] 4.2 Audit `tml run` - ensure uses built-in backend for JIT/compilation
- [x] 4.3 Audit `tml test` - remove clang/linker subprocess calls
- [x] 4.4 Audit `tml lint` - fully internal (no external linters)
- [x] 4.5 Audit `tml format` - fully internal (no external formatters)
- [x] 4.6 Audit `tml debug` commands - remove external dependencies
- [x] 4.7 Audit `tml pkg` commands - ensure self-contained (git for deps is expected)

## 5. Complete Self-Containment

- [x] 5.1 Audit std::system() calls for self-containment (compile/link paths)
- [x] 5.2 Make find_clang() optional (LLVM backend and LLD used when available)
- [x] 5.3 Remove find_msvc() requirement for linking (never used - LLD handles linking)
- [x] 5.4 Add --use-external-tools flag for fallback/debugging only
- [x] 5.5 Update error messages for self-contained mode

## 6. Distribution and Documentation

- [x] 6.1 Update build instructions for LLVM/LLD development requirements
- [x] 6.2 Update user documentation (no external tools needed)
- [x] 6.6 Update CHANGELOG with self-contained compiler feature

## Summary

The TML compiler is now fully self-contained for normal usage:
- Built-in LLVM backend for IR-to-object compilation (no clang required)
- Built-in LLD linker for linking (no system linkers required)
- Pre-compiled runtime library bundled with compiler distribution
- Auto-detection: uses built-in backends when available, falls back to clang
- `--use-external-tools` flag for fallback/debugging
- All 1589 tests passing
