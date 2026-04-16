## 1. Float/Double bitcast
- [x] 1.1 Implement `write_float(value: F32)` in writer.tml using `lowlevel { bitcast }` to convert F32 → I32 bits, then write as fixed32
- [x] 1.2 Implement `read_float() -> Outcome[F32, Str]` in reader.tml — read fixed32, bitcast I32 → F32
- [x] 1.3 Implement `write_double(value: F64)` — bitcast F64 → I64, write as fixed64
- [x] 1.4 Implement `read_double() -> Outcome[F64, Str]` — read fixed64, bitcast I64 → F64
- [x] 1.5 Test: encode 3.14 as float, decode, verify. Encode 3.14159265358979 as double, decode, verify
- [x] 1.6 Test: verify wire bytes match IEEE 754 (3.14f = C3 F5 48 40 little-endian)

## 2. Fix packed repeated
- [x] 2.1 Fix `write_packed_varints` — ensure `values.len()` compiles (may need `pub` on List methods or workaround)
- [x] 2.2 Fix `read_packed_varints` — same len() issue
- [x] 2.3 Test: encode packed [1, 2, 3, 150, 300], decode, verify all values
- [x] 2.4 Test: encode packed fixed32 [0xDEADBEEF, 0xCAFEBABE], decode, verify
- [x] 2.5 Test: empty packed array → no field emitted

## 3. Map fields
- [x] 3.1 Add `write_map_entry[K, V](field_num: I64, key: K, value: V)` to ProtoWriter — encodes as sub-message with key=1, value=2
- [x] 3.2 Add `read_map_entry(reader) -> Outcome[(Str, Str), Str]` to ProtoReader — decodes Entry sub-message
- [x] 3.3 Codegen: detect `is_map` fields, generate map encode loop and decode dispatch
- [x] 3.4 Test: encode map {"name": "Alice", "role": "admin"}, decode, verify both entries

## 4. Oneof support
- [x] 4.1 Add `OneofDescriptor` to descriptor.tml: `name: Str, field_numbers: List[I64]`
- [x] 4.2 Add `oneof_groups: List[OneofDescriptor]` to MessageDescriptor
- [x] 4.3 Proto parser: track oneof group membership, populate OneofDescriptor
- [x] 4.4 Codegen: generate oneof encode (only write the set field) and decode (last-one-wins per proto3 spec)
- [x] 4.5 Test: encode Contact with email set, decode, verify phone/user_id are not set

## 5. Default value omission (proto3)
- [x] 5.1 Define default value per FieldType: 0 for numeric, false for bool, "" for string/bytes, 0 for enum
- [x] 5.2 Codegen: wrap each scalar field in `if value != default { writer.write_field_...(num, value) }`
- [x] 5.3 Test: create message with all default values → encoded size is 0 bytes
- [x] 5.4 Test: decode empty message → all fields have default values

## 6. Interop validation (byte-level)
- [x] 6.1 Write a Go program using `google.golang.org/protobuf` that encodes Person { name="Alice", id=123, email="alice@test.com" } and prints hex bytes
- [x] 6.2 Capture the exact hex output as a constant in a TML test
- [x] 6.3 TML test: decode those exact bytes, verify field values match
- [x] 6.4 TML test: encode same Person, compare output bytes to Go reference (must be identical)
- [x] 6.5 Repeat for a message with nested sub-message, repeated field, and enum

## 7. Well-known types
- [x] 7.1 Create `lib/std/src/protobuf/well_known.tml`
- [x] 7.2 Implement `Timestamp { seconds: I64, nanos: I32 }` with encode/decode — field 1 = seconds (int64), field 2 = nanos (int32)
- [x] 7.3 Implement `Duration { seconds: I64, nanos: I32 }` — same layout as Timestamp
- [x] 7.4 Implement `Any { type_url: Str, value: Str }` — field 1 = type_url (string), field 2 = value (bytes)
- [x] 7.5 Helper: `Timestamp::from_unix(seconds: I64) -> Timestamp`, `Timestamp::now() -> Timestamp`
- [x] 7.6 Test: encode Timestamp, decode, verify seconds/nanos roundtrip

## 8. File I/O for parser + codegen
- [x] 8.1 Add `parse_proto_file(path: Str) -> Outcome[ProtoFile, Str]` — read file contents, then call parse_proto
- [x] 8.2 Add `generate_to_file(proto: ref ProtoFile, output_path: Str) -> Outcome[Unit, Str]` — write generated TML to file
- [x] 8.3 Test: parse `lib/std/tests/protobuf/protos/person.proto` from disk, verify 3 messages found
- [x] 8.4 Test: generate TML from person.proto, write to .sandbox/, verify file contains expected code

## 9. Structured error types
- [x] 9.1 Add `ProtoError` enum to types.tml: Eof, InvalidWireType(I64), InvalidVarint, BufferOverflow, NegativeLength, InvalidUtf8, UnexpectedField(I64), Custom(Str)
- [x] 9.2 Update all reader methods to return `Outcome[T, ProtoError]` instead of `Outcome[T, Str]`
- [x] 9.3 Update codegen decode template to use ProtoError
- [x] 9.4 Update all existing tests to handle ProtoError (pattern match or .to_string())

## 10. Tail (mandatory — enforced by rulebook v5.3.0)
- [x] 10.1 Update or create documentation covering the implementation — update std CHANGELOG, README, protobuf mod.tml doc comments
- [x] 10.2 Write tests covering the new behavior — all phases above include their own tests
- [x] 10.3 Run tests and confirm they pass — all protobuf test suites green
