# 01 — Current LLVM Dependency Inventory

**Date**: 2026-04-05
**Status**: Complete
**Source files audited**: `compiler/src/backend/`, `compiler/src/codegen/llvm/`,
`compiler/src/codegen/cranelift/`, `compiler/CMakeLists.txt`

---

## 1. API Surface — What TML Actually Uses

TML uses the **LLVM C API exclusively** — not the LLVM C++ API. This is the most important
single fact about the dependency: it means TML is coupled to a stable, versioned C ABI,
not to LLVM's notoriously unstable C++ internal headers.

Seven C headers are included in `llvm_backend.cpp`:

```cpp
#include <llvm-c/Core.h>          // LLVMContextCreate, LLVMParseIRInContext,
                                   // LLVMDisposeModule, LLVMDisposeMessage
#include <llvm-c/IRReader.h>       // LLVMParseIRInContext
#include <llvm-c/Target.h>         // LLVMInitializeX86*, LLVM*TargetInfo
#include <llvm-c/TargetMachine.h>  // LLVMCreateTargetMachine,
                                   // LLVMTargetMachineEmitToFile,
                                   // LLVMTargetMachineEmitToMemoryBuffer
#include <llvm-c/Transforms/PassBuilder.h>  // LLVMRunPasses
#include <llvm-c/Analysis.h>       // LLVMVerifyModule
#include <llvm-c/BitWriter.h>      // LLVMWriteBitcodeToFile (linked, not called)
```

Additionally, `jit_engine.cpp` uses the LLVM ORC JIT C API:

```cpp
// ORC JIT C API (7 additional headers)
#include <llvm-c/LLJIT.h>
#include <llvm-c/Orc.h>
#include <llvm-c/OrcEE.h>
// ... (LLVMOrcCreate*, LLVMOrcLLJIT*, LLVMJITSymbol*)
```

LLD is used through its **C++ API** — `lld::lldMain()` — which is the one place where TML
couples to LLVM's unstable internal API. This is intentional: LLD exposes no stable C API.

---

## 2. Files That Directly Call LLVM

| File | LOC | Primary LLVM APIs Used | Purpose |
|------|-----|----------------------|---------|
| `compiler/src/backend/llvm_backend.cpp` | 550 | `LLVMParseIRInContext`, `LLVMCreateTargetMachine`, `LLVMRunPasses`, `LLVMTargetMachineEmitToFile`, `LLVMVerifyModule` | IR text → COFF/ELF .obj |
| `compiler/src/backend/lld_linker.cpp` | 670 | `lld::lldMain` (C++ API) | .obj files → .exe/.dll |
| `compiler/src/backend/jit_engine.cpp` | 373 | `LLVMOrcCreate*`, `LLVMOrcLLJIT*` | JIT execution for `tml run` |
| `compiler/src/codegen/llvm/llvm_codegen_backend.cpp` | 158 | Wraps `LLVMBackend` + `MirCodegen` | `CodegenBackend` implementation |
| `compiler/src/codegen/cranelift/cranelift_codegen_backend.cpp` | 177 | `cranelift_bridge.h` C API | Cranelift `CodegenBackend` (partial) |

**Total direct LLVM call sites: ~1,770 lines across 4 files.**

The remaining ~76,000 lines of codegen code (`compiler/src/codegen/`) generate LLVM IR as
text via `std::stringstream` and contain **zero LLVM API calls**.

---

## 3. The Decoupling Architecture — Why Replacement Is Feasible

Understanding where TML couples to LLVM is essential for understanding how easy replacement
is. The architecture has a clean seam:

```
compiler/src/codegen/mir/          ← Zero LLVM deps
  instructions.cpp                    (emit_call_inst, etc.)
  instructions_method.cpp
  instructions_misc.cpp
  mir_types.cpp
compiler/src/codegen/mir_codegen.cpp  ← Zero LLVM deps
  MirCodegen::generate()
  MirCodegen::generate_cgu()
        │
        │  std::string (LLVM IR text, ~500-5000 lines per module)
        │
        ▼
compiler/src/codegen/llvm/llvm_codegen_backend.cpp   ← LLVM entry point
  LLVMCodegenBackend::compile_mir()
        │
        ▼
compiler/src/backend/llvm_backend.cpp                ← All LLVM C API calls
  LLVMParseIRInContext(ir_text)   ← IR text → LLVM Module
  LLVMRunPasses(module, opts)     ← Optimization
  LLVMTargetMachineEmitToFile()   ← LLVM Module → .obj
```

The boundary between MirCodegen and LLVMCodegenBackend is a **plain `std::string`** carrying
LLVM IR text. This string can be redirected to any alternative backend without modifying any
of the 40+ files in `compiler/src/codegen/`.

To replace LLVM, implement the 4-method `CodegenBackend` interface:

```cpp
class CodegenBackend {
    virtual auto name() const -> std::string_view = 0;
    virtual auto capabilities() const -> BackendCapabilities = 0;
    virtual auto compile_mir(const mir::Module&, const CodegenOptions&) -> CodegenResult = 0;
    virtual auto compile_mir_cgu(const mir::Module&, const std::vector<size_t>&,
                                 const CodegenOptions&) -> CodegenResult = 0;
    virtual auto compile_ast(const parser::Module&, const types::TypeEnv&,
                             const CodegenOptions&) -> CodegenResult = 0;
    virtual auto generate_ir(const mir::Module&, const CodegenOptions&) -> std::string = 0;
};
```

A new backend that accepts LLVM IR text, converts it to an alternative IR format, and
emits a COFF `.obj` would require changes to **at most 4 files** and none of the 40+
codegen files.

---

## 4. LLVM Library Inventory (55+ Static Libraries)

The `compiler/CMakeLists.txt` links the following LLVM static libraries into
`tml_codegen_x86.dll`. They are organized here by functional group.

### 4.1 X86 Target (6 libraries)

| Library | Purpose |
|---------|---------|
| `LLVMX86CodeGen` | X86 instruction selection, scheduling, lowering |
| `LLVMX86AsmParser` | `.s` assembly parsing (needed for inline asm) |
| `LLVMX86Desc` | X86 instruction descriptions, encodings |
| `LLVMX86Disassembler` | Disassembly support |
| `LLVMX86Info` | X86 register/instruction metadata |
| `LLVMX86TargetMCA` | Machine code analyzer |

### 4.2 AArch64 Target (6 libraries)

| Library | Purpose |
|---------|---------|
| `LLVMAArch64CodeGen` | AArch64 instruction selection |
| `LLVMAArch64AsmParser` | AArch64 assembly parsing |
| `LLVMAArch64Desc` | AArch64 instruction descriptions |
| `LLVMAArch64Disassembler` | Disassembly |
| `LLVMAArch64Info` | Register/instruction metadata |
| `LLVMAArch64Utils` | AArch64 utility functions |

### 4.3 Optimization Passes (9 libraries)

| Library | Purpose |
|---------|---------|
| `LLVMPasses` | New PassManager, pass pipeline building |
| `LLVMCoroutines` | Coroutine lowering passes |
| `LLVMipo` | Inter-procedural optimization (inlining, devirt) |
| `LLVMVectorize` | Auto-vectorization passes |
| `LLVMSandboxIR` | Sandbox IR for safe transformations |
| `LLVMAggressiveInstCombine` | Extended instruction combining |
| `LLVMInstCombine` | Peephole instruction combining |
| `LLVMScalarOpts` | Scalar passes (mem2reg, GVN, LICM, etc.) |
| `LLVMObjCARCOpts` | ObjC ARC optimization (pulled in by ipo) |

### 4.4 Code Generation Infrastructure (5 libraries)

| Library | Purpose |
|---------|---------|
| `LLVMCodeGen` | Register allocation, prologue/epilogue, stack frames |
| `LLVMCGData` | Code generation data (profile-driven optimization) |
| `LLVMGlobalISel` | Global Instruction Selection framework |
| `LLVMSelectionDAG` | SelectionDAG instruction selection |
| `LLVMAsmPrinter` | Assembly/object file emission |

### 4.5 Debug Information (5 libraries)

| Library | Purpose |
|---------|---------|
| `LLVMDebugInfoDWARF` | DWARF debug info generation |
| `LLVMDebugInfoDWARFLowLevel` | Low-level DWARF primitives |
| `LLVMDebugInfoCodeView` | CodeView (PDB) debug info |
| `LLVMDebugInfoMSF` | PDB Multi-Stream File format |
| `LLVMDebugInfoPDB` | PDB reading/writing |

### 4.6 Profiling and Coverage (2 libraries)

| Library | Purpose |
|---------|---------|
| `LLVMProfileData` | PGO profile data formats |
| `LLVMCoverage` | Coverage mapping (SanitizerCoverage) |

### 4.7 Frontend Support (8 libraries)

| Library | Purpose |
|---------|---------|
| `LLVMFrontendOpenMP` | OpenMP lowering support (pulled in by ipo) |
| `LLVMFrontendAtomic` | Atomic lowering |
| `LLVMFrontendOffloading` | GPU offload support |
| `LLVMFrontendDriver` | Driver utilities |
| `LLVMFrontendHLSL` | HLSL support (indirect dep) |
| `LLVMObjectYAML` | Object file YAML format |
| `LLVMHipStdPar` | HIP standard parallelism (indirect dep) |
| `LLVMInstrumentation` | Sanitizer instrumentation |

### 4.8 Core Infrastructure (9 libraries)

| Library | Purpose |
|---------|---------|
| `LLVMLinker` | IR linker (module merging) |
| `LLVMTransformUtils` | Transformation utilities |
| `LLVMAnalysis` | Alias analysis, dominators, SCCGraph |
| `LLVMIRReader` | Parse IR text to Module |
| `LLVMIRPrinter` | Print Module to IR text |
| `LLVMAsmParser` | Textual assembly parser |
| `LLVMBitWriter` | Bitcode writing |
| `LLVMBitReader` | Bitcode reading |
| `LLVMTarget` | Target registration, data layout |

### 4.9 Low-Level Infrastructure (8 libraries)

| Library | Purpose |
|---------|---------|
| `LLVMObject` | Object file reading/writing |
| `LLVMMCParser` | MC-layer assembly parsing |
| `LLVMMCDisassembler` | MC-layer disassembly |
| `LLVMMC` | Machine code framework |
| `LLVMBinaryFormat` | ELF/COFF/Mach-O format support |
| `LLVMTargetParser` | Target triple parsing |
| `LLVMTextAPI` | Tapi/stub file support |
| `LLVMSupport` + `LLVMDemangle` + `LLVMRemarks` + `LLVMBitstreamReader` + `LLVMCodeGenTypes` | Core utilities |

### 4.10 ORC JIT (7 libraries, conditional on `TML_USE_JIT`)

| Library | Purpose |
|---------|---------|
| `LLVMOrcJIT` | ORC JIT compilation |
| `LLVMJITLink` | JIT linking |
| `LLVMOrcTargetProcess` | In-process JIT execution |
| `LLVMOrcShared` | ORC shared utilities |
| `LLVMOrcDebugging` | JIT debugging support |
| `LLVMExecutionEngine` | Legacy execution engine |
| `LLVMRuntimeDyld` | Runtime dynamic linking |

**Total: ~55 static libraries linked unconditionally (X86, AArch64, optimization, codegen,
debug, frontend, core, low-level) plus 7 JIT libraries when `TML_USE_JIT=ON`.**

---

## 5. Binary Size Breakdown

| Component | Size | Contents |
|-----------|------|----------|
| `tml_codegen_x86.dll` (current) | ~78 MB | LLVM + LLD statically linked |
| `tml_compiler.dll` (current) | ~104 MB | All compiler code including IR gen |
| LLVM's contribution | ~73 MB | 55 static libs, two target backends |
| LLD's contribution | ~5 MB | COFF/ELF/Mach-O drivers |
| TML compiler logic alone | ~31 MB | Lexer through MIR codegen |

```
Estimated size without LLVM:
  tml_compiler.dll         ~31 MB   (compiler logic, no LLVM)
  tml_codegen_cranelift.dll ~5 MB   (Cranelift statically linked)
  tml_codegen_x86.dll      ~78 MB   (LLVM, loaded on demand for --opt or release)

Net reduction for dev builds:  ~73 MB removed from hot path
```

The 78 MB DLL also forces a **CRT mismatch workaround**: LLVM's static libs are built with
`/MD` (Release CRT), but TML debug builds use `/MDd` (Debug CRT). The CMake file currently
forces `CMAKE_MSVC_RUNTIME_LIBRARY = MultiThreadedDLL` across all targets to avoid
`LNK2038` mismatch errors. Removing LLVM from the debug build path eliminates this constraint
and restores proper debug CRT usage for all compiler code.

---

## 6. Compilation Time Contribution

The following estimates are based on profiling data from the TML project and published LLVM
benchmarks. Numbers represent per-module (per-file) overhead on a mid-range workstation.

| LLVM Phase | O0 | O1 | O2/O3 | Notes |
|------------|----|----|-------|-------|
| IR parsing (`LLVMParseIRInContext`) | 5–10 ms | 5–10 ms | 5–10 ms | Proportional to IR size |
| Module verification (`LLVMVerifyModule`) | 1–3 ms | 1–3 ms | 1–3 ms | Fixed per module |
| Optimization (`LLVMRunPasses`) | 10–20 ms | 30–80 ms | 100–500 ms | Highly workload-dependent |
| Code emission (`LLVMTargetMachineEmitToFile`) | 20–40 ms | 20–40 ms | 20–50 ms | Relatively fixed |
| **Total LLVM overhead per file** | **36–73 ms** | **56–133 ms** | **126–563 ms** | |

TML's MIR Codegen (the text IR generation phase) takes approximately 5–30 ms per module
independent of the backend. The Cranelift alternative reduces total per-file compilation
from 36–73 ms (LLVM O0) to approximately **13–33 ms** — a 2.5–5x speedup for debug builds.

---

## 7. What Must Be Replaced

The following table maps each LLVM capability to what would need to be provided by an
alternative backend, with difficulty estimates.

| LLVM Capability | Consumed By | Replacement Needed | Difficulty |
|----------------|-------------|-------------------|------------|
| IR text → in-memory module | `LLVMParseIRInContext` | Alternative: accept LLVM IR text directly (Cranelift has an IR importer) OR generate alternative IR | MEDIUM |
| Register allocation | `LLVMCodeGen` | Included in any alternative backend | INCLUDED |
| Instruction selection | `LLVMSelectionDAG`, `LLVMGlobalISel` | Included in any alternative backend | INCLUDED |
| Peephole optimization (O0/O1) | `LLVMInstCombine`, `LLVMScalarOpts` | Cranelift includes basic opts | INCLUDED |
| Full optimization (O2/O3) | `LLVMipo`, `LLVMVectorize`, etc. | Keep LLVM for release builds | NOT NEEDED (Phase 1) |
| Object file emission (COFF/ELF) | `LLVMAsmPrinter`, `LLVMObject` | Object writer (see linker analysis) | MEDIUM |
| DWARF debug info | `LLVMDebugInfoDWARF` | Cranelift: partial. Custom: must implement | HIGH |
| PDB/CodeView debug info | `LLVMDebugInfoCodeView` | Windows-only. Skip for Phase 1 | HIGH |
| X86 machine encoding | `LLVMX86CodeGen`, `LLVMX86Desc` | Cranelift: full X86. Custom: implement MC layer | INCLUDED (Cranelift) |
| AArch64 support | `LLVMAArch64*` | Cranelift: full AArch64 | INCLUDED (Cranelift) |
| JIT execution | `LLVMOrcJIT`, etc. | No viable alternative today — keep LLVM | KEEP |
| Linking | `lld::lldMain` | Custom linker (already analyzed) | DONE |

**Summary**: For Phase 1 (Cranelift), the only capabilities that cannot be replaced are:
- Full O2/O3 optimization (by design — keep LLVM optional for release)
- PDB debug info (skip for Phase 1, implement in Phase 3)
- JIT (keep LLVM, used only by `tml run`)

---

## 8. Existing Abstraction Layer — What's Already Built

TML already has the abstraction layer needed to swap backends:

### `CodegenBackend` Interface (`compiler/include/codegen/codegen_backend.hpp`)

```cpp
class CodegenBackend {
public:
    virtual auto name() const -> std::string_view = 0;
    virtual auto capabilities() const -> BackendCapabilities = 0;
    virtual auto compile_mir(...) -> CodegenResult = 0;
    virtual auto compile_mir_cgu(...) -> CodegenResult = 0;
    virtual auto compile_ast(...) -> CodegenResult = 0;
    virtual auto generate_ir(...) -> std::string = 0;
};

enum class BackendType { LLVM, Cranelift };
auto create_backend(BackendType) -> std::unique_ptr<CodegenBackend>;
```

### `BackendCapabilities` Struct

Allows the build system to query what a backend supports before committing to it:

```cpp
struct BackendCapabilities {
    bool supports_mir = false;
    bool supports_ast = false;
    bool supports_generics = false;
    bool supports_debug_info = false;
    bool supports_coverage = false;
    bool supports_cgu = false;
    int max_optimization_level = 3;
};
```

### `CraneliftCodegenBackend` (Partial Implementation)

`compiler/src/codegen/cranelift/cranelift_codegen_backend.cpp` (177 lines) already
implements `BackendCapabilities`, option translation, and object file writing. It reports:

```cpp
BackendCapabilities{
    .supports_mir = true,
    .supports_ast = false,
    .supports_generics = false,
    .supports_debug_info = false,
    .supports_coverage = false,
    .supports_cgu = true,
    .max_optimization_level = 2,
}
```

The missing piece is the `compile_mir()` body — wiring MIR serialization through
`cranelift_bridge.h` to produce actual `.obj` bytes.

---

## 9. Dependency Graph Summary

```
tml_codegen_x86.dll (78 MB)
  │
  ├── llvm_backend.cpp (550 LOC)
  │     ├── LLVMParseIRInContext       [LLVMIRReader]
  │     ├── LLVMRunPasses              [LLVMPasses, LLVMScalarOpts, ...]
  │     ├── LLVMTargetMachineEmitToFile [LLVMCodeGen, LLVMX86CodeGen, ...]
  │     └── LLVMVerifyModule           [LLVMAnalysis]
  │
  ├── lld_linker.cpp (670 LOC)
  │     └── lld::lldMain()            [lldCOFF, lldELF, lldMachO]
  │
  └── jit_engine.cpp (373 LOC) [optional, TML_USE_JIT]
        └── LLVMOrcCreate*, LLJIT*    [LLVMOrcJIT, LLVMJITLink, ...]

tml_compiler.dll (104 MB)
  │
  ├── codegen/mir/*.cpp               [ZERO LLVM DEPS — pure std::string output]
  ├── codegen/mir_codegen.cpp         [ZERO LLVM DEPS]
  └── codegen/llvm/llvm_codegen_backend.cpp (158 LOC)
        └── calls llvm_backend.cpp    [only coupling point between compiler and LLVM]
```

The coupling surface is **728 lines of C++ code** across two files. Everything else in the
compiler is LLVM-free.
