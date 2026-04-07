## Status: 0/22 items complete

## Phase 1: Liveness Analysis
- [ ] 1.1 Compute `def` and `use` sets for each MachBlock (which VirtualRegs are defined/used in the block)
- [ ] 1.2 Implement iterative dataflow for `live_in` and `live_out` sets (backward analysis, iterate until fixed point)
- [ ] 1.3 Compute live intervals: for each VirtualReg, record [first_def_position, last_use_position] as a half-open range over the linearized instruction sequence
- [ ] 1.4 Assign linear instruction numbers: number every MachInst sequentially across all blocks (block order = reverse post-order)

## Phase 2: Linear Scan Core
- [ ] 2.1 Sort live intervals by start position (ascending) into a priority queue
- [ ] 2.2 Implement `expire_old_intervals`: free physical registers when their interval's end position is before the current interval's start
- [ ] 2.3 Implement register assignment: pick the first available physical register from the free set; if none available, call spill decision
- [ ] 2.4 Track active intervals (sorted by end position) and free set (initially all caller-saved registers: RAX, RCX, RDX, R8, R9, R10, R11)
- [ ] 2.5 Implement interval splitting: when a register is needed across a call, split the interval and insert spill/reload around the call

## Phase 3: Spill Code Generation
- [ ] 3.1 When spilling: assign VirtualReg to a stack slot, insert `Spill` MachInst before the definition, insert `Reload` MachInst before each use
- [ ] 3.2 Implement spill heuristic: evict the active interval with the farthest end position (Poletto/Sarkar original algorithm)
- [ ] 3.3 Post-allocation: replace all remaining VirtualReg references with assigned PhysReg or stack slot operands

## Phase 4: Calling Convention Enforcement
- [ ] 4.1 Win64: assign first 4 integer args to RCX, RDX, R8, R9; remaining args on stack at [RSP+32], [RSP+40], ...
- [ ] 4.2 Win64: assign first 4 float args to XMM0-XMM3; return value in RAX (int) or XMM0 (float)
- [ ] 4.3 Mark callee-saved registers (RBX, RBP, RDI, RSI, RSP, R12-R15) — emit save/restore in prologue/epilogue if used
- [ ] 4.4 SysV AMD64: assign first 6 integer args to RDI, RSI, RDX, RCX, R8, R9; first 8 float args to XMM0-XMM7

## Phase 5: Register Coalescing
- [ ] 5.1 Identify copy instructions (MOV vr1, vr2) where vr1 and vr2 have non-overlapping live intervals
- [ ] 5.2 Coalesce: merge the two virtual registers into one, eliminating the MOV instruction

## Phase 6: Testing
- [ ] 6.1 Verify liveness analysis: for 3 test programs, manually compute expected live intervals and compare to computed output
- [ ] 6.2 Verify register allocation: check that no two intervals with overlapping ranges receive the same physical register
- [ ] 6.3 Benchmark stack-only (phase18) vs linear scan (phase19) on 5 programs — target 3-5x speedup for integer-heavy code
- [ ] 6.4 Run full native backend test suite with linear scan enabled — verify all phase18 integration tests still pass

## 1. Tail (mandatory — enforced by rulebook v5.3.0)
- [ ] 1.1 Update or create documentation covering the implementation
- [ ] 1.2 Write tests covering the new behavior
- [ ] 1.3 Run tests and confirm they pass
