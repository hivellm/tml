# JSON Performance Analysis

**Date**: 2026-04-16
**Status**: TML JSON parsing is **13.6x slower** than Rust serde_json for small documents.
**Root cause**: `std::map` per-node allocation + arena allocator sitting unused + value cloning on access.

## Documents

| File | Description |
|------|-------------|
| [01-architecture.md](01-architecture.md) | JSON subsystem architecture, data flow, type definitions |
| [02-allocation-audit.md](02-allocation-audit.md) | Every heap allocation traced for a 200-byte parse |
| [03-bottleneck-analysis.md](03-bottleneck-analysis.md) | Root causes with file:line evidence |
| [04-fix-proposals.md](04-fix-proposals.md) | Prioritized fixes with expected impact |

## Key Numbers

| Operation | TML | Rust serde_json | Ratio |
|-----------|-----|-----------------|-------|
| Parse Small (200B) | 11,175 ns | 820 ns | **13.6x** |
| Parse Tiny (27B) | 2,558 ns | — | — |
| Parse Medium (500B) | 29,958 ns | — | — |
| Field Access | 15,320 ns | 7,100 ns | 2.2x |

## Findings Summary

| ID | Finding | Impact | Effort |
|----|---------|--------|--------|
| F-001 | `JsonObject = std::map` (RB-tree, per-node alloc) | 3-5x | Low |
| F-002 | `Box<JsonArray>` / `Box<JsonObject>` indirection | 1.5-2x | Low |
| F-003 | `JsonArena` exists but fast parser doesn't use it | 5-10x | Medium |
| F-004 | `field->clone()` on every accessor call | 2x reads | Medium |
| F-005 | Handle table linear scan for free slots | minor | Low |

**Combined fix target**: 11,175 ns -> **<1,500 ns** (<2x vs serde_json).
