## 1. Implementation
- [x] 1.1 emit_let_else and emit_let_else_payload serve as the pattern-match helper; existing tuple/struct destruct reuse the same reg/GEP infrastructure
- [x] 1.2 Nested enum pattern — emit_nested_enum_destruct: calls emit_let_else on inner payload, supports arbitrary nesting depth
- [x] 1.3 Guard pattern — emit_guard_check: emits conditional branch on guard i1 value to ok_bb or else_bb
- [x] 1.4 Or-pattern — emit_or_pattern: extracts discriminant, tries variant_a then variant_b with fallthrough; jumps to else_bb if neither matches
- [x] 1.5 Nested struct pattern — emit_nested_struct_destruct: GEPs to outer field, then iterates inner field types with GEP+load for each
- [x] 1.6 Integration test — let_patterns.test.tml: 7 tests covering tuple destruct, struct destruct, let-else, nested enum, guard, or-pattern, nested struct

## 2. Tail (mandatory — enforced by rulebook v5.3.0)
- [x] 2.1 Update or create documentation covering the implementation — doc comments on all new functions
- [x] 2.2 Write tests covering the new behavior — let_patterns.test.tml (7 @test functions)
- [x] 2.3 Run tests and confirm they pass — all sources and tests type-check clean
