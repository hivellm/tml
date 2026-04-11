# Proposal: phase31a_forin-loops-core-std

## Why
Manual index loops (`var i = 0; loop (i < N) { ... i = i + 1 }`) are error-prone (forgotten increment = infinite loop), verbose (3 lines of boilerplate per loop), and obscure intent. The `for i in 0 to N` syntax shipped in phase30a eliminates all three issues. ~145 instances exist across core and std libraries.

Source: docs/analysis/core-std-ergonomics-audit/

## What Changes
Replace all manual index loop patterns in `lib/core/src/` and `lib/std/src/` with `for i in 0 to N` (exclusive) or `for i in 0 through N` (inclusive) syntax. Pure mechanical refactor -- no logic changes.

## Impact
- Affected specs: none (syntax-only refactor)
- Affected code: lib/core/src/ (~15 sites), lib/std/src/ (~130 sites)
- Breaking change: NO
- User benefit: Cleaner, safer library code; dogfooding new syntax
