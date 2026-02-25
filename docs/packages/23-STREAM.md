# std::stream — Streaming Byte I/O

> **Status**: IMPLEMENTED (2026-02-24)

## 1. Overview

The `std::stream` module provides byte-oriented I/O abstractions inspired by Node.js streams, adapted for TML's synchronous ownership model.

```tml
use std::stream::{Readable, Writable, ByteStream, BufferedReader, BufferedWriter, pipe, copy_bytes}
```

## 2. Module Structure

| File | Description |
|------|-------------|
| `readable.tml` | `Readable` behavior — byte source interface |
| `writable.tml` | `Writable` behavior — byte sink interface |
| `byte_stream.tml` | `ByteStream` — in-memory read/write stream + utility functions |
| `buffered.tml` | `BufferedReader` / `BufferedWriter` — buffered wrappers |
| `pipe.tml` | `pipe()` / `copy_bytes()` — stream piping utilities |

## 3. Core Behaviors

### 3.1 Readable

The fundamental byte source abstraction. Anything you can `read()` from.

```tml
pub behavior Readable {
    /// Pull bytes from the source into `buf`.
    /// Returns byte count (0 = EOF). May return fewer bytes than buffer size.
    func read(mut this, buf: mut ref [U8]) -> Outcome[I64, IoError]
}
```

**Contract:**
- Returns the number of bytes placed into `buf` (may be less than `buf.len()`)
- Returns `0` to signal end-of-stream (EOF)
- Returns `Err(IoError)` on failure

### 3.2 Writable

The fundamental byte sink abstraction. Anything you can `write()` to.

```tml
pub behavior Writable {
    /// Push bytes from `buf` into the sink.
    /// Returns byte count consumed (may be less than `buf.len()` on backpressure).
    func write(mut this, buf: ref [U8]) -> Outcome[I64, IoError]

    /// Flush all buffered data to the underlying destination.
    func flush(mut this) -> Outcome[Unit, IoError]
}
```

## 4. Types

### 4.1 ByteStream

In-memory growable byte buffer implementing both `Readable` and `Writable`. Ideal for testing stream-based APIs and constructing HTTP bodies.

```tml
pub type ByteStream {
    handle: *Unit    // Internal header: data ptr, len, capacity, read_pos
}

impl ByteStream {
    /// Create an empty stream with default capacity.
    pub func new() -> ByteStream

    /// Create a stream with specific initial capacity.
    pub func with_capacity(cap: I64) -> ByteStream

    /// Create a stream pre-filled with string data.
    pub func from_string(s: Str) -> ByteStream

    /// Current number of bytes written.
    pub func len(this) -> I64

    /// Number of bytes remaining to read (len - read_pos).
    pub func remaining(this) -> I64

    /// Reset read position to beginning.
    pub func reset_read(mut this)

    /// Convert all written bytes to a string.
    pub func to_string(this) -> Str

    /// Free all allocated memory.
    pub func destroy(mut this)
}

impl Readable for ByteStream { ... }
impl Writable for ByteStream { ... }
```

**Example:**
```tml
use std::stream::{ByteStream, Readable, Writable}

var stream = ByteStream::from_string("Hello, streams!")
var buf: [U8; 5] = [0 as U8; 5]
let n = stream.read(mut ref buf)!   // n == 5, buf = "Hello"
stream.destroy()
```

### 4.2 BufferedReader

Wraps any `Readable` source with an internal buffer for line-oriented reading. Essential for parsing HTTP headers, config files, etc.

```tml
pub type BufferedReader {
    handle: *Unit    // Internal: buf_data, buf_cap, buf_pos, buf_filled
}

impl BufferedReader {
    /// Create with default capacity (8 KB).
    pub func new() -> BufferedReader

    /// Create with specific buffer capacity.
    pub func with_capacity(cap: I64) -> BufferedReader

    /// Read one line from the source (strips \r\n or \n).
    pub func read_line(mut this, source: mut ref ByteStream) -> Outcome[Str, IoError]

    /// Read all remaining data from the source as a string.
    pub func read_all(mut this, source: mut ref ByteStream) -> Outcome[Str, IoError]

    /// Check if end-of-stream was reached.
    pub func is_eof(this) -> Bool

    /// Free buffer memory.
    pub func destroy(mut this)
}
```

**Example:**
```tml
use std::stream::{ByteStream, BufferedReader}

var stream = ByteStream::from_string("line1\r\nline2\r\nline3\r\n")
var reader = BufferedReader::new()
let line1 = reader.read_line(mut ref stream)!  // "line1"
let line2 = reader.read_line(mut ref stream)!  // "line2"
let line3 = reader.read_line(mut ref stream)!  // "line3"
reader.destroy()
stream.destroy()
```

### 4.3 BufferedWriter

Batches writes and flushes when the buffer is full. Reduces syscall overhead for small writes.

```tml
pub type BufferedWriter {
    handle: *Unit
}

impl BufferedWriter {
    /// Create with default capacity (8 KB).
    pub func new() -> BufferedWriter

    /// Create with specific buffer capacity.
    pub func with_capacity(cap: I64) -> BufferedWriter

    /// Write string data to the buffer.
    pub func write_string(mut this, dest: mut ref ByteStream, data: Str) -> Outcome[I64, IoError]

    /// Flush all buffered data to the destination.
    pub func flush(mut this, dest: mut ref ByteStream) -> Outcome[Unit, IoError]

    /// Free buffer memory.
    pub func destroy(mut this)
}
```

## 5. Utility Functions

### 5.1 pipe

Copy all remaining bytes from source to destination.

```tml
pub func pipe(source: mut ref ByteStream, dest: mut ref ByteStream) -> Outcome[I64, IoError]
```

**Example:**
```tml
use std::stream::{ByteStream, pipe}

var src = ByteStream::from_string("Hello World")
var dst = ByteStream::new()
let n = pipe(mut ref src, mut ref dst)!  // n == 11
assert_eq(dst.to_string(), "Hello World")
```

### 5.2 copy_bytes

Copy up to `limit` bytes from source to destination. Stops at EOF or limit.

```tml
pub func copy_bytes(source: mut ref ByteStream, dest: mut ref ByteStream, limit: I64) -> Outcome[I64, IoError]
```

### 5.3 read_all / read_to_string / write_all / write_string

Convenience functions in `byte_stream.tml`:

```tml
/// Read all remaining bytes into a new ByteStream.
pub func read_all(source: mut ref ByteStream) -> Outcome[ByteStream, IoError]

/// Read all remaining bytes as a UTF-8 string.
pub func read_to_string(source: mut ref ByteStream) -> Outcome[Str, IoError]

/// Write all bytes from data, retrying on partial writes.
pub func write_all(dest: mut ref ByteStream, data: ref [U8]) -> Outcome[I64, IoError]

/// Write a string's bytes to the stream.
pub func write_string(dest: mut ref ByteStream, s: Str) -> Outcome[I64, IoError]
```

## 6. Usage with HTTP

The stream module is the foundation for HTTP body handling:

```tml
use std::stream::{ByteStream, BufferedReader}
use std::http::client::HttpClient

// HTTP client uses ByteStream internally for request/response bodies
let client = HttpClient::new()
let resp = client.get("https://example.com")!
// Response body is parsed from the raw TCP byte stream
```

## 7. Test Coverage

6 test files in `lib/std/tests/stream/`:

| File | Tests | Description |
|------|-------|-------------|
| `readable.test.tml` | 15+ | ByteStream read operations, EOF, partial reads |
| `readable_basic.test.tml` | 2 | Basic smoke tests |
| `writable.test.tml` | 8+ | ByteStream write operations |
| `buffered.test.tml` | 20+ | BufferedReader line reading, BufferedWriter batching |
| `pipe.test.tml` | 3+ | Pipe and copy operations |
| `copy.test.tml` | 3+ | copy_bytes with limits |
