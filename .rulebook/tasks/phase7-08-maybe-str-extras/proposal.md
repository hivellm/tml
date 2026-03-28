# Proposal: Maybe[T] Extra Methods

## Why
Maybe[T] sits at approximately 90% coverage with six useful methods missing. These methods (`is_just_and`, `get_or_insert`, `replace`, `unzip`) appear regularly in idiomatic Rust code and their absence forces verbose workarounds.

## What Changes
Add the six remaining Maybe methods to `lib/core/src/types/option.tml`.

## Impact
- Affected specs: core::option
- Affected code: lib/core/src/types/option.tml, lib/core/tests/option/
- Breaking change: NO
- User benefit: Complete Maybe API eliminates verbose when/match workarounds for common optional-value patterns
