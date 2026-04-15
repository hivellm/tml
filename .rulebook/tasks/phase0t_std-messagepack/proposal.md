# Proposal: phase0t_std-messagepack

## Why

TML has JSON serialization (`std::json`) but no compact binary serialization
format. MessagePack is the natural complement — same data model as JSON (maps,
arrays, strings, ints, floats, booleans, nil, binary) but 2-5× smaller on the
wire and faster to encode/decode. Needed by the UzDB database project for its
binary protocol between client and server (replacing JSON-over-TCP). Also useful
for IPC, caching, and any performance-sensitive data exchange. Standard in major
ecosystems: Rust (`rmp`/`rmp-serde`), Go (`msgpack`), Python (`msgpack`), C
(`msgpack-c`).

## What Changes

Create `lib/std/src/msgpack.tml` (or `lib/std/src/encode/msgpack.tml`) implementing:

1. **Encoder**: `MsgPackWriter` — writes MessagePack bytes to a `Buffer`.
   Methods: `write_nil`, `write_bool`, `write_i64`, `write_u64`, `write_f64`,
   `write_str`, `write_bin`, `write_array_header`, `write_map_header`,
   `write_ext`. Handles fixint/fixstr/fixarray/fixmap compact encoding.

2. **Decoder**: `MsgPackReader` — reads MessagePack bytes from a `Slice[U8]`.
   Methods: `read_nil`, `read_bool`, `read_i64`, `read_u64`, `read_f64`,
   `read_str`, `read_bin`, `read_array_header`, `read_map_header`, `peek_type`.

3. **Value type** (optional): `MsgPackValue` enum for dynamic/untyped access,
   similar to `JsonValue`.

4. **Integration**: `Serialize`/`Deserialize` behavior support so user types
   with `@auto(serialize, deserialize)` can round-trip through MessagePack.

Pure TML implementation using `Buffer`, `BinaryWriter`/`BinaryReader`, and
existing memory intrinsics. No C runtime needed.

## Impact

- Affected specs: `lib/std/src/msgpack.tml` (new module)
- Affected code: stdlib only — no compiler changes
- Breaking change: NO (additive)
- User benefit: compact binary serialization for databases, IPC, networking,
  caching. UzDB agent specifically requested this for their client/server
  protocol.
