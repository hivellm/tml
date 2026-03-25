# Tasks: Seek Behavior — Random Access I/O

**Status**: Phase 1 Complete (8/10)
**Priority**: MEDIUM
**Phase**: 2 — Stdlib Completeness

## Phase 1: Seek Behavior — DONE

- [x] 1.1 Define `SeekFrom` enum: `Start(I64)`, `End(I64)`, `Current(I64)`
- [x] 1.2 Define `Seek` behavior: `func seek(mut this, pos: SeekFrom) -> Outcome[I64, IoError]`
- [ ] 1.3 Implement `Seek` for `File` — needs `fseek`/`ftell` FFI (deferred)
- [x] 1.4 Implement `Cursor` type — in-memory seekable stream over byte buffer
- [x] 1.5 Implement `Seek` for `Cursor` — Start/End/Current with bounds checking
- [x] 1.6 Helper: `stream_position[S: Seek]` — shorthand for `seek(Current(0))`
- [x] 1.7 Helper: `rewind[S: Seek]` — shorthand for `seek(Start(0))`
- [x] 1.8 Update `stream/mod.tml` to export Seek, SeekFrom, Cursor
- [x] 1.9 Tests: 10 tests (from_str, read_byte, seek start/current/end, errors, reset, peek, eof)
- [ ] 1.10 Run full stream test suite — pending (Seek for ByteStream needs handle layout knowledge)

## Notes
- `Cursor` replaces `Seek for ByteStream` — simpler, self-contained, no handle layout dependency
- File seeking deferred until `fseek`/`ftell` FFI wrappers are added
- Cursor methods: from_str, from_raw, position, len, remaining, is_eof, read_byte, peek_byte, set_position, reset
