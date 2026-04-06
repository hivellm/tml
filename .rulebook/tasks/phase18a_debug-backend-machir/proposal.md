# Proposal: phase18a — MIR → MachIR Lowering

## Why

The TML compiler currently depends entirely on LLVM for code generation. LLVM is a ~500MB binary dependency with complex build requirements, slow compilation of the compiler itself, and an API surface that couples TML tightly to LLVM internals. ERA 2 eliminates this dependency by building a native backend in pure TML. Phase 18a establishes the foundation: a machine-level intermediate representation (MachIR) that sits between the existing MIR and raw bytes.

MachIR is the architectural separation point that makes the rest of ERA 2 possible. Phases 18b (encoding) and 19a (register allocation) operate entirely on MachIR — they never touch MIR. This means the register allocator can be swapped from stack-only (Phase 18, MVP) to linear scan (Phase 19) without changing any lowering logic.

## What Changes

- New TML module `compiler/native/machir.tml` — MachIR data types (VirtualReg, MachInst, MachBlock, MachFunc)
- New TML module `compiler/native/mir_lower.tml` — MIR → MachIR lowering pass
- New TML module `compiler/native/stack_alloc.tml` — stack-only register allocator (every VirtualReg → stack slot)
- New TML module `compiler/native/frame.tml` — stack frame layout, prologue/epilogue emission
- MachIR is NOT emitted to disk; it is an in-memory structure consumed by phase 18b encoder

## Design Decisions

**Unlimited virtual registers**: VirtualReg is a U64 counter. The lowering phase never reuses registers. This simplifies correctness — no SSA destruction needed. The allocator (phase 19) handles physical register assignment.

**Stack-only allocation as Phase 18 MVP**: Every VirtualReg gets its own 8-byte stack slot. This is correct but slow. The tradeoff is acceptable for phase 18 because correctness is the only goal. Linear scan in phase 19 replaces this path without changing MachIR.

**Phi node destruction via parallel copies**: MIR phi nodes are lowered to parallel-copy sequences inserted at block predecessors. This matches the standard SSA destruction algorithm and avoids swap cycles.

## Impact

- Affected specs: docs/specs/native-backend.md (new)
- Affected code: compiler/src/backend/ (new native/ subdirectory), compiler/src/cli/ (--backend=native flag stub)
- Breaking change: NO — LLVM backend remains default, native backend is opt-in via --backend=native
- User benefit: First step toward eliminating the 500MB LLVM dependency; faster compiler builds

## Risk

LOW. MachIR is a pure data structure transformation. It does not touch the parser, type checker, or existing codegen. If MachIR lowering produces wrong output, the only symptom is incorrect machine code — the existing LLVM path is unaffected. Tests verify MachIR structure directly without executing the output.

## Reference

- chibicc: IR → code generation in ~500 LOC (codegen.c)
- qbe: SSA → machine code lowering in amd64/isel.c
- TCC: tccgen.c register and stack slot management
