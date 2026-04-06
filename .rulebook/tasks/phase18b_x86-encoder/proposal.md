# Proposal: phase18b — x86_64 Instruction Encoding

## Why

Phase 18a produces MachIR — an in-memory list of abstract machine instructions with virtual registers. Phase 18b converts that list to raw bytes. This is the lowest layer of the native backend: given a MachInst and physical register assignments (or stack slots from phase18a's stack-only allocator), produce the correct sequence of bytes that the x86_64 CPU will execute.

x86_64 instruction encoding is notoriously complex: variable-length instructions (1–15 bytes), REX prefixes for 64-bit operands, ModRM and SIB bytes for memory addressing, RIP-relative addressing for position-independent code. Getting these right is a prerequisite for every subsequent phase. Phase 18c (COFF emission) and Phase 19 (register allocator) both depend on this encoder being correct.

## What Changes

- New TML module `compiler/native/x86_encode.tml` — encoding functions for each instruction class
- New TML module `compiler/native/x86_emit.tml` — `emit_func(MachFunc) -> Buffer` top-level emitter with two-pass branch patching
- Helper types: `ModRM`, `SIB`, `REX`, `PhysReg` enum, `MemOperand` (base + displacement)
- No changes to existing LLVM backend or MIR

## Design Decisions

**Core subset only (Phase 18)**: MOV, ADD, SUB, IMUL, IDIV, NEG, NOT, AND, OR, XOR, SHL, SHR, SAR, CMP, TEST, Jcc, JMP, CALL, RET, PUSH, POP, LEA. SSE/AVX deferred to Phase 20a. This subset is sufficient to compile any TML program that uses only integers and pointers.

**Stack-slot operands only (Phase 18)**: The encoder in phase 18 works with the output of the stack-only allocator from phase 18a. Every operand is either a physical register (RSP, RBP, RAX for IDIV convention) or a [RBP-offset] memory reference. Phase 19 replaces the allocator; the encoder does not change.

**Two-pass branch patching**: Forward references require knowing the target block's byte offset before it is emitted. Pass 1 emits all instructions using placeholder displacements. Pass 2 patches rel8 and rel32 fields once all block offsets are known. rel8 vs rel32 selection: use rel8 if |displacement| <= 127, otherwise rel32 (re-emit with longer form — rare for small functions).

**RIP-relative addressing deferred**: Global variable references use RIP-relative addressing. For Phase 18, all globals are accessed via absolute addresses passed as imm64. RIP-relative for globals is added in Phase 20a.

## Impact

- Affected specs: docs/specs/native-backend.md (encoding reference tables)
- Affected code: compiler/native/ (new), no changes to existing paths
- Breaking change: NO — native backend is separate, LLVM path unaffected
- User benefit: Native backend can emit working x86_64 machine code for integer programs

## Risk

MEDIUM. x86_64 encoding has many edge cases: REX.W required for all 64-bit ops, RSP/RBP have special ModRM encodings, some opcodes encode the register in the low 3 bits of the opcode byte. Errors produce incorrect bytes that crash at runtime with no error message. The reference test (task 6.1) against known-correct bytes is the primary correctness guard.

## Reference

- Intel SDM Vol 2 (instruction set reference) — encoding fields defined per instruction
- chibicc codegen.c — minimal x86_64 encoder, ~300 lines, excellent reference
- TCC tccasm.c — complete encoder including all ModRM/SIB cases
- AMD64 ABI Vol 1 §3.2 — calling convention that drives register usage
