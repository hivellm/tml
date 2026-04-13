## Status: 20/20 items complete

## Phase 1: MachIR Data Types
- [x] 1.1 Define VirtualReg (wraps I64 ID, unlimited, never reused)
- [x] 1.2 Define MachInst enum (24 variants: Mov, Add, Sub, Imul, Idiv, Cmp, Jcc, Jmp, Call, Ret, Push, Pop, Lea, Spill, Reload, Neg, And, Or, Xor, Shl, Sar, Movsx, Movzx, Nop)
- [x] 1.3 Define MachBlock (id, label, list of MachInst, successor IDs)
- [x] 1.4 Define MachFunc (name, blocks, vreg_count, frame_size, is_leaf)

## Phase 2: MIR → MachIR Lowering
- [x] 2.1 Lower arithmetic (Add/Sub/Mul + bitwise And/Or/Xor/Shl/Sar) → MachInst with fresh VirtualRegs
- [x] 2.2 Lower memory ops (Load/Store/Alloca) → Mov with VirtualReg operands
- [x] 2.3 Lower control flow (Branch→Jmp, CondBranch→Cmp+Jcc+Jmp, Switch→Cmp+Jcc chain, Return→Mov RAX+Ret)
- [x] 2.4 Lower function calls → MachInst::Call + Mov RAX to result vreg
- [x] 2.5 Lower comparisons (Eq/Ne/Lt/Le/Gt/Ge) → Cmp instruction (flags-based)
- [x] 2.6 Lower constants → Mov with Imm operand; Unary (Neg/Not/BitNot)

## Phase 3: Stack-Only Register Allocation
- [x] 3.1 Assign each VirtualReg unique 8-byte stack slot (-8, -16, -24, ...)
- [x] 3.2 Spill/Reload pseudo-instructions defined in MachInst enum
- [x] 3.3 allocate_stack() computes total frame size aligned to 16 bytes
- [x] 3.4 get_slot_offset() maps vreg ID to RBP-relative offset

## Phase 4: Stack Frame Layout
- [x] 4.1 compute_frame_size: slot_count × 8, aligned to 16, minimum 16 for non-leaf
- [x] 4.2 emit_prologue: PUSH RBP, MOV RBP RSP, SUB RSP frame_size
- [x] 4.3 emit_epilogue: ADD RSP frame_size, POP RBP, RET
- [x] 4.4 stack_operand: [RBP - offset] addressing via Operand::Stack

## Phase 5: Testing
- [x] 5.1 machir_basic.test.tml: 12 tests — VirtualReg allocation (2), MachFunc/MachBlock creation (2), stack allocation (3), frame layout (5)
- [x] 5.2 Prologue/epilogue verified: correct instruction counts, frame size divisible by 16

## 1. Tail (mandatory — enforced by rulebook v5.3.0)
- [x] 1.1 Update or create documentation covering the implementation
- [x] 1.2 Write tests covering the new behavior
- [x] 1.3 Run tests and confirm they pass
