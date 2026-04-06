# Custom Native Backend — Feasibility Analysis

**Date**: 2026-04-05
**Scope**: Full feasibility study for building a custom x86_64/AArch64 native code generator
written in TML — eliminating all external backend dependencies (LLVM, Cranelift, GCC) for
true self-hosting
**Purpose**: Inform Phase SH-5 (self-hosting) and Phase 6.4 (maximum backend autonomy)
**Status**: Planning — not yet started

---

## 1. Prior Art — Custom Compiler Backends

Building a production-quality native backend from scratch is one of the most ambitious
undertakings in software engineering. Seven projects provide concrete data points on scope,
duration, and lessons learned.

### 1.1 Reference Projects

| Project | Language | Backend Size | Targets | Time to Build | Key Lesson |
|---------|----------|-------------|---------|---------------|------------|
| Go compiler | Go | ~50K lines | 7 targets | ~2 years (initial) | SSA + rewrite rules: best quality/effort tradeoff |
| Zig self-hosted | Zig | ~30K lines (x86 alone) | 5 targets | 4+ years (ongoing) | Most ambitious; AArch64 faster than x86 to implement |
| DMD (D compiler) | D | ~20K lines | x86, x86_64 | Maintained 20+ years | Oldest actively maintained self-hosted backend |
| LuaJIT | C | ~10K lines | 6 targets | DynASM template approach | Fastest JIT: template instantiation beats hand-written encoding |
| V language | V | C backend primary | via C compiler | N/A | Transpiling to C is simpler but adds runtime dependency |
| 8cc / chibicc | C | ~5-7K lines | x86_64 only | Months (each) | Educational quality, proves x86_64 is tractable in a small codebase |
| tcc (Tiny C Compiler) | C | ~15K lines | x86, x86_64, ARM | ~1 year initial | Full C compiler with native backend; no optimization but correct |

### 1.2 Go Compiler Backend (Most Relevant)

Go's backend is the most directly relevant prior art. The Go compiler:

- Started as a translation of the original Plan 9 C compiler to Go
- Moved to a full SSA IR in Go 1.7 (2016) after two years of development
- Uses a **rewrite rules** system: `gen/*.rules` files define pattern → instruction
  substitutions in a declarative mini-language, compiled to Go by `rulegen`
- Backend size: ~50K lines of Go across `cmd/compile/internal/ssa/` (instruction selection,
  register allocation, scheduling) and per-architecture files in `cmd/compile/internal/{amd64,arm64,...}`

**Architecture:**
```
Go AST → SSA (generic) → rewrite rules (arch-specific lowering)
       → register allocation (linear scan + greedy) → machine code emission
```

**Key lessons for TML:**
1. The rewrite rules approach separates concerns: generic SSA optimizations run first,
   then a single pass of architecture rules converts generic instructions to machine
   instructions. TML should adopt this pattern rather than mixing arch-specific logic
   into instruction selection.
2. Linear scan register allocation is production-quality. Go uses a greedy allocator
   (not full linear scan) and ships high-quality binaries.
3. Supporting 7 targets required dedicated sub-teams per architecture. Plan to support
   2 targets initially (x86_64, AArch64) — each additional target is a major effort.
4. The 18-month transition from C backend to Go SSA backend involved hundreds of
   engineer-months across a large team. Starting fresh takes longer but produces cleaner
   architecture.

### 1.3 Zig Self-Hosted Backend

Zig's self-hosted compiler is the most ambitious current example of a new language building
its own backend from scratch. As of 2026:

- x86_64 backend (`src/arch/x86_64/`): ~30K lines of Zig
- AArch64 backend (`src/arch/aarch64/`): ~20K lines of Zig (started after x86, learned from it)
- RISC-V, WASM, SPIR-V: each ~8-15K lines
- Status: x86_64 is production-quality; AArch64 is nearing feature-complete
- The project took 4+ years from first commit to the self-hosted compiler replacing the
  C++ bootstrap compiler (the "stage1" compiler)

**Architecture:**

The Zig x86_64 backend uses a novel "machine IR" approach where it lowers Zig AIR (Analysis
IR, which is SSA-based) directly to machine code in a single pass per basic block. This
avoids a separate MIR layer but makes register allocation more complex.

**Key lessons for TML:**
1. AArch64 was significantly easier to implement than x86_64. Fixed-width instructions and
   regular encoding reduced the encoding complexity by roughly half. Consider starting with
   AArch64 if Apple Silicon matters more than Linux server compatibility.
2. The Zig team spent months on correct Windows PE/COFF output before moving to Linux ELF.
   Windows COFF + PDB is harder than Linux ELF + DWARF.
3. Debug info (PDB on Windows) was one of the hardest and most time-consuming parts —
   longer than the instruction encoder itself.
4. A trivial "debug backend" (every value spills to stack, no register allocation) was
   shipped first and used for months while the full backend was built. This pattern is
   validated and should be TML's Phase A target.

### 1.4 DMD — D Compiler Backend

DMD (Digital Mars D) has the oldest continuously maintained self-hosted backend. Walter
Bright wrote the original backend in C before D existed, then maintained it as D evolved.

- Backend size: ~20K lines in `compiler/src/dmd/backend/`
- Architecture: direct code generation from AST (no intermediate SSA)
- Supports x86 and x86_64 on Windows, Linux, macOS
- Has survived 20+ years of D language evolution with backwards compatibility

**Key lessons for TML:**
1. Direct AST-to-codegen (without SSA) works but limits optimization quality. TML's MIR
   (already SSA) is a better starting point than DMD's AST approach.
2. Accumulating 20 years of platform-specific quirks makes the codebase hard to understand.
   The custom backend must have strict architectural separation between instruction
   selection, encoding, and object file emission from day one.
3. DMD's lack of optimization compared to GDC/LDC (which use GCC/LLVM) is a meaningful
   quality gap. Users who need performance use GDC or LDC. TML must plan for this:
   a custom backend without an optimization layer will be noticeably slower than LLVM for
   compute-intensive code.

### 1.5 LuaJIT's DynASM Approach

LuaJIT uses DynASM — a macro assembler preprocessor that embeds assembly templates
directly in C code. The instruction encoding is handled by the preprocessor, not by
manually written bit-manipulation code.

```c
// DynASM template (mixed C + x86 assembly notation)
|.macro load64, dst, src
|  mov dst, [src]
|.endmacro
```

**Key lessons for TML:**
1. Template instantiation (DynASM approach) produces compact, correct instruction encoders
   faster than writing individual encoding functions by hand. A table-driven encoder in TML
   should adopt a similar principle: define each instruction form as a data record, then
   write a generic encoding engine that reads the records.
2. For a JIT compiler (which LuaJIT is), DynASM's overhead is paid at JIT startup, which
   is acceptable. For an AOT compiler (TML), the same table-driven principle applies —
   the table is read at compile time, not at runtime.

### 1.6 chibicc / 8cc — Educational x86_64 Backends

Rui Ueyama's `chibicc` (2020, ~5K lines) and `8cc` (2015, ~7K lines) are C compilers
with native x86_64 backends specifically designed to be readable and educational.

**Key lessons for TML:**
1. A basic x86_64 backend that handles the common instruction forms is ~5-7K lines of
   readable code with no external dependencies.
2. Most real-world code only uses ~200 of the ~1000+ x86_64 instruction forms. The 80/20
   rule applies strongly: 20% of instructions cover 80% of real code.
3. Starting with a minimal instruction set and expanding only when a test fails is
   effective — do not attempt to implement all ~1000 forms up front.
4. chibicc's code generation generates naive, correct code. It serves as a validation
   reference: if TML's custom backend produces the same bytes as chibicc for the same
   code, the encoding is correct.

### 1.7 Summary Assessment

```
Effort spectrum (x86_64 + AArch64, production quality, with debug info):

  chibicc (educational, no reg alloc)    ████░░░░░░░░░░░░░░░░░░   ~5K LOC,   months
  TCC (C compiler, full backend)         ████████░░░░░░░░░░░░░░  ~15K LOC,   ~1 year
  DMD (D, production, no opt)            ████████████░░░░░░░░░░  ~20K LOC,   ~2 years
  Zig x86_64 alone (full quality)        ██████████████████░░░░  ~30K LOC,   ~3 years
  Go (full, 7 targets, SSA, debug info)  ████████████████████░░  ~50K LOC,   2+ years

  TML estimate (2 targets, SSA input):
    Minimal (O0, x86_64 only, no debug)  ██████████░░░░░░░░░░░░  15-20K LOC, ~10-12 months
    Full (debug info, basic opts, 2 arch) ████████████████░░░░░░  31-49K LOC, ~15-22 months
```

---

## 2. Components of a Native Backend

A native backend decomposes into seven distinct components. Each has independent scope,
difficulty, and test strategies. They are listed in implementation dependency order.

### 2.1 Instruction Selection

**What it does**: Maps MIR instructions (add, load, call, branch, …) to machine instruction
sequences. For most instructions this is a 1:1 or 1:2 mapping. Complex cases (divide,
modulo, large immediates, condition codes) require multi-instruction sequences.

**Approaches:**

| Approach | Quality | Complexity | Notes |
|----------|---------|------------|-------|
| Big switch on MIR opcode | Low-Medium | Low | Sufficient for O0; TML Phase A |
| BURS (Bottom-Up Rewrite) | High | Very High | Too complex to start with |
| Rewrite rules (Go approach) | High | Medium | Recommended for Phase B |
| ISLE DSL (Cranelift) | Highest | Very High | Requires a DSL compiler |

For TML Phase A (trivial backend): big switch is correct and fast to implement. For
Phase B (quality backend): adopt Go's rewrite rules pattern — declarative rule files
compiled to a match function.

**Common MIR → x86_64 instruction mappings:**

```
BinOp(Add, I64, va, vb)  →  ADD r64, r64/m64/imm32
BinOp(Add, I32, va, vb)  →  ADD r32, r32/m32/imm8/imm32
BinOp(FAdd, F64, va, vb) →  ADDSD xmm, xmm/m64
BinOp(Mul, I64, va, vb)  →  IMUL r64, r64/m64/imm32
BinOp(Div, I64, va, vb)  →  CQO; IDIV r64
Load(I64, ptr)           →  MOV r64, [base + disp]
Store(ptr, val_i64)      →  MOV [base + disp], r64
Alloca(ty)               →  SUB RSP, N (in prologue); LEA r64, [RSP + offset]
Call(fn, args)           →  (arg setup); CALL fn; (result in RAX)
Branch(cond, t, f)       →  CMP/TEST; Jcc label_t; JMP label_f
Return(val)              →  MOV RAX, val; (epilogue); RET
```

**Estimate**: 4-6K lines of TML, MEDIUM difficulty

### 2.2 Register Allocation

**What it does**: Assigns physical registers (RAX, RBX, … on x86; X0, X1, … on AArch64)
to the virtual registers produced by instruction selection. When physical registers are
exhausted, inserts load/store ("spill") code around uses of evicted virtual registers.

**Algorithms:**

| Algorithm | Time | Quality | Spills | Complexity | Notes |
|-----------|------|---------|--------|------------|-------|
| Trivial (all-stack) | O(n) | Very low | Always | ~200 lines | Phase A target |
| Linear scan | O(n log n) | Good | Low | 2-3K lines | Go, V8 (initially) |
| Graph coloring (Chaitin) | O(n²) | Optimal | Minimal | 4-6K lines | GCC, LLVM PBQP |
| Backtracking (regalloc2) | O(n log n) | Near-optimal | Low | 3-5K lines | Cranelift |

**Phase A: Trivial allocator (all spill)**

Every virtual register is spilled to a stack slot. Before each use, load from the stack.
After each def, store to the stack. This is 200 lines and is guaranteed correct.

```
# Trivial allocator pseudo-code
for each function:
  assign_stack_slot(vreg) → [rbp - offset] for each virtual register
  for each instruction:
    before use of vregs: emit MOV register, [rbp - offset_i]
    after def of vregs:  emit MOV [rbp - offset_j], register
```

**Phase B: Linear scan**

Compute live intervals (the range of instructions over which each virtual register is
live), sort by start point, greedily assign physical registers by availability within
each interval. Evict the interval with the furthest end point when all registers are full.

Key sub-components:
1. **Liveness analysis** (~500 lines): reverse dataflow pass over the CFG to compute
   the set of live virtual registers at each instruction point
2. **Interval computation** (~300 lines): for each virtual register, the interval
   [first_def, last_use] in a linearized instruction order
3. **Greedy allocation** (~1K lines): assign physical registers by iterating sorted
   intervals, evicting when necessary
4. **Spill insertion** (~500 lines): for spilled virtual registers, insert load/store
   around each use/def

**Estimate**: 3-5K lines of TML, HIGH difficulty (primarily due to correctness
requirements — liveness bugs cause wrong code that only manifests under register pressure)

### 2.3 x86_64 Instruction Encoding

**What it does**: Converts abstract machine instructions (e.g., `MOV RAX, [RBX + 8]`)
into the raw byte sequences that the CPU executes.

x86_64 encoding is notoriously complex. Every instruction is 1-15 bytes, with multiple
optional prefix fields:

```
[Legacy Prefixes] [REX] [Opcode(s)] [ModR/M] [SIB] [Displacement] [Immediate]
```

**REX prefix** (required for 64-bit operands and registers R8-R15):

```
Bit:  7   6   5   4   3   2   1   0
     [ 0   1   0   0 | W | R | X | B ]
  W = 1: 64-bit operand size override
  R:     extends ModR/M.reg field by 1 bit (registers 0-7 → 0-15)
  X:     extends SIB.index field
  B:     extends ModR/M.rm or SIB.base field
```

**ModR/M byte** (encodes register addressing modes):

```
Bit:  7   6   5   4   3   2   1   0
     [ mod      | reg       | rm       ]
  mod = 11: register direct (reg-to-reg)
  mod = 01: memory with 8-bit signed displacement [rm + disp8]
  mod = 10: memory with 32-bit signed displacement [rm + disp32]
  mod = 00: memory indirect [rm], with exceptions (SIB, RIP-relative)
  reg:      register operand (or opcode extension for single-operand forms)
  rm:       register or memory operand
```

**SIB byte** (Scale-Index-Base, used for complex addressing modes):

```
Bit:  7   6   5   4   3   2   1   0
     [ ss       | index     | base     ]
  ss:    scale: 00=1, 01=2, 10=4, 11=8
  index: index register (RSP = no index when mod != 11)
  base:  base register (RBP = no base when mod = 00)
```

**VEX prefix** (2 or 3 bytes, used for SSE/AVX instructions):
Encodes up to 3 source operands and an extended opcode space. Required for most SSE/AVX
instructions. Omitting VEX when required produces an invalid encoding.

**Table-driven approach** (strongly recommended over ad-hoc bit manipulation):

```tml
// Define each instruction form as a data record
struct InstrEncoding {
    opcode:      U8
    ext_opcode:  Maybe[U8]      // secondary opcode byte (0F xx, etc.)
    opcode_reg:  Bool           // register encoded in opcode (+rb/+rd/+ro/+rw)
    reg_in_rm:   Bool           // reg field is destination, rm is source
    needs_rex_w: Bool           // force REX.W for 64-bit
    uses_vex:    Bool           // VEX-encoded (SSE/AVX)
    operand_1:   OperandKind    // Reg64, Reg32, Mem64, Imm8, Imm32, etc.
    operand_2:   OperandKind
    operand_3:   Maybe[OperandKind]
}
```

**Instruction coverage priority:**

| Priority | Forms | Instructions | % real code |
|----------|-------|-------------|-------------|
| Critical | ~50 | MOV, ADD, SUB, IMUL, IDIV, CMP, JMP, Jcc, CALL, RET, LEA | ~65% |
| High | ~40 | PUSH, POP, AND, OR, XOR, NOT, NEG, SHL, SHR, SAR, CDQ/CQO | ~22% |
| Medium | ~30 | MOVSX, MOVZX, CMOV, SETCC, TEST, NOP, XCHG | ~8% |
| Low | ~80 | MOVSD, ADDSD, MULSD, DIVSD, UCOMISD (SSE2 scalar float) | ~4% |
| Deferred | ~800 | AVX2 packed, AVX-512, crypto (AES-NI), PREFETCH | ~1% |

Reference implementations for validation:
- **asmjit** (~30K lines C++): full x86/x86_64 assembler; use as byte-output oracle for tests
- **Zydis** (~20K lines C): decoder; disassemble TML output and compare with expected mnemonics
- **Intel SDM** (Software Developer Manual): authoritative bit-level spec for all encodings

**Estimate**: 5-8K lines of TML, HIGH difficulty

### 2.4 ARM64 (AArch64) Instruction Encoding

**What it does**: Converts abstract machine instructions to the 4-byte AArch64 binary
encoding that runs on Apple Silicon, AWS Graviton, and ARM servers.

AArch64 is significantly simpler than x86_64 in one critical way: all instructions are
exactly 32 bits (4 bytes). No variable-length prefixes, no ModR/M, no SIB byte.

**Encoding structure:**

Most instruction classes follow a regular pattern where specific bit fields encode the
operation, source registers, destination register, and immediate values:

```
# 64-bit register ADD: ADD Xd, Xn, Xm
  31: sf=1 (64-bit)
  30-29: op=00
  28-24: 01011 (type = shifted register arithmetic)
  23-22: shift = 00 (LSL)
  21:    0
  20-16: Rm (5 bits, second source)
  15-10: imm6 = 000000 (shift amount)
   9-5:  Rn (5 bits, first source)
   4-0:  Rd (5 bits, destination)

# Encodes to: 0x8B[Rm][000000][Rn][Rd]
```

The regularity means a small set of encoding functions covers most instructions:

```tml
// Common encoding patterns
func encode_dp_reg(sf: U8, op: U8, rm: Reg, rn: Reg, rd: Reg) -> U32
func encode_dp_imm(sf: U8, op: U8, imm: U16, rn: Reg, rd: Reg) -> U32
func encode_load_store(sf: U8, v: U8, opc: U8, rn: Reg, rt: Reg, offset: I16) -> U32
func encode_branch(cond: Cond, offset: I32) -> U32
func encode_bl(offset: I32) -> U32
```

**AAPCS64 calling convention:**
```
Integer/pointer args:  X0-X7 (8 registers), then stack
Float args:            D0-D7, then stack
Return (integer):      X0 (+ X1 for 128-bit)
Return (float):        D0
Callee-saved:          X19-X28, X29 (frame pointer), X30 (link register = return address)
Caller-saved:          X0-X18, D0-D7, D16-D31
No shadow space        (unlike Windows x64)
Struct passing:        ≤16 bytes in X0+X1; larger via pointer in X0
```

**Estimate**: 3-5K lines of TML, MEDIUM difficulty

### 2.5 Calling Conventions

**What it does**: Implements the ABI rules governing argument passing, return values, and
register preservation. Every function prologue/epilogue and every call site must implement
these rules exactly. Calling convention bugs cause silent wrong results that manifest only
under specific conditions (specific struct sizes, register pressure, mixed caller/callee
code).

**Windows x64 calling convention:**

```
Integer/pointer arguments:  RCX, RDX, R8, R9, then stack (right-to-left)
Float arguments:            XMM0, XMM1, XMM2, XMM3, then stack
Shadow space:               32 bytes ALWAYS reserved on stack before CALL (caller allocates)
                            This space is available to the callee — it may spill regs there
Return (≤8 bytes, integer): RAX
Return (float):             XMM0
Return (>8 bytes, struct):  Caller allocates buffer; pointer passed hidden as first arg in RCX
                            Remaining args shift: RDX gets what was originally RCX, etc.
Callee-saved:               RBX, RBP, RDI, RSI, RSP, R12-R15, XMM6-XMM15
Caller-saved (clobbered):   RAX, RCX, RDX, R8-R11, XMM0-XMM5
Stack alignment:            16 bytes BEFORE the CALL instruction
                            (8 bytes after CALL, due to return address push)
```

**System V AMD64 (Linux, macOS x86_64) calling convention:**

```
Integer/pointer arguments:  RDI, RSI, RDX, RCX, R8, R9, then stack (right-to-left)
Float arguments:            XMM0-XMM7, then stack
No shadow space
Return (≤8 bytes, integer): RAX
Return (9-16 bytes):        RAX + RDX (two registers, split at 8-byte boundary)
Return (float):             XMM0
Return (>16 bytes, struct): sret via hidden first pointer arg in RDI
Callee-saved:               RBX, RBP, R12-R15
Caller-saved (clobbered):   RAX, RCX, RDX, RSI, RDI, R8-R11, XMM0-XMM15
Stack alignment:            16 bytes before CALL
```

**Struct classification (System V — the complex part):**

Structs must be classified into register categories before deciding how to pass them.
Each 8-byte chunk ("eightbyte") is classified as INTEGER, SSE, or MEMORY:

- If the struct is >16 bytes or has unaligned fields: MEMORY (pass via pointer)
- If all eightbytes are INTEGER class: pass in RDI+RSI (or RDX+RCX, etc.)
- If all eightbytes are SSE class: pass in XMM0+XMM1
- Mixed INTEGER+SSE: first eightbyte in integer reg, second in XMM

This classification algorithm is ~200 lines and is the most bug-prone part of System V
ABI implementation.

**Estimate**: 2-3K lines of TML, MEDIUM difficulty

### 2.6 Object File Emission

**What it does**: Writes the compiled machine code into a binary container format that the
linker processes. PE/COFF on Windows; ELF on Linux; Mach-O on macOS (deferred).

**PE/COFF (Windows .obj) structure:**

```
┌──────────────────┐
│  COFF Header     │  20 bytes: machine type (0x8664=x86_64), section count, timestamp,
│                  │  symbol table offset, symbol count, optional header size, flags
├──────────────────┤
│  Section Headers │  40 bytes each: 8-char name, virtual size, virtual addr, raw data size,
│                  │  raw data offset, reloc offset, line number offset, reloc count,
│                  │  line number count, characteristics flags
├──────────────────┤
│  .text section   │  Machine code bytes for all functions
├──────────────────┤
│  .rdata section  │  Read-only data: string literals, jump tables, vtables
├──────────────────┤
│  .data section   │  Mutable globals, initialized static data
├──────────────────┤
│  Relocations     │  Per section: array of {virtual_addr, symbol_idx, type}
│                  │  Types: IMAGE_REL_AMD64_ADDR64, ADDR32NB, REL32, REL32_1..5
├──────────────────┤
│  Symbol Table    │  Each entry: 8-char name or string table ref, value,
│                  │  section number, type, storage class, aux count
├──────────────────┤
│  String Table    │  4-byte total length, then null-terminated strings for names >8 chars
└──────────────────┘
```

COFF is specified in Microsoft's PE/COFF specification (publicly available as a PDF).
It is simpler than ELF — fewer section types, fewer relocation types.

**ELF (Linux .o) structure:**

```
┌──────────────────┐
│  ELF Header      │  64 bytes: magic (\x7fELF), class (2=64-bit), endian (1=LE),
│                  │  type (1=REL), machine (0x3e=x86_64), entry, phoff, shoff, flags,
│                  │  ehsize, phentsize, phnum, shentsize, shnum, shstrndx
├──────────────────┤
│  Section data    │  .text, .rodata, .data, .bss, .symtab, .strtab, .shstrtab,
│                  │  .rela.text, .rela.data (relocatable sections)
├──────────────────┤
│  Section Headers │  64 bytes each: sh_name (offset into .shstrtab), sh_type, sh_flags,
│                  │  sh_addr, sh_offset, sh_size, sh_link, sh_info, sh_addralign, sh_entsize
└──────────────────┘
```

Common ELF relocation types for x86_64:
- `R_X86_64_64` (type 1): 64-bit absolute symbol reference
- `R_X86_64_PC32` (type 2): 32-bit PC-relative reference (CALL, conditional jump)
- `R_X86_64_PLT32` (type 4): 32-bit PLT-relative (external function calls)
- `R_X86_64_32` (type 10): 32-bit absolute (data references)

**Estimate**: PE/COFF 2-3K lines MEDIUM; ELF 2-3K lines MEDIUM

### 2.7 Debug Information

**What it does**: Embeds source location metadata, type descriptions, and variable
location information so that debuggers can map machine instructions back to TML source code.
This is the hardest, most effort-intensive component of a custom backend.

**DWARF (Linux/macOS ELF and Mach-O):**

DWARF 5 is a well-documented standard. Key sections:

| Section | Contents | Priority |
|---------|---------|----------|
| `.debug_line` | Instruction offset → source file/line/column table | Critical |
| `.debug_info` | Hierarchical DIEs: compile unit, functions, variables, types | Critical |
| `.debug_abbrev` | Compressed encoding abbreviation table | Critical (required by .debug_info) |
| `.debug_str` | String pool for type/function/variable names | High |
| `.debug_aranges` | Address range index for fast debugger lookup | Medium |
| `.debug_loc` | Variable location expressions (register or stack offset) | Medium |
| `.eh_frame` | Call frame info for stack unwinding | High (required for LLDB) |

Minimum viable DWARF (enables `break main` and line stepping): `.debug_line` + `.debug_info`
(compile unit DIE + function DIEs only) + `.debug_abbrev` + `.debug_str` + `.eh_frame`.
Estimate: ~1-2K lines for this minimum.

Full DWARF (variable watching, type inspection in debugger): all sections above plus
location expressions for variables, full type DIEs for all TML types. Estimate: 3-5K lines.

**CodeView/PDB (Windows):**

PDB (Program Database) is Microsoft's debug information format, used by WinDbg, Visual
Studio, and the Windows crash reporter. It is a container format (MSF — Multi-Stream
Format) holding several typed streams:

| Stream | Contents |
|--------|---------|
| MSF header | Magic, block size, free block map, stream directory |
| Stream 1 (PDB header) | Named streams table, compiler version string |
| Stream 2 (TPI) | Type information records (struct, function, pointer, array types) |
| Stream 3 (DBI) | Debug info: module list, section contributions, source files |
| Stream 4 (IPI) | ID records: function names, source file IDs |
| Module streams | Per-.obj symbol records: S_LPROC32_ID, S_LOCAL, S_DEFRANGE_REGISTER |
| Line info streams | Per-module IP-to-source-line tables (CV_Lines records) |
| Named stream `/names` | Global string table |

The MSF format is partially documented in the LLVM source (`llvm/lib/DebugInfo/MSF/`)
and the CodeView type records in `cvinfo.h` (from Microsoft's DIA SDK). Approximately
60% of the format is formally documented; the rest requires reading LLVM's implementation
or empirical testing.

Implementation approach:
1. Implement MSF container writer first (pure file format, no debug content)
2. Implement TPI stream with basic types (void, int, pointer) only
3. Implement DBI stream with one module entry
4. Implement symbol records: S_LPROC32_ID (function start/end) — gives WinDbg function names
5. Implement line info records — gives source line stepping
6. Expand type records to cover all TML types
7. Add S_LOCAL + S_DEFRANGE_REGISTER for variable inspection

**DWARF estimate**: 3-5K lines of TML, HIGH difficulty
**PDB estimate**: 4-6K lines of TML, VERY HIGH difficulty

---

## 3. TML's MIR as Backend Input

TML's MIR is already an ideal starting point for a custom native backend. Most custom
backend projects start from a higher-level IR and must perform significant lowering. TML's
MIR is already at the right abstraction level.

### 3.1 What MIR Provides

```
MIR module
├── Functions
│   ├── Basic blocks (ordered list of instructions + explicit terminator)
│   ├── SSA values with concrete types (I32, I64, F32, F64, Ptr, Bool, struct types)
│   ├── Explicit control flow (no implicit fallthrough between blocks)
│   ├── Explicit terminators: Jump, Branch, Return, Unreachable, Switch
│   └── Source location annotations per instruction
├── Type definitions (struct layouts with field offsets pre-computed)
├── Extern declarations (C runtime functions with signatures)
└── Global data (static strings, global variables)
```

SSA form means every virtual register is assigned exactly once, which simplifies
liveness analysis and register allocation. No phi-node insertion is needed in the backend —
MIR already has phi instructions at join points.

Typed values mean instruction selection never needs to infer types. Every MIR instruction
knows the concrete types of its operands (I32, I64, F64, Ptr, etc.).

Lowered generics mean by the time MIR is produced, all `List[T]` instantiations are
concrete struct types like `{ptr: Ptr, len: I64, cap: I64}`. The custom backend never
encounters generic parameters.

### 3.2 What MIR Lacks (Backend Must Add)

| Missing | What Backend Must Do |
|---------|---------------------|
| Physical register assignment | Run register allocator over virtual registers |
| Stack frame layout | Assign stack offsets to Alloca instructions and spilled regs |
| Aggregate lowering | Decompose struct args/returns per calling convention |
| Instruction selection | Match each MIR opcode to an architecture instruction |
| Prologue/epilogue code | PUSH callee-saved regs, SUB RSP, MOV RBP, RSP |
| Object file format | Write COFF/ELF with sections, symbol table, relocations |
| Debug information | Generate DWARF/PDB from source location metadata |

### 3.3 MIR → Machine Code Data Flow

```
MIR function (SSA, typed, explicit CFG)
    │
    ▼ 1. Aggregate Lowering
    │   Decompose struct arguments/return values per ABI
    │   Insert sret pointer argument for large return types
    │   Classify each aggregate arg as register-class or memory-class
    │
    ▼ 2. Instruction Selection
    │   Match each MIR instruction to one or more machine instructions
    │   Virtual registers throughout (no physical reg assignment yet)
    │
    ▼ 3. Liveness Analysis
    │   Reverse dataflow over basic blocks
    │   Output: live set (set of virtual regs) at each instruction point
    │
    ▼ 4. Register Allocation
    │   Assign physical registers or stack slots to virtual registers
    │   Insert spill (store) and reload (load) instructions
    │
    ▼ 5. Prologue/Epilogue Insertion
    │   PUSH callee-saved registers
    │   SUB RSP, frame_size (align to 16 bytes before first CALL)
    │   (optional) MOV RBP, RSP
    │   Epilogue: ADD RSP, frame_size; POP callee-saved; RET
    │
    ▼ 6. Instruction Encoding
    │   Emit bytes for each abstract machine instruction
    │   Track instruction offsets (needed for branch targets + debug info)
    │
    ▼ 7. Relocation Recording
    │   Record fixup entries: {offset, symbol, type} for external references
    │   CALL targets → REL32 relocation; global data → ADDR64 relocation
    │
    ▼ 8. Object File Emission
        Write section headers, machine code, symbol table, relocations → .obj / .o
```

---

## 4. Effort Estimate Summary

Total estimated effort for a production-quality custom backend supporting x86_64 and
AArch64 with debug information:

| Component | Lines | Duration | Difficulty |
|-----------|-------|----------|------------|
| Instruction selection (big switch, O0) | 4-6K | 2-3 months | MEDIUM |
| Register allocator (trivial → linear scan) | 3-5K | 1-2 months | HIGH |
| x86_64 encoder (table-driven, ~200 common forms) | 5-8K | 2-3 months | HIGH |
| AArch64 encoder (fixed-width, regular encoding) | 3-5K | 2-3 months | MEDIUM |
| Calling conventions (Win x64 + SysV + AAPCS64) | 2-3K | 3-4 weeks | MEDIUM |
| PE/COFF object writer | 2-3K | 3-4 weeks | MEDIUM |
| ELF object writer | 2-3K | 2-3 weeks | MEDIUM |
| DWARF debug info (line tables + function DIEs) | 3-5K | 2-3 months | HIGH |
| PDB debug info (Windows — MSF + DBI + TPI) | 4-6K | 3-4 months | VERY HIGH |
| Basic peephole optimizations | 3-5K | 2-3 months | MEDIUM |
| **Total** | **31-49K** | **15-22 months** | |

**Comparison with alternative backends:**

| Backend | External Dep | Debug Build Speed | Release Quality | Integration Effort |
|---------|-------------|-------------------|-----------------|-------------------|
| LLVM (current) | C++ (external) | 200-400ms/file | Best | Done |
| Cranelift | Rust (external) | 20-50ms/file | 80-90% LLVM | 2-3 months |
| Custom (Phase A, O0, trivial) | None | 5-15ms/file | 40-60% LLVM | 10-12 months |
| Custom (Phase D, with debug info) | None | 10-30ms/file | 70-80% LLVM | 18-22 months |

---

## 5. Phased Implementation Strategy

The custom backend must be built in phases. Each phase is independently shippable and
can replace or supplement LLVM for its target use case before the next phase begins.

### Phase A — Trivial Backend (O0, x86_64 only, no debug info)

**Duration**: 4-6 months  
**Goal**: Compile any TML program to a correct (but slow) x86_64 executable with no external
backend dependencies

Deliverables:
1. Trivial register allocator: all values spill to stack (100% spill rate, ~200 lines)
2. MIR instruction selection: big switch, x86_64 only, ~50 critical instruction forms
3. x86_64 encoder: common instruction forms only (MOV, ADD, SUB, IMUL, IDIV, CMP,
   JMP/Jcc, CALL, RET, PUSH, POP, LEA)
4. Windows PE/COFF object writer: .text + .rdata + symbol table + REL32/ADDR64 relocations
5. Windows x64 calling convention only
6. Backend integration: `tml build --backend=custom` selects the new backend

Expected output quality: ~40-50% of LLVM O0 in terms of runtime performance. Binaries
will be larger than LLVM-compiled code due to all-spill register allocation. Functionally
correct.

Validation: every test in the TML test suite must pass when compiled with the custom
backend. Compare binary output byte-by-byte for deterministic functions against LLVM O0.

### Phase B — Register Allocation + Basic Optimizations

**Duration**: 3-4 months  
**Goal**: Produce competitive development-build quality comparable to Cranelift

Deliverables:
1. Liveness analysis (reverse dataflow over CFG)
2. Linear scan register allocator (sorted intervals, greedy allocation, spill insertion)
3. Peephole optimizer: eliminate adjacent redundant load/store pairs
4. Dead instruction elimination: remove instructions producing unused virtual registers
5. Constant folding: evaluate constant-only expressions at compile time

Expected output quality: ~70-80% of LLVM O0 runtime performance. Sufficient for
development builds where debugging correctness is the priority.

### Phase C — AArch64 Support

**Duration**: 2-3 months  
**Goal**: Correct binaries for Apple Silicon Macs and ARM64 Linux servers

Deliverables:
1. AArch64 encoder (~500 instruction forms, all fixed 4-byte width)
2. AAPCS64 calling convention (X0-X7 args, no shadow space)
3. ELF object writer for Linux AArch64
4. PE/COFF for Windows ARM64 (if needed — lower priority than Linux AArch64)
5. Platform detection: `tml build` automatically selects x86_64 or AArch64 encoder

Note: based on Zig's experience, AArch64 takes roughly half the time of x86_64 once
the infrastructure (instruction selection, register allocator, object file writers)
is already built.

### Phase D — DWARF Debug Info

**Duration**: 2-3 months  
**Goal**: Custom backend binaries are debuggable with LLDB/GDB on Linux

Deliverables:
1. `.debug_line`: source line number tables mapping instruction offsets to file/line/col
2. `.debug_abbrev`: abbreviation table (required by .debug_info)
3. `.debug_info`: compile unit DIE + function DIEs with parameter info
4. `.debug_str`: string pool for function and file names
5. `.eh_frame`: call frame information for stack unwinding (enables LLDB stack traces)

Minimum viable: line tables + function names. Users can set breakpoints by source line
and see TML function names in stack traces. Full variable inspection requires additional
location expressions (`.debug_loc`) and type DIEs.

### Phase E — PDB Debug Info (Windows)

**Duration**: 3-4 months  
**Goal**: Custom backend binaries are debuggable with WinDbg/Visual Studio on Windows

This is the highest-risk phase due to partial documentation of the PDB format.
Recommended incremental approach:

1. Implement MSF container writer (multi-stream file format, no debug content yet)
2. Add PDB header stream (named stream table, compiler version)
3. Add TPI stream with primitive types (void, int, bool, pointer)
4. Add DBI stream with module entry list and section contributions
5. Add per-module symbol records: S_LPROC32_ID (function start/end markers)
   → WinDbg can now show TML function names in stack traces
6. Add per-module line info (CV_Lines): IP-to-line mapping
   → WinDbg can now step by source line
7. Add S_LOCAL + S_DEFRANGE_REGISTER symbol records for local variable inspection
8. Expand TPI type records to cover all TML struct/enum types

Test each layer against WinDbg and `llvm-pdbutil dump` before proceeding to the next.

### Phase F — Advanced Optimizations

**Duration**: Ongoing  
**Goal**: Bring custom backend release-build quality to 85-95% of LLVM O2

Priority order:
1. **Function inlining** (highest impact): inline small, hot functions at call sites.
   Requires call graph, function size heuristics, inlining budget. ~2-3K lines.
2. **Better register allocation**: upgrade linear scan to backtracking allocator.
   ~2-3K additional lines.
3. **Loop-invariant code motion** (LICM): hoist computations out of loop bodies.
   ~2K lines.
4. **Instruction scheduling**: reorder independent instructions to hide memory latency.
   Requires a CPU scheduling model (latencies per instruction). ~3K lines.
5. **SIMD auto-vectorization**: detect scalar loops over contiguous arrays, emit SIMD.
   Most complex optimization. ~5-8K lines.

---

## 6. Self-Hosting Implications

The primary motivation for building a custom backend beyond removing external dependencies
is true self-hosting: the TML compiler compiles itself using only TML code.

### 6.1 Bootstrap Path

```
Stage 0 (today):
  C++ compiler (Clang/MSVC) + LLVM → compiles TML compiler
  TML compiler uses LLVM backend → produces correct executables

Stage 1 (Phase A complete):
  C++ compiler + LLVM → compiles TML compiler
  TML compiler → compiles custom backend (written in TML) using LLVM backend
  Custom backend → first machine code generated by TML-compiled code

Stage 2 (Phase A stable):
  Stage 1 custom backend → compiles the custom backend again (self-hosted compilation)
  Test: Stage 1 output and Stage 2 output must be bit-for-bit identical

Stage 3 (fully self-hosted):
  TML compiler (written in TML) + custom backend (written in TML)
  → compiles itself with no C++ compiler at runtime
  → only requires a C++ compiler for the initial bootstrap (like Go 1.5 required Go 1.4)
```

### 6.2 Reproducibility Testing

Bit-for-bit reproducibility between Stage 1 and Stage 2 is the gold standard proof of
correctness. The test:

```bash
# Build custom backend with LLVM (Stage 1)
tml build src/codegen/custom/ --backend=llvm -o stage1_backend.exe

# Build custom backend with Stage 1 custom backend (Stage 2)
./stage1_backend.exe build src/codegen/custom/ --backend=custom -o stage2_backend.exe

# Reproducibility check
sha256sum stage1_backend.exe stage2_backend.exe
# Both hashes must be identical (requires deterministic output: no timestamps, sorted symbols)
```

Go uses a three-stage bootstrap test (Stage 1 → Stage 2 → Stage 3 must all match).
Zig uses a similar approach. Achieving reproducibility proves the custom backend correctly
implements the same semantics as LLVM for all code patterns it encounters.

### 6.3 What Self-Hosting Unlocks

| Capability | With LLVM Backend | With Custom Backend |
|-----------|------------------|---------------------|
| Compile TML without C++ toolchain installed | No | Yes |
| Ship TML compiler as single static executable | No | Yes |
| Modify compiler in TML, rebuild with TML only | No | Yes |
| Bootstrap TML on a new platform from source | Requires LLVM port | Only requires new encoder |
| True language independence | No | Yes |

---

## 7. Risk Assessment

| Risk | Impact | Probability | Mitigation |
|------|--------|-------------|------------|
| x86_64 encoding bugs produce wrong code | HIGH | HIGH | Unit tests comparing byte output against NASM/asmjit for every instruction form |
| Register allocator liveness error | HIGH | MEDIUM | Validate liveness against brute-force checker on small test cases; fuzz with random programs |
| Windows PE/COFF relocation incorrectness | HIGH | MEDIUM | Compare against LLD-produced .obj files for the same code patterns |
| PDB format undocumented corner cases | MEDIUM | HIGH | Implement incrementally; test against WinDbg at each layer; use LLVM as reference |
| Calling convention ABI mismatch | HIGH | MEDIUM | Round-trip tests: C calling TML, TML calling C, for every arg/return type combination |
| Performance regression vs LLVM/Cranelift | MEDIUM | MEDIUM | Benchmark suite from day one; regress on each phase completion |
| Scope creep beyond phase boundaries | HIGH | HIGH | Hard gate: Phase N+1 does not start until Phase N passes the full test suite |
| x86_64 instruction forms missing (causes crash) | MEDIUM | LOW | Start with 200 critical forms; test-driven expansion (add form only when a test needs it) |
| AArch64 divergence from x86_64 behavior | LOW | MEDIUM | Shared test suite; same TML programs must produce identical outputs on both targets |
| Developer time underestimate | HIGH | HIGH | Zig spent 4+ years; Go spent 2+ years with larger teams. Add 50% buffer to all estimates |

### 7.1 Highest-Risk Sub-Problems in Detail

**x86_64 encoding correctness** is the largest technical risk. A single wrong bit in
a REX prefix, ModR/M byte, or opcode extension produces wrong code that can manifest as:
incorrect computation results (wrong output, silent), invalid instruction faults (crash),
or subtle ABI breakage (wrong register interpretation). Mitigation: write the instruction
encoder as a pure function `encode(instruction) -> bytes`, test every form in isolation
against a known-good reference (NASM, asmjit), and add integration tests that compare
full function output against LLVM O0.

**Calling convention ABI** is the second-largest risk. The Windows x64 shadow space rule
(32 bytes must always be allocated before the first argument, even for leaf functions) and
the sret register shift (sret pointer in RCX bumps all other args by one register) are the
two most commonly mis-implemented parts. Mitigation: a focused test suite that exercises
every combination of argument count, arg types (int, float, small struct, large struct),
and return type.

**PDB format scope** is the largest effort risk. The format has roughly 40% undocumented
behavior. The mitigation is strict scope control: implement only what makes WinDbg show
function names (S_LPROC32_ID records), declare that phase complete, and treat any further
PDB work as separate scope.

---

## 8. Recommendation

### 8.1 Feasibility Verdict

Building a custom native backend for TML is **technically feasible** and is validated by
prior art (Go, Zig, DMD, TCC). It is the most expensive and highest-risk option in the
backend strategy space. The question is not whether it can be done, but when it is the
right investment relative to other priorities.

**The custom backend is the right choice when:**
1. True self-hosting is a hard requirement (no external backend at runtime)
2. Cranelift's Rust dependency is unacceptable (embedded targets, supply chain constraints)
3. Target-specific control is required that neither LLVM nor Cranelift exposes

**Cranelift is the better choice when:**
1. Development speed is the priority: Cranelift delivers 80-90% of the benefit
   in 3-4 months vs 15-22 months for a custom backend
2. Self-hosting can be deferred: LLVM for release builds + Cranelift for development
   builds covers 95% of the use case without building a custom backend

### 8.2 Recommended Phased Strategy

```
Phase 1 (now — 0 months):
  LLVM backend — production release builds, all platforms, best code quality

Phase 2 (3-4 months from now):
  Cranelift backend — development builds, 5-10x faster compile cycles

Phase 3 (18+ months from now, only if self-hosting is committed):
  Custom backend Phase A — trivial x86_64, all-spill, no debug info
  Purpose: research prototype, validate pipeline, not a production deliverable

Phase 4 (only after Phase 3 evaluation):
  Decide based on Phase 3 experience: continue to Phase B-F, or stay with Cranelift
  If continuing: Phase B (register allocation) → Phase C (AArch64) → Phase D (DWARF)
  Phase E (PDB) is optional — Windows DWARF via LLDB is acceptable if PDB is too costly
```

### 8.3 Starting Point if Proceeding with Phase A

If the decision to proceed with Phase A is made, the recommended starting point:

1. **Study chibicc source first** — Ueyama's 5K-line compiler generates correct x86_64;
   use it as a mental model for the encoding layer and a byte-output oracle for tests
2. **Build the x86_64 encoder unit test suite before writing any encoder code** — define
   what byte sequences are expected for specific instructions; this test infrastructure
   drives and validates all encoding work
3. **Implement trivial register allocator (200 lines) and verify end-to-end** — a single
   simple function (add two I64 arguments, return) compiled to a working .obj file
   proves the entire pipeline is connected
4. **Add instruction forms test-by-test** — compile the TML test suite, find the first
   crash (missing instruction form), add that form, repeat. Never add forms speculatively
5. **Run the TML test suite weekly against the custom backend** from Phase A day one —
   regression testing from the start prevents accumulation of undetected correctness bugs

---

## 9. Component Architecture Diagram

```
TML MIR (SSA, typed, explicit CFG, pre-lowered generics)
    │
    ▼
┌────────────────────────────────────────────────────────────────────┐
│                  CustomBackend (written in TML)                     │
│                                                                     │
│  ┌─────────────────────────────────────────────────────────────┐   │
│  │  ABI Lowering                                                │   │
│  │  classify_arg(ty) → RegClass | MemClass                     │   │
│  │  insert_sret_arg(func) — add hidden pointer for big returns │   │
│  │  expand_struct_args(func) — decompose into scalar args      │   │
│  └─────────────────────────────────────────────────────────────┘   │
│                             │                                       │
│  ┌─────────────────────────────────────────────────────────────┐   │
│  │  Instruction Selection                                       │   │
│  │  lower_inst(MirInst) → List[MachInst]                       │   │
│  │  MachInst: abstract instruction + virtual register operands │   │
│  └─────────────────────────────────────────────────────────────┘   │
│                             │                                       │
│  ┌─────────────────────────────────────────────────────────────┐   │
│  │  Liveness Analysis + Register Allocation                     │   │
│  │  Phase A: trivial_alloc() — all values → stack slots        │   │
│  │  Phase B: liveness(cfg) → live_sets                         │   │
│  │           linear_scan(live_sets) → VReg → PReg|StackSlot    │   │
│  │           insert_spills(func, alloc)                        │   │
│  └─────────────────────────────────────────────────────────────┘   │
│                             │                                       │
│  ┌──────────────────────┐   │   ┌──────────────────────────────┐   │
│  │  x86_64 Encoder      │◄──┼──►│  AArch64 Encoder (Phase C)   │   │
│  │  Table-driven        │   │   │  Fixed 32-bit instructions    │   │
│  │  REX/ModRM/SIB/VEX  │   │   │  encode_dp_reg(sf,op,rd,rn,rm)│   │
│  │  ~200 critical forms │   │   │  encode_load_store(...)       │   │
│  └──────────────────────┘   │   └──────────────────────────────┘   │
│                             │                                       │
│  ┌──────────────────────┐   │   ┌──────────────────────────────┐   │
│  │  PE/COFF Writer      │   │   │  ELF Writer                  │   │
│  │  (Windows .obj)      │   │   │  (Linux .o)                  │   │
│  │  COFF header         │   │   │  ELF64 header                │   │
│  │  Section headers     │   │   │  Section headers + data      │   │
│  │  Symbol table        │   │   │  .symtab + .strtab           │   │
│  │  Relocations         │   │   │  .rela.text relocations      │   │
│  └──────────────────────┘   │   └──────────────────────────────┘   │
│                             │                                       │
│  ┌─────────────────────────────────────────────────────────────┐   │
│  │  Debug Info (Phase D/E, optional)                            │   │
│  │  DWARF: .debug_line, .debug_info, .debug_abbrev, .eh_frame  │   │
│  │  PDB:   MSF container, DBI stream, TPI types, CV_Lines      │   │
│  └─────────────────────────────────────────────────────────────┘   │
└────────────────────────────────────────────────────────────────────┘
    │
    ▼
.obj (Windows) or .o (Linux) → LLD linker → .exe / ELF executable
```

---

*Related documents: [04-cranelift-deep-dive.md](./04-cranelift-deep-dive.md) | [06-hybrid-strategy.md](./06-hybrid-strategy.md) | [08-self-contained-toolchain-design.md](./08-self-contained-toolchain-design.md) | [compiler-selfhosting/00-executive-summary.md](../compiler-selfhosting/00-executive-summary.md)*
