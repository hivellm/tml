## 1. Implementation
- [ ] 1.1 Read protobuf wire format spec (encoding.md) — understand varint, zigzag, wire types, tag encoding
- [ ] 1.2 Create `lib/std/src/protobuf.tml` — module root
- [ ] 1.3 Implement varint encoding/decoding — LEB128 for uint, zigzag for sint
- [ ] 1.4 Implement `ProtoWriter` — write_tag, write_varint, write_fixed32/64, write_bytes, write_string, write_field_* helpers
- [ ] 1.5 Implement `ProtoReader` — read_tag, read_varint, read_fixed32/64, read_bytes, read_string, skip_field, nested message decoding
- [ ] 1.6 Define `ProtoMessage` behavior — `encode(writer: ref ProtoWriter)` and `decode(reader: ref ProtoReader) -> Outcome[Self, ProtoError]`
- [ ] 1.7 Implement packed repeated fields support (wire type 2 with multiple varints/fixed values)

## 2. Tail (mandatory — enforced by rulebook v5.3.0)
- [ ] 2.1 Update or create documentation covering the implementation
- [ ] 2.2 Write tests covering the new behavior — varint edge cases (0, 1, max_u64, zigzag negatives), field encoding, nested messages, packed repeated, round-trip encode/decode, compatibility with protobuf reference test vectors
- [ ] 2.3 Run tests and confirm they pass
