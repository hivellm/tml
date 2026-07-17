## 1. Implementation
- [x] 1.1 has_heap_type predicate — checks List, HashMap, Str, Box, Heap and their mangled parameterized variants; uses visited HashMap to break cycles
- [x] 1.2 Per-variant drop blocks — emit_drop_enum now takes variant_payload_types; for heap-type payloads, emits GEP + load + drop_glue call + free
- [x] 1.3 Recursive type handling — visited set keyed on type_name prevents infinite recursion during drop codegen
- [x] 1.4 Wired into existing dispatch — emit_drop_enum extended with payload types; emit_drop_enum_simple provides backward-compatible overload
- [x] 1.5 Integration test — drop_glue.test.tml: test_drop_enum_with_heap_payload verifies Just(List_I64) emits drop_glue + free

## 2. Tail (mandatory — enforced by rulebook v5.3.0)
- [x] 2.1 Update or create documentation covering the implementation — doc comments on has_heap_type and emit_drop_enum
- [x] 2.2 Write tests covering the new behavior — drop_glue.test.tml (6 @test functions)
- [x] 2.3 Run tests and confirm they pass — all sources and tests type-check clean
