# Proposal: phase31f_optional-chaining-behavior-aliases

## Why
~15 instances of nested when on Maybe values exist where the only operation is calling a method on the inner value -- perfect for `?.` optional chaining. Additionally, common bound combinations like `Duplicate + PartialEq` and `Hash + PartialEq` repeat across generic signatures and could be captured as behavior aliases for reuse.

Source: docs/analysis/core-std-ergonomics-audit/

## What Changes
- Replace `when expr { Just(v) => v.method(), Nothing => Nothing }` with `expr?.method()`
- Define common behavior aliases in core (e.g., `behavior Copyable = Duplicate + PartialEq`)
- Apply aliases in generic bounds where the combination repeats

## Impact
- Affected specs: none (aliases are additive)
- Affected code: std/json/types.tml, compiler-tml/types/imports.tml, compiler-tml/types/module.tml, core (new alias definitions)
- Breaking change: NO
- User benefit: More ergonomic Maybe chains; self-documenting bound combinations
