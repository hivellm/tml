# Proposal: phase0a_jit-cmake-integration

## Why
TML needs a JIT execution mode to enable scripting and faster iteration. The first step is linking the LLVM ORC JIT libraries that already exist in `src/llvm-install/lib/` but are not currently linked into the compiler. Without these libraries, no JIT functionality can be implemented.

## What Changes
- Add ORC JIT libraries to `compiler/CMakeLists.txt` target link list
- Add conditional compilation flag `TML_HAS_JIT=1`
- Verify the build succeeds with the additional libraries on Windows
- Measure binary size impact (expected +15-25MB)

## Libraries to Add
| Library | Size | Purpose |
|---------|------|---------|
| `LLVMOrcJIT.lib` | 21.1 MB | Core ORC framework (LLJIT, ExecutionSession) |
| `LLVMJITLink.lib` | 9.8 MB | In-memory object linking |
| `LLVMOrcTargetProcess.lib` | 2.2 MB | In-process execution support |
| `LLVMOrcShared.lib` | 0.3 MB | Shared utilities |
| `LLVMOrcDebugging.lib` | 1.5 MB | Debug info for JIT'd code |
| `LLVMExecutionEngine.lib` | 0.6 MB | Base execution engine |
| `LLVMRuntimeDyld.lib` | 2.3 MB | Dynamic loader (ORC dependency) |

All libraries already exist at `src/llvm-install/lib/`. Zero new compilation needed.

## Impact
- Affected specs: none (build system only)
- Affected code: `compiler/CMakeLists.txt`
- Breaking change: NO
- User benefit: Foundation for JIT execution mode

## Reference
- Analysis: `docs/analyses/jit-execution-analysis.md`
- LLVM version: 23.0.0 (trunk)
- Current LLVM libs linked: 70+ static libraries
