# Proposal: phase30e_native-drop-glue-complete

## Why
TML types such as `Maybe[List[I64]]` and `Outcome[List[Str], Err]` contain heap
payloads nested inside enum variants. The native backend's current drop-glue
generator only handles flat structs and skips enum variants entirely. Without
recursive drop for variant payloads, any program that drops a heap-containing
enum leaks memory. This is the primary source of memory leaks in native-compiled
TML programs today and blocks the backend from being usable for any long-running
or allocation-heavy code.

## What Changes
- `compiler-tml/src/native/x86/emit_drop.tml` gains a per-variant drop pass:
  for each enum variant that carries a payload, the emitter inspects the payload
  type recursively; if it is a heap type (List, HashMap, Str, Box, or any struct
  whose fields include heap types), a variant-specific drop block is emitted that
  extracts the payload and calls its drop function before freeing the tag field.
- A new `has_heap_fields(ty)` predicate returns true for any type that transitively
  contains a heap-allocated field, driving the decision of whether a drop block
  is needed.
- Recursive types (e.g. `Maybe[Maybe[List[I64]]]`) are handled by depth-first
  traversal with a cycle-break set to avoid infinite recursion.

## Impact
- Affected specs: native-backend/drop-glue
- Affected code: compiler-tml/src/native/x86/emit_drop.tml
- Breaking change: NO
- User benefit: Enum values containing heap payloads are correctly freed, eliminating the primary class of memory leaks in native-compiled TML programs.
