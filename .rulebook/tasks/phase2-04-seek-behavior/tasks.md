# Tasks: Seek Behavior — Random Access I/O

**Status**: Proposed
**Priority**: MEDIUM
**Phase**: 2 — Stdlib Completeness

## Motivation

TML has `Readable` (sequential read) and `Writable` (sequential write) behaviors but no `Seek` for random access. This means you can't jump to a position in a file, implement database page access, or rewind a buffer. Rust's `std::io::Seek` completes the I/O trait trio.

## Phase 1: Seek Behavior (`lib/std/src/stream/seek.tml`)

- [ ] 1.1 Define `SeekFrom` enum: `Start(offset: I64)`, `End(offset: I64)`, `Current(offset: I64)`
- [ ] 1.2 Define `Seek` behavior: `func seek(mut this, pos: SeekFrom) -> Outcome[I64, IoError]` (returns new position)
- [ ] 1.3 Implement `Seek` for `File` — use C FFI `fseek`/`ftell`
- [ ] 1.4 Implement `Seek` for `ByteStream` — in-memory seeking
- [ ] 1.5 Implement `Seek` for `Buffer` — buffer position seeking
- [ ] 1.6 Helper: `stream_position(mut this) -> Outcome[I64, IoError]` — shorthand for `seek(Current(0))`
- [ ] 1.7 Helper: `rewind(mut this) -> Outcome[Unit, IoError]` — shorthand for `seek(Start(0))`
- [ ] 1.8 Update `stream/mod.tml` to export Seek and SeekFrom
- [ ] 1.9 Write tests: seek in file, seek in ByteStream, rewind, stream_position
- [ ] 1.10 Run stream + file test suites
