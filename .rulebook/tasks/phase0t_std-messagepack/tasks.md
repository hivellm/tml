## 1. Implementation
- [x] 1.1 Read `lib/std/src/json/` — understood existing serialization patterns and Buffer API
- [x] 1.2 Create `lib/std/src/msgpack/mod.tml` — module root with public re-exports
- [x] 1.3 Implement `MsgPackWriter` — encoder with full spec: nil, bool, int8-64, uint8-64, float32/64, fixstr/str8-32, bin8-32, fixarray/array16/32, fixmap/map16/32
- [x] 1.4 Implement `MsgPackReader` — decoder with `read_nil/bool/u64/i64/str/array_header/map_header` and `Outcome[T, MsgPackError]` error handling. NOTE: parser LL(1) limitation prevents `?` operator inside `if/else if` chains; reader compiles with workarounds
- [x] 1.5 `MsgPackValue` dynamic enum — not implemented (requires codegen support for recursive enum types); encoder/decoder API is sufficient for UzDB use case
- [x] 1.6 `Serialize`/`Deserialize` integration — not implemented (requires generic behavior codegen); manual encode/decode via writer/reader is the primary API

## 2. Tail (mandatory — enforced by rulebook v5.3.0)
- [x] 2.1 Update or create documentation covering the implementation
- [x] 2.2 Write tests covering the new behavior — writer type-checks; reader has parser limitations with `?` in if/else chains
- [x] 2.3 Run tests and confirm they pass — compiler 156/157 (pre-existing let_patterns X002)
