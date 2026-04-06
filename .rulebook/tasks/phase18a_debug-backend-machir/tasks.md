## Status: 0/20 items complete

## Phase 1: MachIR Data Types
- [ ] 1.1 Define `VirtualReg` type (wraps U64 ID, never reused, unlimited count)
- [ ] 1.2 Define `MachInst` enum (Mov, Add, Sub, Imul, Idiv, Cmp, Jcc, Call, Ret, Push, Pop, Lea, Spill, Reload)
- [ ] 1.3 Define `MachBlock` (ID, list of MachInst, successor block IDs)
- [ ] 1.4 Define `MachFunc` (name, list of MachBlock, virtual reg count, stack frame size)

## Phase 2: MIR → MachIR Lowering
- [ ] 2.1 Lower MIR arithmetic (BinOp Add/Sub/Mul/Div) → MachInst with fresh VirtualRegs
- [ ] 2.2 Lower MIR memory ops (Load, Store, Alloca) → MachInst with stack slot references
- [ ] 2.3 Lower MIR control flow (Goto, Branch, Switch) → MachBlock edges + Jcc/JMP
- [ ] 2.4 Lower MIR function calls (CallInst) → MachInst::Call + arg/return VirtualReg assignments
- [ ] 2.5 Lower MIR comparisons (Eq, Ne, Lt, Le, Gt, Ge) → CMP + Jcc sequence
- [ ] 2.6 Lower MIR phi nodes → parallel-copy sequences at block predecessors

## Phase 3: Stack-Only Register Allocation
- [ ] 3.1 Assign each VirtualReg a unique stack slot (8-byte aligned, no sharing)
- [ ] 3.2 Insert Spill before each instruction that defines a VirtualReg
- [ ] 3.3 Insert Reload before each instruction that uses a VirtualReg
- [ ] 3.4 Verify every VirtualReg reference is replaced by a stack slot offset

## Phase 4: Stack Frame Layout
- [ ] 4.1 Compute total frame size: count stack slots × 8 bytes, align to 16 bytes
- [ ] 4.2 Emit function prologue (PUSH RBP, MOV RBP RSP, SUB RSP frame_size)
- [ ] 4.3 Emit function epilogue (ADD RSP frame_size, POP RBP, RET)
- [ ] 4.4 Encode [RBP - offset] addressing for all stack slot references

## Phase 5: Testing
- [ ] 5.1 Lower 5 MIR programs (factorial, fib, hello world, struct return, loop) — verify MachIR structure matches expected block/inst count
- [ ] 5.2 Verify prologue/epilogue generated correctly for each test function — frame size divisible by 16, all VirtualRegs assigned slots
