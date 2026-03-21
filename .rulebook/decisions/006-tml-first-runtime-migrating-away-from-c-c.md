# 6. TML-First Runtime: Migrating Away from C/C++

**Status**: proposed
**Date**: 2026-03-15

## Context

Project roadmap targets self-hosting (compiler rewritten in TML). Every new line of C/C++ is debt. TML has memory intrinsics (ptr_read, ptr_write, mem_alloc, etc.) sufficient for most algorithms. Three-tier rule: Pure TML > @extern("c") FFI > New C code (last resort).

## Decision

Freeze C runtime except essential FFI (I/O, panic, test harness in essential.c, malloc/free in mem.c). All new algorithms implemented in pure TML. C runtime dirs (collections/, text/, math/, search/) marked MIGRATE — no new code. @extern("c") for system APIs.

## Alternatives Considered

- Keep C runtime and optimize it (faster short-term but blocks self-hosting)
- Immediate full migration (too disruptive)
- Mixed approach with gradual migration (chosen)

## Consequences

Some TML implementations initially slower than optimized C. Memory intrinsics require careful correctness validation. But pure TML implementations serve double duty: work today AND prepare for self-hosting. Every migrated function removes one piece of debt from the self-hosting critical path.
