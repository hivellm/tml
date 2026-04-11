# TML Toolchain Analysis — Self-Contained Compiler & Linker

## Purpose

This analysis was produced to inform TML strategy for building a **complete, self-contained
toolchain** that eliminates ALL external dependencies — Zig CC, system Clang, MSVC, and
any separately installed linker. TML ships its own compiler, C/C++ frontend, linker, and
archiver. Users download one package and have everything needed to build TML programs and
the TML compiler itself.

## Two Use Cases

### Use Case 1: Building the TML Compiler from C++ Source

The TML compiler (`tml.exe`) is written in C++. Today it requires Zig CC (a Clang 20
wrapper) to compile ~100,000 lines of C++. The toolchain replaces this:

- `tml-cc.exe` compiles all C runtime objects (`compiler/runtime/*.c`)
- `tml-cxx.exe` compiles the compiler C++ source (`compiler/src/**/*.cpp`)
- `tml-link.exe` links the resulting objects into `tml.exe` and `tml_compiler.dll`

CMake integration via `cmake/tml-toolchain.cmake` makes this a drop-in replacement for
the current Zig CC build path.

### Use Case 2: Linking TML Programs at Compile Time

When a user writes a `.tml` file and runs `tml build`, the compiler:

1. Runs the query pipeline (Lexer → Parser → TypeChecker → HIR → MIR → LLVM IR)
2. Calls `LLVMTargetMachineEmitToMemoryBuffer` — produces COFF/ELF object bytes in memory
3. Links the in-memory object with C runtime objects using `tml-link`
4. Produces `program.exe` (Windows) or `program` (Linux)

Today step 3 uses embedded LLD (`lld::lldMain()`). The toolchain replaces this with
`tml-link` — a custom linker that is faster, purpose-built for TML's output, and supports
incremental relinking in under 5ms.

## Toolchain Components

| Binary | Purpose | Wraps / Implements |
|--------|---------|-------------------|
| `tml.exe` | TML compiler | Query pipeline + LLVM backend |
| `tml-cc.exe` | C compiler | Bundled Clang 20 with TML sysroot |
| `tml-cxx.exe` | C++ compiler | Bundled Clang 20 with C++ stdlib headers |
| `tml-link.exe` | Linker | Custom PE/COFF, ELF, Mach-O writer |
| `tml-ar.exe` | Archiver | COFF/ELF archive creation |

All five binaries ship together. No Visual Studio, no Zig, no system Clang required.

## What TML Has Today

TML already has a sophisticated two-layer backend:

1. **LLVM C API backend** (`compiler/src/backend/llvm_backend.cpp`): Compiles LLVM IR
   text to COFF `.obj` files in-process using `LLVMParseIRInContext`,
   `LLVMCreateTargetMachine`, `LLVMRunPasses`, and `LLVMTargetMachineEmitToFile`.
   `compile_ir_to_buffer()` also exists for in-memory output.

2. **Embedded LLD linker** (`compiler/src/backend/lld_linker.cpp`): Links `.obj` files
   to `.exe` and `.dll` in-process using `lld::lldMain()` with full COFF, ELF, MachO,
   MinGW, and Wasm drivers. LLD is vendored in `src/llvm-project/` and linked statically.
   The integration includes mutex serialization, canRunAgain poisoning detection, and a
   15-second deadlock timeout via a detached thread.

## Critical Finding: Zig CC Scope Is Narrow

Zig CC is only used to compile TML's own C++ compiler source. It is not used at runtime
to link TML programs. The `zig-cc.bat` and `zig-cxx.bat` wrappers translate to:

```
zig cc  -target x86_64-windows-msvc [clang args]
zig c++ -target x86_64-windows-msvc -nostdlib++ [clang args]
```

These wrappers exist because Zig bundles Clang 20 with portable MSVC-ABI support,
eliminating the need for a Visual Studio installation. `tml-cxx.exe` (bundling Clang
as a library) provides the same capability without requiring Zig.

## Strategy: Three Phases Toward Full Self-Containment

### Phase 1: Eliminate Zig CC Dependency (1-2 weeks) — Low Risk

Validate and promote the existing `--clang` flag in `scripts/build.bat` as the primary
non-Zig build path. This removes the hard dependency on `zig.exe` immediately, with no
impact on TML program linking.

### Phase 1b: Bundle Clang as tml-cc / tml-cxx (2-4 weeks) — Medium Risk

Package Clang 20 as `tml-cc.exe` and `tml-cxx.exe` — eliminating the dependency on any
externally installed compiler. TML already links against LLVM; adding the Clang frontend
library is incremental. Bundle UCRT and libc++ headers for zero-dependency compilation.

### Phase 2: In-Memory Object Passing (2-4 weeks) — Medium Risk

Wire `compile_ir_to_buffer()` directly into LLD, eliminating the temp file round-trip
for TML-generated objects. Performance target: 30% link-time reduction.

### Phase 3: Custom PE/COFF Linker — tml-link (4-8 weeks) — High Risk/Reward

Replace embedded LLD with a purpose-built PE/COFF linker for TML program output.
Performance target: 10ms link time (10x vs today's ~100ms). The linker handles the full
scope of C and C++ linking including exception tables, COMDAT, TLS, and static
constructors — everything needed to link both TML programs and the TML compiler itself.

### Phase 4: Incremental Linking (4-8 weeks) — High Risk/Reward

Sub-5ms relinking for single-function changes during iterative development.

### Phase 5: Cross-Platform (ELF/Mach-O) (4-8 weeks) — Medium/High Risk

Linux and macOS backends reusing Phase 3 symbol resolution and section layout logic.

### Phase 6: Advanced Features — PDB, LTO, PGO (ongoing)

Debug info generation, link-time optimization, and profile-guided optimization.

## Self-Bootstrapping Path

```
Stage 1 (current): Zig CC compiles C++  →  embedded LLD links TML programs
Stage 2 (Phase 1): system Clang OR tml-cc compiles C++  →  embedded LLD
Stage 3 (Phase 3): tml-cc compiles C++  →  tml-link links everything
Stage 4 (future):  TML compiler written in TML  →  tml-link links tml.exe itself
```

In Stage 4, the only C++ remaining is the LLVM backend. The compiler front-end,
type system, MIR pipeline, and standard library are all pure TML.

## Performance Expectations

| Milestone                     | Link Time (hello world) | vs Today |
|-------------------------------|------------------------|----------|
| Today (embedded LLD)          | ~100ms                 | baseline |
| Phase 2 (in-memory)           | ~70ms                  | 1.4x     |
| Phase 3 (tml-link, cold)      | ~10ms                  | 10x      |
| Phase 4 (incremental, 1 fn)   | ~5ms relink            | 20x      |

## Risk Assessment

| Phase | Risk | Notes |
|-------|------|-------|
| Phase 1 (Zig removal) | LOW | `--clang` flag already exists |
| Phase 1b (tml-cc bundle) | MEDIUM | Clang library API, header licensing |
| Phase 2 (in-memory obj) | MEDIUM | LLD MemoryBuffer API needs validation |
| Phase 3 (custom linker) | HIGH | Relocation correctness requires thorough testing |
| Phase 4 (incremental) | HIGH | In-place binary patching is subtle |
| Phase 5 (ELF/Mach-O) | MEDIUM/HIGH | ELF documented; Mach-O less so |

## File Map

| File                           | Contents |
|--------------------------------|----------|
| 01-zig-linker-architecture.md  | Deep dive into Zig self-hosted linker: File interface, incremental linking, PE/COFF and ELF backends, symbol resolution, performance characteristics |
| 02-lld-comparison.md           | LLD architecture, COFF backend details, TML current LLD integration, gap analysis vs custom linker |
| 03-tml-current-state.md        | Exact TML pipeline, Zig CC usage, LLD CMake integration, capability matrix |
| 04-custom-linker-design.md     | Proposed tml-link architecture and full toolchain design (tml-cc, tml-cxx, tml-link, tml-ar), CMake integration, PE/COFF format, incremental design |
| 05-implementation-roadmap.md   | Phased plan with tasks, acceptance criteria, risk per phase — includes Phase 1b (bundle Clang) and Phase 6 (advanced features) |
| 06-performance-targets.md      | Benchmarks to beat, per-phase performance gains, profiling strategy |
