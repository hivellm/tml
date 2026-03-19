---
name: HTTP/2 Module Notes (Sprint 8 + Sprint 9)
description: Implementation notes for lib/std/src/http/h2/ — codegen workarounds, API patterns, stream/connection design
type: project
---

## H2 Module Location
`lib/std/src/http/h2/` with `frame.tml`, `hpack.tml`, `stream.tml`, `connection.tml`, `server.tml`, `mod.tml`

## Codegen Workarounds Applied

1. **Struct-by-value with Buffer fields**: `H2Frame` contains `Buffer` (which has `*Unit`).
   Passing `H2Frame` by value to functions causes GEP codegen error.
   **Fix**: Use `h2_encode_frame_raw(type, flags, stream_id, payload)` which takes fields individually.
   Also added `H2Frame::encode()` method (uses `this` pointer receiver, works).

2. **List of tuples**: `List[(H2Setting, I64)]` and `List[(I32, I64)]` both cause
   "base element of getelementptr must be sized" error on `struct.tuple_*`.
   **Fix**: Created `H2SettingsResult` struct with parallel `ids: Buffer` and `values: Buffer`
   accessed by index methods `get_id(i)` and `get_value(i)`.

3. **Outcome with tuple**: `Outcome[(I64, I64), H2Error]` causes struct layout issues.
   **Fix**: Created `HpackIntResult { value, consumed }` and `HpackStrResult { value, consumed }`
   structs instead of returning tuples inside Outcome.

4. **Tuple returns from functions**: `hpack_static_find` returning `(I64, Bool)` and
   `HpackDynamicTable::find` returning `(I64, Bool)` need struct replacements.
   **Fix**: Created `HpackFindResult { index, name_only }`.

5. **`str::byte_at` doesn't exist**: Use `str::char_at` for ASCII byte access, or use
   `lowlevel { ptr_read[U8]((s as I64 + j) as *U8) }` for direct pointer reads.

6. **`mut ref` on struct params**: Mutations inside `hpack_encode_headers(ctx: mut ref HpackContext, ...)`
   do NOT propagate back to the caller's variable for dynamic table updates.
   **Workaround**: Don't test dynamic table state through caller's ref after encode.

7. **CRITICAL: ptr_read/ptr_write with structs containing Buffer/pointer fields causes HEAP CORRUPTION**.
   `ptr_read[T]`/`ptr_write[T]` for types with `Buffer`, `*Unit`, or other opaque pointer fields
   generates incorrect LLVM IR. The struct layout the compiler assumes doesn't match reality.
   **Fix**: ANY struct stored on the heap via `mem_alloc` + `ptr_write` MUST be a pure scalar struct
   (only I32, I64, H2StreamState-like newtypes). Move all Buffer fields out of heap-stored structs.
   This is why `H2Stream` was redesigned to remove `headers_buf: Buffer`.

8. **Returning Outcome[LargeStruct, Error] where LargeStruct has Buffer fields crashes**.
   `H2StreamEvent` with 2 Buffer fields caused heap corruption when returned through Outcome.
   **Fix**: Use scalar-only result structs (e.g., `H2StreamResult` with only I32/I64 fields).

9. **Private methods in impl blocks not resolved through this.method() calls**.
   `func handle_settings(this, ...)` (non-pub) called from `pub func process_frame(this, ...)`
   fails with "Unknown method: handle_settings".
   **Fix**: Make all methods `pub` — no private methods in impl blocks.

10. **Incremental cache fingerprinting ignores dependency changes**.
    Changing `connection.tml` does NOT invalidate the test file's cached codegen if the test
    file itself is unchanged. `mcp__tml__cache_invalidate` only affects meta cache, not incr cache.
    **Fix**: Add a comment like `// v2` to force test file hash change when dependencies change.

## Design Decisions (Sprint 9)

### H2Stream is pure scalar (no Buffer fields)
- 7 fields: id(I64), state(H2StreamState), send_window(I64), recv_window(I64),
  receiving_headers(I64), end_stream_received(I64), end_stream_sent(I64)
- Safe for `ptr_read[H2Stream]` / `ptr_write[H2Stream]` on heap
- Header block accumulation moved to H2Connection level

### H2StreamResult is scalar-only return type
- 5 I32/I64 fields, no Buffer
- Safe to return through `Outcome[H2StreamResult, H2Error]`

### H2Connection uses flat stream table
- `H2StreamTable`: fixed-size array of MAX_STREAMS (256) I64 pointers
- Linear scan for lookup (fine for <100 concurrent streams)
- Streams allocated via `mem_alloc(128)` + `ptr_write[H2Stream]`

### All frame encoding uses h2_encode_frame_raw() directly
- Never construct `H2Frame` struct and call `.encode()` — that triggers struct-by-value crash
- `h2_encode_frame_raw(type_byte, flags, stream_id, payload_buffer)` works reliably

## Test Files (all passing — 11 files)
### Sprint 8 (frame codec + HPACK)
- `h2_frame_type.test.tml` — frame type to/from U8, flags, equality
- `h2_frame_encode.test.tml` — encoding all frame types
- `h2_frame_decode.test.tml` — decoding, settings decode, roundtrips
- `h2_hpack_integer.test.tml` — integer encode/decode
- `h2_hpack_static.test.tml` — all 61 static table entries
- `h2_hpack_string.test.tml` — string literal encode/decode
- `h2_hpack_headers.test.tml` — indexed headers, dynamic table, roundtrip

### Sprint 9 (stream + connection + server)
- `h2_stream_state.test.tml` — stream creation, state names, can_send/can_recv
- `h2_stream_transitions.test.tml` — all state transitions (13 tests)
- `h2_connection_frames.test.tml` — settings, ping, goaway, window_update (8 tests)
- `h2_connection_streams.test.tml` — HEADERS, DATA, RST_STREAM, multi-stream (7 tests)
- `h2_flow_control.test.tml` — recv window decrement, send window update (3 tests)
- `h2_server_basic.test.tml` — preface validation, request extraction (6 tests)
