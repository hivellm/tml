## 1. Implementation
- [ ] 1.1 Implement `has_heap_fields(ty: Type) -> Bool` predicate in emit_drop.tml: returns true for List, HashMap, Str, Box, and any struct or enum type whose fields/variants transitively contain a heap type; include a visited-set to break cycles
- [ ] 1.2 Add per-variant drop block emission: for each enum variant whose payload satisfies `has_heap_fields`, emit a case branch that reads the payload pointer and calls the payload type's drop function before the outer free
- [ ] 1.3 Handle recursive enum types (e.g. `Maybe[Maybe[List[I64]]]`): depth-first traversal with a cycle-break HashSet[Str] keyed on type name to avoid infinite recursion during drop codegen
- [ ] 1.4 Wire the new variant-aware drop logic into the existing `emit_drop_for_type` dispatch so all enum drop sites automatically pick up per-variant blocks
- [ ] 1.5 Integration test: allocate `Maybe[List[I64]]` with a Just variant containing a 1000-element list, drop it, and confirm no memory leak via heap-allocated counter or allocator stats

## 2. Tail (mandatory — enforced by rulebook v5.3.0)
- [ ] 2.1 Update or create documentation covering the implementation
- [ ] 2.2 Write tests covering the new behavior
- [ ] 2.3 Run tests and confirm they pass
