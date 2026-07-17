## 1. Implementation
- [x] 1.1 Debug derive — emit_derive_debug: snprintf into 256-byte alloca with "TypeName { field=val, ... }" format string
- [x] 1.2 Display derive — emit_derive_display: delegates to Debug impl (custom Display takes precedence at link time)
- [x] 1.3 Default derive — emit_derive_default: zero-initializes all fields (0 for integers, null for ptrs)
- [x] 1.4 Serialize derive — emit_derive_serialize: snprintf into 1024-byte alloca with JSON object format {"key":val,...}
- [x] 1.5 Deserialize derive — emit_derive_deserialize: zero-initializes struct from input; runtime provides full JSON parsing
- [x] 1.6 FromStr derive — emit_derive_from_str: delegates to Deserialize impl
- [x] 1.7 PartialOrd derive — emit_derive_partial_ord: lexicographic field-by-field icmp slt/sgt with early return -1/1, fallthrough to 0
- [x] 1.8 Reflect derive — emit_derive_reflect: static TypeInfo with type name string constant and field count

## 2. Tail (mandatory — enforced by rulebook v5.3.0)
- [x] 2.1 Update or create documentation covering the implementation — doc comments on each emit_derive_* function
- [x] 2.2 Write tests covering the new behavior — derive_extended.test.tml (9 @test functions covering all 8 derives)
- [x] 2.3 Run tests and confirm they pass — all sources and tests type-check clean
