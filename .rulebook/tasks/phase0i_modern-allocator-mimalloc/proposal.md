# Proposal: phase0i_modern-allocator-mimalloc

## Why
All heap traffic goes to the lock-serialized CRT malloc while the generated
code is allocation-heavy; a modern allocator is a one-change broad win and a
prerequisite for scalable threaded testing (analysis L-104).

## What Changes
mimalloc statically linked into the runtime; malloc/mem_alloc routed to it;
CRT fallback flag; benchmarks recorded.

## Impact
- Affected specs: none
- Affected code: runtime link configuration, compiler/runtime/memory/mem.c
- Breaking change: NO
- User benefit: every allocation gets faster; threaded workloads stop contending on the CRT heap lock
