## Status: 0/20 items complete

## Phase 1: AArch64 Encoding Infrastructure
- [ ] 1.1 Define AArch64 register file: X0-X30 (64-bit), W0-W30 (32-bit aliases), XZR (zero), SP, PC — 32 general-purpose registers
- [ ] 1.2 Implement fixed 32-bit instruction word builder with bit-field helpers (bits[hi:lo] = value)
- [ ] 1.3 Implement immediate encoding for AArch64: shifted imm12 (ADD/SUB), bitmask immediates (AND/ORR/EOR), MOVZ/MOVK for 64-bit constants
- [ ] 1.4 Implement AArch64 addressing modes: [Xn], [Xn, #imm], [Xn, Xm], pre/post-indexed [Xn, #imm]!
- [ ] 1.5 Implement PC-relative addressing: ADRP (page) + ADD (page offset) for globals, ADR for ±1MB range

## Phase 2: AArch64 Instruction Set
- [ ] 2.1 Encode data processing: ADD, ADDS, SUB, SUBS, MUL, SDIV, UDIV, NEG, AND, ORR, EOR, LSL, LSR, ASR
- [ ] 2.2 Encode load/store: LDR X, STR X (64-bit), LDR W, STR W (32-bit), LDP/STP (load/store pair for prologue/epilogue)
- [ ] 2.3 Encode compare and branch: CMP (alias SUBS XZR), CBNZ/CBZ (compare-and-branch-nonzero/zero), TBNZ/TBZ (test-bit-and-branch)
- [ ] 2.4 Encode unconditional branches: B (±128MB), BL (call, saves PC+4 to X30), BR (branch to register), BLR (call via register), RET (BX X30)
- [ ] 2.5 Encode conditional branches: B.EQ, B.NE, B.LT, B.LE, B.GT, B.GE, B.CS, B.CC and CSEL/CSINC (conditional select)

## Phase 3: AArch64 Calling Convention
- [ ] 3.1 Assign integer args to X0-X7 (first 8); additional args on stack; return value in X0 (or X0/X1 for 128-bit)
- [ ] 3.2 Assign float args to V0-V7; return float in V0; sret pointer in X8 (indirect result location register)
- [ ] 3.3 Emit AArch64 prologue: STP X29, X30, [SP, #-frame]!; MOV X29, SP — and epilogue: LDP X29, X30, [SP], #frame; RET

## Phase 4: ELF Object Emission for AArch64
- [ ] 4.1 Emit ELF64 file header with e_machine=0xB7 (EM_AARCH64), e_type=ET_REL
- [ ] 4.2 Emit .text, .data, .rodata sections with AArch64 section flags; emit sh_addralign=4 (.text) and sh_addralign=8 (.data)
- [ ] 4.3 Emit AArch64 relocations: R_AARCH64_CALL26 (BL instructions), R_AARCH64_ADR_PREL_PG_HI21 + R_AARCH64_ADD_ABS_LO12_NC (ADRP+ADD global refs)

## Phase 5: Mach-O Object Emission for AArch64
- [ ] 5.1 Emit Mach-O 64-bit object file header with CPU_TYPE_ARM64, CPU_SUBTYPE_ARM64_ALL
- [ ] 5.2 Emit __TEXT/__text, __DATA/__data, __TEXT/__cstring sections with correct Mach-O flags and alignment
- [ ] 5.3 Emit ARM64 relocations: ARM64_RELOC_BRANCH26, ARM64_RELOC_PAGE21, ARM64_RELOC_PAGEOFF12

## Phase 6: Testing
- [ ] 6.1 Cross-compile test programs from x86_64 host: verify emitted AArch64 bytes decode correctly via binutils aarch64-linux-gnu-objdump
- [ ] 6.2 Run native backend integration test suite on an AArch64 target (Apple Silicon Mac or ARM64 Linux): verify hello world, factorial, and struct-return programs produce correct output

## 1. Tail (mandatory — enforced by rulebook v5.3.0)
- [ ] 1.1 Update or create documentation covering the implementation
- [ ] 1.2 Write tests covering the new behavior
- [ ] 1.3 Run tests and confirm they pass
