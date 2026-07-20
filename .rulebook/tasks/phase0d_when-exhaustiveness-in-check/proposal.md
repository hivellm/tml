# Proposal: phase0d_when-exhaustiveness-in-check

## Why
A missing enum arm compiles and runs silently today: the exhaustiveness checker
sits in a layer real programs bypass, and even when it runs it is demoted to a
log line (analysis L-043, probe-proven). The whole error-handling story of the
language rests on `when` being total.

## What Changes
Exhaustiveness runs during type-check for every program with a real diagnostic
(code, span, missing variants, fix-it), deny-by-default; the THIR log-line
duplication is removed.

## Impact
- Affected specs: pattern-matching spec section, explain registry
- Affected code: compiler/src/types/checker/ (when handling), compiler/src/thir/exhaustiveness.cpp reuse, compiler/src/query/query_core.cpp
- Breaking change: YES (previously-silent non-exhaustive `when`s now fail check — each is a real latent bug)
- User benefit: the compiler finally guarantees the core idiom; whole class of logic bugs caught at check time
