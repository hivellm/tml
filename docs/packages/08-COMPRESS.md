# std::zlib — Compression, Checksums & Streaming

## 1. Overview

The `std::zlib` module provides compression, decompression, and checksum computation using industry-standard algorithms: **zlib/deflate**, **gzip**, **Brotli**, and **Zstandard (Zstd)**. It follows a Node.js-inspired API with both one-shot and streaming interfaces.

```tml
use std::zlib::deflate::{deflate, inflate}
use std::zlib::gzip::{gzip, gunzip}
use std::zlib::brotli::{brotli_compress, brotli_decompress}
use std::zlib::zstd::{zstd_compress, zstd_decompress}
use std::zlib::crc32::{crc32, adler32}
```

**Module path:** `std::zlib`

**Runtime:** Backed by native C libraries (zlib, brotli, zstd) via FFI. DLLs are bundled automatically by `tml run` and `tml build`.

## 2. Submodules

| Module | Description |
|--------|-------------|
| `std::zlib::deflate` | Deflate/inflate compression (RFC 1951, RFC 1950) |
| `std::zlib::gzip` | Gzip compression (RFC 1952) |
| `std::zlib::brotli` | Brotli compression (Google) |
| `std::zlib::zstd` | Zstandard compression (Facebook) |
| `std::zlib::crc32` | CRC32 and Adler32 checksums |
| `std::zlib::stream` | Streaming compression/decompression classes |
| `std::zlib::options` | Configuration types (ZlibOptions, BrotliOptions, ZstdOptions) |
| `std::zlib::constants` | Numeric constants (levels, strategies, flush modes) |
| `std::zlib::error` | Error types (ZlibError, ZlibErrorKind) |

## 3. CRC32 & Adler32 Checksums

### 3.1 One-Shot Functions

```tml
use std::zlib::crc32::{crc32, crc32_buffer, adler32, adler32_buffer}

// CRC32 of a string
let checksum: I64 = crc32("Hello, TML!")

// CRC32 of a buffer
let buf = Buffer::from("binary data")
let checksum: I64 = crc32_buffer(ref buf)

// Adler32 of a string
let adler: I64 = adler32("Hello, TML!")
```

### 3.2 Incremental Updates

```tml
use std::zlib::crc32::{crc32_update, crc32_combine}

// Start with initial CRC (0)
let mut crc: U32 = 0
crc = crc32_update(crc, "Hello, ")
crc = crc32_update(crc, "world!")

// Combine two independent checksums
let crc1: U32 = crc32_update(0, "Hello")
let crc2: U32 = crc32_update(0, "World")
let combined: U32 = crc32_combine(crc1, crc2, 5)  // len2 = length of second input
```

### 3.3 Function Reference

| Function | Signature | Description |
|----------|-----------|-------------|
| `crc32` | `(data: Str) -> I64` | CRC32 of string |
| `crc32_buffer` | `(data: ref Buffer) -> I64` | CRC32 of buffer |
| `crc32_update` | `(crc: U32, data: Str) -> U32` | Incremental CRC32 |
| `crc32_update_buffer` | `(crc: U32, data: ref Buffer) -> U32` | Incremental CRC32 (buffer) |
| `crc32_combine` | `(crc1: U32, crc2: U32, len2: I64) -> U32` | Combine two CRCs |
| `adler32` | `(data: Str) -> I64` | Adler32 of string |
| `adler32_buffer` | `(data: ref Buffer) -> I64` | Adler32 of buffer |
| `adler32_update` | `(adler: U32, data: Str) -> U32` | Incremental Adler32 |
| `adler32_update_buffer` | `(adler: U32, data: ref Buffer) -> U32` | Incremental Adler32 (buffer) |
| `adler32_combine` | `(adler1: U32, adler2: U32, len2: I64) -> U32` | Combine two Adler32s |

## 4. Deflate / Inflate (Zlib Format)

### 4.1 One-Shot Compression

```tml
use std::zlib::deflate::{deflate, inflate, deflate_raw, inflate_raw}

// Compress a string (zlib format with header)
let compressed: Outcome[Buffer, ZlibError] = deflate("Hello, world!")

// Decompress back to string
let original: Outcome[Str, ZlibError] = inflate(ref compressed.unwrap())

// Raw deflate (no zlib header) — RFC 1951
let raw_compressed: Outcome[Buffer, ZlibError] = deflate_raw("Hello, world!")
let raw_original: Outcome[Str, ZlibError] = inflate_raw(ref raw_compressed.unwrap())
```

### 4.2 Buffer I/O

```tml
use std::zlib::deflate::{deflate_buffer, inflate_to_buffer}

// Compress buffer -> buffer
let input = Buffer::from("binary data")
let compressed: Outcome[Buffer, ZlibError] = deflate_buffer(ref input)

// Decompress buffer -> buffer
let decompressed: Outcome[Buffer, ZlibError] = inflate_to_buffer(ref compressed.unwrap())
```

### 4.3 With Options

```tml
use std::zlib::deflate::{deflate_with_options, inflate_with_options}
use std::zlib::options::ZlibOptions

let opts = ZlibOptions::default().with_level(9)  // Best compression
let compressed = deflate_with_options("Hello, world!", opts)
```

### 4.4 Auto-Detect Decompression

```tml
use std::zlib::deflate::{unzip, unzip_to_buffer}

// Automatically detects zlib or gzip format
let original: Outcome[Str, ZlibError] = unzip(ref compressed_data)
```

### 4.5 Function Reference

| Function | Signature | Description |
|----------|-----------|-------------|
| `deflate` | `(data: Str) -> Outcome[Buffer, ZlibError]` | Compress string (zlib format) |
| `deflate_buffer` | `(data: ref Buffer) -> Outcome[Buffer, ZlibError]` | Compress buffer |
| `deflate_with_options` | `(data: Str, options: ZlibOptions) -> Outcome[Buffer, ZlibError]` | Compress with options |
| `deflate_raw` | `(data: Str) -> Outcome[Buffer, ZlibError]` | Raw deflate (no header) |
| `deflate_raw_buffer` | `(data: ref Buffer) -> Outcome[Buffer, ZlibError]` | Raw deflate buffer |
| `inflate` | `(data: ref Buffer) -> Outcome[Str, ZlibError]` | Decompress to string |
| `inflate_to_buffer` | `(data: ref Buffer) -> Outcome[Buffer, ZlibError]` | Decompress to buffer |
| `inflate_raw` | `(data: ref Buffer) -> Outcome[Str, ZlibError]` | Raw inflate to string |
| `inflate_raw_to_buffer` | `(data: ref Buffer) -> Outcome[Buffer, ZlibError]` | Raw inflate to buffer |
| `unzip` | `(data: ref Buffer) -> Outcome[Str, ZlibError]` | Auto-detect and decompress |
| `unzip_to_buffer` | `(data: ref Buffer) -> Outcome[Buffer, ZlibError]` | Auto-detect to buffer |

All functions also have `_sync` suffix aliases (e.g., `deflate_sync`) and `_with_options` variants.

## 5. Gzip

### 5.1 One-Shot

```tml
use std::zlib::gzip::{gzip, gunzip, gzip_buffer, gunzip_to_buffer}

// Compress
let compressed: Outcome[Buffer, ZlibError] = gzip("Hello, world!")

// Decompress
let original: Outcome[Str, ZlibError] = gunzip(ref compressed.unwrap())

// Buffer variants
let comp_buf = gzip_buffer(ref my_buffer)
let decomp_buf = gunzip_to_buffer(ref comp_buf.unwrap())
```

### 5.2 Gzip Header Parsing

```tml
use std::zlib::gzip::read_gzip_header

let header = read_gzip_header(ref gzip_data)
if header.is_ok() {
    let h = header.unwrap()
    println("OS: " + h.os().to_string())
    println("Filename: " + h.name())
    println("Comment: " + h.comment())
}
```

### 5.3 Function Reference

| Function | Signature | Description |
|----------|-----------|-------------|
| `gzip` | `(data: Str) -> Outcome[Buffer, ZlibError]` | Gzip compress string |
| `gzip_buffer` | `(data: ref Buffer) -> Outcome[Buffer, ZlibError]` | Gzip compress buffer |
| `gzip_with_options` | `(data: Str, options: ZlibOptions) -> Outcome[Buffer, ZlibError]` | Gzip with options |
| `gunzip` | `(data: ref Buffer) -> Outcome[Str, ZlibError]` | Gunzip to string |
| `gunzip_to_buffer` | `(data: ref Buffer) -> Outcome[Buffer, ZlibError]` | Gunzip to buffer |
| `read_gzip_header` | `(data: ref Buffer) -> Outcome[GzipHeader, ZlibError]` | Parse gzip header |

## 6. Brotli

Brotli is Google's modern compression algorithm, widely used in HTTP compression.

### 6.1 One-Shot

```tml
use std::zlib::brotli::{brotli_compress, brotli_decompress}

let compressed = brotli_compress("Hello, world!")
let original = brotli_decompress(ref compressed.unwrap())
```

### 6.2 With Options

```tml
use std::zlib::brotli::{brotli_compress_with_options}
use std::zlib::options::BrotliOptions

// Maximum compression quality
let opts = BrotliOptions::best()
let compressed = brotli_compress_with_options("Hello, world!", opts)

// Optimized for text content
let text_opts = BrotliOptions::text()
let compressed = brotli_compress_with_options(article_text, text_opts)
```

### 6.3 Streaming

```tml
use std::zlib::brotli::{BrotliCompress, BrotliDecompress}
use std::zlib::options::BrotliOptions

// Create streaming compressor
let mut encoder = BrotliCompress::new(BrotliOptions::default()).unwrap()

// Feed chunks
let chunk1 = encoder.write("Hello, ")
let chunk2 = encoder.write("world!")
let final_chunk = encoder.finish()

// Clean up
encoder.destroy()
```

### 6.4 Function Reference

| Function | Signature | Description |
|----------|-----------|-------------|
| `brotli_compress` | `(data: Str) -> Outcome[Buffer, ZlibError]` | Compress string |
| `brotli_compress_buffer` | `(data: ref Buffer) -> Outcome[Buffer, ZlibError]` | Compress buffer |
| `brotli_compress_with_options` | `(data: Str, options: BrotliOptions) -> Outcome[Buffer, ZlibError]` | Compress with options |
| `brotli_decompress` | `(data: ref Buffer) -> Outcome[Str, ZlibError]` | Decompress to string |
| `brotli_decompress_to_buffer` | `(data: ref Buffer) -> Outcome[Buffer, ZlibError]` | Decompress to buffer |

## 7. Zstandard (Zstd)

Zstandard is Facebook's fast compression algorithm with excellent compression ratios.

### 7.1 One-Shot

```tml
use std::zlib::zstd::{zstd_compress, zstd_decompress}

let compressed = zstd_compress("Hello, world!")
let original = zstd_decompress(ref compressed.unwrap())
```

### 7.2 With Options

```tml
use std::zlib::zstd::{zstd_compress_with_options}
use std::zlib::options::ZstdOptions

// Maximum compression
let opts = ZstdOptions::best()
let compressed = zstd_compress_with_options("Hello, world!", opts)

// Multi-threaded compression
let parallel_opts = ZstdOptions::parallel(4)  // 4 worker threads
let compressed = zstd_compress_with_options(large_data, parallel_opts)
```

### 7.3 Utility Functions

```tml
use std::zlib::zstd::{
    zstd_content_size, zstd_is_frame,
    zstd_min_level, zstd_max_level, zstd_default_level
}

// Check if data is a valid zstd frame
let is_zstd: Bool = zstd_is_frame(ref data)

// Get decompressed size (if available in frame header)
let size: I64 = zstd_content_size(ref data)

// Query level range
println("Zstd levels: " + zstd_min_level().to_string() + "-" + zstd_max_level().to_string())
```

### 7.4 Function Reference

| Function | Signature | Description |
|----------|-----------|-------------|
| `zstd_compress` | `(data: Str) -> Outcome[Buffer, ZlibError]` | Compress string |
| `zstd_compress_buffer` | `(data: ref Buffer) -> Outcome[Buffer, ZlibError]` | Compress buffer |
| `zstd_compress_with_options` | `(data: Str, options: ZstdOptions) -> Outcome[Buffer, ZlibError]` | Compress with options |
| `zstd_decompress` | `(data: ref Buffer) -> Outcome[Str, ZlibError]` | Decompress to string |
| `zstd_decompress_to_buffer` | `(data: ref Buffer) -> Outcome[Buffer, ZlibError]` | Decompress to buffer |
| `zstd_content_size` | `(data: ref Buffer) -> I64` | Get decompressed size |
| `zstd_decompress_bound` | `(data: ref Buffer) -> I64` | Upper bound of decompressed size |
| `zstd_is_frame` | `(data: ref Buffer) -> Bool` | Validate zstd frame |
| `zstd_min_level` | `() -> I32` | Minimum compression level |
| `zstd_max_level` | `() -> I32` | Maximum compression level (22) |
| `zstd_default_level` | `() -> I32` | Default compression level (3) |

## 8. Streaming Classes

The `std::zlib::stream` module provides streaming compression/decompression for processing data in chunks.

### 8.1 Available Streaming Types

| Type | Description |
|------|-------------|
| `Deflate` | Streaming zlib compression |
| `DeflateRaw` | Streaming raw deflate |
| `Gzip` | Streaming gzip compression |
| `Inflate` | Streaming zlib decompression |
| `InflateRaw` | Streaming raw inflate |
| `Gunzip` | Streaming gzip decompression |
| `Unzip` | Streaming auto-detect decompression |

### 8.2 Factory Functions

```tml
use std::zlib::stream::{create_deflate, create_gzip, create_inflate, create_gunzip}

let mut compressor = create_gzip().unwrap()
let mut decompressor = create_gunzip().unwrap()
```

### 8.3 Common Streaming Interface

All streaming types share these methods:

```tml
// Feed data chunk
let output: Outcome[Buffer, ZlibError] = compressor.write("chunk of data")

// Feed binary data
let output: Outcome[Buffer, ZlibError] = compressor.write_bytes(ref buffer)

// Flush buffered data
let output: Outcome[Buffer, ZlibError] = compressor.flush(Z_SYNC_FLUSH)

// Finalize stream (must call when done)
let output: Outcome[Buffer, ZlibError] = compressor.finish()

// Release resources
compressor.destroy()
```

## 9. Configuration Types

### 9.1 ZlibOptions

```tml
use std::zlib::options::ZlibOptions

// Defaults: level=-1, window_bits=15, mem_level=8, strategy=0
let opts = ZlibOptions::default()

// Presets
let gzip_opts = ZlibOptions::gzip()           // For gzip format
let raw_opts = ZlibOptions::deflate_raw()      // For raw deflate
let auto_opts = ZlibOptions::auto_detect()     // For auto-detection

// Method chaining
let custom = ZlibOptions::default().with_level(9)
```

### 9.2 BrotliOptions

```tml
use std::zlib::options::BrotliOptions

let opts = BrotliOptions::default()    // quality=4
let best = BrotliOptions::best()       // quality=11
let text = BrotliOptions::text()       // optimized for text
let font = BrotliOptions::font()       // optimized for fonts

let custom = BrotliOptions::default().with_quality(8)
```

### 9.3 ZstdOptions

```tml
use std::zlib::options::ZstdOptions

let opts = ZstdOptions::default()          // level=3
let best = ZstdOptions::best()             // level=22
let parallel = ZstdOptions::parallel(4)    // 4 worker threads

let custom = ZstdOptions::default()
    .with_level(15)
    .with_checksum(true)
```

## 10. Error Handling

### 10.1 ZlibError

```tml
use std::zlib::error::{ZlibError, ZlibErrorKind}

let result = deflate("data")
when result {
    Ok(buf) => println("Compressed " + buf.len().to_string() + " bytes")
    Err(e) => {
        println("Error: " + e.message)
        println("Kind: " + e.kind.to_message())
    }
}
```

### 10.2 Error Kinds

| Kind | Description |
|------|-------------|
| `Ok` | Success |
| `StreamError` | Internal stream state error |
| `DataError` | Input data corrupted |
| `MemoryError` | Insufficient memory |
| `BufferError` | Output buffer too small |
| `VersionError` | Library version mismatch |
| `NeedDict` | Dictionary required |
| `InvalidLevel` | Invalid compression level |
| `InvalidWindowBits` | Invalid window bits value |
| `InvalidMemLevel` | Invalid memory level |
| `InvalidStrategy` | Invalid strategy value |
| `InputTooLarge` | Input exceeds maximum size |
| `OutputTooLarge` | Output exceeds maximum size |
| `InvalidParameter` | Generic invalid parameter |

## 11. Constants

### 11.1 Compression Levels (Zlib)

| Constant | Value | Description |
|----------|-------|-------------|
| `Z_NO_COMPRESSION` | 0 | Store only |
| `Z_BEST_SPEED` | 1 | Fastest compression |
| `Z_DEFAULT_COMPRESSION` | -1 | Balanced (level 6) |
| `Z_BEST_COMPRESSION` | 9 | Best compression ratio |

### 11.2 Flush Modes

| Constant | Value | Description |
|----------|-------|-------------|
| `Z_NO_FLUSH` | 0 | Accumulate input |
| `Z_SYNC_FLUSH` | 2 | Flush and align on byte boundary |
| `Z_FULL_FLUSH` | 3 | Flush and reset state |
| `Z_FINISH` | 4 | Finish compression |

### 11.3 Brotli Quality Levels

| Constant | Value | Description |
|----------|-------|-------------|
| `BROTLI_MIN_QUALITY` | 0 | Fastest |
| `BROTLI_DEFAULT_QUALITY` | 4 | Balanced |
| `BROTLI_MAX_QUALITY` | 11 | Best compression |

### 11.4 Zstd Compression Levels

| Constant | Value | Description |
|----------|-------|-------------|
| `ZSTD_MIN_CLEVEL` | 1 | Fastest |
| `ZSTD_DEFAULT_CLEVEL` | 3 | Balanced |
| `ZSTD_MAX_CLEVEL` | 22 | Best compression (slow) |

## 12. Complete Example

```tml
// Hash Table Generator using CRC32
use std::zlib::crc32::{crc32}

func main() -> I32 {
    let messages = ["Hello", "World", "TML"]

    loop msg in messages {
        let checksum: I64 = crc32(msg)
        print(msg)
        print(" -> CRC32: ")
        println(checksum.to_string())
    }

    return 0
}
```

## 13. Algorithm Comparison

| Algorithm | Speed | Ratio | Use Case |
|-----------|-------|-------|----------|
| **Deflate/Zlib** | Medium | Good | General purpose, HTTP |
| **Gzip** | Medium | Good | Files, HTTP (Content-Encoding) |
| **Brotli** | Slow | Excellent | HTTP static assets |
| **Zstd** | Fast | Excellent | Real-time compression, storage |

---

*Previous: [07-HTTP.md](./07-HTTP.md)*
*Next: [09-JSON.md](./09-JSON.md) — JSON Parsing and Serialization*
