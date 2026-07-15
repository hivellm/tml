# Proposal: phase0u_std-protobuf

## Why

Protocol Buffers (protobuf) is the de-facto standard for schema-driven binary
serialization in microservices, gRPC, and cross-language IPC. While MessagePack
covers schemaless serialization, protobuf fills the schema-first niche: strongly
typed, compact, backward-compatible wire format with field numbering. Essential
for any TML project that needs to interoperate with Go/Rust/Java/C++ services
via gRPC or protobuf-over-TCP. Combined with MessagePack (`phase0t`), TML would
have complete binary serialization coverage.

## What Changes

Create `lib/std/src/protobuf.tml` implementing the protobuf wire format:

1. **Wire format encoder/decoder**: `ProtoWriter` / `ProtoReader` — handles
   varint, fixed32/64, length-delimited, and start/end group wire types.
   Methods: `write_varint`, `write_fixed32`, `write_fixed64`, `write_bytes`,
   `write_string`, `write_tag`, `read_varint`, `read_fixed32`, etc.

2. **Field-level API**: `write_field_int32(field_num, value)`,
   `write_field_string(field_num, value)`, `write_field_message(field_num, msg)`,
   `write_field_repeated(field_num, items)`. Handles packed repeated fields.

3. **Message trait**: `ProtoMessage` behavior with `encode(writer: ref ProtoWriter)`
   and `decode(reader: ref ProtoReader) -> Outcome[Self, ProtoError]`. User types
   implement this to define their wire layout.

4. **Future**: `.proto` file parser and code generator (`tml protoc`) — out of
   scope for this task, but the wire format library is the foundation.

Pure TML implementation using `Buffer`, `BinaryWriter`/`BinaryReader`.

## Impact

- Affected specs: `lib/std/src/protobuf.tml` (new module)
- Affected code: stdlib only — no compiler changes
- Breaking change: NO (additive)
- User benefit: interop with gRPC services, compact schema-driven serialization,
  backward-compatible wire format for evolving APIs.
