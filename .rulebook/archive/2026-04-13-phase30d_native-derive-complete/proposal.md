# Proposal: phase30d_native-derive-complete

## Why
The native derive codegen only generates `Duplicate` (clone) and `PartialEq`
(equality) implementations. The remaining eight standard behaviors —
`Debug`, `Display`, `Default`, `Serialize`, `Deserialize`, `FromStr`,
`PartialOrd`, and `Reflect` — are needed by virtually all non-trivial programs:
logging requires `Debug`/`Display`, configuration parsing requires `FromStr`/
`Deserialize`, sorting requires `PartialOrd`, and JSON serialization requires
`Serialize`. Without these, any `@auto` or `@derive` annotation on user types
falls back to the LLVM backend.

## What Changes
- `compiler-tml/src/codegen/emit_derive.tml` is extended with eight new
  derive-generation branches:
  - `Debug`: format string showing type name and field name=value pairs.
  - `Display`: user-facing format (falls back to Debug format if no custom impl).
  - `Default`: zero-initialise each field (0 for numerics, "" for Str, Nothing for Maybe).
  - `Serialize`: emit JSON object/array representation recursively.
  - `Deserialize`: parse JSON object keys to struct fields; emit error on unknown key.
  - `FromStr`: call `Deserialize` from a JSON string or delegate to a per-type parse.
  - `PartialOrd`: lexicographic field comparison returning `Maybe[Ordering]`.
  - `Reflect`: emit a static `TypeInfo` record with field names, types, and offsets.

## Impact
- Affected specs: native-backend/derive
- Affected code: compiler-tml/src/codegen/emit_derive.tml
- Breaking change: NO
- User benefit: All eight common behavior derives work natively, enabling printing, serialization, comparison, and reflection without LLVM.
