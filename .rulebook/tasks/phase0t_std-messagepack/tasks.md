## 1. Implementation
- [ ] 1.1 Read `lib/std/src/json.tml` and `lib/std/src/encode/` — understand existing serialization patterns and Buffer/BinaryWriter usage
- [ ] 1.2 Create `lib/std/src/msgpack.tml` — module root with public re-exports
- [ ] 1.3 Implement `MsgPackWriter` — encoder writing to `Buffer`; fixint/fixstr/fixarray/fixmap compact forms; full MessagePack spec (nil, bool, int8-64, uint8-64, float32/64, str8-32, bin8-32, array16/32, map16/32, ext)
- [ ] 1.4 Implement `MsgPackReader` — decoder reading from `Slice[U8]`; `peek_type() -> MsgPackType`, typed read methods, error handling via `Outcome[T, MsgPackError]`
- [ ] 1.5 Implement `MsgPackValue` enum — dynamic value type for untyped access (like JsonValue); `to_msgpack()`/`from_msgpack()` conversions
- [ ] 1.6 Integration with `Serialize`/`Deserialize` behaviors — `to_msgpack_bytes(value: ref T) -> Buffer` and `from_msgpack_bytes[T](data: Slice[U8]) -> Outcome[T, MsgPackError]`

## 2. Tail (mandatory — enforced by rulebook v5.3.0)
- [ ] 2.1 Update or create documentation covering the implementation
- [ ] 2.2 Write tests covering the new behavior — encode/decode round-trips for all types, edge cases (empty strings, max-size arrays, nested structures), compatibility with reference MessagePack test vectors
- [ ] 2.3 Run tests and confirm they pass
