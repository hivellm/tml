## 1. Reader completions (asymmetric gaps)
- [ ] 1.1 Add `read_f32() -> Maybe[F32]` to reader — decode float 32 (0xca), 4 big-endian bytes via `Buffer.read_f32_be`
- [ ] 1.2 Add `read_f64() -> Maybe[F64]` to reader — decode float 64 (0xcb), 8 big-endian bytes via `Buffer.read_f64_be`
- [ ] 1.3 Add `read_bin() -> Maybe[Buffer]` to reader — decode bin 8/16/32 (0xc4/0xc5/0xc6), return raw byte buffer
- [ ] 1.4 Write roundtrip tests for f32, f64, and binary read/write

## 2. Generic decode utilities
- [ ] 2.1 Add `peek_type() -> MsgPackType` to reader — inspect next byte without advancing `pos`, return classified type
- [ ] 2.2 Add `advance_past_value() -> Bool` to reader — consume one complete value including nested arrays/maps (recursive depth)
- [ ] 2.3 Update `MsgPackType` enum — add `Timestamp` variant
- [ ] 2.4 Write tests for peek_type (all type categories) and advance_past_value (primitives, strings, nested arrays/maps)

## 3. Extension types (write + read)
- [ ] 3.1 Add `write_ext(type_id: I32, data: ref Buffer)` to writer — auto-select fixext 1/2/4/8/16 or ext 8/16/32 based on data length
- [ ] 3.2 Add `read_ext() -> Maybe[(I32, Buffer)]` to reader — decode fixext 1/2/4/8/16 (0xd4-0xd8) and ext 8/16/32 (0xc7-0xc9)
- [ ] 3.3 Write roundtrip tests for ext types: fixext 1/2/4/8/16 and ext 8/16/32 with various data sizes

## 4. Timestamp support (ext type -1)
- [ ] 4.1 Add `write_timestamp(seconds: I64, nanos: U32)` to writer — auto-select timestamp 32 (seconds fits U32, nanos=0), timestamp 64 (seconds fits U34 + nanos), or timestamp 96 (full range)
- [ ] 4.2 Add `read_timestamp() -> Maybe[(I64, U32)]` to reader — decode all 3 timestamp formats from ext type -1
- [ ] 4.3 Write tests for all 3 timestamp encodings: 32-bit (simple epoch), 64-bit (epoch + nanos), 96-bit (large/negative epoch + nanos)

## 5. MsgPackValue dynamic type
- [ ] 5.1 Add `MsgPackValue` enum to types.tml — variants: Nil, Bool(Bool), Int(I64), UInt(U64), Float32(F32), Float64(F64), Str(Str), Bin(Buffer), Array(List[MsgPackValue]), Map(List[(MsgPackValue, MsgPackValue)]), Ext(I32, Buffer), Timestamp(I64, U32)
- [ ] 5.2 Add `read_value() -> Maybe[MsgPackValue]` to reader — generic decode dispatching on peek_type, recursively decodes arrays/maps
- [ ] 5.3 Add `write_value(mut this, value: ref MsgPackValue)` to writer — generic encode dispatching on variant
- [ ] 5.4 Write roundtrip tests for MsgPackValue with mixed nested structures (array of maps, map with binary values, etc.)
- [ ] 5.5 If recursive enum codegen hits K001, implement a non-recursive workaround (e.g. flat tagged-union with index-based children in a List)

## 6. Module exports and API cleanup
- [ ] 6.1 Update `mod.tml` to re-export new public types (`MsgPackValue`, timestamp helpers)
- [ ] 6.2 Add doc comments to all new public functions following existing style

## 7. Tail (mandatory — enforced by rulebook v5.3.0)
- [ ] 7.1 Update `docs/patches/` with new version entry covering all additions
- [ ] 7.2 Update CHANGELOG.md with conventional commit entry
- [ ] 7.3 Bump VERSION
- [ ] 7.4 Run full msgpack test suite and confirm all pass
- [ ] 7.5 Run `mcp__tml__check` on all modified files and confirm zero errors
