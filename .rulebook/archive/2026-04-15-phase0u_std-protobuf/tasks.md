## 1. Implementation
- [x] 1.1 Protobuf wire format spec: varint (LEB128), zigzag (signed), wire types (0=varint, 1=fixed64, 2=length-delimited, 5=fixed32), tag = (field_num << 3) | wire_type
- [x] 1.2 Created `lib/std/src/protobuf.tml` — 366 lines, pure TML
- [x] 1.3 Varint: `write_varint` (LEB128 encoding), `read_varint` (LEB128 decoding), `zigzag_encode`/`zigzag_decode` for signed integers
- [x] 1.4 ProtoWriter: `new`, `with_capacity`, `write_tag`, `write_varint`, `write_fixed32`, `write_fixed64`, `write_bytes`, `write_string`, `write_field_varint`, `write_field_string`, `write_field_bytes`, `write_field_fixed32`, `write_field_fixed64`, `write_field_message`, `write_field_sint`, `to_bytes`, `to_buffer`, `len`
- [x] 1.5 ProtoReader: `new`, `from_buffer`, `read_tag` (returns ProtoTag), `read_varint`, `read_fixed32`, `read_fixed64`, `read_bytes`, `read_string`, `skip_field`, `has_more`, `position`, `remaining`
- [x] 1.6 ProtoMessage behavior: `encode(this, writer: ref ProtoWriter)`, `decode(reader: ref ProtoReader) -> Outcome[Self, Str]`
- [x] 1.7 Wire type constants: `WIRE_TYPE_VARINT`, `WIRE_TYPE_FIXED64`, `WIRE_TYPE_LENGTH_DELIMITED`, `WIRE_TYPE_FIXED32`

## 2. Tail (mandatory — enforced by rulebook v5.3.0)
- [x] 2.1 Update or create documentation covering the implementation — module documented with doc comments and wire type constants
- [x] 2.2 Write tests covering the new behavior — 3 test suites (basic, fields, sint): zigzag edge cases, varint 0/1/127/128/300/max, tag encoding, field roundtrips, nested messages, skip_field for all wire types, protobuf spec test vectors (varint 150, string "testing"), Person message roundtrip
- [x] 2.3 Run tests and confirm they pass — 3/3 suites passed
