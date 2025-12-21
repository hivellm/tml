# std.compress — Compression Algorithms

## 1. Overview

The `std.compress` package provides compression and decompression for common formats: gzip, zlib, deflate, bzip2, lz4, zstd, and tar/zip archives.

```tml
import std.compress
import std.compress.{gzip, zlib, zstd}
```

## 2. Capabilities

```tml
// No capabilities required - pure data transformation
```

## 3. Common Interface

### 3.1 Compression Trait

```tml
public trait Compressor {
    /// Compress data
    func compress(this, input: &[U8]) -> Result[List[U8], CompressError]

    /// Compress to writer
    func compress_to[W: Write](this, input: &[U8], output: &mut W) -> Result[U64, CompressError]
}

public trait Decompressor {
    /// Decompress data
    func decompress(this, input: &[U8]) -> Result[List[U8], DecompressError]

    /// Decompress to writer
    func decompress_to[W: Write](this, input: &[U8], output: &mut W) -> Result[U64, DecompressError]
}
```

### 3.2 Compression Level

```tml
public type CompressionLevel =
    | None       // No compression (store only)
    | Fast       // Fastest compression
    | Default    // Balanced
    | Best       // Best compression ratio
    | Custom(I32) // Algorithm-specific level
```

## 4. Deflate / Zlib / Gzip

### 4.1 Deflate (Raw)

```tml
module deflate

public type Encoder {
    level: CompressionLevel,
}

extend Encoder {
    public func new() -> This {
        return This { level: Default }
    }

    public func level(this, level: CompressionLevel) -> This {
        this.level = level
        this
    }

    public func compress(this, input: &[U8]) -> Result[List[U8], CompressError]
}

public type Decoder { }

extend Decoder {
    public func new() -> This
    public func decompress(this, input: &[U8]) -> Result[List[U8], DecompressError]
}

/// One-shot compression
public func compress(input: &[U8]) -> Result[List[U8], CompressError]
public func compress_level(input: &[U8], level: CompressionLevel) -> Result[List[U8], CompressError]

/// One-shot decompression
public func decompress(input: &[U8]) -> Result[List[U8], DecompressError]
```

### 4.2 Zlib

```tml
module zlib

public type Encoder {
    level: CompressionLevel,
}

extend Encoder {
    public func new() -> This
    public func level(this, level: CompressionLevel) -> This
    public func compress(this, input: &[U8]) -> Result[List[U8], CompressError]
}

public type Decoder { }

extend Decoder {
    public func new() -> This
    public func decompress(this, input: &[U8]) -> Result[List[U8], DecompressError]
}

/// One-shot functions
public func compress(input: &[U8]) -> Result[List[U8], CompressError]
public func decompress(input: &[U8]) -> Result[List[U8], DecompressError]
```

### 4.3 Gzip

```tml
module gzip

public type Encoder {
    level: CompressionLevel,
    filename: Option[String],
    comment: Option[String],
    mtime: Option[U32],
}

extend Encoder {
    public func new() -> This
    public func level(this, level: CompressionLevel) -> This
    public func filename(this, name: &str) -> This
    public func comment(this, comment: &str) -> This
    public func mtime(this, time: U32) -> This
    public func compress(this, input: &[U8]) -> Result[List[U8], CompressError]
}

public type Decoder { }

extend Decoder {
    public func new() -> This
    public func decompress(this, input: &[U8]) -> Result[List[U8], DecompressError]

    /// Get gzip header info (after decompression)
    public func filename(this) -> Option[&str]
    public func comment(this) -> Option[&str]
    public func mtime(this) -> Option[U32]
}

/// One-shot functions
public func compress(input: &[U8]) -> Result[List[U8], CompressError]
public func decompress(input: &[U8]) -> Result[List[U8], DecompressError]
```

### 4.4 Streaming Gzip

```tml
module gzip.stream

/// Gzip compression writer
public type GzipWriter[W: Write] {
    inner: W,
    encoder: Encoder,
    buffer: List[U8],
}

extend GzipWriter[W: Write] {
    public func new(inner: W) -> This
    public func with_level(inner: W, level: CompressionLevel) -> This
    public func finish(this) -> Result[W, IoError]
}

extend GzipWriter[W: Write] with Write {
    func write(this, buf: &[U8]) -> Result[U64, IoError]
    func flush(this) -> Result[Unit, IoError]
}

/// Gzip decompression reader
public type GzipReader[R: Read] {
    inner: R,
    decoder: Decoder,
    buffer: List[U8],
}

extend GzipReader[R: Read] {
    public func new(inner: R) -> Result[This, DecompressError]
    public func into_inner(this) -> R
}

extend GzipReader[R: Read] with Read {
    func read(this, buf: &mut [U8]) -> Result[U64, IoError]
}
```

## 5. Bzip2

```tml
module bzip2

public type Encoder {
    level: CompressionLevel,  // 1-9
}

extend Encoder {
    public func new() -> This
    public func level(this, level: I32) -> This  // 1-9
    public func compress(this, input: &[U8]) -> Result[List[U8], CompressError]
}

public type Decoder { }

extend Decoder {
    public func new() -> This
    public func decompress(this, input: &[U8]) -> Result[List[U8], DecompressError]
}

/// One-shot functions
public func compress(input: &[U8]) -> Result[List[U8], CompressError]
public func decompress(input: &[U8]) -> Result[List[U8], DecompressError]

/// Streaming
module stream
public type BzipWriter[W: Write] { ... }
public type BzipReader[R: Read] { ... }
```

## 6. LZ4

```tml
module lz4

public type Encoder {
    level: CompressionLevel,
    block_size: BlockSize,
}

public type BlockSize = B64KB | B256KB | B1MB | B4MB

extend Encoder {
    public func new() -> This
    public func level(this, level: CompressionLevel) -> This
    public func block_size(this, size: BlockSize) -> This
    public func compress(this, input: &[U8]) -> Result[List[U8], CompressError]
}

public type Decoder { }

extend Decoder {
    public func new() -> This
    public func decompress(this, input: &[U8]) -> Result[List[U8], DecompressError]
}

/// One-shot functions
public func compress(input: &[U8]) -> Result[List[U8], CompressError]
public func decompress(input: &[U8]) -> Result[List[U8], DecompressError]

/// Frame format (with checksums, streaming)
module frame
public type Lz4Writer[W: Write] { ... }
public type Lz4Reader[R: Read] { ... }

/// Block format (no framing, raw blocks)
module block
public func compress_block(input: &[U8]) -> List[U8]
public func decompress_block(input: &[U8], max_output: U64) -> Result[List[U8], DecompressError]
```

## 7. Zstandard (Zstd)

```tml
module zstd

public type Encoder {
    level: I32,  // 1-22 (default 3)
    dict: Option[Dictionary],
}

public type Dictionary {
    data: List[U8],
}

extend Encoder {
    public func new() -> This
    public func level(this, level: I32) -> This
    public func dict(this, dict: Dictionary) -> This
    public func compress(this, input: &[U8]) -> Result[List[U8], CompressError]
}

public type Decoder {
    dict: Option[Dictionary],
}

extend Decoder {
    public func new() -> This
    public func dict(this, dict: Dictionary) -> This
    public func decompress(this, input: &[U8]) -> Result[List[U8], DecompressError]
}

/// One-shot functions
public func compress(input: &[U8]) -> Result[List[U8], CompressError]
public func compress_level(input: &[U8], level: I32) -> Result[List[U8], CompressError]
public func decompress(input: &[U8]) -> Result[List[U8], DecompressError]

/// Dictionary training
public func train_dict(samples: &[&[U8]], dict_size: U64) -> Result[Dictionary, Error]

/// Streaming
module stream
public type ZstdWriter[W: Write] { ... }
public type ZstdReader[R: Read] { ... }
```

## 8. Brotli

```tml
module brotli

public type Encoder {
    quality: U32,    // 0-11 (default 11)
    window: U32,     // Window size (10-24)
}

extend Encoder {
    public func new() -> This
    public func quality(this, q: U32) -> This
    public func window_size(this, size: U32) -> This
    public func compress(this, input: &[U8]) -> Result[List[U8], CompressError]
}

public type Decoder { }

extend Decoder {
    public func new() -> This
    public func decompress(this, input: &[U8]) -> Result[List[U8], DecompressError]
}

/// One-shot functions
public func compress(input: &[U8]) -> Result[List[U8], CompressError]
public func decompress(input: &[U8]) -> Result[List[U8], DecompressError]

/// Streaming
module stream
public type BrotliWriter[W: Write] { ... }
public type BrotliReader[R: Read] { ... }
```

## 9. Tar Archives

```tml
module tar

public type Archive[R: Read] {
    reader: R,
}

extend Archive[R: Read] {
    public func new(reader: R) -> This

    /// Iterate over entries
    public func entries(this) -> Entries[R]

    /// Extract all to directory
    public func extract_all(this, dst: &Path) -> Result[Unit, TarError]
    effects: [io.file.write]
}

public type Entries[R: Read] { ... }

extend Entries[R: Read] with Iterator {
    type Item = Result[Entry[R], TarError]
}

public type Entry[R: Read] {
    header: Header,
    reader: R,
}

extend Entry[R: Read] {
    public func path(this) -> Result[PathBuf, TarError]
    public func size(this) -> U64
    public func mode(this) -> U32
    public func mtime(this) -> U64
    public func is_file(this) -> Bool
    public func is_dir(this) -> Bool
    public func is_symlink(this) -> Bool
    public func link_name(this) -> Result[Option[PathBuf], TarError]

    /// Read entry contents
    public func read_all(this) -> Result[List[U8], IoError]

    /// Extract to path
    public func extract(this, dst: &Path) -> Result[Unit, TarError]
    effects: [io.file.write]
}

extend Entry[R: Read] with Read { ... }

/// Builder for creating tar archives
public type Builder[W: Write] {
    writer: W,
}

extend Builder[W: Write] {
    public func new(writer: W) -> This

    /// Add file from path
    public func append_path(this, path: &Path) -> Result[Unit, TarError]
    effects: [io.file.read]

    /// Add file with custom header
    public func append_file(this, path: &str, file: &mut File) -> Result[Unit, TarError]

    /// Add directory
    public func append_dir(this, path: &str, src: &Path) -> Result[Unit, TarError]
    effects: [io.file.read]

    /// Add data with custom header
    public func append_data(this, header: &mut Header, path: &str, data: &[U8]) -> Result[Unit, TarError]

    /// Finish archive
    public func finish(this) -> Result[W, TarError]
}

public type Header { ... }

extend Header {
    public func new() -> This
    public func set_path(this, path: &str) -> Result[Unit, TarError]
    public func set_size(this, size: U64)
    public func set_mode(this, mode: U32)
    public func set_mtime(this, mtime: U64)
    public func set_entry_type(this, kind: EntryType)
}

public type EntryType = Regular | Directory | Symlink | Hardlink | ...
```

## 10. Zip Archives

```tml
module zip

public type Archive[R: Read + Seek] {
    reader: R,
    entries: List[ZipEntry],
}

extend Archive[R: Read + Seek] {
    public func new(reader: R) -> Result[This, ZipError]

    /// Number of entries
    public func len(this) -> U64

    /// Get entry by index
    public func by_index(this, index: U64) -> Result[ZipFile[R], ZipError]

    /// Get entry by name
    public func by_name(this, name: &str) -> Result[ZipFile[R], ZipError]

    /// Iterate over entries
    public func entries(this) -> ZipEntries[R]

    /// Extract all to directory
    public func extract_all(this, dst: &Path) -> Result[Unit, ZipError]
    effects: [io.file.write]
}

public type ZipFile[R] {
    name: String,
    size: U64,
    compressed_size: U64,
    compression: CompressionMethod,
    is_dir: Bool,
    reader: R,
}

extend ZipFile[R: Read] {
    public func name(this) -> &str
    public func size(this) -> U64
    public func is_dir(this) -> Bool
    public func compression(this) -> CompressionMethod

    /// Read entire file
    public func read_all(this) -> Result[List[U8], ZipError]

    /// Extract to path
    public func extract(this, dst: &Path) -> Result[Unit, ZipError]
    effects: [io.file.write]
}

extend ZipFile[R: Read] with Read { ... }

public type CompressionMethod =
    | Stored
    | Deflated
    | Bzip2
    | Lzma
    | Zstd
    | Unknown(U16)

/// Builder for creating zip archives
public type ZipWriter[W: Write + Seek] {
    writer: W,
}

extend ZipWriter[W: Write + Seek] {
    public func new(writer: W) -> This

    /// Start new file entry
    public func start_file(this, name: &str, options: FileOptions) -> Result[Unit, ZipError]

    /// Write data to current file
    public func write_all(this, data: &[U8]) -> Result[Unit, ZipError]

    /// Add file from path
    public func add_file(this, path: &Path, name: &str) -> Result[Unit, ZipError]
    effects: [io.file.read]

    /// Add directory recursively
    public func add_directory(this, path: &Path, prefix: &str) -> Result[Unit, ZipError]
    effects: [io.file.read]

    /// Finish archive
    public func finish(this) -> Result[W, ZipError]
}

public type FileOptions {
    compression: CompressionMethod,
    compression_level: CompressionLevel,
    unix_permissions: Option[U32>,
}

extend FileOptions {
    public func default() -> This
    public func compression(this, method: CompressionMethod) -> This
    public func compression_level(this, level: CompressionLevel) -> This
    public func unix_permissions(this, mode: U32) -> This
}
```

## 11. Error Types

```tml
public type CompressError {
    kind: CompressErrorKind,
    message: String,
}

public type CompressErrorKind =
    | InvalidInput
    | BufferTooSmall
    | CompressionFailed
    | UnsupportedLevel

public type DecompressError {
    kind: DecompressErrorKind,
    message: String,
}

public type DecompressErrorKind =
    | InvalidData
    | CorruptedData
    | ChecksumMismatch
    | UnexpectedEof
    | BufferTooSmall
```

## 12. Examples

### 12.1 Gzip File

```tml
module gzip_example
caps: [io.file]

import std.fs.File
import std.compress.gzip

func compress_file(src: &Path, dst: &Path) -> Result[Unit, Error] {
    let data = fs.read(src)?
    let compressed = gzip.compress(&data)?
    fs.write(dst, &compressed)?
    return Ok(unit)
}

func decompress_file(src: &Path, dst: &Path) -> Result[Unit, Error] {
    let compressed = fs.read(src)?
    let data = gzip.decompress(&compressed)?
    fs.write(dst, &data)?
    return Ok(unit)
}
```

### 12.2 Streaming Compression

```tml
module streaming
caps: [io.file]

import std.fs.File
import std.compress.gzip.stream.GzipWriter

func compress_large_file(src: &Path, dst: &Path) -> Result[Unit, Error] {
    let input = File.open(src)?
    let output = File.create(dst)?

    var gz = GzipWriter.new(output)

    var buf: [U8; 8192] = [0; 8192]
    loop {
        let n = input.read(&mut buf)?
        if n == 0 { break }
        gz.write_all(&buf[0..n])?
    }

    gz.finish()?
    return Ok(unit)
}
```

### 12.3 Creating Zip Archive

```tml
module zip_example
caps: [io.file]

import std.fs.File
import std.compress.zip.{ZipWriter, FileOptions, CompressionMethod}

func create_archive(files: &[(&Path, &str)], output: &Path) -> Result[Unit, Error] {
    let file = File.create(output)?
    var zip = ZipWriter.new(file)

    let options = FileOptions.default()
        .compression(CompressionMethod.Deflated)
        .compression_level(CompressionLevel.Best)

    loop (path, name) in files {
        let data = fs.read(path)?
        zip.start_file(name, options)?
        zip.write_all(&data)?
    }

    zip.finish()?
    return Ok(unit)
}
```

### 12.4 Extracting Tar.gz

```tml
module tarball
caps: [io.file]

import std.fs.File
import std.compress.gzip.stream.GzipReader
import std.compress.tar.Archive

func extract_tarball(src: &Path, dst: &Path) -> Result[Unit, Error] {
    let file = File.open(src)?
    let gz = GzipReader.new(file)?
    var tar = Archive.new(gz)

    tar.extract_all(dst)?

    return Ok(unit)
}
```

---

*Previous: [07-HTTP.md](./07-HTTP.md)*
*Next: [09-JSON.md](./09-JSON.md) — JSON Parsing and Serialization*
