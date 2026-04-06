# Proposal: MIR Builder — Rewrite in TML

## Why

The MIR (Mid-level IR) builder is the central transformation stage in the TML compiler pipeline. It
converts the typed THIR representation into SSA-form basic blocks and typed instructions that all 52
optimization passes and the LLVM codegen consume. The current implementation spans ~9,851 LOC of C++
across thir_mir_builder*.cpp and builder/*.cpp. Porting it to TML moves a critical self-hosting
milestone forward and eliminates the largest C++ subsystem between THIR and codegen.

## What Changes

The C++ THIR→MIR path is replaced by a TML implementation in `compiler-tml/src/mir/`. The port
covers the full instruction set (40+ kinds), SSA value numbering, basic block construction, and
alloca-first strategy for locals (mem2reg promotes them to SSA registers in phase15d).

### Architecture

```
compiler-tml/src/mir/
  mod.tml              — module root, re-exports public API
  types.tml            — MirType enum (Primitive, Struct, Ref, Ptr, Func, Array, Tuple, ...)
  inst.tml             — MirInst enum: 40+ kinds (BinOp, UnaryOp, Call, Load, Store,
                          Alloca, GEP, Cast, Phi, Select, ExtractValue, InsertValue, ...)
  block.tml            — BasicBlock: label, List[MirInst], Terminator
  module.tml           — MirModule: List[MirFunc], type registry, globals
  printer.tml          — text format matching C++ mir_printer output
  builder/
    mod.tml            — MirBuilder struct + build() entry point
    lower_expr.tml     — THIR expressions → MIR instructions
    lower_control.tml  — if/loop/when → basic block CFG
```

### Key Design Decisions

- **MirInst as enum with 40+ variants** — one variant per instruction kind, matches C++ exactly
  so MIR-diff testing can compare instruction-by-instruction
- **Alloca-first, mem2reg-later** — local variables are emitted as alloca + store; the mem2reg
  pass (phase15d item 2.1) promotes them to SSA registers, matching the established C++ strategy
- **Phi nodes inserted by mem2reg, not the builder** — the builder never emits Phi directly;
  mem2reg rewrites allocas after the full function body is built
- **Value numbering via counter** — a monotonic I64 counter on MirBuilder assigns %0, %1, ...
  register names to instruction results, same as C++ hir_mir_builder
- **BasicBlock graph via BlockId** — blocks are stored in a List[BasicBlock] and referenced by
  index; terminators hold BlockId targets for branch/switch

## Impact

- Affected code: compiler/src/mir/ (replaced), compiler/src/codegen/ (now calls TML MIR API)
- Affected passes: all 52 MIR passes (phase15d) consume the new MirModule type
- Breaking change: NO — MIR-diff testing ensures output is instruction-identical to C++
- User benefit: self-hosting progress; TML MIR builder is inspectable and modifiable in TML

## Success Criteria

MIR-diff shows zero instruction differences between TML and C++ builder output on all stdlib
modules and the full test suite. The LLVM IR produced from TML-built MIR is identical to the
IR produced from C++-built MIR (verified via IR-diff in phase15d item 6.2).

## Dependencies

- **Requires**: phase15b (ThirModule type available in TML), phase12a (single THIR→MIR path active)
- **Blocks**: phase15d (MIR passes need MirModule), Phase 16 (codegen needs MirModule)
- **Risk**: High — SSA construction requires correct phi insertion and alloca promotion.
  Mitigated by alloca-first strategy (defers SSA complexity to mem2reg) and instruction-level
  MIR-diff testing at each phase.
