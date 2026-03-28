# Proposal: Seek Behavior — Random Access I/O

## Status: PROPOSED

## Summary

TML has `Readable` and `Writable` behaviors for sequential I/O but no way to seek to a position. This task adds a `Seek` behavior with a `SeekFrom` enum (Start, End, Current) and implements it for `File`, `ByteStream`, and `Buffer`. This completes the I/O behavior trio that Rust's `std::io::{Read, Write, Seek}` defines.

## Motivation

Without `Seek`, TML code cannot:
- Read a binary file format that has a header, then jump to a data section at a known offset
- Implement a database page reader that accesses pages by position
- Rewind a `ByteStream` to re-parse it after partial consumption
- Implement a streaming parser that needs to backtrack

These patterns appear in JSON/MessagePack parsers, SQLite page access, HTTP range requests, and ZIP file reading. All of them require random access.

## Design

`SeekFrom` is an enum with three variants:
- `Start(offset: I64)` — absolute position from the beginning
- `End(offset: I64)` — position relative to the end (typically negative)
- `Current(offset: I64)` — position relative to current cursor

The `Seek` behavior has one required method: `seek(mut this, pos: SeekFrom) -> Outcome[I64, IoError]` returning the new absolute position. Two helper functions are provided as default implementations on the behavior: `stream_position` (= `seek(Current(0))`) and `rewind` (= `seek(Start(0))`).

Implementations:
- `File` uses `fseek`/`ftell` via `@extern("c")`
- `ByteStream` maintains an internal position index and bounds-checks
- `Buffer` seeks within its current byte extent

The behavior is defined in `lib/std/src/stream/seek.tml` and exported from `stream/mod.tml`.

## What Changes

- New: `lib/std/src/stream/seek.tml` — SeekFrom enum, Seek behavior
- Modified: `lib/std/src/file/file.tml` — implement Seek for File
- Modified: `lib/std/src/stream/byte_stream.tml` — implement Seek for ByteStream
- Modified: `lib/std/src/collections/buffer.tml` — implement Seek for Buffer
- Modified: `lib/std/src/stream/mod.tml` — export Seek, SeekFrom
- New: `lib/std/tests/stream/seek.test.tml`

## Dependencies

- Depends on: `Readable`, `Writable` behaviors, `IoError`, `File`, `ByteStream`, `Buffer`
- Enables: binary file format parsers, database page access, HTTP range requests
- Enables: `phase2-05-bigint` (BigInt parsing from byte streams)

## Risks

- `File::seek` with `SeekFrom::End` requires knowing the file size; `fseek` handles this but the returned position from `ftell` after `fseek(SEEK_END, 0)` may behave differently on Windows vs POSIX for files opened in text mode
- `Buffer::seek` must decide whether seeking beyond current length is an error or silently extends — it should return an error to match `ByteStream` semantics
