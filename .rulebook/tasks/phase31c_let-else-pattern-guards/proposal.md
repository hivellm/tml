# Proposal: phase31c_let-else-pattern-guards

## Why
Nested `when` blocks for Maybe/Outcome unwrapping create 2-4 levels of indentation, making code hard to read and maintain. ~80 instances across core, std, and compiler-tml can be flattened using `let-else` (for early-return unwrapping) and pattern guards (for conditional arms). These features shipped in phase30b/30i.

Source: docs/analysis/core-std-ergonomics-audit/

## What Changes
- Replace nested `when expr { Just(x) => { ... } }` with `let Just(x) = expr else { return/continue }`
- Replace nested `when expr { Just(x) => { if cond { ... } } }` with `Just(x) if cond => ...`
- Pure logic-preserving refactor

## Impact
- Affected specs: none
- Affected code: core/types/option.tml, core/types/result.tml, core/async/task.tml, std/types.tml, std/json/types.tml, std/thread/mod.tml, compiler-tml/types/imports.tml, compiler-tml/types/register.tml, compiler-tml/types/checker/check_expr.tml
- Breaking change: NO
- User benefit: Flatter, more readable library code; reduces cognitive complexity
