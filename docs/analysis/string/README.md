# String Performance Analysis

**Date**: 2026-04-16
**Status**: TML `Text` is **1.15x** Rust for log building (near parity). `Str +=` is **1,098x** slower (fundamentally O(n^2)). `I64.to_string()` is **5.9x** slower (malloc + snprintf).

## Documents

| File | Description |
|------|-------------|
| [01-type-architecture.md](01-type-architecture.md) | Str vs Text vs Interned — type design and memory layout |
| [02-operation-costs.md](02-operation-costs.md) | Per-operation cost breakdown with allocation trace |
| [03-bottleneck-analysis.md](03-bottleneck-analysis.md) | Root causes with file:line evidence |
| [04-fix-proposals.md](04-fix-proposals.md) | Prioritized fixes with expected impact |

## Key Numbers

| Operation | TML | Rust | Ratio | Notes |
|-----------|-----|------|-------|-------|
| Concat Small (literals) | 0 ns | 180 ns | **TML wins** | Compile-time folding |
| Text push_str (100K) | 4 ns | 1 ns | 4x | FFI strlen overhead |
| Str += loop (10K) | 3,293 ns | 3 ns | **1,098x** | O(n^2) vs O(n) |
| Int to String | 41 ns | 7 ns | **5.9x** | malloc + snprintf vs stack |
| Log building Text (10K) | 60 ns | 52 ns | **1.15x** | Near parity |
| Log building Str (1K) | 4,155 ns | 93 ns | 44.7x | O(n^2) |

## Findings Summary

| ID | Finding | Impact | Effort |
|----|---------|--------|--------|
| F-001 | `I64.to_string()` = `malloc(24) + snprintf` | 5.9x gap | Low |
| F-002 | `Text.as_str()` heap-copies every call | Pervasive waste | Low |
| F-003 | `strlen` FFI for known-length literals in `push_str` | 4x gap | Medium |
| F-004 | `Str +=` is O(n^2) — unfixable without type change | 1,098x gap | N/A (lint) |
| F-005 | `tml_str_free` does HeapValidate (~100 ns) on Windows | Minor | Low |
