# Tasks: Self-Contained TML Compiler

## 1. LLVM Backend Integration

- [ ] 1.1 Add LLVM library dependencies to CMakeLists.txt (LLVMCore, LLVMSupport, LLVMMC, LLVMCodeGen, LLVMX86CodeGen)
- [ ] 1.2 Create compiler/src/backend/llvm_backend.hpp interface
- [ ] 1.3 Implement LLVMBackend::compile_ir_to_object() in llvm_backend.cpp
- [ ] 1.4 Support optimization levels (-O0 through -O3)
- [ ] 1.5 Support debug info emission
- [ ] 1.6 Replace clang subprocess in object_compiler.cpp with LLVMBackend
- [ ] 1.7 Write unit tests for LLVM backend

## 2. LLD Linker Integration

- [ ] 2.1 Add LLD library dependencies to CMakeLists.txt (lldCOFF, lldELF, lldMachO, lldCommon)
- [ ] 2.2 Create compiler/src/backend/lld_linker.hpp interface
- [ ] 2.3 Implement LLDLinker::link_executable() for executables
- [ ] 2.4 Implement LLDLinker::link_shared_lib() for DLLs
- [ ] 2.5 Handle Windows CRT linking (ucrt, vcruntime, kernel32)
- [ ] 2.6 Replace external linker calls in builder code with LLDLinker
- [ ] 2.7 Write unit tests for LLD linker wrapper

## 3. Pre-compiled C Runtime

- [ ] 3.1 Add CMake custom commands to compile runtime/*.c at build time
- [ ] 3.2 Create tml_runtime.lib/tml_runtime.a static library from compiled objects
- [ ] 3.3 Bundle runtime library with compiler distribution
- [ ] 3.4 Update get_runtime_objects() to use pre-compiled objects
- [ ] 3.5 Remove ensure_c_compiled() runtime compilation
- [ ] 3.6 Test runtime works on clean machine without clang

## 4. CLI Command Independence

- [ ] 4.1 Audit `tml build` - remove all external tool dependencies
- [ ] 4.2 Audit `tml run` - ensure uses built-in backend for JIT/compilation
- [ ] 4.3 Audit `tml test` - remove clang/linker subprocess calls
- [ ] 4.4 Audit `tml lint` - ensure fully internal (no external linters)
- [ ] 4.5 Audit `tml format` - ensure fully internal (no external formatters)
- [ ] 4.6 Audit `tml debug` commands - remove external dependencies
- [ ] 4.7 Audit `tml pkg` commands - ensure self-contained

## 5. Complete Self-Containment

- [ ] 5.1 Remove all std::system() calls for compilation/linking across entire codebase
- [ ] 5.2 Remove find_clang() requirement (make optional for --use-external-tools flag)
- [ ] 5.3 Remove find_msvc() requirement for linking
- [ ] 5.4 Add --use-external-tools flag for fallback/debugging only
- [ ] 5.5 Update error messages for self-contained mode

## 6. Distribution and Documentation

- [ ] 6.1 Update build instructions for LLVM/LLD development requirements
- [ ] 6.2 Update user documentation (no external tools needed)
- [ ] 6.3 Create standalone Windows installer package
- [ ] 6.4 Create standalone Linux package
- [ ] 6.5 Verify fresh install works for ALL commands without prerequisites
- [ ] 6.6 Update CHANGELOG with self-contained compiler feature
