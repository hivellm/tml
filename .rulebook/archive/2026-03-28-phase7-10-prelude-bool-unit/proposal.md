# Proposal: Prelude, Bool Methods, Unit Impls

## Why

No prelude means every file needs explicit `use` for Maybe, Outcome, etc. Bool lacks `then()`/`then_some()` — popular utilities that replace `if cond { Just(x) } else { Nothing }`. Unit type has no Display/Default/Debug impls.

## What Changes

1. Add `then`/`then_some` to Bool impl. 2. Add Display/Default/Debug for Unit. 3. Create prelude module + compiler auto-import.

## Impact
- Affected specs: core::prelude, Bool, Unit
- Affected code: `lib/core/src/prelude.tml` (new), compiler type checker (prelude injection)
- Breaking change: NO (prelude is additive; Bool/Unit methods are additive)
- User benefit: Less boilerplate imports, Bool utility methods, Unit formatting
