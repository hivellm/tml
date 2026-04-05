# TML Linker Analysis --- Executive Summary

## Purpose

This analysis was produced to inform TML strategy for eliminating the Zig CC dependency
and building a custom high-performance linker. The goal is maximum performance: fast
compilation, fast linking, and eventually incremental linking for sub-second iteration.

## What TML Has Today

TML already has a sophisticated two-layer backend:

1. LLVM C API backend (compiler/src/backend/llvm_backend.cpp): Compiles LLVM IR text
   to COFF .obj files in-process using LLVMParseIRInContext, LLVMCreateTargetMachine,
   LLVMRunPasses, and LLVMTargetMachineEmitToFile. No subprocess needed.

2. Embedded LLD linker (compiler/src/backend/lld_linker.cpp): Links .obj files to .exe
   and .dll in-process using lld::lldMain() with full COFF, ELF, MachO, MinGW, and Wasm
   drivers. LLD is vendored in src/llvm-project/ and linked statically. The integration
   includes mutex serialization, canRunAgain poisoning detection, and a 15-second deadlock
   timeout via a detached thread.

## Critical Finding: Zig CC Scope Is Narrow

Zig CC is only used to compile TML own C++ compiler source code. It is not used at
runtime to link TML programs. The zig-cc.bat and zig-cxx.bat wrappers translate to:

    zig cc  -target x86_64-windows-msvc [clang args]
    zig c++ -target x86_64-windows-msvc -nostdlib++ [clang args]

These wrappers exist because Zig bundles Clang 20 with portable MSVC-ABI support,
eliminating the need for a Visual Studio installation. The --clang build flag already
exists in scripts/build.bat as an alternative. Eliminating Zig CC is straightforward.

## Two-Track Strategy

### Track 1: Eliminate Zig CC (Low Risk, 1-2 Weeks)

Replace Zig CC with direct Clang or validate the existing --clang flag. The build system
already supports three compilers: Zig CC (default when available), Clang (--clang flag),
and MSVC (--msvc flag). Track 1 is just removing the Zig hard-dependency.

### Track 2: Custom PE/COFF Linker (High Reward, 12-20 Weeks)

Evolve TML embedded LLD usage toward a custom PE/COFF linker (tml-link). Phased effort:

- Phase 2 (2-4 wk): In-memory .obj passing --- eliminate temp file round-trip
- Phase 3 (4-8 wk): Custom PE/COFF writer --- replace LLD-COFF for TML output
- Phase 4 (4-8 wk): Incremental linking --- patch only changed functions
- Phase 5 (4-8 wk): ELF/Mach-O backends for Linux/macOS

## Performance Expectations

| Milestone              | Link Time          | vs Today  |
|------------------------|--------------------|-----------|
| Today (embedded LLD)   | ~100ms hello world | baseline  |
| Phase 2 (in-memory)    | ~70ms              | 1.4x      |
| Phase 3 (custom writer)| ~10ms              | 10x       |
| Phase 4 (incremental)  | ~5ms relink        | 20x       |

## Risk Assessment

- Track 1 (Zig CC removal): Low risk. Clang flag already exists.
- Phase 2 (in-memory obj): Medium risk. LLD MemoryBuffer API needs validation.
- Phase 3 (custom writer): High risk. Relocation correctness requires thorough testing.
- Phase 4 (incremental): High risk. In-place binary patching is subtle.
- Phase 5 (ELF/MachO): Medium risk. ELF is well-documented; Mach-O less so.

## File Map

| File                           | Contents |
|--------------------------------|----------|
| 01-zig-linker-architecture.md  | Deep dive into Zig self-hosted linker: File interface, incremental linking, PE/COFF and ELF backends, symbol resolution, performance characteristics |
| 02-lld-comparison.md           | LLD architecture, COFF backend details, TML current LLD integration, gap analysis vs custom linker |
| 03-tml-current-state.md        | Exact TML pipeline, Zig CC usage, LLD CMake integration, capability matrix |
| 04-custom-linker-design.md     | Proposed tml-link architecture, data structures, PE/COFF format, incremental design |
| 05-implementation-roadmap.md   | Phased plan with tasks, acceptance criteria, risk per phase |
| 06-performance-targets.md      | Benchmarks to beat, per-phase performance gains, profiling strategy |
