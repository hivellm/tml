## Status: 0/22 items complete

## Phase 1: Encoding Infrastructure
- [ ] 1.1 Implement `ModRM` byte builder (mod[2], reg[3], rm[3] fields, addressing mode enum)
- [ ] 1.2 Implement `SIB` byte builder (scale[2], index[3], base[3] fields)
- [ ] 1.3 Implement `REX` prefix builder (REX.W for 64-bit ops, REX.R/X/B for register extension to R8-R15)
- [ ] 1.4 Implement immediate encoding helpers (imm8, imm16, imm32, imm64 → little-endian bytes appended to Buffer)
- [ ] 1.5 Define physical register enum (RAX=0, RCX=1, RDX=2, RBX=3, RSP=4, RBP=5, RSI=6, RDI=7, R8-R15)

## Phase 2: Data Movement
- [ ] 2.1 Encode `MOV r64, r64` (REX.W 0x89 ModRM) and `MOV r64, imm64` (REX.W 0xB8+rd imm64)
- [ ] 2.2 Encode `MOV r64, [RBP-disp]` and `MOV [RBP-disp], r64` with disp8 and disp32 forms
- [ ] 2.3 Encode `LEA r64, [RBP-disp]` (REX.W 0x8D ModRM displacement) for stack address loads
- [ ] 2.4 Encode `PUSH r64` (0x50+rd, REX.B prefix for R8-R15) and `POP r64` (0x58+rd)

## Phase 3: Arithmetic
- [ ] 3.1 Encode `ADD r64, r64`, `ADD r64, imm32`, `SUB r64, r64`, `SUB r64, imm32`
- [ ] 3.2 Encode `IMUL r64, r64` (REX.W 0x0F 0xAF ModRM) and `IDIV r64` (REX.W 0xF7 /7 — dividend in RDX:RAX)
- [ ] 3.3 Encode `NEG r64`, `NOT r64`, `AND r64, r64`, `OR r64, r64`, `XOR r64, r64`
- [ ] 3.4 Encode `SHL r64, CL`, `SHL r64, imm8`, `SHR r64, CL`, `SAR r64, CL` (shift group D2/D3/C1)

## Phase 4: Comparison and Branches
- [ ] 4.1 Encode `CMP r64, r64`, `CMP r64, imm32`, `TEST r64, r64`
- [ ] 4.2 Encode all Jcc rel8 (short) and rel32 (near) forms: JE/JNE/JL/JLE/JG/JGE/JB/JBE/JA/JAE
- [ ] 4.3 Encode `JMP rel8`, `JMP rel32`, `JMP r64` and `CALL rel32`, `CALL r64`
- [ ] 4.4 Encode `RET` (0xC3) and `RET imm16` (0xC2 + imm16 for callee-cleanup conventions)

## Phase 5: MachIR → Bytes Emission
- [ ] 5.1 Implement `emit_func(MachFunc) -> Buffer` — iterate MachBlocks, emit each MachInst to a growing Buffer
- [ ] 5.2 Implement two-pass branch patching: pass 1 records block start offsets, pass 2 patches all Jcc/JMP displacements
- [ ] 5.3 Handle forward references: use 32-bit displacement placeholders (0x00000000), back-patch after all blocks emitted

## Phase 6: Testing
- [ ] 6.1 Encode 20 known instruction sequences, compare output bytes byte-for-byte against nasm/objdump reference
- [ ] 6.2 End-to-end: lower factorial MIR → MachIR (phase18a) → x86 bytes → write to executable memory page → call via FFI → verify return value
