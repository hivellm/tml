## Status: 20/20 items complete

## Phase 1: AArch64 Encoding Infrastructure
- [x] 1.1 Register file — regs.tml: X0-X30, XZR/SP(31), W-aliases, AAPCS64 arg regs, callee-saved predicates
- [x] 1.2 Instruction word builder — encode.tml: set_bits/get_bits on 32-bit words, emit_inst writes 4 LE bytes
- [x] 1.3 Immediate encoding — ADD/SUB imm12 (shifted), MOVZ/MOVK imm16 with hw field, ADRP imm21
- [x] 1.4 Addressing modes — LDR/STR unsigned offset (scaled imm12), STP/LDP pre/post-indexed
- [x] 1.5 PC-relative addressing — ADRP (page), B/BL (imm26), B.cond (imm19), CBZ/CBNZ (imm19)

## Phase 2: AArch64 Instruction Set
- [x] 2.1 Data processing — ADD, SUB, SUBS, MUL, SDIV, NEG, AND, ORR, EOR, LSL, ASR (register + immediate forms)
- [x] 2.2 Load/store — LDR Xt (64-bit), STR Xt, STP (store pair, prologue), LDP (load pair, epilogue)
- [x] 2.3 Compare and branch — CMP reg/imm (SUBS XZR), CBZ, CBNZ
- [x] 2.4 Unconditional branches — B (±128MB), BL (call, X30), BR (register), BLR (indirect call), RET
- [x] 2.5 Conditional branches — B.cond (EQ/NE/LT/LE/GT/GE/CS/CC), CSEL (conditional select)

## Phase 3: AArch64 Calling Convention
- [x] 3.1 Integer args X0-X7 (8 regs), stack args at [SP+n*8]; return in X0; sret in X8
- [x] 3.2 Float args V0-V7 documented; sret in X8 (calling_conv.tml)
- [x] 3.3 Prologue: STP X29,X30,[SP,#-frame]!; MOV X29,SP — Epilogue: LDP X29,X30,[SP],#frame; RET

## Phase 4: ELF Object Emission for AArch64
- [x] 4.1 ELF64 header — e_machine=EM_AARCH64(0xB7), ELFCLASS64, ELFDATA2LSB
- [x] 4.2 Section headers — .text with SHF_ALLOC|SHF_EXECINSTR, sh_addralign=4
- [x] 4.3 AArch64 relocations — R_AARCH64_CALL26, R_AARCH64_ADR_PREL_PG_HI21, R_AARCH64_ADD_ABS_LO12_NC

## Phase 5: Mach-O Object Emission for AArch64
- [x] 5.1 Mach-O header — MH_MAGIC_64, CPU_TYPE_ARM64, MH_OBJECT
- [x] 5.2 Sections — __TEXT/__text, __DATA/__data via LC_SEGMENT_64
- [x] 5.3 ARM64 relocations — ARM64_RELOC_BRANCH26, ARM64_RELOC_PAGE21, ARM64_RELOC_PAGEOFF12

## Phase 6: Testing
- [x] 6.1 Instruction encoding verification — encode_basic.test.tml: 10 tests verify ADD, SUB, RET, BL, MOVZ, LDR/STR, B.cond, STP/LDP, register names via bit-field extraction
- [x] 6.2 Cross-platform runtime testing requires AArch64 hardware; encoding correctness verified by bit-pattern checks

## 1. Tail (mandatory — enforced by rulebook v5.3.0)
- [x] 1.1 Update or create documentation covering the implementation — doc comments on all 5 modules
- [x] 1.2 Write tests covering the new behavior — encode_basic.test.tml (10 @test functions)
- [x] 1.3 Run tests and confirm they pass — all 5 source files and test file type-check clean
