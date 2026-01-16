# std::compress — Compression Algorithms

## 1. Overview

The \x60std::compress` package provides compression and decompression for common formats: gzip, zlib, deflate, bzip2, lz4, zstd, and tar/zip archives.

```tml
use std::compress
use std::compress.{gzip, zlib, zstd}
```

## 2. Capabilities

```tml
// No capabilities required - pure data transformation
```

## 3. Common Interface

### 3.1 Compression Trait

```tml
pub behaviorCompressor {
    /// Compress data
    func compress(this, input: ref [U8]) -> Outcome[List[U8], CompressError]

    /// Compress to writer
    func compress_to[W: Write](this, input: ref [U8], output: mut ref W) -> Outcome[U64, CompressError]
}

pub behaviorDecompressor {
    /// Decompress data
    func decompress(this, input: ref [U8]) -> Outcome[List[U8], DecompressError]

    /// Decompress to writer
    func decompress_to[W: Write](this, input: ref [U8], output: mut ref W) -> Outcome[U64, DecompressError]
}
```

### 3.2 Compression Level

```tml
pub type CompressionLevel =
    | None       // No compression (store only)
    | Fast       // Fastest compression
    | Default    // Balanced
    | Best       // Best compression ratio
    | Custom(I32) // Algorithm-specific level
```

## 4. Deflate / Zlib / Gzip

### 4.1 Deflate (Raw)

```tml
mod deflate

pub type Encoder {
    level: CompressionLevel,
}

extend Encoder {
    pub func new() -> This {
        return This { level: Default }
    }

    pub func level(this, level: CompressionLevel) -> This {
        this.level = level
        this
    }

    pub func compress(this, input: ref [U8]) -> Outcome[List[U8], CompressError]
}

pub type Decoder { }

extend Decoder {
    pub func new() -> This
    pub func decompress(this, input: ref [U8]) -> Outcome[List[U8], DecompressError]
}

/// One-shot compression
pub func compress(input: ref [U8]) -> Outcome[List[U8], CompressError]
pub func compress_level(input: ref [U8], level: CompressionLevel) -> Outcome[List[U8], CompressError]

/// One-shot decompression
pub func decompress(input: ref [U8]) -> Outcome[List[U8], DecompressError]
```

### 4.2 Zlib

```tml
mod zlib

pub type Encoder {
    level: CompressionLevel,
}

extend Encoder {
    pub func new() -> This
    pub func level(this, level: CompressionLevel) -> This
    pub func compress(this, input: ref [U8]) -> Outcome[List[U8], CompressError]
}

pub type Decoder { }

extend Decoder {
    pub func new() -> This
    pub func decompress(this, input: ref [U8]) -> Outcome[List[U8], DecompressError]
}

/// One-shot functions
pub func compress(input: ref [U8]) -> Outcome[List[U8], CompressError]
pub func decompress(input: ref [U8]) -> Outcome[List[U8], DecompressError]
```

### 4.3 Gzip

```tml
mod gzip

pub type Encoder {
    level: CompressionLevel,
    filename: Maybe[String],
    comment: Maybe[String],
    mtime: Maybe[U32],
}

extend Encoder {
    pub func new() -> This
    pub func level(this, level: CompressionLevel) -> This
    pub func filename(this, name: ref str) -> This
    pub func comment(this, comment: ref str) -> This
    pub func mtime(this, time: U32) -> This
    pub func compress(this, input: ref [U8]) -> Outcome[List[U8], CompressError]
}

pub type Decoder { }

extend Decoder {
    pub func new() -> This
    pub func decompress(this, input: ref [U8]) -> Outcome[List[U8], DecompressError]

    /// Get gzip header info (after decompression)
    pub func filename(this) -> Maybe[ref str]
    pub func comment(this) -> Maybe[ref str]
    pub func mtime(this) -> Maybe[U32]
}

/// One-shot functions
pub func compress(input: ref [U8]) -> Outcome[List[U8], CompressError]
pub func decompress(input: ref [U8]) -> Outcome[List[U8], DecompressError]
```

### 4.4 Streaming Gzip

```tml
mod gzip.stream

/// Gzip compression writer
pub type GzipWriter[W: Write] {
    inner: W,
    encoder: Encoder,
    buffer: List[U8],
}

extend GzipWriter[W: Write] {
    pub func new(inner: W) -> This
    pub func with_level(inner: W, level: CompressionLevel) -> This
    pub func finish(this) -> Outcome[W, IoError]
}

extend GzipWriter[W: Write] with Write {
    func write(this, buf: ref [U8]) -> Outcome[U64, IoError]
    func flush(this) -> Outcome[Unit, IoError]
}

/// Gzip decompression reader
pub type GzipReader[R: Read] {
    inner: R,
    decoder: Decoder,
    buffer: List[U8],
}

extend GzipReader[R: Read] {
    pub func new(inner: R) -> Outcome[This, DecompressError]
    pub func into_inner(this) -> R
}

extend GzipReader[R: Read] with Read {
    func read(this, buf: mut ref [U8]) -> Outcome[U64, IoError]
}
```

## 5. Bzip2

```tml
mod bzip2

pub type Encoder {
    level: CompressionLevel,  // 1-9
}

extend Encoder {
    pub func new() -> This
    pub func level(this, level: I32) -> This  // 1-9
    pub func compress(this, input: ref [U8]) -> Outcome[List[U8], CompressError]
}

pub type Decoder { }

extend Decoder {
    pub func new() -> This
    pub func decompress(this, input: ref [U8]) -> Outcome[List[U8], DecompressError]
}

/// One-shot functions
pub func compress(input: ref [U8]) -> Outcome[List[U8], CompressError]
pub func decompress(input: ref [U8]) -> Outcome[List[U8], DecompressError]

/// Streaming
mod stream
pub type BzipWriter[W: Write] { ... }
pub type BzipReader[R: Read] { ... }
```

## 6. LZ4

```tml
mod lz4

pub type Encoder {
    level: CompressionLevel,
    block_size: BlockSize,
}

pub type BlockSize = B64KB | B256KB | B1MB | B4MB

extend Encoder {
    pub func new() -> This
    pub func level(this, level: CompressionLevel) -> This
    pub func block_size(this, size: BlockSize) -> This
    pub func compress(this, input: ref [U8]) -> Outcome[List[U8], CompressError]
}

pub type Decoder { }

extend Decoder {
    pub func new() -> This
    pub func decompress(this, input: ref [U8]) -> Outcome[List[U8], DecompressError]
}

/// One-shot functions
pub func compress(input: ref [U8]) -> Outcome[List[U8], CompressError]
pub func decompress(input: ref [U8]) -> Outcome[List[U8], DecompressError]

/// Frame format (with checksums, streaming)
mod frame
pub type Lz4Writer[W: Write] { ... }
pub type Lz4Reader[R: Read] { ... }

/// Block format (no framing, raw blocks)
mod block
pub func compress_block(input: ref [U8]) -> List[U8]
pub func decompress_block(input: ref [U8], max_output: U64) -> Outcome[List[U8], DecompressError]
```

## 7. Zstandard (Zstd)

```tml
mod zstd

pub type Encoder {
    level: I32,  // 1-22 (default 3)
    dict: Maybe[Dictionary],
}

pub type Dictionary {
    data: List[U8],
}

extend Encoder {
    pub func new() -> This
    pub func level(this, level: I32) -> This
    pub func dict(this, dict: Dictionary) -> This
    pub func compress(this, input: ref [U8]) -> Outcome[List[U8], CompressError]
}

pub type Decoder {
    dict: Maybe[Dictionary],
}

extend Decoder {
    pub func new() -> This
    pub func dict(this, dict: Dictionary) -> This
    pub func decompress(this, input: ref [U8]) -> Outcome[List[U8], DecompressError]
}

/// One-shot functions
pub func compress(input: ref [U8]) -> Outcome[List[U8], CompressError]
pub func compress_level(input: ref [U8], level: I32) -> Outcome[List[U8], CompressError]
pub func decompress(input: ref [U8]) -> Outcome[List[U8], DecompressError]

/// Dictionary training
pub func train_dict(samples: ref [ref [U8]], dict_size: U64) -> Outcome[Dictionary, Error]

/// Streaming
mod stream
pub type ZstdWriter[W: Write] { ... }
pub type ZstdReader[R: Read] { ... }
```

## 8. Brotli

```tml
mod brotli

pub type Encoder {
    quality: U32,    // 0-11 (default 11)
    window: U32,     // Window size (10-24)
}

extend Encoder {
    pub func new() -> This
    pub func quality(this, q: U32) -> This
    pub func window_size(this, size: U32) -> This
    pub func compress(this, input: ref [U8]) -> Outcome[List[U8], CompressError]
}

pub type Decoder { }

extend Decoder {
    pub func new() -> This
    pub func decompress(this, input: ref [U8]) -> Outcome[List[U8], DecompressError]
}

/// One-shot functions
pub func compress(input: ref [U8]) -> Outcome[List[U8], CompressError]
pub func decompress(input: ref [U8]) -> Outcome[List[U8], DecompressError]

/// Streaming
mod stream
pub type BrotliWriter[W: Write] { ... }
pub type BrotliReader[R: Read] { ... }
```

## 9. Tar Archives

```tml
mod tar

pub type Archive[R: Read] {
    reader: R,
}

extend Archive[R: Read] {
    pub func new(reader: R) -> This

    /// Iterate over entries
    pub func entries(this) -> Entries[R]

    /// Extract all to directory
    pub func extract_all(this, dst: ref Path) -> Outcome[Unit, TarError]
    effects: [io::file.write]
}

pub type Entries[R: Read] { ... }

extend Entries[R: Read] with Iterator {
    type Item = Outcome[Entry[R], TarError]
}

pub type Entry[R: Read] {
    header: Header,
    reader: R,
}

extend Entry[R: Read] {
    pub func path(this) -> Outcome[PathBuf, TarError]
    pub func size(this) -> U64
    pub func mode(this) -> U32
    pub func mtime(this) -> U64
    pub func is_file(this) -> Bool
    pub func is_dir(this) -> Bool
    pub func is_symlink(this) -> Bool
    pub func link_name(this) -> Outcome[Maybe[PathBuf], TarError]

    /// Read entry contents
    pub func read_all(this) -> Outcome[List[U8], IoError]

    /// Extract to path
    pub func extract(this, dst: ref Path) -> Outcome[Unit, TarError]
    effects: [io::file.write]
}

extend Entry[R: Read] with Read { ... }

/// Builder for creating tar archives
pub type Builder[W: Write] {
    writer: W,
}

extend Builder[W: Write] {
    pub func new(writer: W) -> This

    /// Add file from path
    pub func append_path(this, path: ref Path) -> Outcome[Unit, TarError]
    effects: [io::file.read]

    /// Add file with custom header
    pub func append_file(this, path: ref str, file: mut ref File) -> Outcome[Unit, TarError]

    /// Add directory
    pub func append_dir(this, path: ref str, src: ref Path) -> Outcome[Unit, TarError]
    effects: [io::file.read]

    /// Add data with custom header
    pub func append_data(this, header: mut ref Header, path: ref str, data: ref [U8]) -> Outcome[Unit, TarError]

    /// Finish archive
    pub func finish(this) -> Outcome[W, TarError]
}

pub type Header { ... }

extend Header {
    pub func new() -> This
    pub func set_path(this, path: ref str) -> Outcome[Unit, TarError]
    pub func set_size(this, size: U64)
    pub func set_mode(this, mode: U32)
    pub func set_mtime(this, mtime: U64)
    pub func set_entry_type(this, kind: EntryType)
}

pub type EntryType = Regular | Directory | Symlink | Hardlink | ...
```

## 10. Zip Archives

```tml
mod zip

pub type Archive[R: Read + Seek] {
    reader: R,
    entries: List[ZipEntry],
}

extend Archive[R: Read + Seek] {
    pub func new(reader: R) -> Outcome[This, ZipError]

    /// Number of entries
    pub func len(this) -> U64

    /// Get entry by index
    pub func by_index(this, index: U64) -> Outcome[ZipFile[R], ZipError]

    /// Get entry by name
    pub func by_name(this, name: ref str) -> Outcome[ZipFile[R], ZipError]

    /// Iterate over entries
    pub func entries(this) -> ZipEntries[R]

    /// Extract all to directory
    pub func extract_all(this, dst: ref Path) -> Outcome[Unit, ZipError]
    effects: [io::file.write]
}

pub type ZipFile[R] {
    name: String,
    size: U64,
    compressed_size: U64,
    compression: CompressionMethod,
    is_dir: Bool,
    reader: R,
}

extend ZipFile[R: Read] {
    pub func name(this) -> ref str
    pub func size(this) -> U64
    pub func is_dir(this) -> Bool
    pub func compression(this) -> CompressionMethod

    /// Read entire file
    pub func read_all(this) -> Outcome[List[U8], ZipError]

    /// Extract to path
    pub func extract(this, dst: ref Path) -> Outcome[Unit, ZipError]
    effects: [io::file.write]
}

extend ZipFile[R: Read] with Read { ... }

pub type CompressionMethod =
    | Stored
    | Deflated
    | Bzip2
    | Lzma
    | Zstd
    | Unknown(U16)

/// Builder for creating zip archives
pub type ZipWriter[W: Write + Seek] {
    writer: W,
}

extend ZipWriter[W: Write + Seek] {
    pub func new(writer: W) -> This

    /// Start new file entry
    pub func start_file(this, name: ref str, options: FileOptions) -> Outcome[Unit, ZipError]

    /// Write data to current file
    pub func write_all(this, data: ref [U8]) -> Outcome[Unit, ZipError]

    /// Add file from path
    pub func add_file(this, path: ref Path, name: ref str) -> Outcome[Unit, ZipError]
    effects: [io::file.read]

    /// Add directory recursively
    pub func add_directory(this, path: ref Path, prefix: ref str) -> Outcome[Unit, ZipError]
    effects: [io::file.read]

    /// Finish archive
    pub func finish(this) -> Outcome[W, ZipError]
}

pub type FileOptions {
    compression: CompressionMethod,
    compression_level: CompressionLevel,
    unix_permissions: Maybe[U32>,
}

extend FileOptions {
    pub func default() -> This
    pub func compression(this, method: CompressionMethod) -> This
    pub func compression_level(this, level: CompressionLevel) -> This
    pub func unix_permissions(this, mode: U32) -> This
}
```

## 11. Error Types

```tml
pub type CompressError {
    kind: CompressErrorKind,
    message: String,
}

pub type CompressErrorKind =
    | InvalidInput
    | BufferTooSmall
    | CompressionFailed
    | UnsupportedLevel

pub type DecompressError {
    kind: DecompressErrorKind,
    message: String,
}

pub type DecompressErrorKind =
    | InvalidData
    | CorruptedData
    | ChecksumMismatch
    | UnexpectedEof
    | BufferTooSmall
```

## 12. Examples

### 12.1 Gzip File

```tml
mod gzip_example
caps: [io::file]

use std::fs.File
use std::compress.gzip

func compress_file(src: ref Path, dst: ref Path) -> Outcome[Unit, Error] {
    let data = fs.read(src)!
    let compressed = gzip.compress(ref data)!
    fs.write(dst, ref compressed)!
    return Ok(unit)
}

func decompress_file(src: ref Path, dst: ref Path) -> Outcome[Unit, Error] {
    let compressed = fs.read(src)!
    let data = gzip.decompress(ref compressed)!
    fs.write(dst, ref data)!
    return Ok(unit)
}
```

### 12.2 Streaming Compression

```tml
mod streaming
caps: [io::file]

use std::fs.File
use std::compress.gzip.stream.GzipWriter

func compress_large_file(src: ref Path, dst: ref Path) -> Outcome[Unit, Error] {
    let input = File.open(src)!
    let output = File.create(dst)!

    var gz = GzipWriter.new(output)

    var buf: [U8; 8192] = [0; 8192]
    loop {
        let n = input.read(mut ref buf)!
        if n == 0 { break }
        gz.write_all(&buf[0 to n])!
    }

    gz.finish()!
    return Ok(unit)
}
```

### 12.3 Creating Zip Archive

```tml
mod zip_example
caps: [io::file]

use std::fs.File
use std::compress.zip.{ZipWriter, FileOptions, CompressionMethod}

func create_archive(files: ref [(ref Path, ref str)], output: ref Path) -> Outcome[Unit, Error] {
    let file = File.create(output)!
    var zip = ZipWriter.new(file)

    let options = FileOptions.default()
        .compression(CompressionMethod.Deflated)
        .compression_level(CompressionLevel.Best)

    loop (path, name) in files {
        let data = fs.read(path)!
        zip.start_file(name, options)!
        zip.write_all(ref data)!
    }

    zip.finish()!
    return Ok(unit)
}
```

### 12.4 Extracting Tar.gz

```tml
mod tarball
caps: [io::file]

use std::fs.File
use std::compress.gzip.stream.GzipReader
use std::compress.tar.Archive

func extract_tarball(src: ref Path, dst: ref Path) -> Outcome[Unit, Error] {
    let file = File.open(src)!
    let gz = GzipReader.new(file)!
    var tar = Archive.new(gz)

    tar.extract_all(dst)!

    return Ok(unit)
}
```

---

*Previous: [07-HTTP.md](./07-HTTP.md)*
*Next: [09-JSON.md](./09-JSON.md) — JSON Parsing and Serialization*
