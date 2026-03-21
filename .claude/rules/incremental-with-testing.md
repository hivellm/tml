# MANDATORY: Implement Incrementally — Test Each Stage Before Proceeding

**NEVER implement everything at once then fight cascading errors.**

## The Rule

1. **Break every task into small, testable stages** (1-3 files max per stage)
2. **Implement stage 1 → compile → test → fix → ONLY THEN move to stage 2**
3. **Never delegate a large implementation to a single agent without incremental checkpoints**
4. **Feed learnings into memory as you go** — don't lose context

## When Stuck

**If 2-3 fix attempts fail on the same error, STOP.**

1. Delete the broken code entirely (don't try to salvage it)
2. Re-analyze the problem from scratch
3. Choose a DIFFERENT approach (different data structure, different API, different order)
4. Implement the new approach incrementally
5. "The line between persistence and stupidity is very thin"

## Why

Implementing 300 lines across 6 files in one shot leads to cascading failures where you spend hours patching symptoms instead of getting the foundation right. Step-by-step with testing at each stage produces excellent results consistently.

## Anti-Pattern (FORBIDDEN)

```
Write 6 files → compile error → fix → runtime crash → fix → logic error →
fix → new crash → fix → still broken → spend 4 hours patching...
```

## Correct Pattern

```
Write core type → test it compiles ✓
Write decoder → test 1 simple case ✓
Add edge cases → test each one ✓
Integrate into system → test integration ✓
Full test suite → all pass ✓
```