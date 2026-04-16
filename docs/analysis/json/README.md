# JSON Performance Analysis

**Date**: 2026-04-16
**Status**: TML JSON parsing is **13.6x slower** than Rust serde_json for small documents.
**Root cause**: `std::map` per-node allocation + arena allocator sitting unused + value cloning on access.

## Phase 1b update (2026-04-16, after `std::map → std::vector<std::pair>`)

| Operation | Before | After | Δ |
|-----------|--------|-------|---|
| Parse Small (200B) | 11,175 ns | 9,494 ns | **-15%** |

F-001 (std::map RB-tree per-node alloc) — **resolved**. `JsonObject` is now a
flat `std::vector<std::pair<std::string, JsonValue>>` with `reserve(8)` in
`parse_object()`. Insertion order is now preserved (previously sorted by key).
The remaining gap to serde_json is mostly in F-002/F-003/F-004 (Box
indirection, value cloning, string allocation) — tracked in phase1c/d/e.

## Phase 1c update (2026-04-16, `parse_string` fast path)

| Operation | Baseline (phase1b) | After phase1c |
|-----------|--------------------|---------------|
| Parse Small (200B) | 9,494 ns | 9,949 ns |

`parse_string()` now splits into two paths: a fast path that detects
escape-free strings via a leading `find_string_special_simd` scan and
constructs the result directly from the input view (bypassing
`string_buffer_`), and a slow path that still uses `string_buffer_` for
strings containing `\n`, `\t`, `\u`, etc. Correctness unchanged — all 22
std/json test suites pass.

The raw wall-clock improvement is not realized yet because `JsonValue`
still stores `std::string` by value, so the fast path copies bytes into
a fresh heap allocation. The structural refactor here is the prerequisite
for phase1d (F-003), which changes `JsonValue`'s string storage so the
fast path can return views without copying.

## Phase 1d update (2026-04-16, borrowed handles for accessors)

| Operation | Before | After | Δ |
|-----------|--------|-------|---|
| Field Access | 15,320 ns | 10,353 ns | **-32%** |
| Array Iteration | 11,007 ns | 10,696 ns | -3% |
| Nested Object Access | 11,174 ns | 10,541 ns | -5% |

F-004 (deep-clone on every field access) — **resolved**.
`tml_json_object_get`, `tml_json_array_get`, and
`tml_json_object_value_at` now return a "borrowed" handle that points
in-place at the element inside the parent document. The caller must
ensure the parent handle outlives any borrow.

Still ~1.5× slower than serde_json's 7,100 ns Field Access — the
residual overhead is the FFI handle-allocation cost and per-call
dispatch, not the clone. Handle-pool amortization is the natural next
step but is out of scope for phase1.

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
