# TML Independence: Phased Roadmap

**Date**: 2026-04-05
**Horizon**: 4–6 years (2026–2031)
**Model**: Single developer baseline, with parallel track annotations for team of 2–3

---

## Overview

Four eras, seventeen phases, each building on the previous. The eras overlap intentionally — Era 2
(custom backend) can begin in parallel with late Era 1 work, and Era 3 (custom linker) can begin
once the backend is stable enough to emit objects.

| Era | Focus | Months | Phases | Key Milestone |
|-----|-------|--------|--------|---------------|
| **Era 1** | Self-Hosted Compiler | 0–30 | 0–5 | TML compiles itself |
| **Era 2** | Custom Backend | 24–48 | 6–9 | LLVM eliminated |
| **Era 3** | Custom Linker | 30–48 | 10–13 | LLD eliminated |
| **Era 4** | C/C++ Frontend | 36–60 | 14–17 | Complete toolchain |

The single most important milestone is **Phase 5: Bootstrap** — once TML compiles itself,
every subsequent phase is implemented in TML, which accelerates all remaining work.

---

## Era 1: Self-Hosted Compiler (Months 0–30)

**Goal**: The TML compiler compiles itself. The C++ codebase is retired except for the
1,593 LOC LLVM/LLD shim, which remains until Era 2 and Era 3 complete.

**Total work**: ~120,000 LOC TML replacing ~184,000 LOC C++

---

### Phase 0: Foundation (Months 0–4)

**Purpose**: Build the infrastructure required before any compiler subsystem can be ported.
No compiler code is ported in this phase — only tooling and consolidation.

| Task | Duration | Output | Risk |
|------|----------|--------|------|
| Consolidate MIR paths (retire hir_mir_builder) | 4–6 weeks | -5K C++ removed | Medium |
| Build `std::intern` (string interning) | 3–5 days | ~200 TML | Low |
| Document type checker invariants | 3–4 weeks | 50–100 pages | High |
| Build IR-diff testing tool | 2–3 weeks | ~500 TML | Medium |
| Build AST/TypeEnv serializers | 4–6 weeks | ~1,500 TML | Medium |
| Create hybrid pipeline framework | 2–3 weeks | ~800 TML | Medium |

**MIR consolidation detail**: The compiler currently has two MIR builder paths —
HIR→MIR (legacy, `hir_mir_builder.cpp`) and THIR→MIR (new, `thir_mir_builder.cpp`).
Maintaining both doubles the porting effort for Phase 3. The legacy path must be retired
first, all tests verified passing on the THIR path, then only the THIR path is ported.

**IR-diff tool**: Takes two LLVM IR files and reports semantic differences, ignoring
cosmetic differences (instruction names, label numbering, whitespace). Used throughout
Era 1 to verify each ported subsystem produces identical IR to the C++ compiler.

**AST/TypeEnv serializers**: Required for the hybrid pipeline — TML-implemented
subsystems must serialize their output so C++-implemented downstream subsystems can
consume it. Binary format (msgpack-compatible) for performance.

**Hybrid pipeline framework**: Allows individual compiler stages to be swapped between
C++ and TML implementations at runtime. Controlled via `--stage=<subsystem>:tml` flags.
This is the scaffolding that allows incremental subsystem migration and testing.

**Prerequisites**: Phase 7 of the Rust-parity task complete (phase7-16 is the only
remaining item as of 2026-04-05). The MIR is not stable enough to freeze until
Rust-parity work is done.

**Success criteria**:
- IR-diff tool passes on all 1,700+ test files with trivial program
- Hybrid pipeline compiles a trivial program using TML lexer + C++ rest
- `std::intern` benchmarks show < 5% overhead vs C++ `std::unordered_map` interning

**Deliverables**: `std::intern`, IR-diff tool, AST serializers, TypeEnv serializer,
invariant document (~50 pages), MIR consolidated to single path

---

### Phase 1: Frontend in TML (Months 4–8)

**Purpose**: Port the lexer and parser to TML. This is the first compiler subsystem
to run in TML — a proof of concept for the hybrid pipeline.

| Component | C++ LOC | TML LOC | Files |
|-----------|---------|---------|-------|
| Lexer | 2,830 | ~1,800 | lexer.tml, token.tml |
| Parser (core) | 3,200 | ~2,100 | parser.tml |
| Parser (expressions) | 1,800 | ~1,200 | parser_expr.tml |
| Parser (declarations) | 1,327 | ~860 | parser_decl.tml |
| AST types | ~500 | ~340 | ast.tml |
| **Total** | **~9,657** | **~6,300** | **8 files** |

**Integration path**:
1. TML lexer/parser runs, serializes AST to binary
2. C++ deserializes AST, continues with type checking
3. Verified via IR-diff: output must be identical to all-C++ path

**Ordering within phase**:
- Week 1–2: Token type definitions, lexer for literals and keywords
- Week 3–4: Lexer for operators, edge cases (raw strings, nested comments)
- Week 5–7: Expression parser (Pratt, precedence climbing)
- Week 8–10: Declaration parser (functions, types, impls, behaviors)
- Week 11–14: Integration with hybrid pipeline, differential testing
- Week 15–16: Performance tuning (serialization overhead target < 5%)

**Prerequisites**: Phase 0 complete (serializers, hybrid pipeline, `std::intern`)

**Success criteria**:
- All 1,700+ tests pass with TML lexer/parser feeding C++ type checker
- IR-diff shows zero regressions
- Serialization overhead < 5% of total compile time on the benchmark suite

**Risk**: Low. The lexer and parser are pure data transformations over well-defined
input. There are no global state dependencies, no subtle ordering requirements, and
the output is a concrete data structure (the AST) that can be compared directly.

**Key challenge**: AST serialization format must be forwards-compatible — later phases
add nodes to the AST (THIR, MIR), and the serializer must handle these gracefully
without requiring simultaneous updates across subsystems.

---

### Phase 2: Type System in TML (Months 8–16)

**Purpose**: Port the type checker — the largest single subsystem and the critical path
for self-hosting. This phase is expected to take 8 months for a single developer.

| Component | C++ LOC | TML LOC | Complexity |
|-----------|---------|---------|------------|
| Type checker (src files) | 9,246 | ~6,000 | Very High |
| Type checker (headers) | ~12,070 | ~7,600 | Very High |
| **Total** | **~21,316** | **~13,600** | **CRITICAL** |

The type checker is the most complex subsystem because it implements:
- Hindley-Milner type inference with constraint generation and unification
- Four-phase checking order: register types → resolve imports → process impls → check bodies
- Generic instantiation and monomorphization decisions
- Behavior (trait) dispatch with associated types
- Coercion insertion (later consumed by THIR)

**Sub-phases** (each independently testable and mergeable):

**Sub-phase 2a — Type Registration** (months 8–9, ~2,500 TML LOC):
Build the symbol table. Every type, function, behavior declaration is registered
with its name, parameters, and generic constraints. No inference yet — only
structural registration. Output: TypeEnv with all declarations, no inferred types.

**Sub-phase 2b — Module Resolution** (months 9–10, ~2,000 TML LOC):
Implement `use` statement resolution, visibility checking, re-exports, and the
module path lookup algorithm. This is the most mechanical sub-phase — it is
essentially a graph traversal over the module tree.

**Sub-phase 2c — Type Inference** (months 10–13, ~5,500 TML LOC):
Implement Hindley-Milner constraint generation and unification. This is the
hardest sub-phase. The invariant document from Phase 0 is required reading.
Key risk: the unification algorithm has subtle handling of recursive types,
higher-kinded types, and associated type projections that are not obvious from
the C++ source alone.

**Sub-phase 2d — Behavior Dispatch** (months 13–16, ~3,600 TML LOC):
Implement trait solver, behavior inheritance, generic bounds checking,
and the final coercion insertion pass that feeds THIR. This sub-phase integrates
all previous sub-phases — it is the first time the full type checker pipeline runs
end-to-end in TML.

**Differential testing strategy**:
After each sub-phase, run every test file through BOTH the C++ type checker and the
TML type checker. Serialize both TypeEnvs and compare field by field. The IR-diff
tool from Phase 0 is used for final verification (identical IR = identical TypeEnv).

**Known invariants requiring documentation** (discovered from C++ source):
- Type variable unification must use union-find with path compression
- Generic bounds are checked lazily (at instantiation, not at declaration)
- Associated type normalization happens during coercion insertion, not inference
- The 4-phase order is a hard constraint — body checking requires complete impl info

**Prerequisites**: Phase 1 complete; invariant document from Phase 0
**Success criteria**: Differential TypeEnv comparison passes on all 1,700+ test files;
zero IR-diff regressions
**Risk**: CRITICAL. This is the highest-risk phase in Era 1.
**Team parallel track**: With 2–3 developers, sub-phases 2a/2b can be developed in
parallel (they touch different parts of the type system). Sub-phases 2c and 2d are
sequential.

---

### Phase 3: IR Pipeline in TML (Months 16–22)

**Purpose**: Port HIR lowering, THIR lowering, MIR construction, and all 30+ MIR
optimization passes.

| Component | C++ LOC | TML LOC | Complexity |
|-----------|---------|---------|------------|
| HIR builder (src + headers) | 6,994 | ~4,500 | High |
| THIR lowerer | 1,873 | ~1,200 | Medium |
| MIR builder (consolidated THIR path) | ~25,000 | ~16,200 | High |
| MIR passes (30+ passes) | ~6,700 | ~4,350 | Medium-High |
| **Total** | **~40,567** | **~26,250** | **High** |

**HIR lowering** (months 16–18):
Transforms the AST into a typed, desugared form. Key transforms: type resolution
(all expressions get concrete types), desugaring (`for` → iterator, `if let` → `when`,
`var` → `let mut`), field and variant index resolution, closure capture analysis, and
monomorphization decisions. The TypeEnv from Phase 2 is the primary input.

**THIR lowering** (months 18–19):
A thin pass over HIR that inserts coercion nodes, resolves method calls via the
trait solver, desugars operator overloading (a + b → a.add(b)), and performs
exhaustiveness checking on `when` patterns. Relatively straightforward given that
Phase 2 already implemented the trait solver.

**MIR construction** (months 19–21):
Converts THIR into SSA-form MIR with basic blocks, phi nodes, and terminators.
This is the second-largest sub-phase. Key concern: the MIR representation has 40+
instruction types; each must be ported with identical semantics.

**MIR passes** (months 21–22):
Port all 30+ optimization passes individually. Each pass is a self-contained
module: it takes a `mir::Module` and returns a transformed `mir::Module`. The
`mem2reg` pass is most critical (converts alloca-based variables to SSA values)
and must be ported first. Other critical passes: dead function elimination,
unreachable block removal, constant folding.

**Pass porting order** (by criticality):
1. `mem2reg` — correctness depends on this (all tests fail without it)
2. `dead_function_elimination` — affects code size significantly
3. `block_merge` — affects control flow quality
4. Remaining passes by dependency order

**MIR-diff tool**: Built alongside this phase (1–2 days, ~300 TML LOC). Compares
MIR output instruction-by-instruction, ignoring label names. Used to verify each
pass produces identical output to the C++ implementation.

**Prerequisites**: Phase 2 complete (TML type checker producing correct TypeEnv)
**Success criteria**: MIR output matches C++ MIR for all test files; all passes
verified individually via MIR-diff
**Risk**: High — MIR has 30+ passes, but each is isolated and independently testable

---

### Phase 4: Codegen in TML (Months 22–26)

**Purpose**: Port MIR → LLVM IR text generation. This is the largest subsystem by LOC,
but the output is a text string, which makes differential testing trivial.

| Component | C++ LOC | TML LOC | Complexity |
|-----------|---------|---------|------------|
| MIR codegen (core: declarations, functions) | ~35,000 | ~22,750 | High |
| MIR codegen (instructions: calls, ops, memory) | ~25,000 | ~16,250 | High |
| MIR codegen (types: struct layout, ABI) | ~16,336 | ~10,600 | Medium |
| **Total** | **~76,336** | **~49,600** | **High** |

**Key advantage of this phase**: The output is a TEXT STRING — LLVM IR.
TML's `Text` type and template literals are ideal for IR generation. The IR-diff
tool from Phase 0 provides exact verification. Every function's IR output can be
compared character-by-character (modulo instruction name normalization) against the
C++ codegen output.

**Porting strategy** (by instruction category):
1. Type declarations (struct layouts, function signatures) — weeks 1–3
2. Arithmetic and comparison instructions — weeks 3–5
3. Memory instructions (load, store, alloca, GEP) — weeks 5–7
4. Control flow (br, switch, phi) — weeks 7–9
5. Function call ABI (sret, byval, Win64 vs SysV) — weeks 9–12
6. Method dispatch (virtual calls, trait objects) — weeks 12–14
7. Runtime intrinsics and extern declarations — weeks 14–16

**ABI handling**: The calling convention code is the highest-risk area. The Win64
and SysV ABIs have subtle rules about struct passing (sret for large structs,
byval for copy semantics, register allocation order). The C++ implementation has
been tuned against the Rust reference IR; the TML port must produce identical ABI
decisions.

**Rust-parity codegen rule**: Per `CLAUDE.md`, whenever a codegen pattern is ported,
it must be compared against the equivalent Rust IR. This is especially important
for:
- `Maybe[T]` layout (target: 8 bytes, not 16)
- Struct constructors (target: `insertvalue` chains, not alloca+store+load)
- Integer overflow (target: checked arithmetic with panic, not `add nsw`)

**Prerequisites**: Phase 3 complete (MIR pipeline produces correct MIR)
**Success criteria**: IR-diff shows identical LLVM IR for all test files; all
Rust-parity codegen targets met
**Risk**: High due to LOC volume, but mitigated by excellent testability

---

### Phase 5: Bootstrap (Months 26–30)

**Purpose**: Wire all ported subsystems into a complete self-hosting compiler.
Port remaining tooling. Execute the three-stage bootstrap verification.

| Component | C++ LOC | TML LOC | Complexity |
|-----------|---------|---------|------------|
| Query system (demand-driven pipeline) | 2,126 | ~1,400 | Medium |
| CLI and diagnostics | 1,990 | ~1,300 | Medium |
| Testing system (subprocess, NDJSON) | 9,968 | ~6,500 | Medium |
| Formatter | 1,181 | ~770 | Low |
| **Total** | **15,265** | **~9,970** | **Medium** |

**Query system port**: The query system is a demand-driven memoization layer over
the compilation pipeline. It is relatively straightforward to port — the core
abstraction is a typed key-value cache with fingerprint-based invalidation. The
main complexity is the incremental compilation cache (`.incr-cache/incr.bin`);
the binary format must be stable across the C++ and TML implementations.

**Bootstrap verification procedure**:

```
Stage 0: C++ compiler (tml.exe)
  — permanent reference, NEVER deleted
  — used to bootstrap forever if needed

Stage 1: tml.exe compiles TML compiler source → tml-stage1.exe
  — first TML-compiled TML compiler
  — ~30 min compile time expected

Stage 2: tml-stage1.exe compiles TML compiler source → tml-stage2.exe
  — TML compiler compiled by itself
  — must produce identical output to Stage 1

Verification: diff(Stage1 IR for all test files, Stage2 IR for all test files)
  — zero differences = bootstrap verified
  — any difference = bug in codegen determinism
```

The Stage 1 → Stage 2 comparison is the definitive test of correctness. If
tml-stage2 produces the same output as tml-stage1, the self-hosted compiler is
correct — any bug in tml-stage1 would propagate consistently to tml-stage2 and
would be caught by the regular test suite.

**Retained C++ code after Phase 5**:
- `compiler/runtime/core/essential.c` (I/O, panic, test harness) — kept forever
- `compiler/runtime/memory/mem.c` (malloc/free wrappers) — kept until allocator ported
- LLVM/LLD shim (~1,593 LOC) — kept until Era 2 and Era 3 complete

**Success criteria**:
- Stage 2 produces identical output to Stage 1 (IR-diff clean)
- All 1,700+ tests pass with tml-stage2
- Build time of tml-stage2 within 2x of tml.exe (C++)
- `tml-stage2.exe` is the new default compiler for further development

**SELF-HOSTING ACHIEVED** at end of Phase 5.

---

## Era 2: Custom Native Backend (Months 24–48)

**Goal**: Replace LLVM with a custom native code generator. Eliminates the 100MB+
LLVM dependency and reduces binary size from ~100MB to ~5MB.

**Can start in parallel with Era 1 Phase 4** — backend work is independent of
self-hosting. The MIR is stable enough after Phase 3 to serve as the backend input.

---

### Phase 6: Debug Backend (Months 24–30)

**Purpose**: Build a minimal viable native backend — correct but unoptimized.
Proves the architecture and gets something running before investing in the register
allocator.

| Component | TML LOC | Complexity |
|-----------|---------|------------|
| MIR → machine IR lowering | ~2,000 | Medium |
| x86_64 instruction encoding (core subset) | ~3,000 | High |
| Stack-only allocation (no register allocator) | ~500 | Low |
| PE/COFF object emission | ~2,500 | High |
| Section management (.text, .data, .rdata, .bss) | ~1,000 | Medium |
| **Total** | **~9,000** | **High** |

**Design decision**: Use a machine IR (MachIR) intermediate representation between
MIR and raw bytes. MachIR uses virtual registers (unlimited) and is later translated
to physical registers by the allocator in Phase 7. This separation makes the Phase 6
and Phase 7 work independent.

**Stack-only allocation**: Every virtual register maps to a stack slot. This is
correct but produces terrible performance (all values spilled). It is acceptable
for Phase 6 because the goal is correctness, not speed.

**x86_64 instruction subset for Phase 6**:
- MOV (register, immediate, memory)
- ADD, SUB, IMUL, IDIV
- CMP, JMP, JE, JNE, JL, JLE, JG, JGE
- CALL, RET
- PUSH, POP
- LEA

Extended instructions (SSE, AVX, atomics) are deferred to Phase 8.

**Reference implementations**:
- chibicc (5K LOC C) — minimal C compiler with clean x86_64 emission
- TCC (15K LOC C) — production-quality tiny C compiler
- qbe (13K LOC C) — production IR → x86_64/AArch64 backend

**Prerequisites**: MIR is stable (Phase 3 complete or nearing completion)
**Success criteria**: `tml build --backend=native hello.tml` produces a working binary
that passes the basic test suite (excluding performance-sensitive tests)
**Risk**: High — x86_64 encoding is complex (variable-length CISC instructions,
REX prefixes, ModRM/SIB bytes, RIP-relative addressing)

---

### Phase 7: Register Allocator (Months 30–36)

**Purpose**: Replace stack-only allocation with a real register allocator.
Generated code must be usable in production (within 3–5x of LLVM -O2).

| Component | TML LOC | Complexity |
|-----------|---------|------------|
| Liveness analysis (dataflow equations) | ~1,000 | High |
| Linear scan allocator (core algorithm) | ~2,000 | Very High |
| Calling convention enforcement (Win64 + SysV) | ~1,500 | High |
| Spill code generation | ~1,000 | Medium |
| Coalescing (merge same-value virtual registers) | ~500 | Medium |
| **Total** | **~6,000** | **Very High** |

**Algorithm choice**: Linear scan register allocation (Poletto/Sarkar, 1999).
Rationale: simpler than graph coloring, O(n log n) instead of NP-hard, produces
code within 5–10% of graph coloring for most programs. Used by LLVM's fast
register allocator and the JVM JIT.

**Calling conventions**:
- Win64: RCX, RDX, R8, R9 for integer args; XMM0–XMM3 for float args
- SysV AMD64: RDI, RSI, RDX, RCX, R8, R9 for integer args; XMM0–XMM7 for floats
- sret (large struct return): first argument is pointer to caller-allocated memory
- Callee-saved registers: Win64 uses RBX, RBP, RDI, RSI, R12–R15; SysV uses fewer

**Prerequisites**: Phase 6 complete
**Success criteria**:
- Generated code runs 3–5x faster than stack-only Phase 6 output
- All tests pass (register allocation must not change program semantics)
- Correct handling of Win64 and SysV calling conventions verified against C ABI tests

**Risk**: Very High. Register allocation is one of the hardest problems in compiler
engineering. The main risks are: incorrect liveness analysis (missed live ranges →
wrong spills), spill code generating incorrect values, and calling convention edge
cases causing ABI mismatches with C runtime functions.

---

### Phase 8: Production Backend (Months 36–42)

**Purpose**: Complete the x86_64 backend, add AArch64, and add basic optimizations.
LLVM becomes optional rather than required.

| Component | TML LOC | Complexity |
|-----------|---------|------------|
| Extended x86_64 (SSE2, SSE4.2, AVX2) | ~3,000 | High |
| x86_64 atomic instructions (LOCK prefix) | ~500 | Medium |
| AArch64 instruction encoder | ~4,000 | Medium |
| AArch64 register allocator integration | ~500 | Low |
| ELF object emission (Linux) | ~2,500 | Medium |
| Mach-O object emission (macOS) | ~2,000 | Medium |
| Basic peephole optimizations | ~1,500 | Medium |
| Constant propagation at MachIR level | ~1,000 | Medium |
| **Total** | **~15,000** | **High** |

**AArch64 advantage**: AArch64 is a RISC architecture with fixed-width instructions,
orthogonal register file, and simpler encoding than x86_64. AArch64 support is
easier to implement correctly and is critical for Apple Silicon (M-series) targets.

**Optimization scope for Phase 8**:
Phase 8 does NOT attempt to match LLVM -O2. The target is code quality comparable
to LLVM -O0 with local optimizations (peephole, constant propagation, dead store
elimination). Deep optimizations (vectorization, loop transforms, global value
numbering) are deferred to a future phase or kept as LLVM's domain.

**LLVM role after Phase 8**:
`--backend=native` (default) — uses Phase 6–8 native backend
`--backend=llvm` (available) — uses LLVM for maximum optimization
Users who need maximum performance can still opt into LLVM. The native backend
is the default for fast compilation (development builds).

**Prerequisites**: Phase 7 complete
**Success criteria**:
- Custom backend produces code within 2x of LLVM -O0 on benchmarks
- All platforms supported: Windows (PE/COFF), Linux (ELF), macOS (Mach-O)
- Both x86_64 and AArch64 targets passing full test suite

---

### Phase 9: Debug Information (Months 42–48)

**Purpose**: Emit debug information so debuggers (VS, WinDbg, lldb, gdb) can set
breakpoints, inspect variables, and step through TML source code.

| Component | TML LOC | Complexity |
|-----------|---------|------------|
| PDB format writer (Windows) | ~4,000 | Critical |
| DWARF format writer (Linux/macOS) | ~3,000 | High |
| Source location tracking through MIR | ~1,000 | Medium |
| Variable scope tracking | ~500 | Medium |
| Type info emission (CodeView/DWARF DIE) | ~1,500 | High |
| **Total** | **~10,000** | **Critical** |

**PDB complexity**: Microsoft's PDB format is partially documented (llvm-pdbutil,
reverse engineering work by randomascii and others). Key structures: DBI stream
(module info), TPI stream (type info, CodeView types), public/global symbols, and
section contributions. The llvm-pdb open-source implementation is the primary reference.

**DWARF by comparison**: DWARF is fully documented (dwarfstd.org), used by Linux/macOS.
DIE (Debug Information Entry) trees describe variables, types, scopes, and line tables.
DWARF 5 is the current standard.

**Source location propagation**: Every MIR instruction must carry a source span
(file, line, column) from the original TML source. This span is threaded through
HIR → THIR → MIR → MachIR → object file. Phase 9 adds the final step: emitting
these spans into the debug info sections.

**Prerequisites**: Phase 8 complete
**Success criteria**: Set breakpoint in VS Code on a TML function, step through it,
inspect local variable values — all working correctly
**Deliverable**: LLVM fully eliminated from the default build path

---

## Era 3: Custom Linker (Months 30–48)

**Goal**: Replace LLD with `tml-link`, a custom linker. Enables sub-10ms incremental
linking and eliminates the remaining large external dependency.

---

### Phase 10: PE/COFF Linker (Months 30–36)

**Purpose**: Build a Windows linker first (primary development platform).

| Component | TML LOC | Complexity |
|-----------|---------|------------|
| PE/COFF object file parser | ~2,000 | Medium |
| Symbol table construction | ~1,000 | Medium |
| Symbol resolution (strong/weak, duplicates) | ~1,000 | Medium |
| Relocation processing (all x86_64 reloc types) | ~1,500 | High |
| PE executable output emission | ~2,000 | High |
| Import library (.lib) parsing | ~1,000 | Medium |
| **Total** | **~8,500** | **High** |

**PE/COFF reference**: The PE/COFF specification is published by Microsoft
(pecoff_v83.docx). Key structures: COFF file header, optional header (PE32+),
section headers, symbol table, relocation table. The format is well-documented
and stable.

**Critical relocation types for x86_64 Windows**:
- `IMAGE_REL_AMD64_ADDR64` — absolute 64-bit address
- `IMAGE_REL_AMD64_REL32` — PC-relative 32-bit offset (used for near calls, branches)
- `IMAGE_REL_AMD64_REL32_1` through `_5` — PC-relative with addend

**Prerequisites**: Phase 6 complete (PE/COFF object emission from Phase 6 backend)
**Success criteria**: `tml-link foo.obj bar.obj -o foo.exe` produces a working binary
that passes the basic test suite; replaces LLD for Windows builds

---

### Phase 11: ELF Linker (Months 36–40)

| Component | TML LOC | Complexity |
|-----------|---------|------------|
| ELF object file parser (ELF64) | ~1,500 | Medium |
| Symbol resolution + symbol versioning | ~1,200 | Medium |
| Relocation processing (x86_64 + AArch64) | ~1,500 | High |
| GOT/PLT construction (dynamic linking) | ~1,500 | High |
| ELF executable + shared object output | ~1,500 | High |
| **Total** | **~7,200** | **High** |

**Dynamic linking complexity**: Shared libraries require Global Offset Table (GOT)
and Procedure Linkage Table (PLT) construction. Every call to an external symbol
goes through the PLT at runtime; the dynamic linker patches the GOT at load time.
This is the hardest part of the ELF linker.

**Prerequisites**: Phase 10 complete; Phase 8 ELF emission working
**Success criteria**: `tml-link` produces working ELF executables and shared objects
on Linux; replaces LLD for Linux builds

---

### Phase 12: Mach-O Linker (Months 40–44)

| Component | TML LOC | Complexity |
|-----------|---------|------------|
| Mach-O object file parser (64-bit) | ~1,500 | Medium |
| Symbol resolution (two-level namespace) | ~1,000 | High |
| Mach-O output (LC_SEGMENT, LC_SYMTAB, etc.) | ~1,500 | High |
| Code signing (ad-hoc, for development) | ~800 | Critical |
| **Total** | **~4,800** | **High** |

**Code signing requirement**: macOS Monterey+ requires all executables to have at
minimum an ad-hoc code signature. The `LC_CODE_SIGNATURE` load command and SHA-256
hash of the `__TEXT` segment must be present. Without this, the binary is killed
by the kernel on launch.

**Prerequisites**: Phase 11 complete; Phase 8 Mach-O emission working
**Success criteria**: Binaries execute on macOS without code signing errors; replaces
LLD for macOS builds

---

### Phase 13: Incremental Linking (Months 44–48)

**Purpose**: Track link state across builds and only re-link what changed.
Target: < 10ms re-link after a single-file change.

| Component | TML LOC | Complexity |
|-----------|---------|------------|
| Link state database (per-output binary) | ~1,500 | Medium |
| Section-level change detection | ~1,000 | Medium |
| Delta relocation recomputation | ~1,500 | High |
| In-place binary section patching | ~1,500 | High |
| **Total** | **~5,500** | **High** |

**Architecture**: The linker stores a `link.state` file alongside each output binary.
This file records: the symbol table, section layout, relocation decisions, and
fingerprints of each input object. On re-link, only changed sections are re-processed
and patched into the existing binary. Symbol addresses that didn't change require no
work.

**Performance target**: Single file change → re-link in < 10ms. This is achievable
because most re-links only modify 1–3 sections (typically `.text` of the changed
module plus updated `.pdata`/`.rdata`).

**Prerequisites**: Phases 10–12 complete (all three format linkers working)
**Success criteria**: `tml build hello.tml` followed by minor edit followed by
`tml build hello.tml` completes in < 10ms; binary is correct
**Deliverable**: LLD fully eliminated; `tml-link` is the default linker across
Windows, Linux, and macOS

---

## Era 4: C/C++ Frontend (Months 36–60)

**Goal**: TML can compile C and C++ code directly, completing the toolchain independence.
With a C frontend, TML can compile its own C runtime (`essential.c`, `mem.c`) and
eventually build all C dependencies (SQLite, OpenSSL, zlib) from source.

---

### Phase 14: C Preprocessor (Months 36–40)

**Purpose**: Build a standards-compliant C preprocessor as a standalone TML module.
This is a prerequisite for both the C and C++ frontends.

| Component | TML LOC | Complexity |
|-----------|---------|------------|
| `#include` with search path resolution | ~1,000 | Medium |
| `#define` object-like and function-like macros | ~1,500 | High |
| `#if` / `#ifdef` / `#elif` / `#endif` | ~800 | Medium |
| Token pasting (`##`) and stringification (`#`) | ~500 | High |
| Predefined macros (`__FILE__`, `__LINE__`, etc.) | ~300 | Low |
| **Total** | **~4,100** | **Medium-High** |

**Key complexity**: C macro expansion has well-known edge cases around rescan
prevention (a macro should not expand to itself), argument prescan order, and
stringification of macro arguments. The C11 standard (§6.10) is authoritative.

**Output**: Token stream (no AST). The preprocessor output feeds directly into
the C lexer and parser.

**Prerequisites**: Phase 5 (self-hosting) for the TML compiler to compile the preprocessor
**Success criteria**: `tml-cc -E file.c` produces output identical to GCC's preprocessor
for the TML runtime C files (`essential.c`, `mem.c`)

---

### Phase 15: C17 Parser and Type Checker (Months 40–46)

**Purpose**: Implement a full C17 compiler frontend that emits MIR.

| Component | TML LOC | Complexity |
|-----------|---------|------------|
| C lexer | ~1,200 | Low |
| C parser — declarations | ~2,500 | High |
| C parser — expressions | ~2,000 | Medium |
| C parser — statements | ~1,500 | Medium |
| C type checker (integer promotions, conversions) | ~3,500 | High |
| C ABI lowering (struct layout, alignment) | ~1,500 | High |
| C → MIR lowering | ~4,000 | High |
| **Total** | **~16,200** | **High** |

**C type system key features**:
- Integer promotion rules (§6.3.1.1): integer types smaller than `int` promote
- Usual arithmetic conversions (§6.3.1.8): type unification for binary operators
- Struct layout: platform-specific alignment, padding, bitfields
- Pointer arithmetic: pointer+integer, pointer subtraction, void pointer rules

**C → MIR mapping**:
C constructs map cleanly to MIR because C is the original inspiration for SSA IR.
`goto` maps to unconditional branch, `switch` maps to MIR switch, `setjmp`/`longjmp`
require special handling (unwind tables). C's type punning via union requires
careful `bitcast` handling in MIR.

**Prerequisites**: Phase 14 complete; Phase 5 (self-hosting)
**Success criteria**: `tml-cc essential.c mem.c -o tml_runtime.lib` produces a binary-
compatible runtime library; all TML programs linked against tml-cc-compiled runtime
pass the test suite

---

### Phase 16: C++ Subset (Months 46–54)

**Purpose**: Implement C++ classes, templates, namespaces, overloading, and RAII.
This phase enables compiling the TML compiler's own C++ dependencies from source.

| Component | TML LOC | Complexity |
|-----------|---------|------------|
| C++ parser — class declarations | ~3,000 | High |
| C++ parser — template declarations | ~4,000 | Very High |
| C++ parser — namespaces, using | ~1,000 | Medium |
| C++ type checker — overload resolution | ~5,000 | Very High |
| C++ type checker — template instantiation | ~5,000 | Critical |
| C++ template specialization (partial + full) | ~2,000 | Very High |
| RAII — destructor insertion, scope tracking | ~2,000 | High |
| C++ → MIR lowering | ~8,000 | Very High |
| **Total** | **~30,000** | **Very High** |

**Template instantiation complexity**: C++ templates are Turing-complete and have
notoriously complex instantiation semantics (SFINAE, two-phase name lookup, partial
ordering of specializations). The implementation scope for Phase 16 is limited to
the subset required to compile LLVM headers — which, while large, is a defined
target rather than full C++17 template metaprogramming.

**Target**: Compile enough C++ to build the LLVM headers and LLD, enabling complete
elimination of pre-built LLVM binaries from the toolchain.

**Prerequisites**: Phase 15 complete
**Success criteria**: `tml-cxx` can compile a representative subset of LLVM (the
headers + one backend); resulting binary passes correctness tests

---

### Phase 17: Full C++20 (Months 54–60)

**Purpose**: Complete C++20 support. Concepts, ranges, coroutines, modules.
This phase is community-driven — the core team provides infrastructure,
the community implements features.

| Component | TML LOC | Complexity |
|-----------|---------|------------|
| Concepts (constraints, requires expressions) | ~5,000 | High |
| Ranges library support (range-based for) | ~3,000 | Medium |
| Coroutines (`co_await`, `co_yield`, `co_return`) | ~8,000 | Very High |
| Modules (`import`, `export module`) | ~6,000 | High |
| **Total** | **~22,000** | **Very High** |

**Prerequisites**: Phase 16 complete
**Success criteria**: Compile the LLVM codebase with `tml-cxx`; produce identical
object files (or IR-diff equivalent) to MSVC/Clang

---

## Timeline (Gantt View)

```
Month:  0    4    8    12   16   20   24   28   32   36   40   44   48   52   56   60
        |    |    |    |    |    |    |    |    |    |    |    |    |    |    |    |
Era 1:  [====][====][========][==========][========][====]
        P0   P1       P2          P3         P4      P5
                                                     ^ SELF-HOSTED (Month 30)

Era 2:                                [====][======][==========][========]
                                      P6    P7          P8          P9
                                                                    ^ LLVM-FREE (Month 48)

Era 3:                                     [==========][========][========][======]
                                           P10          P11       P12       P13
                                                                            ^ LLD-FREE (Month 48)

Era 4:                                               [====][==========][============][====]
                                                     P14    P15          P16          P17
                                                                                      ^ FULL (Month 60)
```

**Parallel opportunities** (team of 2–3 developers):
- Era 1 Phase 2 sub-phases 2a+2b can be developed in parallel
- Era 2 can begin at Month 24 while Era 1 Phase 4 is in progress
- Era 3 can begin at Month 30 while Era 2 Phase 7 is in progress
- Era 4 Phase 14 can begin at Month 36 while Era 2 Phase 8 is in progress
- With 3 developers, total horizon compresses to ~42 months (2026–2030)

---

## Dependency Graph

```
Phase 0 (Foundation)
    |
    v
Phase 1 (Frontend TML)
    |
    v
Phase 2 (Type System TML)
    |
    v
Phase 3 (IR Pipeline TML) -----> Phase 6 (Debug Backend)
    |                                  |
    v                                  v
Phase 4 (Codegen TML)          Phase 7 (Register Allocator)
    |                                  |
    v                                  v
Phase 5 (Bootstrap)            Phase 8 (Production Backend) --> Phase 10 (PE Linker)
    |                                  |                             |
    v                                  v                             v
SELF-HOSTED                    Phase 9 (Debug Info)          Phase 11 (ELF Linker)
                                       |                             |
                                       v                             v
                               LLVM-FREE                     Phase 12 (Mach-O Linker)
                                                                     |
                                                                     v
                                                             Phase 13 (Incremental)
                                                                     |
                                                                     v
                                                               LLD-FREE

Phase 5 (Bootstrap) ------------> Phase 14 (C Preprocessor)
                                         |
                                         v
                                  Phase 15 (C17 Frontend)
                                         |
                                         v
                                  Phase 16 (C++ Subset)
                                         |
                                         v
                                  Phase 17 (Full C++20)
                                         |
                                         v
                                    COMPLETE
```

---

## LOC Summary by Era

| Era | Phases | New TML LOC | C++ Replaced | External Deps Eliminated |
|-----|--------|-------------|--------------|--------------------------|
| Era 1: Self-Hosted | 0–5 | ~120,000 | ~184K C++ compiler | — |
| Era 2: Custom Backend | 6–9 | ~39,000 | — | LLVM (~100MB) |
| Era 3: Custom Linker | 10–13 | ~25,000 | — | LLD (~60MB) |
| Era 4: C/C++ Frontend | 14–17 | ~55,000 | — | Clang/GCC/MSVC |
| **Total** | **0–17** | **~239,000** | **~184K C++** | **LLVM + LLD + C/C++ compiler** |

---

## Risk Register

| Risk | Probability | Impact | Mitigation |
|------|-------------|--------|------------|
| Type checker invariants undocumented | High | Critical | Phase 0 invariant document mandatory |
| HM inference edge cases in Phase 2 | High | High | 300+ targeted edge-case tests from invariant doc |
| x86_64 encoding bugs in Phase 6 | Medium | High | Test every instruction with known disassembly |
| Register allocator liveness analysis bug | High | High | Property-based testing with random programs |
| PDB format incompatibilities in Phase 9 | Medium | Medium | Validate against VS debugger incrementally |
| C++ template instantiation scope creep | High | Medium | Define target as "compile LLVM headers" not "full C++17" |
| Bootstrap cycle not terminating | Low | Critical | IR-diff verification after each stage; Stage 0 (C++) never deleted |
| Timeline slip in Phase 2 (type checker) | High | High | 8 months budgeted; break into 4 sub-phases with independent milestones |

---

## Key Invariants Across All Phases

1. **Stage 0 (C++ compiler) is never deleted.** It is the permanent fallback and the
   foundation of the bootstrap chain. Even after full independence, it is kept as a
   verified reference.

2. **Every ported subsystem is verified with differential testing before integration.**
   No subsystem is declared "done" until its output matches the C++ equivalent for
   the full test suite.

3. **The IR-diff tool from Phase 0 is the source of truth.** Identical LLVM IR output
   means correct porting. Any IR difference, however small, must be investigated and
   resolved before proceeding to the next phase.

4. **Self-hosting is a prerequisite for Era 4.** The C/C++ frontend must be implemented
   in TML (not C++), so Phase 5 bootstrap must complete before Phase 14 begins.

5. **The TML standard library is not frozen during Era 1.** Features needed to implement
   the compiler (serialization, graph algorithms, string processing) will be added to
   `lib/std` as needed. This is expected and planned for.
