# Proposal: phase31d_auto-directives-repr

## Why
50+ manual `impl Duplicate` and `impl PartialEq` blocks exist on simple data structs that just copy fields or compare field-by-field. These are boilerplate that `@auto(duplicate, equal)` (shipped in phase30e) eliminates entirely. Additionally, 15+ enums with <256 variants waste memory using default I64 discriminants when `@repr(U8)` would save 7 bytes per instance.

Source: docs/analysis/core-std-ergonomics-audit/

## What Changes
- Replace manual `impl Duplicate` with `@auto(duplicate)` on simple data structs
- Replace manual `impl PartialEq` with `@auto(equal)` on simple data structs
- Add `@repr(U8)` to enums with <256 variants, remove manual tag() functions where applicable
- Verify K001 codegen bug does not affect @auto on types with Heap fields (skip those)

## Impact
- Affected specs: none
- Affected code: core/net/*.tml, core/time.tml, core/types/any.tml, core/traits/hash.tml, std/thread/mod.tml, std/uuid.tml, compiler-tml/token.tml, compiler-tml/ast/*.tml, compiler-tml/types/ty.tml
- Breaking change: NO
- User benefit: Reduced boilerplate, smaller enum memory footprint
