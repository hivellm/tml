# Proposal: phase1b_msgpack-spec-complete

## Why

The current `std::msgpack` module (shipped in phase0t) covers ~61% of the official MessagePack specification (https://github.com/msgpack/msgpack/blob/master/spec.md). Key gaps prevent interoperability with other MessagePack implementations:

1. **Reader is asymmetric with writer** — `write_f32`/`write_f64`/`write_bin` exist but the reader has no corresponding `read_f32`/`read_f64`/`read_bin`, so roundtrip fails for floats and binary data.
2. **No extension type support** — the entire ext family (8 formats: fixext 1/2/4/8/16, ext 8/16/32) is absent, blocking Timestamp (ext type -1) and any custom type extensions.
3. **No Timestamp support** — the spec defines Timestamp as a built-in extension type (-1) with 3 encodings (32/64/96 bit); none are implemented.
4. **No generic decode** — there's no `peek_type()` to inspect the next value without consuming it, and no `skip()` to skip unknown fields. Both are essential for schema-flexible deserialization.
5. **No `MsgPackValue` dynamic type** — prevents building a generic in-memory representation of arbitrary MessagePack data (analogous to `JsonValue`).

This blocks the UzDB binary protocol from handling float columns, binary blobs, timestamps, and forward-compatible schema evolution.

## What Changes

### Reader completions (asymmetric gaps)
- Add `read_f32() -> Maybe[F32]` — decode 0xca
- Add `read_f64() -> Maybe[F64]` — decode 0xcb
- Add `read_bin() -> Maybe[Buffer]` — decode 0xc4/0xc5/0xc6

### Extension types (write + read)
- Add `write_ext(type_id: I8, data: ref Buffer)` — auto-selects fixext or ext 8/16/32
- Add `read_ext() -> Maybe[(I8, Buffer)]` — decode all ext formats
- Add `write_fixext(type_id: I8, data: ref Buffer, size: I64)` — explicit fixext 1/2/4/8/16

### Timestamp (ext type -1)
- Add `write_timestamp(seconds: I64, nanos: U32)` — auto-selects 32/64/96 encoding
- Add `read_timestamp() -> Maybe[(I64, U32)]` — decode all 3 timestamp formats

### Generic decode utilities
- Add `peek_type() -> MsgPackType` — inspect next byte without advancing cursor
- Add `skip()` — skip one complete value (including nested arrays/maps)
- Add `read_type() -> MsgPackType` — consume and classify the type byte

### MsgPackValue dynamic type (if codegen supports recursive enums)
- Add `MsgPackValue` enum with variants for all types
- Add `read_value() -> Maybe[MsgPackValue]` — generic decode
- Add `write_value(value: ref MsgPackValue)` — generic encode

## Impact
- Affected specs: `lib/std/src/msgpack/`
- Affected code: `writer.tml`, `reader.tml`, `types.tml`, `mod.tml`
- Breaking change: NO (purely additive)
- User benefit: Full MessagePack spec compliance, interop with all other msgpack implementations, timestamp support, schema-flexible deserialization
