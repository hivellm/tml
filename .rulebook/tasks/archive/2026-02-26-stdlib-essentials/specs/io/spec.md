# Spec: Buffered I/O Module

## Overview

Buffered wrappers for Read and Write to reduce syscall overhead.

## Types

### BufRead Behavior

Interface for buffered readers.

```tml
pub behavior BufRead: Read {
    /// Return internal buffer, filling it if empty
    func fill_buf(this) -> Outcome[ref [U8], IoError]

    /// Mark n bytes as consumed
    func consume(this, amt: U64)

    // Provided methods
    func read_until(this, byte: U8, buf: mut ref Vec[U8]) -> Outcome[U64, IoError] {
        // Read until byte found or EOF
    }

    func read_line(this, buf: mut ref Text) -> Outcome[U64, IoError] {
        // Read until \n, strip \r if present
    }

    func lines(this) -> Lines[This] {
        Lines { inner: this }
    }

    func split(this, byte: U8) -> Split[This] {
        Split { inner: this, delim: byte }
    }
}
```

### BufReader[R]

Buffered wrapper for any Read.

```tml
pub type BufReader[R] {
    inner: R,
    buf: Vec[U8],
    pos: U64,
    cap: U64,
}

pub const DEFAULT_BUF_SIZE: U64 = 8192

extend BufReader[R] where R: Read {
    /// Create with default buffer size (8KB)
    pub func new(inner: R) -> BufReader[R] {
        BufReader::with_capacity(DEFAULT_BUF_SIZE, inner)
    }

    /// Create with custom buffer size
    pub func with_capacity(capacity: U64, inner: R) -> BufReader[R] {
        BufReader {
            inner: inner,
            buf: Vec::with_capacity(capacity),
            pos: 0,
            cap: 0,
        }
    }

    /// Get reference to underlying reader
    pub func get_ref(this) -> ref R {
        ref this.inner
    }

    /// Get mutable reference to underlying reader
    pub func get_mut(this) -> mut ref R {
        mut ref this.inner
    }

    /// Return buffer contents
    pub func buffer(this) -> ref [U8] {
        ref this.buf[this.pos to this.cap]
    }

    /// Return buffer capacity
    pub func capacity(this) -> U64 {
        this.buf.capacity()
    }

    /// Consume wrapper and return inner reader
    pub func into_inner(this) -> R {
        this.inner
    }

    /// Seek to offset from current position, possibly within buffer
    pub func seek_relative(this, offset: I64) -> Outcome[Unit, IoError]
}

extend BufReader[R] with Read where R: Read {
    func read(this, buf: mut ref [U8]) -> Outcome[U64, IoError] {
        // If buffer empty, fill it
        // Copy from buffer to output
    }
}

extend BufReader[R] with BufRead where R: Read {
    func fill_buf(this) -> Outcome[ref [U8], IoError] {
        if this.pos >= this.cap {
            this.cap = this.inner.read(mut ref this.buf)!
            this.pos = 0
        }
        Ok(ref this.buf[this.pos to this.cap])
    }

    func consume(this, amt: U64) {
        this.pos = (this.pos + amt).min(this.cap)
    }
}
```

### BufWriter[W]

Buffered wrapper for any Write.

```tml
pub type BufWriter[W] {
    inner: Maybe[W],
    buf: Vec[U8],
    panicked: Bool,
}

extend BufWriter[W] where W: Write {
    /// Create with default buffer size (8KB)
    pub func new(inner: W) -> BufWriter[W] {
        BufWriter::with_capacity(DEFAULT_BUF_SIZE, inner)
    }

    /// Create with custom buffer size
    pub func with_capacity(capacity: U64, inner: W) -> BufWriter[W] {
        BufWriter {
            inner: Just(inner),
            buf: Vec::with_capacity(capacity),
            panicked: false,
        }
    }

    /// Get reference to underlying writer
    pub func get_ref(this) -> ref W {
        ref this.inner.as_ref().unwrap()
    }

    /// Get mutable reference to underlying writer
    pub func get_mut(this) -> mut ref W {
        mut ref this.inner.as_mut().unwrap()
    }

    /// Return buffer contents
    pub func buffer(this) -> ref [U8] {
        ref this.buf[..]
    }

    /// Return buffer capacity
    pub func capacity(this) -> U64 {
        this.buf.capacity()
    }

    /// Consume wrapper and return inner writer
    /// Flushes buffer first
    pub func into_inner(this) -> Outcome[W, IntoInnerError[BufWriter[W]]] {
        this.flush_buf()!
        Ok(this.inner.take().unwrap())
    }

    // Internal: flush buffer to inner writer
    func flush_buf(this) -> Outcome[Unit, IoError] {
        let written = this.inner.as_mut().unwrap().write(ref this.buf[..])?
        this.buf.drain(0 to written)
        Ok(unit)
    }
}

extend BufWriter[W] with Write where W: Write {
    func write(this, buf: ref [U8]) -> Outcome[U64, IoError] {
        // If data fits in buffer, copy it
        // Otherwise flush and write directly
        if this.buf.len() + buf.len() > this.buf.capacity() {
            this.flush_buf()!
        }
        if buf.len() >= this.buf.capacity() {
            // Write directly, bypass buffer
            this.inner.as_mut().unwrap().write(buf)
        } else {
            this.buf.extend_from_slice(buf)
            Ok(buf.len())
        }
    }

    func flush(this) -> Outcome[Unit, IoError] {
        this.flush_buf()!
        this.inner.as_mut().unwrap().flush()
    }
}

extend BufWriter[W] with Drop where W: Write {
    func drop(this) {
        if not this.panicked {
            let _ = this.flush_buf()
        }
    }
}

pub type IntoInnerError[W] {
    inner: W,
    error: IoError,
}
```

### LineWriter[W]

Writer that flushes on newline.

```tml
pub type LineWriter[W] {
    inner: BufWriter[W],
}

extend LineWriter[W] where W: Write {
    pub func new(inner: W) -> LineWriter[W] {
        LineWriter::with_capacity(1024, inner)
    }

    pub func with_capacity(capacity: U64, inner: W) -> LineWriter[W] {
        LineWriter {
            inner: BufWriter::with_capacity(capacity, inner),
        }
    }

    pub func get_ref(this) -> ref W
    pub func get_mut(this) -> mut ref W
    pub func into_inner(this) -> Outcome[W, IntoInnerError[LineWriter[W]]]
}

extend LineWriter[W] with Write where W: Write {
    func write(this, buf: ref [U8]) -> Outcome[U64, IoError] {
        let written = this.inner.write(buf)!
        // Check if buf contains newline
        if buf.contains(b'\n') {
            this.inner.flush()!
        }
        Ok(written)
    }

    func flush(this) -> Outcome[Unit, IoError] {
        this.inner.flush()
    }
}
```

### Iterators

```tml
pub type Lines[B] {
    buf: B,
}

extend Lines[B] with Iterator where B: BufRead {
    type Item = Outcome[Text, IoError]

    func next(this) -> Maybe[Outcome[Text, IoError]] {
        var line = Text::new()
        when this.buf.read_line(mut ref line) {
            Ok(0) => Nothing,
            Ok(_) => {
                // Strip trailing newline
                if line.ends_with("\n") {
                    line.pop()
                    if line.ends_with("\r") {
                        line.pop()
                    }
                }
                Just(Ok(line))
            },
            Err(e) => Just(Err(e)),
        }
    }
}

pub type Split[B] {
    buf: B,
    delim: U8,
}

extend Split[B] with Iterator where B: BufRead {
    type Item = Outcome[Vec[U8], IoError]

    func next(this) -> Maybe[Outcome[Vec[U8], IoError]] {
        var buf = Vec::new()
        when this.buf.read_until(this.delim, mut ref buf) {
            Ok(0) => Nothing,
            Ok(_) => {
                // Strip delimiter
                if buf.last() == Just(this.delim) {
                    buf.pop()
                }
                Just(Ok(buf))
            },
            Err(e) => Just(Err(e)),
        }
    }
}
```

## Performance Notes

- Default buffer size of 8KB provides good tradeoff
- BufReader reduces read syscalls by ~100x for small reads
- BufWriter reduces write syscalls similarly
- LineWriter is optimal for line-oriented output (logs, console)

## Example

```tml
use std::fs::File
use std::io::{BufReader, BufWriter, BufRead}

func process_file(path: Str) -> Outcome[Unit, IoError] {
    // Buffered reading
    let file = File::open(path)!
    let reader = BufReader::new(file)

    for line in reader.lines() {
        let line = line!
        println(line)
    }

    // Buffered writing
    let out = File::create("output.txt")!
    var writer = BufWriter::new(out)

    writer.write_all(b"Hello\n")!
    writer.write_all(b"World\n")!
    // Automatically flushed on drop

    Ok(unit)
}
```
