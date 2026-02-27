# std::stream — Streaming Byte I/O

## Overview

The `std::stream` module provides streaming interfaces for byte-level I/O with support for buffering, transformation, and composition.

```tml
use std::stream                                    // all re-exports
use std::stream::{Readable, Writable}
use std::stream::{BufferedReader, BufferedWriter}
use std::stream::{ByteStream, DuplexStream}
use std::stream::{PassThroughStream, PipelineStream}
use std::stream::{pipe, copy_bytes, read_all, write_all}
```

## Core Behaviors

### Readable

Interface for reading bytes from a stream:

```tml
behavior Readable {
  func read(buf: ref Buffer, len: U64) -> Outcome[I64, Str]
    Read up to len bytes into buf, return bytes read or error
}
```

### Writable

Interface for writing bytes to a stream:

```tml
behavior Writable {
  func write(buf: ref Buffer, len: U64) -> Outcome[I64, Str]
    Write up to len bytes from buf, return bytes written or error
}
```

## Types

### BufferedReader

Wraps any `Readable` with configurable buffer:

```tml
type BufferedReader {
  inner: Heap[ref Readable],
  capacity: U64
}

func BufferedReader::new(inner: ref Readable, capacity: U64) -> Heap[BufferedReader]
  Create buffered reader with given capacity

func read_line(reader: ref BufferedReader) -> Maybe[Str]
  Read until newline (inclusive)

func read_all(reader: ref BufferedReader) -> Str
  Read entire stream to string

func is_eof(reader: ref BufferedReader) -> Bool
  Check if at end of stream
```

### BufferedWriter

Wraps any `Writable` with configurable buffer (auto-flush at capacity):

```tml
type BufferedWriter {
  inner: Heap[ref Writable],
  capacity: U64
}

func BufferedWriter::new(inner: ref Writable, capacity: U64) -> Heap[BufferedWriter]
  Create buffered writer with given capacity (auto-flush at capacity)

func write_line(writer: ref BufferedWriter, line: Str) -> Outcome[I64, Str]
  Write line with newline, return bytes written or error

func flush(writer: ref BufferedWriter) -> Outcome[I64, Str]
  Flush buffer immediately
```

### ByteStream

In-memory read/write stream:

```tml
type ByteStream {
  data: Heap[Buffer]
}

func ByteStream::new(capacity: U64) -> Heap[ByteStream]
  Create in-memory stream with initial capacity

func ByteStream::from_str(s: Str) -> Heap[ByteStream]
  Create stream pre-loaded with string content
```

### DuplexStream

Combined readable + writable stream with separate buffers:

```tml
type DuplexStream {
  read_buffer: Heap[Buffer],
  write_buffer: Heap[Buffer]
}

func DuplexStream::new(read_capacity: U64, write_capacity: U64) -> Heap[DuplexStream]
  Create duplex stream (read and write independently)
```

### PassThroughStream

Identity stream with optional transformation callback:

```tml
type PassThroughStream {
  transform: Maybe[I64]  // optional callback (I64 function pointer)
}

func PassThroughStream::new(transform_fn: I64) -> Heap[PassThroughStream]
  Create pass-through with optional per-byte/chunk transformation
```

### PipelineStream

Chain multiple streams with backpressure handling:

```tml
type PipelineStream {
  sources: List[Heap[ref Readable]],
  transforms: List[Heap[ref PassThroughStream]],
  sinks: List[Heap[ref Writable]]
}

func PipelineStream::new() -> Heap[PipelineStream]
  Create empty pipeline

func add_source(pipeline: ref PipelineStream, source: Heap[ref Readable]) -> Outcome[(), Str]
  Add readable source to pipeline

func add_transform(pipeline: ref PipelineStream, transform: Heap[PassThroughStream]) -> Outcome[(), Str]
  Add transformation stage to pipeline

func add_sink(pipeline: ref PipelineStream, sink: Heap[ref Writable]) -> Outcome[(), Str]
  Add writable sink to pipeline

func run(pipeline: ref PipelineStream) -> Outcome[(), Str]
  Execute pipeline with proper backpressure
```

## Utility Functions

### Basic Piping

```tml
func pipe(reader: ref Readable, writer: ref Writable) -> Outcome[I64, Str]
  Copy all bytes from reader to writer, return bytes transferred

func copy_bytes(reader: ref Readable, writer: ref Writable, buf_size: U64) -> Outcome[I64, Str]
  Copy with custom buffer size, return bytes transferred
```

### Reading

```tml
func read_all(reader: ref Readable) -> Str
  Read entire stream to string (uses default 8KB buffer)

func read_to_string(reader: ref Readable) -> Maybe[Str]
  Read stream, return Nothing on error
```

### Writing

```tml
func write_all(writer: ref Writable, data: Str) -> Outcome[I64, Str]
  Write entire string, return bytes written or error

func write_string(writer: ref Writable, s: Str) -> Maybe[I64]
  Write string, return Nothing on error
```

## Example: Reading a File with Buffering

```tml
use std::stream::{BufferedReader, Readable}
use std::file::File

func count_lines() {
  let file = File::open("data.txt").unwrap()
  let reader = BufferedReader::new(file, 4096)

  loop when reader.read_line() {
    Just(line) => print("Line: {line}\n"),
    Nothing => break
  }
}
```

## See Also

- [std::file](./01-FS.md) — File I/O
- [std::net](./02-NET.md) — Network I/O
- [std::http](./07-HTTP.md) — HTTP with streaming responses
- [std::buffer](./03-BUFFER.md) — Binary buffer operations

---

*Previous: [22-](./22-.md)*
*Next: [24-SQLITE.md](./24-SQLITE.md)*
