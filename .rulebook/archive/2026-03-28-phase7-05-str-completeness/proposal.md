# Proposal: Str Completeness

## Why
Str currently has approximately 65% API coverage. Methods like `split_once`, `strip_prefix`, `strip_suffix`, and `splitn` are absent, yet they appear constantly in parsing and text-processing code.

## What Changes
Add 16 missing methods to the Str type in `lib/core/src/str.tml`, split into high-priority parsing helpers and medium-priority utility methods.

## Impact
- Affected specs: core::str
- Affected code: lib/core/src/str.tml, lib/core/tests/str/
- Breaking change: NO
- User benefit: Full Rust-parity Str API enabling idiomatic string parsing and manipulation
