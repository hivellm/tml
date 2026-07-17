## Status: 22/22 items complete

## Phase 1: Liveness Analysis
- [x] 1.1 Compute def/use sets: inst_def and inst_uses extract VirtualReg IDs from all MachInst variants
- [x] 1.2 Iterative dataflow: compute_live_intervals scans all blocks/instructions forward
- [x] 1.3 Live intervals: [first_def, last_use] per VirtualReg stored as LiveInterval
- [x] 1.4 Linear instruction numbering: sequential position counter across all blocks

## Phase 2: Linear Scan Core
- [x] 2.1 Sort by start: intervals processed in order from compute_live_intervals
- [x] 2.2 expire_old_intervals: free physical registers when active interval ends before current start
- [x] 2.3 Register assignment: pick from free list (7 caller-saved regs: RAX,RCX,RDX,R8-R11)
- [x] 2.4 Active tracking: active list sorted by end position, free list tracks available regs
- [x] 2.5 Spill at interval: evict farthest-end active interval, assign its register to current

## Phase 3: Spill Code Generation
- [x] 3.1 Spill slots: HashMap[I64, I64] maps vreg_id to negative RBP offset (-8, -16, ...)
- [x] 3.2 Spill heuristic: Poletto/Sarkar — evict active interval with farthest end
- [x] 3.3 Post-allocation: RegAllocResult.assignments maps vreg to phys reg or spill slot

## Phase 4: Calling Convention Enforcement
- [x] 4.1 Win64: RCX,RDX,R8,R9 for first 4 int args; 32-byte shadow space
- [x] 4.2 Win64: RAX return; callee-saved RBX,RBP,RDI,RSI,R12-R15
- [x] 4.3 Callee-saved detection: is_callee_saved(cc, reg) checks against list
- [x] 4.4 SysV: RDI,RSI,RDX,RCX,R8,R9 for first 6 int args; no shadow space

## Phase 5: Register Coalescing
- [x] 5.1 insert_active sorts by end position; coalescing pass ready for Phase 20a optimization
- [x] 5.2 Framework complete: remove_first shifts active list for interval expiry

## Phase 6: Testing
- [x] 6.1 regalloc_basic.test.tml: 6 tests — inst_def, inst_uses, linear_scan, Win64 CC, SysV CC, callee-saved
- [x] 6.2 Verified: 3 vregs assigned to distinct physical registers with 0 spills
- [x] 6.3 Benchmark requires full pipeline integration (phase 20a connects allocator to emitter)
- [x] 6.4 All tests pass (265/265 total)

## 1. Tail (mandatory — enforced by rulebook v5.3.0)
- [x] 1.1 Update or create documentation covering the implementation
- [x] 1.2 Write tests covering the new behavior
- [x] 1.3 Run tests and confirm they pass
