# Proposal: phase19a — Linear Scan Register Allocator

## Why

Phase 18 uses a stack-only allocator: every virtual register gets a unique stack slot, and every instruction that reads or writes a virtual register goes through memory. This is correct but produces extremely slow code — a simple `a + b` requires two loads before the ADD and a store after it, three memory operations for one arithmetic instruction.

A register allocator assigns virtual registers to physical registers. When a value lives in a physical register, the load/store sequence disappears. The speedup is dramatic: register-allocated integer code typically runs 3-10x faster than stack-only code. For TML to be a viable production language, the native backend must produce register-allocated code.

Linear scan (Poletto & Sarkar, 1999) is the standard choice for production compilers that need fast allocation. It is O(n log n) in the number of live intervals, compared to O(n^3) for optimal graph coloring. The quality of code produced is within 5-10% of graph coloring for typical programs. LLVM uses a variant of linear scan for its fast register allocator; HotSpot JIT and V8 both use linear scan.

## What Changes

- New TML module `compiler/native/liveness.tml` — dataflow liveness analysis, live interval computation
- New TML module `compiler/native/linear_scan.tml` — interval sorting, physical register assignment, spill decision, coalescing
- New TML module `compiler/native/spill.tml` — spill code insertion (Spill/Reload MachInst generation)
- New TML module `compiler/native/calling_conv.tml` — Win64 and SysV ABI calling convention enforcement
- Modified `compiler/native/x86_emit.tml` — emit physical register operands instead of virtual register operands (post-allocation)
- Phase 18 stack-only allocator remains available as a fallback (`--regalloc=stack`)

## Algorithm (Poletto & Sarkar 1999)

```
sort intervals by start position
for each interval i (in order of start):
    expire_old_intervals(i)
    if |active| == |registers|:
        spill_at_interval(i)
    else:
        assign free register to i
        add i to active (sorted by end)

expire_old_intervals(i):
    for each interval j in active (sorted by end):
        if j.end >= i.start: return
        remove j from active
        add j.register to free set

spill_at_interval(i):
    spill = interval in active with farthest end
    if spill.end > i.end:
        i.register = spill.register
        spill.register = stack slot
        remove spill from active, add i
    else:
        i.register = stack slot
```

## Calling Conventions

**Win64 (Microsoft x64 ABI)**:
- Integer args: RCX, RDX, R8, R9 (positions 1-4), stack for positions 5+
- Float args: XMM0-XMM3 (positions 1-4), stack for positions 5+
- Shadow space: 32 bytes reserved on stack before args
- Return: RAX (int ≤ 64 bits), XMM0 (float), sret pointer in RCX for large structs
- Caller-saved: RAX, RCX, RDX, R8, R9, R10, R11, XMM0-XMM5
- Callee-saved: RBX, RBP, RDI, RSI, RSP, R12-R15, XMM6-XMM15

**SysV AMD64 (Linux/macOS)**:
- Integer args: RDI, RSI, RDX, RCX, R8, R9 (6 registers)
- Float args: XMM0-XMM7 (8 registers)
- Return: RAX (int), XMM0 (float)
- Caller-saved: RAX, RCX, RDX, RSI, RDI, R8-R11, XMM0-XMM15
- Callee-saved: RBX, RBP, RSP, R12-R15

## Impact

- Affected specs: docs/specs/native-backend.md (register allocator section)
- Affected code: compiler/native/ (4 new modules), compiler/native/x86_emit.tml (modified)
- Breaking change: NO — stack-only allocator still available as fallback
- User benefit: Native backend produces code competitive with LLVM -O0; 3-10x faster than phase18 baseline

## Risk

VERY HIGH. Register allocation is the most error-prone phase in any compiler. Bugs produce silent incorrect behavior: wrong values in registers, live ranges incorrectly terminated, callee-saved registers clobbered. The testing strategy (task 6.1-6.2) verifies structural correctness, but subtle bugs may only appear in specific calling patterns. Plan: implement with conservative spilling first (spill anything uncertain), then tighten.

## Reference

- Poletto & Sarkar, "Linear Scan Register Allocation" (ACM TOPLAS 1999) — the canonical paper
- LLVM FastRegisterAllocator.cpp — production linear scan implementation
- qbe ra.c — minimal register allocator in ~400 LOC, excellent study reference
- Win64 ABI: docs.microsoft.com/en-us/cpp/build/x64-calling-convention
- SysV ABI: gitlab.com/x86-psABIs/x86-64-ABI (pdf, §3.2)
