# 10 — Gap Analysis

## All Gaps Ranked by Severity

### Critical (>10x or blocking)

| ID | Category | Ratio | Root Cause | Fix Complexity |
|----|----------|-------|------------|----------------|
| G-001 | Struct access (nested) | 18x | alloca/load/store, no mem2reg | Medium |
| G-002 | Struct creation (small) | 11.6x | No insertvalue codegen | Medium |
| G-003 | Struct field access | 10.9x | No register promotion | Medium |
| G-004 | Point creation | 10.7x | alloca+store pattern | Medium |
| G-005 | When/match dense | 9.5x | No jump table / switch inst | Low |
| G-006 | Compilation time | 27x | Debug compiler, plugin load | High |
| G-007 | string_bench | BLOCKED | K001 codegen bug | High |
| G-008 | json_bench | BLOCKED | K001 codegen bug | High |
| G-009 | crypto_bench | BLOCKED | N002 link failure | Medium |

### Severe (5-10x)

| ID | Category | Ratio | Root Cause | Fix Complexity |
|----|----------|-------|------------|----------------|
| G-010 | If-else chain (4 branches) | 8.4x | No CMOV/select lowering | Low |
| G-011 | Function pointer dispatch | 8.2x | No devirtualization | Medium |
| G-012 | Filter simulation | 8.4x | No closure inlining | Medium |
| G-013 | Stack struct medium (64B) | 9.2x | No register promotion | Medium |
| G-014 | Sequential access | 6.7x | No auto-vectorization | Medium |
| G-015 | List random access | 5.5x | Bounds check overhead | Low |
| G-016 | List set | ~5x+ | Bounds check + no vectorize | Low |

### Moderate (2-5x)

| ID | Category | Ratio | Root Cause | Fix Complexity |
|----|----------|-------|------------|----------------|
| G-017 | Integer addition | 4.3x | No loop vectorization | Medium |
| G-018 | Short-circuit AND | 4.2x | Extra branch codegen | Low |
| G-019 | Short-circuit OR | 3.8x | Extra branch codegen | Low |
| G-020 | When/match sparse | 3.9x | Linear scan, no binary search | Low |
| G-021 | List push (reserved) | 3.6x | Push not inlined | Low |
| G-022 | Base64 encode (13B) | 3.1x | String alloc overhead | Medium |
| G-023 | Empty loop | 3.1x | Loop not eliminated | Low |
| G-024 | Random access (memory) | 2.6x | Bounds check / indirection | Low |
| G-025 | Base64 encode (95B) | 2.4x | String alloc overhead | Medium |
| G-026 | Binary size | 2.4x | No LTO, debug metadata | Medium |
| G-027 | List pop | 2.4x | Pop not inlined | Low |
| G-028 | Integer modulo | 2.2x | No loop optimization | Low |
| G-029 | Bitwise operations | 2.2x | No loop vectorization | Medium |
| G-030 | List push (grow) | 2.0x | Amortized overhead | Low |

### Minor (1.3-2x)

| ID | Category | Ratio | Root Cause | Fix Complexity |
|----|----------|-------|------------|----------------|
| G-031 | HashMap insert | 1.6x | Hash function overhead | Low |
| G-032 | HashMap insert (reserved) | 1.5x | Hash function overhead | Low |
| G-033 | HashMap lookup | 1.4x | Probe sequence overhead | Low |
| G-034 | HashMap contains | 1.3x | Probe sequence overhead | Low |
| G-035 | Integer division | 1.2x | Minimal | — |
| G-036 | Float addition | 1.0x | None | — |
| G-037 | Float multiplication | 0.8x | TML wins | — |

### TML Wins

| ID | Category | Ratio | Notes |
|----|----------|-------|-------|
| S-001 | HashMap remove | 0.77x | Better tombstone strategy |
| S-002 | Array copy (1000 elts) | 0.018x | Optimized memcpy |
| S-003 | Array fill (1000 elts) | 0.005x | Optimized memset |
| S-004 | Hex encode (13B) | 0.2x | Better impl than format!() |
| S-005 | Integer multiplication | 0.85x | Slight win |
| S-006 | Nested if (4 levels) | 0.95x | Equal |

## Impact Matrix

```
                    Low Complexity ◄───────────► High Complexity
                    │                                        │
High Impact   ┌─────┤ G-005 (switch)    G-001-004 (mem2reg) │
              │     │ G-010 (CMOV)      G-006 (compile time)│
              │     │ G-018-019 (bool)  G-011 (devirt)      │
              │     │ G-015-016 (bounds)G-012 (inline close) │
              │     │ G-021,027 (inline)                     │
              │     │                                        │
              ├─────┤                                        │
              │     │                                        │
Low Impact    │     │ G-023 (loop elim) G-026 (LTO)         │
              │     │ G-028 (modulo)    G-022-025 (base64)  │
              │     │ G-030 (push grow)                      │
              └─────┤                                        │
                    │                                        │
```

## Quick Wins (Low Complexity, High Impact)

1. **G-005**: Emit LLVM `switch` for dense `when` → 5-10x improvement
2. **G-010**: Lower if-else to CMOV/select → 4-8x improvement
3. **G-018/G-019**: Fix boolean short-circuit codegen → 2-4x improvement
4. **G-015/G-016**: Bounds-check elimination for `for-in` → 2-3x improvement
5. **G-021/G-027**: Inline `List.push()`/`List.pop()` → 2x improvement
