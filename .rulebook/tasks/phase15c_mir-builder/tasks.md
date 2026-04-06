# Tasks: MIR Builder — Rewrite in TML

**Status**: Planned (0/24)
**Depends on**: phase15b (THIR output available), phase12a (MIR consolidated to THIR path)
**Blocks**: phase15d (passes need MIR), Phase 16 (codegen needs MIR)
**Duration**: 8–10 weeks
**Risk**: High — MIR has 40+ instruction types, SSA form requires careful phi insertion
**C++ reference**: ~9,851 LOC → ~6,400 TML

---

## Phase 1: MIR Data Types (5 items)

- [ ] 1.1 Create `compiler-tml/src/mir/mod.tml` — module root
- [ ] 1.2 Create `compiler-tml/src/mir/types.tml` — `MirType` enum matching C++ mir.hpp (Primitive, Struct, Ref, Ptr, Func, Array, etc.)
- [ ] 1.3 Create `compiler-tml/src/mir/inst.tml` — `MirInst` enum: 40+ instruction kinds (BinOp, UnaryOp, Call, Load, Store, Alloca, GEP, Cast, Phi, etc.)
- [ ] 1.4 Create `compiler-tml/src/mir/block.tml` — `BasicBlock` struct: label, instructions list, terminator (Return, Branch, Switch, Unreachable)
- [ ] 1.5 Create `compiler-tml/src/mir/module.tml` — `MirModule` struct: functions (List[MirFunc]), types, globals

## Phase 2: MIR Builder Core (6 items)

- [ ] 2.1 Create `compiler-tml/src/mir/builder/mod.tml` — `MirBuilder` struct: current function, current block, value numbering counter
- [ ] 2.2 Implement `build(thir: ThirModule) -> MirModule` — entry point
- [ ] 2.3 Implement function building: create entry block, lower params, lower body, add return terminator
- [ ] 2.4 Implement value numbering: assign SSA register names (%0, %1, ...) to instruction results
- [ ] 2.5 Implement basic block creation: `new_block(label: Str) -> BlockId`, `set_current_block(id: BlockId)`
- [ ] 2.6 Implement alloca insertion: local variables start as allocas, mem2reg pass (15d) promotes to SSA later

## Phase 3: Expression → MIR (5 items)

- [ ] 3.1 Create `compiler-tml/src/mir/builder/lower_expr.tml` — THIR expr → MIR instructions
- [ ] 3.2 Implement arithmetic/comparison: `a + b` → `%r = add i64 %a, %b` instruction
- [ ] 3.3 Implement memory ops: field access → GEP, index → GEP, ref → alloca + store, deref → load
- [ ] 3.4 Implement function calls: lower args, emit Call instruction with calling convention
- [ ] 3.5 Implement closures: construct closure struct, capture variables, emit indirect call

## Phase 4: Control Flow → MIR (4 items)

- [ ] 4.1 Create `compiler-tml/src/mir/builder/lower_control.tml` — control flow → basic blocks
- [ ] 4.2 Implement if/else: condition → branch to then_bb/else_bb, merge at join_bb
- [ ] 4.3 Implement loop: header_bb → body_bb → latch_bb → back-edge to header, break → exit_bb
- [ ] 4.4 Implement when (match): switch on discriminant → one bb per arm, merge at join_bb

## Phase 5: MIR Printer + Serialization (2 items)

- [ ] 5.1 Create `compiler-tml/src/mir/printer.tml` — pretty-print MIR (text format matching C++ output)
- [ ] 5.2 Implement MIR binary serialization compatible with existing format in compiler/src/mir/serializer/

## Phase 6: Differential Testing (2 items)

- [ ] 6.1 Build MIR for 20 stdlib modules → MIR-diff against C++ output (instruction by instruction)
- [ ] 6.2 Build MIR for full test suite → verify zero diffs against C++ MIR builder
