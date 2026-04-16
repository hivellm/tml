# Proposal: phase1a_protobuf-complete-implementation

## Why

The `std::protobuf` module is ~75% complete. The wire format core (writer, reader,
varint, zigzag, packed, nested messages) is solid and tested. The `.proto` parser
and TML codegen compile and pass tests. But several features are missing for a
production-grade protobuf implementation:

1. **float/double bitcast** — `write_float`/`read_float` declared but untested; need
   IEEE 754 bit-level reinterpretation (`bitcast` in lowlevel)
2. **Packed repeated end-to-end** — `packed.tml` functions exist but no working test
   (K001 `len()` on `List[I64]` in packed writer functions)
3. **Map fields** — descriptor supports `is_map` but no automatic encode/decode
   (proto spec: map = repeated Entry { key=1; value=2; })
4. **Oneof runtime** — parser marks oneof fields as optional, but no validation that
   only one field in the group is set
5. **Default value omission** — proto3 rule: fields with default values (0, false, "")
   must not be serialized. Codegen should emit `if field != default { write }`
6. **Interop validation** — no byte-level comparison against protoc (Go/C++/Python)
   reference implementation
7. **Well-known types** — google.protobuf.Timestamp, Duration, Any, Struct, Value,
   FieldMask, Wrapper types
8. **Proto file I/O** — parser reads from string; need file-based `parse_proto_file(path)`
9. **Codegen file output** — `generate_tml` returns string; need `generate_to_file(proto, path)`
10. **Error types** — reader returns `Outcome[T, Str]`; should use structured `ProtoError` enum

Without these, the module can't be used for real-world gRPC interop or production
binary serialization.

## What Changes

### 1. Float/Double support
- Implement `write_float`/`read_float` using `lowlevel { bitcast }` for IEEE 754
- Implement `write_double`/`read_double` (currently uses fixed64, needs bitcast)
- Test with known float wire bytes (e.g., 3.14 = 0x4048F5C3 in big-endian IEEE 754)

### 2. Fix packed repeated
- Fix K001 `len()` issue: make packed writer functions work with current codegen
- Add end-to-end test: encode packed int32 array, decode, verify values match

### 3. Map field encode/decode
- Add `write_map_entry(field_num, key_writer, value_writer)` to ProtoWriter
- Add `read_map_entries(reader)` that decodes repeated Entry sub-messages
- Codegen: generate map encode/decode for `map<K,V>` fields

### 4. Oneof validation
- Add `OneofDescriptor` to descriptor.tml with field list
- Codegen: generate oneof encode (write only the set field) and decode (last-one-wins)

### 5. Default value omission
- Codegen: wrap each scalar field encode in `if value != default { write }`
- Define default values per type: 0 for numeric, false for bool, "" for string/bytes

### 6. Interop tests
- Write a Go program that encodes a Person message using `google.golang.org/protobuf`
- Capture the raw bytes as a hex string
- Write TML test that decodes those exact bytes and verifies field values
- Write TML test that encodes the same message and compares bytes

### 7. Well-known types
- Create `lib/std/src/protobuf/well_known.tml`
- Implement: Timestamp { seconds: I64, nanos: I32 },
  Duration { seconds: I64, nanos: I32 },
  Any { type_url: Str, value: Str },
  Struct/Value/ListValue for JSON mapping

### 8. File I/O for parser + codegen
- `parse_proto_file(path: Str) -> Outcome[ProtoFile, Str]` — reads file, then parses
- `generate_to_file(proto: ref ProtoFile, output_path: Str) -> Outcome[Unit, Str]`

### 9. Structured errors
- Replace `Outcome[T, Str]` with `Outcome[T, ProtoError]` in reader
- `ProtoError` enum: Eof, InvalidWireType, InvalidVarint, BufferOverflow, NegativeLength, InvalidUtf8, Custom(Str)

## Impact

- Affected specs: std/protobuf
- Affected code: `lib/std/src/protobuf/*.tml`, `lib/std/tests/protobuf/*.test.tml`
- Breaking change: YES (reader return types change from `Outcome[T, Str]` to `Outcome[T, ProtoError]`)
- User benefit: Production-grade protobuf for gRPC interop, schema-driven serialization, cross-language IPC
