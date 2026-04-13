## Status: 22/22 items complete

## Phase 1: Encoding Infrastructure
- [x] 1.1 Implement ModRM byte builder (mod[2], reg[3], rm[3])
- [x] 1.2 Implement SIB byte builder (scale[2], index[3], base[3])
- [x] 1.3 Implement REX prefix (REX.W, REX.R/X/B for R8-R15 extension)
- [x] 1.4 Implement immediate helpers (imm8, imm32, imm64 little-endian, fits_imm8/32)
- [x] 1.5 Physical register constants (RAX=0 through R15=15) in machir.tml

## Phase 2: Data Movement
- [x] 2.1 Encode MOV r64,r64 (REX.W 0x89 ModRM) and MOV r64,imm64 (REX.W 0xB8+rd imm64)
- [x] 2.2 Encode MOV r64,[RBP-disp] and MOV [RBP-disp],r64 with disp8/disp32
- [x] 2.3 Encode LEA r64,[RBP-disp] (REX.W 0x8D ModRM)
- [x] 2.4 Encode PUSH r64 (0x50+rd) and POP r64 (0x58+rd) with REX.B for R8-R15

## Phase 3: Arithmetic
- [x] 3.1 Encode ADD/SUB r64,r64 and r64,imm32 (with imm8 short form)
- [x] 3.2 Encode IMUL r64,r64 (0x0F 0xAF) and IDIV r64 (0xF7 /7)
- [x] 3.3 Encode NEG, NOT, AND, OR, XOR r64,r64
- [x] 3.4 Encode SHL/SAR r64,imm8 (0xC1 shift group)

## Phase 4: Comparison and Branches
- [x] 4.1 Encode CMP r64,r64 and CMP r64,imm32 (with imm8 short form)
- [x] 4.2 Encode Jcc rel32 (0x0F 0x84-0x8F) for all 8 condition codes
- [x] 4.3 Encode JMP rel32/rel8, CALL rel32
- [x] 4.4 Encode RET (0xC3), NOP (0x90)

## Phase 5: MachIR → Bytes Emission
- [x] 5.1 Implement emit_func(MachFunc) → EmitResult with byte buffer
- [x] 5.2 Two-pass branch patching: pass 1 records block offsets, pass 2 patches rel32 displacements
- [x] 5.3 Forward reference handling: placeholder 0x00000000, back-patched via patch_imm32

## Phase 6: Testing
- [x] 6.1 22 encoding tests: ModRM (2), REX (2), imm helpers (2), MOV (2), ADD/SUB (2), IMUL (1), NEG (1), CMP (1), PUSH/POP (1), RET (1), NOP (1), JMP (1), CALL (1), encoding byte verification against Intel SDM reference
- [x] 6.2 All tests pass, byte sequences verified against known-correct encodings

## 1. Tail (mandatory — enforced by rulebook v5.3.0)
- [x] 1.1 Update or create documentation covering the implementation
- [x] 1.2 Write tests covering the new behavior
- [x] 1.3 Run tests and confirm they pass
