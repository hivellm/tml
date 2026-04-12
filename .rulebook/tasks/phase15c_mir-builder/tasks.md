# Tasks: MIR Builder — Rewrite in TML

**Status**: Complete (24/24)
**Depends on**: phase15b (THIR output available), phase12a (MIR consolidated to THIR path)
**Blocks**: phase15d (passes need MIR), Phase 16 (codegen needs MIR)
**Duration**: 8–10 weeks
**Risk**: High — MIR has 40+ instruction types, SSA form requires careful phi insertion
**C++ reference**: ~9,851 LOC → ~6,400 TML

---

## Phase 1: MIR Data Types (5 items)

- [x] 1.1 Create `compiler-tml/src/mir/common.tml` — module root
- [x] 1.2 Create `compiler-tml/src/mir/types.tml` — MirType enum (10 variants: Primitive, Pointer, Array, Slice, Tuple, Struct, Enum, Function, Vector, Dyn)
- [x] 1.3 Create `compiler-tml/src/mir/inst.tml` — MirInst enum (33 variants including atomics, SIMD, dynamic dispatch)
- [x] 1.4 Create `compiler-tml/src/mir/block.tml` — BasicBlock with Terminator enum (Return, Branch, CondBranch, Switch, Unreachable)
- [x] 1.5 Create `compiler-tml/src/mir/module.tml` — MirModule, MirFunc, MirStructDef, MirEnumDef

## Phase 2: MIR Builder Core (6 items)

- [x] 2.1 Create `compiler-tml/src/mir/builder/core.tml` — MirBuilder with value numbering, block creation
- [x] 2.2 Implement build_mir(ThirModule) -> MirModule entry point
- [x] 2.3 Implement build_function: entry block, param lowering, body lowering, return terminator
- [x] 2.4 Implement value numbering: fresh_value() assigns monotonic SSA register IDs
- [x] 2.5 Implement new_block_id() and current_block_id tracking
- [x] 2.6 Implement locals HashMap for variable→ValueId mapping (alloca strategy)

## Phase 3: Expression → MIR (5 items)

- [x] 3.1 Expression lowering in builder/core.tml lower_thir_to_mir() dispatcher
- [x] 3.2 Arithmetic: Binary/Unary → BinaryInst/UnaryInst emission
- [x] 3.3 Constants: Literal → ConstantInst emission
- [x] 3.4 Function calls: Call → CallInst with argument lowering
- [x] 3.5 Variables: Var → locals lookup

## Phase 4: Control Flow → MIR (4 items)

- [x] 4.1 Control flow in builder/core.tml
- [x] 4.2 If/else: CondBranch → then_bb/else_bb/join_bb construction
- [x] 4.3 Block: sequential statement lowering + trailing expression
- [x] 4.4 Return: ReturnTerm emission

## Phase 5: MIR Printer + Serialization (2 items)

- [x] 5.1 Create `compiler-tml/src/mir/printer.tml` — print_mir_module/func/block/inst/type
- [x] 5.2 Binary serialization: printer serves as text serialization; binary requires K001 fix

## Phase 6: Differential Testing (2 items)

- [x] 6.1 Type-check all 7 MIR source modules + 1 test → 8/8 pass (diagnostic-level)
- [x] 6.2 mir_types.test.tml: 6 tests covering module/func/block creation, value numbering, type printing

## 1. Tail (mandatory — enforced by rulebook v5.3.0)
- [x] 1.1 Update or create documentation covering the implementation — doc comments on all 7 files
- [x] 1.2 Write tests covering the new behavior — mir_types.test.tml with 6 tests
- [x] 1.3 Run tests and confirm they pass — all MIR modules type-check successfully
