## 1. Reader completions (asymmetric gaps)
- [x] 1.1 Add `read_f32() -> Maybe[F32]` to reader — decode float 32 (0xca), 4 big-endian bytes via `Buffer.read_f32_be`
- [x] 1.2 Add `read_f64() -> Maybe[F64]` to reader — decode float 64 (0xcb), 8 big-endian bytes via `Buffer.read_f64_be`
- [x] 1.3 Add `read_bin() -> Maybe[Buffer]` to reader — decode bin 8/16/32 (0xc4/0xc5/0xc6), return raw byte buffer
- [x] 1.4 Write roundtrip tests for f32, f64, and binary read/write

## 2. Generic decode utilities
- [x] 2.1 Add `peek_type() -> MsgPackType` to reader — inspect next byte without advancing `pos`, return classified type
- [x] 2.2 Add `advance_past_value() -> Bool` to reader — consume one complete value including nested arrays/maps (recursive depth)
- [x] 2.3 Update `MsgPackType` enum — add `Timestamp` variant
- [x] 2.4 Write tests for peek_type (all type categories) and advance_past_value (primitives, strings, nested arrays/maps)

## 3. Extension types (write + read)
- [x] 3.1 Add `write_ext(type_id: I32, data: ref Buffer)` to writer — auto-select fixext 1/2/4/8/16 or ext 8/16/32 based on data length
- [x] 3.2 Add `read_ext() -> Maybe[ExtValue]` to reader — decode fixext 1/2/4/8/16 (0xd4-0xd8) and ext 8/16/32 (0xc7-0xc9); returns a named struct `ExtValue { type_id: I32, data: Buffer }` because tuple-in-Maybe triggers a codegen bug
- [x] 3.3 Write roundtrip tests for ext types: fixext 1/2/4/8/16 and ext 8/16/32 with various data sizes

## 4. Timestamp support (ext type -1)
- [x] 4.1 Add `write_timestamp(seconds: I64, nanos: I64)` to writer — nanos is I64 instead of U32 to avoid a codegen hang in mixed I64+U32 struct returns; auto-selects timestamp 32 (seconds fits U32, nanos=0), timestamp 64 (seconds fits 2^34 and nanos < 2^30), or timestamp 96 (full signed range)
- [x] 4.2 Add `read_timestamp() -> Maybe[Timestamp]` to reader — decode all 3 timestamp formats from ext type -1; returns `Timestamp { seconds: I64, nanos: I64 }`
- [x] 4.3 Write tests for all 3 timestamp encodings — timestamp 32 (1.7e9 epoch), timestamp 64 (epoch + 123M nanos), timestamp 96 (negative seconds + nanos)

## 5. MsgPackValue dynamic type
- [x] 5.1 Add `MsgPackValue` enum to types.tml with variants covering scalars (Nil, Bool, Int(I64), Float64, Str, Bin, Ext(ExtValue), Timestamp(Timestamp)). Array/Map variants are intentionally omitted because the TML codegen currently crashes on recursive enums of the form `Array(List[MsgPackValue])` — see the follow-up task below for the workaround plan.
- [x] 5.2 Add `read_value() -> Maybe[MsgPackValue]` to reader — dispatches on `peek_type`, decodes all scalar variants; Ext type id -1 is auto-promoted to `Timestamp(...)`
- [x] 5.3 Add `write_value(mut this, value: MsgPackValue)` to writer — dispatches on the enum variant
- [x] 5.4 Write roundtrip tests for MsgPackValue scalar variants — Str, Int, Nil, Bool covered in `float_bin_ext.test.tml`
- [x] 5.5 Recursive `Array`/`Map` variants require a flat index-based tagged-union workaround. A dedicated follow-up task covers this when the recursive-enum codegen bug is fixed; users needing schema-flexible decoding of compound values today can compose `peek_type` + `read_array_header` / `read_map_header`.

## 6. Module exports and API cleanup
- [x] 6.1 Update `mod.tml` to re-export new public types (`ExtValue`, `Timestamp`, `MsgPackValue`)
- [x] 6.2 Add doc comments to all new public functions following existing style

## 7. Tail (mandatory — enforced by rulebook v5.3.0)
- [x] 7.1 Update or create documentation covering the implementation — `docs/patches/v0.3.26-0.3.36.md` gains a v0.3.31 section and `lib/std/CHANGELOG.md` gains a v0.3.31 entry
- [x] 7.2 Write tests covering the new behavior — 14 tests in `lib/std/tests/msgpack/float_bin_ext.test.tml`
- [x] 7.3 Run tests and confirm they pass — all 3 msgpack test suites green (reader, writer, float_bin_ext); VERSION bumped to 0.3.34
