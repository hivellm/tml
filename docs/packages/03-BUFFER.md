# std.buffer — Binary Buffers and Streams

## 1. Overview

The `std.buffer` package provides efficient binary data handling: buffers, byte readers/writers, and binary encoding.

```tml
import std.buffer
import std.buffer.{Buffer, ByteReader, ByteWriter}
```

## 2. Capabilities

```tml
// No capabilities required - pure data manipulation
```

## 3. Core Types

### 3.1 Buffer

```tml
public type Buffer {
    data: List[U8],
    position: U64,
}

extend Buffer {
    /// Create empty buffer
    public func new() -> This {
        return This { data: List.new(), position: 0 }
    }

    /// Create buffer with capacity
    public func with_capacity(cap: U64) -> This {
        return This { data: List.with_capacity(cap), position: 0 }
    }

    /// Create from bytes
    public func from_bytes(bytes: ref [U8]) -> This {
        var data = List.with_capacity(bytes.len())
        data.extend_from_slice(bytes)
        return This { data: data, position: 0 }
    }

    /// Create from List
    public func from_list(list: List[U8]) -> This {
        return This { data: list, position: 0 }
    }

    /// Get length
    public func len(this) -> U64 {
        return this.data.len()
    }

    /// Get capacity
    public func capacity(this) -> U64 {
        return this.data.capacity()
    }

    /// Check if empty
    public func is_empty(this) -> Bool {
        return this.data.is_empty()
    }

    /// Current position
    public func position(this) -> U64 {
        return this.position
    }

    /// Set position
    public func set_position(this, pos: U64) {
        this.position = pos.min(this.data.len())
    }

    /// Remaining bytes from position
    public func remaining(this) -> U64 {
        return this.data.len() - this.position
    }

    /// Get bytes slice
    public func as_bytes(this) -> ref [U8] {
        return this.data.as_slice()
    }

    /// Get mutable bytes slice
    public func as_bytes_mut(this) -> mut ref [U8] {
        return this.data.as_mut_slice()
    }

    /// Get remaining bytes from position
    public func remaining_bytes(this) -> ref [U8] {
        return &this.data.as_slice()[this.position..]
    }

    /// Take ownership of internal data
    public func into_inner(this) -> List[U8] {
        return this.data
    }

    /// Clear buffer
    public func clear(this) {
        this.data.clear()
        this.position = 0
    }

    /// Reserve additional capacity
    public func reserve(this, additional: U64) {
        this.data.reserve(additional)
    }

    /// Truncate to length
    public func truncate(this, len: U64) {
        this.data.truncate(len)
        if this.position > len {
            this.position = len
        }
    }

    /// Extend with bytes
    public func extend(this, bytes: ref [U8]) {
        this.data.extend_from_slice(bytes)
    }

    /// Split at position
    public func split_off(this, at: U64) -> Buffer {
        let other = this.data.split_off(at)
        if this.position > at {
            this.position = at
        }
        return Buffer { data: other, position: 0 }
    }
}

extend Buffer with Read {
    func read(this, buf: mut ref [U8]) -> Outcome[U64, IoError] {
        let available = this.remaining()
        let to_read = buf.len().min(available)

        if to_read == 0 {
            return Ok(0)
        }

        buf[..to_read].copy_from_slice(&this.data[this.position..this.position + to_read])
        this.position += to_read
        return Ok(to_read)
    }
}

extend Buffer with Write {
    func write(this, buf: ref [U8]) -> Outcome[U64, IoError] {
        this.data.extend_from_slice(buf)
        return Ok(buf.len())
    }

    func flush(this) -> Outcome[Unit, IoError] {
        return Ok(unit)
    }
}

extend Buffer with Seek {
    func seek(this, pos: SeekFrom) -> Outcome[U64, IoError] {
        let new_pos = when pos {
            Start(n) -> n,
            End(n) -> (this.data.len() as I64 + n) as U64,
            Current(n) -> (this.position as I64 + n) as U64,
        }

        if new_pos > this.data.len() {
            return Err(IoError.new(InvalidInput, "seek beyond end"))
        }

        this.position = new_pos
        return Ok(new_pos)
    }
}
```

### 3.2 ByteReader

```tml
public type ByteReader[R: Read] {
    inner: R,
    buf: [U8; 8],
}

extend ByteReader[R: Read] {
    public func new(inner: R) -> This {
        return This { inner: inner, buf: [0; 8] }
    }

    /// Read exact number of bytes
    public func read_exact(this, buf: mut ref [U8]) -> Outcome[Unit, IoError] {
        this.inner.read_exact(buf)
    }

    /// Read single byte
    public func read_u8(this) -> Outcome[U8, IoError] {
        this.inner.read_exact(mut ref this.buf[0 to 1])!
        return Ok(this.buf[0])
    }

    /// Read signed byte
    public func read_i8(this) -> Outcome[I8, IoError] {
        return Ok(this.read_u8()! as I8)
    }

    // Little-endian reads
    public func read_u16_le(this) -> Outcome[U16, IoError] {
        this.inner.read_exact(mut ref this.buf[0 to 2])!
        return Ok(U16.from_le_bytes([this.buf[0], this.buf[1]]))
    }

    public func read_u32_le(this) -> Outcome[U32, IoError] {
        this.inner.read_exact(mut ref this.buf[0 to 4])!
        return Ok(U32.from_le_bytes([this.buf[0], this.buf[1], this.buf[2], this.buf[3]]))
    }

    public func read_u64_le(this) -> Outcome[U64, IoError] {
        this.inner.read_exact(mut ref this.buf)!
        return Ok(U64.from_le_bytes(this.buf))
    }

    public func read_i16_le(this) -> Outcome[I16, IoError] {
        return Ok(this.read_u16_le()! as I16)
    }

    public func read_i32_le(this) -> Outcome[I32, IoError] {
        return Ok(this.read_u32_le()! as I32)
    }

    public func read_i64_le(this) -> Outcome[I64, IoError] {
        return Ok(this.read_u64_le()! as I64)
    }

    public func read_f32_le(this) -> Outcome[F32, IoError] {
        return Ok(F32.from_bits(this.read_u32_le()!))
    }

    public func read_f64_le(this) -> Outcome[F64, IoError] {
        return Ok(F64.from_bits(this.read_u64_le()!))
    }

    // Big-endian reads
    public func read_u16_be(this) -> Outcome[U16, IoError] {
        this.inner.read_exact(mut ref this.buf[0 to 2])!
        return Ok(U16.from_be_bytes([this.buf[0], this.buf[1]]))
    }

    public func read_u32_be(this) -> Outcome[U32, IoError] {
        this.inner.read_exact(mut ref this.buf[0 to 4])!
        return Ok(U32.from_be_bytes([this.buf[0], this.buf[1], this.buf[2], this.buf[3]]))
    }

    public func read_u64_be(this) -> Outcome[U64, IoError] {
        this.inner.read_exact(mut ref this.buf)!
        return Ok(U64.from_be_bytes(this.buf))
    }

    public func read_i16_be(this) -> Outcome[I16, IoError]
    public func read_i32_be(this) -> Outcome[I32, IoError]
    public func read_i64_be(this) -> Outcome[I64, IoError]
    public func read_f32_be(this) -> Outcome[F32, IoError]
    public func read_f64_be(this) -> Outcome[F64, IoError]

    // Variable-length integers (LEB128)
    public func read_varint(this) -> Outcome[I64, IoError] {
        var result: I64 = 0
        var shift: U32 = 0

        loop {
            let byte = this.read_u8()!
            result |= ((byte & 0x7F) as I64) << shift

            if (byte & 0x80) == 0 {
                // Sign extend
                if shift < 64 and (byte & 0x40) != 0 {
                    result |= !0 << (shift + 7)
                }
                break
            }

            shift += 7
            if shift >= 64 {
                return Err(IoError.new(InvalidData, "varint too long"))
            }
        }

        return Ok(result)
    }

    public func read_uvarint(this) -> Outcome[U64, IoError] {
        var result: U64 = 0
        var shift: U32 = 0

        loop {
            let byte = this.read_u8()!
            result |= ((byte & 0x7F) as U64) << shift

            if (byte & 0x80) == 0 {
                break
            }

            shift += 7
            if shift >= 64 {
                return Err(IoError.new(InvalidData, "uvarint too long"))
            }
        }

        return Ok(result)
    }

    // Strings
    public func read_string(this, len: U64) -> Outcome[String, IoError] {
        var bytes = List.with_capacity(len)
        bytes.resize(len, 0)
        this.inner.read_exact(bytes.as_mut_slice())!
        return String.from_utf8(bytes).map_err(do(_) IoError.new(InvalidData, "invalid UTF-8"))
    }

    public func read_cstring(this) -> Outcome[String, IoError] {
        var bytes = List.new()
        loop {
            let b = this.read_u8()!
            if b == 0 {
                break
            }
            bytes.push(b)
        }
        return String.from_utf8(bytes).map_err(do(_) IoError.new(InvalidData, "invalid UTF-8"))
    }

    public func read_length_prefixed_string(this) -> Outcome[String, IoError] {
        let len = this.read_uvarint()!
        return this.read_string(len)
    }

    /// Get inner reader
    public func into_inner(this) -> R {
        return this.inner
    }
}
```

### 3.3 ByteWriter

```tml
public type ByteWriter[W: Write] {
    inner: W,
    buf: [U8; 8],
}

extend ByteWriter[W: Write] {
    public func new(inner: W) -> This {
        return This { inner: inner, buf: [0; 8] }
    }

    /// Write single byte
    public func write_u8(this, v: U8) -> Outcome[Unit, IoError] {
        this.buf[0] = v
        this.inner.write_all(&this.buf[0 to 1])
    }

    /// Write signed byte
    public func write_i8(this, v: I8) -> Outcome[Unit, IoError] {
        this.write_u8(v as U8)
    }

    // Little-endian writes
    public func write_u16_le(this, v: U16) -> Outcome[Unit, IoError] {
        let bytes = v.to_le_bytes()
        this.inner.write_all(&bytes)
    }

    public func write_u32_le(this, v: U32) -> Outcome[Unit, IoError] {
        let bytes = v.to_le_bytes()
        this.inner.write_all(&bytes)
    }

    public func write_u64_le(this, v: U64) -> Outcome[Unit, IoError] {
        let bytes = v.to_le_bytes()
        this.inner.write_all(&bytes)
    }

    public func write_i16_le(this, v: I16) -> Outcome[Unit, IoError]
    public func write_i32_le(this, v: I32) -> Outcome[Unit, IoError]
    public func write_i64_le(this, v: I64) -> Outcome[Unit, IoError]
    public func write_f32_le(this, v: F32) -> Outcome[Unit, IoError]
    public func write_f64_le(this, v: F64) -> Outcome[Unit, IoError]

    // Big-endian writes
    public func write_u16_be(this, v: U16) -> Outcome[Unit, IoError]
    public func write_u32_be(this, v: U32) -> Outcome[Unit, IoError]
    public func write_u64_be(this, v: U64) -> Outcome[Unit, IoError]
    public func write_i16_be(this, v: I16) -> Outcome[Unit, IoError]
    public func write_i32_be(this, v: I32) -> Outcome[Unit, IoError]
    public func write_i64_be(this, v: I64) -> Outcome[Unit, IoError]
    public func write_f32_be(this, v: F32) -> Outcome[Unit, IoError]
    public func write_f64_be(this, v: F64) -> Outcome[Unit, IoError]

    // Variable-length integers
    public func write_varint(this, v: I64) -> Outcome[Unit, IoError] {
        var value = v
        loop {
            let byte = (value & 0x7F) as U8
            value >>= 7

            let done = (value == 0 and (byte & 0x40) == 0) or
                       (value == -1 and (byte & 0x40) != 0)

            if done {
                this.write_u8(byte)!
                break
            } else {
                this.write_u8(byte | 0x80)!
            }
        }
        return Ok(unit)
    }

    public func write_uvarint(this, v: U64) -> Outcome[Unit, IoError] {
        var value = v
        loop {
            let byte = (value & 0x7F) as U8
            value >>= 7

            if value == 0 {
                this.write_u8(byte)!
                break
            } else {
                this.write_u8(byte | 0x80)!
            }
        }
        return Ok(unit)
    }

    // Strings
    public func write_string(this, s: ref str) -> Outcome[Unit, IoError] {
        this.inner.write_all(s.as_bytes())
    }

    public func write_cstring(this, s: ref str) -> Outcome[Unit, IoError] {
        this.inner.write_all(s.as_bytes())!
        this.write_u8(0)
    }

    public func write_length_prefixed_string(this, s: ref str) -> Outcome[Unit, IoError] {
        this.write_uvarint(s.len())!
        this.write_string(s)
    }

    public func flush(this) -> Outcome[Unit, IoError] {
        this.inner.flush()
    }

    public func into_inner(this) -> W {
        return this.inner
    }
}
```

## 4. Buffered I/O

### 4.1 BufReader

```tml
public type BufReader[R: Read] {
    inner: R,
    buf: List[U8],
    pos: U64,
    cap: U64,
}

public const DEFAULT_BUF_SIZE: U64 = 8192

extend BufReader[R: Read] {
    public func new(inner: R) -> This {
        return This.with_capacity(DEFAULT_BUF_SIZE, inner)
    }

    public func with_capacity(cap: U64, inner: R) -> This {
        var buf = List.with_capacity(cap)
        buf.resize(cap, 0)
        return This { inner: inner, buf: buf, pos: 0, cap: 0 }
    }

    /// Fill internal buffer
    func fill_buf(this) -> Outcome[ref [U8], IoError] {
        if this.pos >= this.cap {
            this.cap = this.inner.read(this.buf.as_mut_slice())!
            this.pos = 0
        }
        return Ok(&this.buf[this.pos..this.cap])
    }

    /// Consume n bytes from buffer
    func consume(this, n: U64) {
        this.pos = (this.pos + n).min(this.cap)
    }

    /// Read until delimiter
    public func read_until(this, delim: U8, buf: mut ref List[U8]) -> Outcome[U64, IoError] {
        var read: U64 = 0
        loop {
            let available = this.fill_buf()!
            if available.is_empty() {
                break
            }

            when available.iter().position(|b| *b == delim) {
                Just(i) -> {
                    buf.extend_from_slice(&available[through i])
                    this.consume(i + 1)
                    return Ok(read + i + 1)
                },
                None -> {
                    buf.extend_from_slice(available)
                    let n = available.len()
                    this.consume(n)
                    read += n
                },
            }
        }
        return Ok(read)
    }

    /// Read line (until \n)
    public func read_line(this, buf: mut ref String) -> Outcome[U64, IoError] {
        var bytes = List.new()
        let n = this.read_until(b'\n', mut ref bytes)!
        let s = String.from_utf8(bytes).map_err(do(_) IoError.new(InvalidData, "invalid UTF-8"))!
        buf.push_str(&s)
        return Ok(n)
    }

    /// Iterator over lines
    public func lines(this) -> Lines[R] {
        return Lines { reader: this }
    }

    public func into_inner(this) -> R {
        return this.inner
    }
}

extend BufReader[R: Read] with Read {
    func read(this, buf: mut ref [U8]) -> Outcome[U64, IoError] {
        // If buf is larger than internal buffer, read directly
        if this.pos == this.cap and buf.len() >= this.buf.len() {
            return this.inner.read(buf)
        }

        let available = this.fill_buf()!
        let n = buf.len().min(available.len())
        buf[..n].copy_from_slice(&available[..n])
        this.consume(n)
        return Ok(n)
    }
}

public type Lines[R: Read] {
    reader: BufReader[R],
}

extend Lines[R: Read] with Iterator {
    type Item = Outcome[String, IoError]

    func next(this) -> Maybe[Outcome[String, IoError]] {
        var line = String.new()
        when this.reader.read_line(mut ref line) {
            Ok(0) -> Nothing,
            Ok(_) -> Just(Ok(line)),
            Err(e) -> Just(Err(e)),
        }
    }
}
```

### 4.2 BufWriter

```tml
public type BufWriter[W: Write] {
    inner: W,
    buf: List[U8],
}

extend BufWriter[W: Write] {
    public func new(inner: W) -> This {
        return This.with_capacity(DEFAULT_BUF_SIZE, inner)
    }

    public func with_capacity(cap: U64, inner: W) -> This {
        return This { inner: inner, buf: List.with_capacity(cap) }
    }

    /// Flush buffer to inner writer
    func flush_buf(this) -> Outcome[Unit, IoError] {
        if not this.buf.is_empty() {
            this.inner.write_all(this.buf.as_slice())!
            this.buf.clear()
        }
        return Ok(unit)
    }

    /// Get reference to inner writer
    public func get_ref(this) -> ref W {
        return &this.inner
    }

    /// Get mutable reference to inner writer
    public func get_mut(this) -> mut ref W {
        return mut ref this.inner
    }

    /// Into inner writer (flushes first)
    public func into_inner(this) -> Outcome[W, IoError] {
        this.flush()!
        return Ok(this.inner)
    }
}

extend BufWriter[W: Write] with Write {
    func write(this, buf: ref [U8]) -> Outcome[U64, IoError] {
        // If buf is larger than capacity, flush and write directly
        if buf.len() > this.buf.capacity() - this.buf.len() {
            this.flush_buf()!
            if buf.len() >= this.buf.capacity() {
                return this.inner.write(buf)
            }
        }

        this.buf.extend_from_slice(buf)
        return Ok(buf.len())
    }

    func flush(this) -> Outcome[Unit, IoError] {
        this.flush_buf()!
        this.inner.flush()
    }
}

extend BufWriter[W: Write] with Disposable {
    func drop(this) {
        this.flush().ok()
    }
}
```

## 5. Ring Buffer

```tml
public type RingBuffer[T] {
    data: List[T],
    head: U64,
    tail: U64,
    len: U64,
}

extend RingBuffer[T] {
    public func new(capacity: U64) -> This {
        var data = List.with_capacity(capacity)
        data.resize(capacity, T.default())
        return This { data: data, head: 0, tail: 0, len: 0 }
    }

    public func capacity(this) -> U64 {
        return this.data.len()
    }

    public func len(this) -> U64 {
        return this.len
    }

    public func is_empty(this) -> Bool {
        return this.len == 0
    }

    public func is_full(this) -> Bool {
        return this.len == this.capacity()
    }

    public func push_back(this, value: T) -> Bool {
        if this.is_full() {
            return false
        }

        this.data[this.tail] = value
        this.tail = (this.tail + 1) % this.capacity()
        this.len += 1
        return true
    }

    public func push_front(this, value: T) -> Bool {
        if this.is_full() {
            return false
        }

        this.head = if this.head == 0 {
            this.capacity() - 1
        } else {
            this.head - 1
        }
        this.data[this.head] = value
        this.len += 1
        return true
    }

    public func pop_front(this) -> Maybe[T] {
        if this.is_empty() {
            return Nothing
        }

        let value = this.data[this.head].duplicate()
        this.head = (this.head + 1) % this.capacity()
        this.len -= 1
        return Just(value)
    }

    public func pop_back(this) -> Maybe[T] {
        if this.is_empty() {
            return Nothing
        }

        this.tail = if this.tail == 0 {
            this.capacity() - 1
        } else {
            this.tail - 1
        }
        this.len -= 1
        return Just(this.data[this.tail].duplicate())
    }

    public func front(this) -> Maybe[ref T] {
        if this.is_empty() { Nothing } else { Just(&this.data[this.head]) }
    }

    public func back(this) -> Maybe[ref T] {
        if this.is_empty() {
            return Nothing
        }
        let idx = if this.tail == 0 { this.capacity() - 1 } else { this.tail - 1 }
        return Just(&this.data[idx])
    }

    public func clear(this) {
        this.head = 0
        this.tail = 0
        this.len = 0
    }
}
```

## 6. Examples

### 6.1 Binary Protocol

```tml
module protocol
import std.buffer.{Buffer, ByteReader, ByteWriter}

type Message {
    id: U32,
    payload: List[U8],
}

extend Message {
    func encode(this) -> List[U8] {
        var buf = Buffer.new()
        var writer = ByteWriter.new(mut ref buf)

        writer.write_u32_be(this.id).unwrap()
        writer.write_u32_be(this.payload.len() as U32).unwrap()
        buf.extend(&this.payload)

        return buf.into_inner()
    }

    func decode(data: ref [U8]) -> Outcome[This, IoError] {
        var buf = Buffer.from_bytes(data)
        var reader = ByteReader.new(mut ref buf)

        let id = reader.read_u32_be()!
        let len = reader.read_u32_be()! as U64
        var payload = List.with_capacity(len)
        payload.resize(len, 0)
        reader.read_exact(payload.as_mut_slice())!

        return Ok(This { id: id, payload: payload })
    }
}
```

### 6.2 Buffered File Processing

```tml
module processor
caps: [io.file]

import std.fs.File
import std.buffer.{BufReader, BufWriter}

func process_file(input: &Path, output: &Path) -> Outcome[Unit, Error] {
    let file_in = File.open(input)!
    let file_out = File.create(output)!

    var reader = BufReader.new(file_in)
    var writer = BufWriter.new(file_out)

    loop line in reader.lines() {
        let line = line!
        let processed = line.to_uppercase()
        writer.write_all(processed.as_bytes())!
        writer.write_all(b"\n")!
    }

    writer.flush()!
    return Ok(unit)
}
```

---

*Previous: [02-NET.md](./02-NET.md)*
*Next: [04-ENCODING.md](./04-ENCODING.md) — Text and Binary Encodings*
