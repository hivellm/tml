# TML Full Independence — Vision and Architecture

**Date**: 2026-04-05
**Status**: Strategic planning
**Part of**: [Independence Plan](README.md)

---

## Table of Contents

1. [The Vision](#1-the-vision)
2. [Architecture Layers](#2-architecture-layers)
3. [The Key Insight: Shared MIR](#3-the-key-insight-shared-mir)
4. [Four Pillars Detailed](#4-four-pillars-detailed)
5. [Independence Progression](#5-independence-progression)
6. [Why Not Just Use LLVM Forever](#6-why-not-just-use-llvm-forever)
7. [Comparison with Zig, Go, Rust, D](#7-comparison-with-zig-go-rust-d)

---

## 1. The Vision

Today's TML compiler is a C++ program that calls LLVM to generate code and LLD to link
binaries. When a developer runs `tml build hello.tml`, the request passes through:

```
TML compiler (C++)  →  LLVM C++ API  →  machine code  →  LLD (C++)  →  .exe
```

Every layer in that chain except the TML frontend is written in C++ by third parties. TML
owns the language semantics but not the backend, not the linker, and not the toolchain users
need to compile C or C++ files alongside TML code.

The vision is to own the complete chain:

```
tml build hello.tml   →  TML frontend (TML)  →  MIR  →  TML backend (TML)  →  tml-link (TML)  →  .exe
tml build main.c      →  C frontend (TML)    →  MIR  →  TML backend (TML)  →  tml-link (TML)  →  .exe
tml build main.cpp    →  C++ frontend (TML)  →  MIR  →  TML backend (TML)  →  tml-link (TML)  →  .exe
tml build project/    →  appropriate frontend per file  →  shared MIR  →  backend  →  linker  →  .exe
```

### Four Compilation Scenarios

**Scenario 1: Pure TML project**

```
hello.tml
  └─ TML Frontend
       ├─ Lexer → Token stream
       ├─ Parser → AST
       ├─ Type Checker → typed AST + symbol table
       ├─ Borrow Checker → validated ownership
       ├─ HIR Builder → high-level IR
       ├─ THIR Lowerer → trait-resolved IR
       └─ MIR Builder → MIR (SSA form)
            └─ MIR Optimizer (30+ passes)
                 └─ TML Native Backend
                      ├─ Instruction Selection (MIR → machine instructions)
                      ├─ Register Allocation
                      ├─ x86_64 Encoder or AArch64 Encoder
                      └─ .obj file
                           └─ tml-link → .exe
```

**Scenario 2: C file**

```
main.c
  └─ C Frontend
       ├─ Preprocessor → #include, #define resolution
       ├─ Lexer → C tokens
       ├─ Parser → C AST
       └─ C Type Checker + MIR Lowering → MIR
            └─ (shared optimizer + backend + linker as above)
```

**Scenario 3: C++ file**

```
main.cpp
  └─ C++ Frontend
       ├─ Preprocessor → #include, template instantiation
       ├─ Lexer → C++ tokens
       ├─ Parser → C++ AST (classes, templates, overloads, exceptions)
       └─ C++ Type Checker + MIR Lowering → MIR
            └─ (shared optimizer + backend + linker as above)
```

**Scenario 4: Mixed project**

```
project/
  ├── main.tml        → TML Frontend → MIR
  ├── util.c          → C Frontend  → MIR
  └── render.cpp      → C++ Frontend → MIR
                              │
                    Shared MIR Optimizer
                              │
                    Shared TML Native Backend
                              │
                    Shared tml-link
                              │
                           project.exe
```

The key architectural advantage: all frontends produce the same MIR, so the optimizer,
backend, and linker are written once and shared by all languages.

---

## 2. Architecture Layers

The complete TML toolchain stack has six layers, numbered from top (user-facing) to bottom
(OS interface):

```
┌─────────────────────────────────────────────────────────────────────────────┐
│  Layer 5: CLI + Build System + Package Manager                              │
│                                                                             │
│  tml build    tml test    tml run    tml fmt    tml lint    tml pkg          │
│                                                                             │
│  Project configuration: tml.toml                                           │
│  Dependency resolution: version constraints, lockfile                       │
│  Build graph: file discovery, change detection, parallel compilation        │
│  Currently: ~26K LOC C++ (CLI commands + builder pipeline)                 │
│  Target: ~17K LOC TML (ported during self-hosting)                         │
└──────────────────────────┬──────────────────────────────────────────────────┘
                           │
┌──────────────────────────▼──────────────────────────────────────────────────┐
│  Layer 4: Language Frontends                                                │
│                                                                             │
│  ┌────────────────────┐  ┌────────────────────┐  ┌─────────────────────┐   │
│  │ TML Frontend       │  │ C Frontend         │  │ C++ Frontend        │   │
│  │                    │  │                    │  │                     │   │
│  │ Lexer              │  │ Preprocessor       │  │ Preprocessor        │   │
│  │ Parser             │  │ Lexer              │  │ Lexer               │   │
│  │ Type Checker       │  │ Parser             │  │ Parser              │   │
│  │ Borrow Checker     │  │ C Type Checker     │  │ C++ Type Checker    │   │
│  │ HIR Builder        │  │ MIR Lowering       │  │ MIR Lowering        │   │
│  │ THIR Lowerer       │  │                    │  │                     │   │
│  │ MIR Builder        │  │ ~16–25K LOC TML    │  │ ~30–50K LOC TML     │   │
│  │                    │  │ (planned)          │  │ (planned)           │   │
│  │ 184K LOC C++ today │  │                    │  │                     │   │
│  │ ~120K LOC TML      │  │                    │  │                     │   │
│  │ (self-hosted)      │  │                    │  │                     │   │
│  └────────────────────┘  └────────────────────┘  └─────────────────────┘   │
│                                                                             │
│  All frontends emit MIR — the boundary between language-specific and        │
│  language-agnostic compilation.                                             │
└──────────────────────────┬──────────────────────────────────────────────────┘
                           │  MIR (SSA form, typed, language-agnostic)
┌──────────────────────────▼──────────────────────────────────────────────────┐
│  Layer 3: MIR — Shared Optimization Layer                                   │
│                                                                             │
│  SSA form basic blocks with typed instructions and explicit control flow.  │
│  Language-agnostic: the same passes work for TML, C, and C++ code.         │
│                                                                             │
│  Current MIR passes (30+):                                                  │
│    mem2reg            — promotes stack allocs to SSA registers (CRITICAL)   │
│    dead_function_elim — removes unreachable functions                       │
│    block_merge        — merges empty jump-only basic blocks                 │
│    constant_folding   — fold known constant expressions at compile time     │
│    dead_code_elim     — remove instructions with no side effects            │
│    inline_small       — inline functions below instruction threshold        │
│    loop_invariant     — hoist loop-invariant computations                   │
│    tail_call_elim     — convert tail calls to jumps                         │
│    ...and 22 more passes                                                    │
│                                                                             │
│  MIR is today: 31,719 LOC C++ (builder) + passes                           │
│  MIR target: ~26K LOC TML (ported during self-hosting)                     │
└──────────────────────────┬──────────────────────────────────────────────────┘
                           │  Optimized MIR
┌──────────────────────────▼──────────────────────────────────────────────────┐
│  Layer 2: Native Code Backend — Replaces LLVM                               │
│                                                                             │
│  Takes optimized MIR and produces native .obj files directly.               │
│  No LLVM IR intermediate — MIR goes straight to machine instructions.       │
│                                                                             │
│  Components:                                                                │
│    Instruction Selection                                                     │
│      Generic MIR instructions → architecture-specific instructions          │
│      Pattern matching via rewrite rules (Go compiler approach)              │
│      Handles: arithmetic, memory, control flow, calls, intrinsics           │
│                                                                             │
│    Register Allocation                                                       │
│      Phase 1: Linear scan (fast, for debug builds)                          │
│      Phase 2: Graph coloring (optimal, for release builds)                  │
│      Spill code generation for values that don't fit in registers           │
│                                                                             │
│    x86_64 Encoder                                                            │
│      Variable-length instruction encoding (1–15 bytes per instruction)      │
│      REX prefix, ModRM, SIB, displacement, immediate encoding               │
│      All addressing modes: register, memory, RIP-relative                   │
│      SIMD: SSE2, AVX, AVX-512 instruction encoding                          │
│                                                                             │
│    AArch64 Encoder                                                           │
│      Fixed 32-bit instruction encoding (simpler than x86_64)                │
│      A64 instruction set: data processing, loads/stores, branches           │
│      NEON SIMD encoding                                                      │
│                                                                             │
│    Object File Writer                                                        │
│      PE/COFF .obj (Windows)                                                  │
│      ELF .o (Linux/BSD)                                                      │
│      Mach-O .o (macOS)                                                       │
│      Symbol table, relocations, debug info (DWARF/PDB stubs)                │
│                                                                             │
│  Estimated size: 30–50K LOC TML                                             │
│  Timeline: 15–22 months after MIR stabilizes                               │
└──────────────────────────┬──────────────────────────────────────────────────┘
                           │  .obj files (PE/COFF, ELF, or Mach-O)
┌──────────────────────────▼──────────────────────────────────────────────────┐
│  Layer 1: Linker (tml-link) — Replaces LLD                                  │
│                                                                             │
│  Takes .obj files and produces final executables and shared libraries.      │
│                                                                             │
│  PE/COFF (Windows)                                                           │
│    .exe (executable), .dll (shared library), .lib (import library)          │
│    Section layout: .text, .data, .rdata, .bss, .pdata, .reloc               │
│    Import table resolution: kernel32.dll, user32.dll, etc.                  │
│    Export table generation for .dll                                          │
│    Base relocation table for ASLR                                            │
│                                                                             │
│  ELF (Linux/BSD)                                                             │
│    executable, .so (shared object), .a (static archive)                     │
│    Program header table, section header table                                │
│    Dynamic linking: DT_NEEDED, PLT/GOT                                      │
│    GNU hash table for fast symbol lookup                                     │
│    DWARF debug info merging                                                  │
│                                                                             │
│  Mach-O (macOS)                                                              │
│    executable, .dylib, .a                                                    │
│    Fat binary (universal binary for x86_64 + ARM64)                         │
│    Code signing placeholder (actual signing requires Apple tools)            │
│    LC_LOAD_DYLIB, LC_SYMTAB, LC_DYSYMTAB commands                           │
│                                                                             │
│  Incremental linking                                                         │
│    Track which object files changed between builds                           │
│    Re-link only changed sections (target: sub-10ms re-link)                  │
│    Persistent symbol table cache across builds                               │
│                                                                             │
│  Estimated size: 15–25K LOC TML                                             │
│  Timeline: 6–18 months (partially parallelizable with backend)              │
└──────────────────────────┬──────────────────────────────────────────────────┘
                           │  .exe / .dll / .so / .dylib
┌──────────────────────────▼──────────────────────────────────────────────────┐
│  Layer 0: OS Interface — Permanent Thin C Shim                              │
│                                                                             │
│  @extern("c") FFI calls to kernel-provided APIs.                            │
│  This layer is the only C code that remains permanently.                    │
│                                                                             │
│  essential.c (1,344 LOC — permanent)                                        │
│    print, eprint, readline — I/O primitives                                 │
│    tml_panic — abort with message                                            │
│    test_harness_* — DLL entry point for test runner                         │
│                                                                             │
│  mem.c (249 LOC — permanent)                                                 │
│    tml_alloc / tml_free — malloc/free wrappers with tracking                │
│    tml_realloc — realloc wrapper                                             │
│                                                                             │
│  After full independence: these two files (~1,600 LOC) are the only         │
│  C code in the entire TML toolchain. Everything else is TML.                │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

## 3. The Key Insight: Shared MIR

The architectural decision that makes the four-pillar plan tractable is: **all language
frontends compile to the same MIR**.

This is not a new idea — it is the same insight behind LLVM IR. But where LLVM IR is a
text-based format requiring an external tool (LLVM) to process, TML's MIR is an in-memory
data structure processed entirely by TML code.

### What Shared MIR Means in Practice

**One optimizer serves all languages.** The 30+ MIR optimization passes — mem2reg, dead code
elimination, constant folding, inlining, loop hoisting — run identically on MIR produced by
the TML frontend, the C frontend, and the C++ frontend. Optimization improvements benefit all
three languages simultaneously.

**One backend generates code for all languages.** When the TML backend learns to generate
optimized code for a new pattern (e.g., vectorized loops), all three frontends benefit. The
x86_64 and AArch64 encoders are written once.

**One linker links all languages.** tml-link sees only .obj files and symbol names. It has
no knowledge of which frontend produced which .obj file. Mixed TML/C/C++ projects link
without friction.

**Cross-language inlining becomes possible.** When a C function and a TML caller both exist
at MIR level simultaneously (as in a mixed project build), the inliner can cross the
language boundary — inlining the C function body directly into the TML call site or vice
versa. This is impossible with separate compiler processes.

**Adding a new language costs only a frontend.** A hypothetical Fortran, Pascal, or
assembly frontend needs only to lower its AST to TML's MIR. The optimizer, backend, and
linker are inherited for free. The marginal cost of a new language is one frontend, not an
entire toolchain.

### MIR Design Constraints for This Goal

For shared MIR to work correctly across TML, C, and C++, the MIR definition must be
expressive enough to represent the semantic differences between languages:

| Feature | TML | C | C++ | MIR Representation |
|---------|-----|---|-----|---------------------|
| Algebraic types | Maybe[T], Outcome[T,E] | struct + enum tag | variant | tagged union in MIR |
| Ownership | move semantics | manual | RAII | MIR tracks live/dead |
| Exceptions | panic (unwind) | setjmp/longjmp | try/catch | MIR unwind edges |
| Closures | `do(x) expr` | function pointers | lambdas | closure struct + fn ptr in MIR |
| SIMD | `#[simd]` loops | `__m256i` intrinsics | `__m256i` intrinsics | MIR vector instructions |
| Integer overflow | wrapping or checked | undefined | undefined | MIR overflow flag |

The MIR instruction set is already rich enough to represent TML programs correctly. Extending
it for C and C++ semantics is an incremental exercise, not a redesign.

---

## 4. Four Pillars Detailed

### Pillar 1: Self-Hosted Compiler

**What it replaces**: 184K LOC C++ compiler codebase (`compiler/src/`, `compiler/include/`)

**What stays in C**: 1,593 LOC — the LLVM backend (550 LOC), LLD wrapper (670 LOC), and JIT
entry point (373 LOC). These are replaced by Pillars 2 and 3.

**Estimated LOC**: ~120K LOC TML (35% reduction from C++ via language expressiveness — no
header/source split, no template boilerplate, algebraic types instead of class hierarchies,
pattern matching instead of visitor dispatch chains).

**Timeline**: 24–30 months. Longest pillar. Must start first, blocks nothing else while in
progress, but its completion is required before the compiler can build itself.

**Prerequisites**:
- Phase 7 complete (current task — language maturity, stdlib coverage)
- TML can express all patterns needed by the compiler (parser combinators, symbol tables,
  hash maps, string interning, arena allocation, trait dispatch)
- Bootstrapping chain: C++ compiler compiles TML stage-0 → stage-0 compiles TML stage-1 →
  stage-1 compiles stage-1 (fixed point)

**What it enables**:
- TML compiler is no longer dependent on Zig CC / Clang to build itself
- New contributors need only TML to build TML (no C++ toolchain required)
- Compiler improvements can use TML language features (generics, behaviors, pattern matching)
- Opens the path to Pillars 2, 3, and 4 (backend and linker written in TML)

**Key technical challenges**:
1. Bootstrapping: the first TML compiler written in TML must be compiled by the C++ compiler.
   This creates a chicken-and-egg that is resolved by maintaining a "stage-0" binary in the
   repository (a pre-compiled TML compiler binary committed to git, updated periodically).
2. Performance: the TML-written compiler must be within 2x of the C++ compiler's throughput.
   The type checker and MIR codegen are the hot paths. These will require careful
   implementation of string interning, arena allocation, and hash table design.
3. Completeness: the C++ compiler has 184K LOC. Every feature must be faithfully reproduced.
   The self-hosting milestone is the point where the TML compiler passes its own test suite.

**Prior art**: Rust (rustc is written in Rust), Go (gc is written in Go), Zig (self-hosted
since 2022), D (DMD partially self-hosted). All faced the same bootstrapping challenge.
Rust's approach (keep a pre-compiled binary as the bootstrap compiler) is the recommended
model for TML.

---

### Pillar 2: Custom Native Backend

**What it replaces**: LLVM (`tml_codegen_x86.dll`, 78 MB) — the library that converts LLVM
IR text to machine code .obj files.

**Estimated LOC**: 30–50K LOC TML — larger than it sounds because it must encode every
x86_64 and AArch64 instruction, handle all addressing modes, implement register allocation,
and write three .obj file formats.

**Timeline**: 15–22 months. Can start in parallel with self-hosting (the backend is written
in TML using the current C++ compiler, not the self-hosted compiler). The backend is complete
when it can compile the TML standard library correctly with all tests passing.

**Prerequisites**:
- MIR definition is stable (changes to MIR instruction set break the backend)
- TML has sufficient intrinsics for byte-level manipulation (encoding requires writing
  specific byte sequences — TML's `Buffer` type handles this today)
- x86_64 ISA reference documentation (Intel SDM or Agner Fog's tables)

**What it enables**:
- Binary size drops from ~140 MB to ~15 MB (no LLVM statically linked)
- Debug build compile speed: LLVM adds 35–580 ms per file; custom backend targets 5–50 ms
- Release builds: LLVM remains optional via a `--llvm` flag for maximum optimization quality
- True self-sufficiency: the compiler binary has no runtime dependency on any external tool

**Key technical challenges**:
1. Instruction encoding correctness: x86_64 has ~1,500 distinct instruction forms across
   ~500 mnemonics, with complex encoding rules (REX prefix, VEX prefix, EVEX prefix for
   AVX-512). A single encoding error causes a crash or incorrect output.
   Mitigation: exhaustive test suite comparing backend output against known-good assembler
   output (NASM or GNU as) for each instruction class.
2. Register allocation quality: naive register allocation produces code 3–10x slower than
   LLVM's output. Linear scan RA is tractable and produces code within 1.5–2x of optimal.
   Graph coloring RA (used for release builds) is NP-hard in theory but polynomial in
   practice with standard heuristics (Briggs/Chaitin-Briggs).
3. ABI correctness: System V AMD64 (Linux) and Microsoft x64 (Windows) have different
   calling conventions for struct passing, variadic arguments, and register preservation.
   Both must be correctly implemented or cross-module calls produce garbage results.
4. SIMD: TML's stdlib makes heavy use of SSE2, AVX2, and AVX-512 for string operations and
   tensor math. The backend must encode these instructions correctly.

**Prior art**: Go's backend (50K lines, production quality, 7 targets), Zig's x86_64 backend
(30K lines, fast, not yet fully production quality), DMD's x86_64 backend (20K lines, mature,
used in production for 20+ years). The Go backend's rewrite-rules approach (declarative
pattern → instruction rules compiled to Go code) is recommended for TML.

---

### Pillar 3: Custom Linker (tml-link)

**What it replaces**: LLD (`tml_codegen_x86.dll` LLD portion, 670 LOC wrapper + LLD itself
at ~2 MB in the binary).

**Estimated LOC**: 15–25K LOC TML — the smallest pillar, but requires deep knowledge of
three object file formats (PE/COFF, ELF, Mach-O) and their dynamic linking protocols.

**Timeline**: 6–18 months. The linker is the most tractable of the four pillars. It has no
algorithmic complexity (linking is mostly parsing, symbol resolution, and byte concatenation).
A working PE/COFF linker can be written in 3–6 months. ELF and Mach-O support add another
3–6 months each.

**Prerequisites**:
- TML has a working `File` I/O API (reads and writes large binary files) — already present
- TML has a `Buffer` type for byte manipulation — already present
- `HashMap` for symbol tables — already present
- Understanding of PE/COFF, ELF, and Mach-O specifications (public domain documents)

**What it enables**:
- Linker can be customized for TML-specific optimizations (dead code elimination at link
  time, whole-program inlining)
- Incremental linking: track which sections changed and re-link only those (today LLD
  re-links everything, which takes 37 seconds for the full compiler binary)
- Parallel linking: link independent sections concurrently
- Deterministic output: same inputs always produce identical binary output (reproducible builds)

**Key technical challenges**:
1. Symbol resolution order: ELF has complex weak symbol, common symbol, and group section
   semantics. PE/COFF has COMDAT sections. Getting these exactly right is subtle.
2. Relocations: each architecture has dozens of relocation types. An incorrect relocation
   produces a binary that crashes at runtime, often with a misleading error.
3. Debug info: DWARF (ELF/Mach-O) and PDB (PE) debug info is complex. A minimal
   implementation that produces working stack traces is achievable; full fidelity requires
   months of additional work.
4. Import/export tables (Windows): PE/COFF has intricate import table and export table
   formats that must be exactly correct for DLL loading to succeed.

**Prior art**: mold linker (50K lines C++, production quality, 8x faster than LLD at linking
large projects), gold linker (GNU, 80K lines C++), the Go linker (written in Go, ~25K lines,
production quality for all Go targets). The mold paper provides an excellent description of
parallel linking algorithms TML can adopt.

---

### Pillar 4: C/C++ Frontend

**What it replaces**: Dependency on Zig CC (Clang 20 wrapper) to compile `compiler/runtime/`
C files and the TML compiler C++ source.

**Estimated LOC**:
- C frontend: 16–25K LOC TML (preprocessor + lexer + parser + C type checker + MIR lowering)
- C++ frontend: 30–50K LOC TML (all of C plus classes, templates, overloads, exceptions)

**Timeline**: 12–24 months for C; 18–36 months for C++ subset. C++ can be scoped to the
subset used by `compiler/src/` — no need to support the full C++23 standard.

**Prerequisites**:
- Pillar 2 (native backend) — the C frontend must be able to produce object files without LLVM
- Pillar 3 (linker) — the C frontend must be able to link programs without LLD
- MIR must be able to represent C semantics (undefined behavior tracking, restrict pointers,
  setjmp/longjmp, variable-length arrays)

**What it enables**:
- TML can compile its own C runtime files (`compiler/runtime/*.c`) without Zig CC
- TML can compile the C++ compiler source (`compiler/src/**/*.cpp`) without Zig CC
- Users can compile C and C++ code alongside TML code in mixed projects
- `tml cc file.c` becomes the recommended way to compile C for TML projects
- `tml c++ file.cpp` for C++ interop

**Key technical challenges**:
1. C preprocessor: the C preprocessor is a Turing-complete macro language with stringization,
   token pasting, variadic macros, and predefined macros. Correct implementation requires
   careful attention to the C standard's expansion rules.
2. C type system: C has implicit integer promotions, usual arithmetic conversions, implicit
   pointer-to-integer conversions, and various undefined behaviors that TML must model
   faithfully to compile existing C code correctly.
3. C++ name mangling: C++ symbol names encode the full type signature (Itanium ABI on
   Linux/macOS, Microsoft ABI on Windows). Incorrect mangling prevents linking.
4. C++ templates: templates in C++ are instantiated during type checking, not before. The
   template instantiation engine is the most complex part of a C++ compiler. Scoping TML's
   C++ support to the templates used in `compiler/src/` is the pragmatic approach.
5. C++ exceptions: exception handling requires generating unwind tables (`.pdata` on Windows,
   `.eh_frame` on ELF) and inserting cleanup code (RAII destructors). This is a significant
   undertaking but is required to compile the TML compiler C++ source correctly.

---

## 5. Independence Progression

The four pillars are built sequentially (with overlap), and independence accumulates as each
pillar completes. Here is what the compilation stack looks like at each stage:

### Today (2026)

```
Source:   TML source (hello.tml)
Frontend: TML compiler (C++, 184K LOC) — Zig CC required to build it
Backend:  LLVM (C++, ~10M LOC) — statically linked into tml_codegen_x86.dll (78 MB)
Linker:   LLD (C++, ~500K LOC) — statically linked into tml_codegen_x86.dll
Output:   .exe

External dependencies: Zig CC (build time), LLVM (runtime embedded), LLD (runtime embedded)
Binary size: ~140 MB
Build time (full): ~100 seconds (I/O bound on linking)
```

### After Era 1: Self-Hosted (2028)

```
Source:   TML source (hello.tml)
Frontend: TML compiler (TML, ~120K LOC) — built by TML itself
Backend:  LLVM (still embedded, still ~78 MB) — unchanged
Linker:   LLD (still embedded) — unchanged
Output:   .exe

External dependencies: none at build time (compiler builds itself), LLVM/LLD still embedded
Binary size: ~140 MB (unchanged — LLVM/LLD still present)
Build time: ~100 seconds (unchanged)
Milestone: TML no longer requires Zig CC to build TML
```

### After Era 2: Custom Backend (2029)

```
Source:   TML source (hello.tml)
Frontend: TML compiler (TML, ~120K LOC)
Backend:  TML native backend (TML, ~40K LOC) — replaces LLVM for debug builds
          LLVM still available via --llvm flag for maximum optimization
Linker:   LLD (still embedded) — unchanged
Output:   .exe (debug: via TML backend; release: via LLVM)

External dependencies: none for debug builds
Binary size: ~60 MB without LLVM (debug), ~140 MB with LLVM (release)
Debug compile speed: 5–50 ms per file (vs 35–580 ms with LLVM)
Milestone: LLVM is optional, not required
```

### After Era 3: Custom Linker (2029–2030)

```
Source:   TML source (hello.tml)
Frontend: TML compiler (TML, ~120K LOC)
Backend:  TML native backend (TML, ~40K LOC)
Linker:   tml-link (TML, ~20K LOC) — replaces LLD entirely
Output:   .exe

External dependencies: NONE for TML programs
Binary size: ~15 MB (no LLVM, no LLD)
Link time: sub-10ms for incremental re-links (vs 37 seconds today)
Milestone: TML programs have ZERO external tool dependencies
           This is the first point of true self-sufficiency for TML source
```

### After Era 4: Full Independence (2031)

```
Source:   hello.tml, main.c, main.cpp — any combination
Frontend: TML frontend (TML) + C frontend (TML) + C++ frontend (TML)
Backend:  TML native backend (TML)
Linker:   tml-link (TML)
Output:   .exe

External dependencies: NONE — not even Zig CC to build the compiler runtime
Binary size: ~15 MB
C/C++ support: compile C runtime files + TML compiler C++ source without Clang
Milestone: TML is a complete, standalone toolchain for TML, C, and C++ projects
           No external tool of any kind required on the developer's machine
           beyond the operating system itself
```

---

## 6. Why Not Just Use LLVM Forever?

LLVM is an excellent tool. The TML compiler owes much of its current correctness and
optimization quality to LLVM. But permanent LLVM dependency has concrete costs that grow
over time and ultimately prevent TML from achieving its goals.

### Binary Size

LLVM is statically linked into `tml_codegen_x86.dll` (78 MB). The total TML distribution
today is ~140 MB. A developer downloading TML for the first time receives 140 MB, most of
which is LLVM code the TML compiler calls through a thin wrapper.

After full independence, the TML binary is approximately 15 MB. A 9x size reduction
improves download times, reduces disk usage, and makes TML more appealing for environments
where binary size matters (embedded systems, minimal VMs, CI runners with limited storage).

### Compile Speed

LLVM is designed for maximum optimization quality, not maximum compilation speed. Even at
`-O0` (no optimization), LLVM performs IR validation, type verification, and IR lowering
that add 35–580 ms of latency per file. For debug builds where optimization quality does
not matter, this is pure overhead.

The TML native backend targets 5–50 ms per file at the equivalent of `-O0`. For a project
with 1,000 TML files, this difference is 35–580 seconds (LLVM) versus 5–50 seconds (TML
backend). In tight edit-compile-test loops, this is a 7–10x improvement in developer
experience.

LLVM remains available via `--llvm` for release builds where optimization quality is
paramount. The custom backend does not need to match LLVM's output quality — it only needs
to produce correct code efficiently.

### Version Coupling

LLVM releases a new major version every 6 months. Each major version makes breaking changes
to the C++ API. The TML compiler must track LLVM's API changes or fall behind on security
fixes and new target support. This creates a recurring engineering tax: every 6 months,
someone must audit the LLVM API changes and update the TML compiler's integration code.

With no LLVM dependency, this tax disappears entirely.

### Distribution Complexity

Statically linking LLVM requires careful CRT (C Runtime) version management on Windows.
The `/MD` vs `/MDd` CRT mismatch between debug and release LLVM builds has caused multiple
build failures in the TML project. Dynamically linking LLVM requires shipping the LLVM
shared library alongside the TML binary and managing version compatibility.

Neither approach is clean. Eliminating LLVM eliminates the problem.

### Control and Optimization Opportunities

LLVM optimizes for the general case. TML's MIR has semantic information that LLVM IR does
not (ownership, move semantics, non-aliasing guarantees from the borrow checker). A custom
backend that understands TML semantics can exploit this information for optimizations that
LLVM cannot perform because it operates on a lower-level, less-informed IR.

For example:
- A TML value that has been moved from is guaranteed never to be read again. The custom
  backend can elide the zeroing/cleanup code that a conservative LLVM would emit.
- Borrow checker guarantees that two `ref T` values never alias. The custom backend can
  assume non-aliasing without requiring `restrict` annotations that the programmer must
  manually add in C.
- `Maybe[T]` is guaranteed by the type checker to be accessed only after a successful
  pattern match. The custom backend can elide the null check that LLVM would conservatively
  emit.

### Self-Sufficiency

A programming language's long-term health depends on owning its full toolchain. Languages
that depend on external backends are subject to the decisions of those backend projects.
If LLVM were to change its licensing, drop a target platform, or fundamentally alter its
IR, TML would be forced to respond on LLVM's timeline, not TML's.

Owning the backend, linker, and C/C++ frontend means TML's development pace is determined
by TML's own priorities, not by upstream projects.

---

## 7. Comparison with Zig, Go, Rust, D

These four languages provide the most directly comparable examples of toolchain
independence efforts. Each made different tradeoffs.

### Detailed Comparison Table

| Capability | TML (2031 goal) | Zig | Go | Rust | D (DMD) |
|-----------|----------------|-----|-----|------|---------|
| **Self-hosted compiler** | ✅ planned (2028) | ✅ since 2022 | ✅ since 2009 | ✅ since 2011 | ✅ DMD in D |
| **Custom backend** | ✅ planned (2029) | ✅ + optional LLVM | ✅ (gc backend) | ❌ LLVM only | ✅ (x86, x86_64) |
| **Custom linker** | ✅ planned (2030) | ✅ (zld, mach-o, elf) | ✅ (cmd/link) | ❌ uses LLD | ✅ (partial) |
| **Compiles C** | ✅ planned (2031) | ✅ (full Clang wrapper) | ❌ | ❌ | ❌ |
| **Compiles C++** | ✅ planned (2031) | ✅ (full Clang wrapper) | ❌ | ❌ | ❌ |
| **No LLVM dep (debug)** | ✅ planned (2029) | ✅ | ✅ | ❌ | ✅ |
| **No LLVM dep (release)** | ⚠️ LLVM optional | ⚠️ LLVM optional | ✅ | ❌ | ✅ |
| **Binary size** | ~15 MB (goal) | ~45 MB | ~15 MB | ~200 MB | ~40 MB |
| **Debug compile speed** | ~5–50 ms/file (goal) | ~20–80 ms/file | ~5–30 ms/file | ~500–2000 ms/file | ~10–50 ms/file |
| **Incremental linking** | ✅ sub-10ms (goal) | ✅ | ✅ | ❌ (LLD) | ✅ |
| **Mixed language build** | ✅ TML/C/C++ | ✅ Zig/C/C++ | ❌ | ❌ | ❌ |
| **Package manager** | ✅ (planned) | ✅ (zig fetch) | ✅ (go mod) | ✅ (cargo) | ✅ (dub) |
| **Cross-compilation** | ✅ (via backend targets) | ✅ (excellent) | ✅ | ✅ (LLVM targets) | ⚠️ (limited) |

### Per-Language Analysis

**Zig** is the most similar to TML's goals. Zig has a self-hosted compiler (since 2022),
a custom backend for debug builds (x86_64, AArch64, WASM, RISC-V, MIPS, PowerPC), and uses
Clang/LLVM for release builds. Zig also wraps Clang to provide `zig cc` and `zig c++` as
a drop-in replacement for system compilers. The main difference: Zig wraps Clang (links it
statically) for C/C++ compilation, while TML plans to write its own C and C++ frontends.
TML's approach is more ambitious but produces a smaller binary and removes the Clang dependency.

**Go** achieved self-hosting earliest (2009, two years after language release). The Go
compiler (`gc`) has a custom backend for all targets and a custom linker (`cmd/link`). Go
does not compile C or C++, and Go's build system calls the system C compiler (`gcc` or
`clang`) for cgo files. Go's compiler is ~200K LOC of Go — similar in scope to TML's ~120K
LOC estimate, and Go's backend (~50K LOC) is the most directly relevant prior art for
TML's custom backend.

**Rust** has no custom backend — `rustc` is permanently coupled to LLVM. Rust's compile
speed is a persistent complaint from users, directly attributable to LLVM's overhead on
every file. Rust uses LLD as its linker (also LLVM). The `rustc` binary is ~200 MB. Rust
achieves excellent code quality but at the cost of being permanently subject to LLVM's
release schedule and binary size overhead. TML explicitly avoids Rust's approach here.

**D (DMD)** is the oldest self-hosting compiler among these examples. DMD is written in D
and has a custom x86/x86_64 backend (no LLVM dependency for those targets). LDC (the LLVM-
based D compiler) is a separate project. DMD's backend is approximately 20K lines and has
been maintained for 20+ years, proving that a small custom backend can achieve production
quality over time. DMD's approach — maintain a simple, fast backend alongside an optional
optimizing backend (LDC) — is the model TML adopts.

### Key Takeaway

The languages that invested in custom backends and linkers (Zig, Go, D) all report
significantly better developer experience (fast incremental builds, small binaries, no
external tool dependencies) compared to Rust, which did not. TML's four-pillar plan
follows the Zig/Go/D model, extended to include C and C++ frontend compilation.

The investment is large (4–6 years, ~210–270K LOC of new TML code) but the payoff is
permanent: a completely self-sufficient toolchain that TML controls entirely, with no
external dependencies for any user on any supported platform.
