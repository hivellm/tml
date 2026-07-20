# Proposal: phase0b_drop-in-place-payload-destructors

## Why
Smart pointers never run their payload's destructor (runtime-proven):
`Heap[File]` leaks the fd, `Shared[List]` leaks the buffer. The needed
primitive `drop_in_place` exists but its codegen self-recurses. The library's
workaround (copy-to-drop) is a full element copy per destruction — pure
overhead Rust doesn't pay (analysis L-022, L-029).

## What Changes
Fix the intrinsic's expansion to real drop glue; Heap/Shared/Sync invoke it
before freeing; copy-to-drop deleted from List; redundant enum-glue special
cases and documented-leak paths removed; stubbed tests re-enabled.

## Impact
- Affected specs: docs/specs/22-LOW-LEVEL.md, docs/specs/06-MEMORY.md
- Affected code: compiler codegen drop/intrinsic emission, lib/core/src/alloc/{heap,shared,sync}.tml, lib/std/src/collections/list.tml
- Breaking change: NO (programs relying on leaks were already wrong; destructors now run)
- User benefit: RAII actually composes through pointers; leak class closed; less copying
