# Compression

TML provides the `std::zlib` module for data compression, decompression, and checksums. It supports four algorithms — **deflate**, **gzip**, **Brotli**, and **Zstandard** — with both one-shot and streaming interfaces.

## Checksums

Checksums detect data corruption. TML provides CRC32 and Adler32:

```tml
use std::zlib::crc32::{crc32, adler32}

func main() -> I32 {
    let msg: Str = "Hello, TML!"

    let c: I64 = crc32(msg)
    println("CRC32:  {c.to_string()}")

    let a: I64 = adler32(msg)
    println("Adler32: {a.to_string()}")

    return 0
}
```

For binary data, use the buffer variants:

```tml
use std::zlib::crc32::{crc32_buffer, adler32_buffer}

let buf = Buffer::from("binary data")
let checksum: I64 = crc32_buffer(ref buf)
```

### Incremental CRC32

Compute CRC32 over multiple chunks:

```tml
use std::zlib::crc32::{crc32_init, crc32_update, crc32_final}

let mut state: U32 = crc32_init()
state = crc32_update(state, "first chunk")
state = crc32_update(state, "second chunk")
let checksum: I64 = crc32_final(state)
```

## Deflate / Inflate

Deflate is the core compression algorithm (RFC 1951). Use it when you need raw compressed data without headers:

```tml
use std::zlib::deflate::{deflate, inflate}

// Compress
let data: Str = "Hello, TML! ".repeat(100)
let compressed: Buffer = deflate(data)
println("Compressed: {compressed.len().to_string()} bytes")

// Decompress
let original: Str = inflate(ref compressed)
println("Decompressed: {original.len().to_string()} bytes")
```

### With Options

```tml
use std::zlib::deflate::{deflate_with_options}
use std::zlib::options::{ZlibOptions}

let opts: ZlibOptions = ZlibOptions {
    level: 9,              // 0=none, 1=fast, 6=default, 9=best compression
    window_bits: 15,       // 8-15 (higher = better compression, more memory)
    mem_level: 8,          // 1-9 (higher = faster, more memory)
    strategy: 0,           // 0=default, 1=filtered, 2=huffman, 3=rle, 4=fixed
}

let compressed: Buffer = deflate_with_options(data, opts)
```

## Gzip

Gzip adds headers and CRC32 checksums on top of deflate. Use it for file compression and HTTP:

```tml
use std::zlib::gzip::{gzip, gunzip}

// Compress
let compressed: Buffer = gzip("data to compress")

// Decompress
let original: Str = gunzip(ref compressed)
```

### Buffer Variants

```tml
use std::zlib::gzip::{gzip_buffer, gunzip_buffer}

let input: Buffer = Buffer::from("binary data")
let compressed: Buffer = gzip_buffer(ref input)
let decompressed: Buffer = gunzip_buffer(ref compressed)
```

## Brotli

Brotli offers higher compression ratios than gzip, especially for text and web content:

```tml
use std::zlib::brotli::{brotli_compress, brotli_decompress}

let compressed: Buffer = brotli_compress("data to compress")
let original: Str = brotli_decompress(ref compressed)
```

### With Options

```tml
use std::zlib::brotli::{brotli_compress_with_options}
use std::zlib::options::{BrotliOptions}

let opts: BrotliOptions = BrotliOptions {
    quality: 11,    // 0-11 (higher = better compression, slower)
    lgwin: 22,      // window size: 10-24
    lgblock: 0,     // block size: 0=auto, 16-24
}

let compressed: Buffer = brotli_compress_with_options("data", opts)
```

## Zstandard (Zstd)

Zstd provides an excellent balance of speed and compression ratio:

```tml
use std::zlib::zstd::{zstd_compress, zstd_decompress}

let compressed: Buffer = zstd_compress("data to compress")
let original: Str = zstd_decompress(ref compressed)
```

### With Options

```tml
use std::zlib::zstd::{zstd_compress_with_options}
use std::zlib::options::{ZstdOptions}

let opts: ZstdOptions = ZstdOptions {
    level: 3,                // 1-22 (higher = better compression, slower)
    enable_checksum: true,   // include XXH64 checksum in frame
}

let compressed: Buffer = zstd_compress_with_options("data", opts)
```

## Streaming Compression

For large data or data that arrives in chunks, use streaming classes. This avoids loading everything into memory at once.

### Streaming Deflate

```tml
use std::zlib::stream::{DeflateStream, InflateStream}

// Compress in chunks
let mut compressor: DeflateStream = DeflateStream.new(6)  // level 6
compressor.write("first chunk of data")
compressor.write("second chunk of data")
let compressed: Buffer = compressor.finish()
compressor.destroy()

// Decompress in chunks
let mut decompressor: InflateStream = InflateStream.new()
decompressor.write(ref compressed)
let output: Str = decompressor.finish_str()
decompressor.destroy()
```

### Streaming Gzip

```tml
use std::zlib::stream::{GzipStream, GunzipStream}

let mut gz: GzipStream = GzipStream.new(6)
gz.write("chunk 1")
gz.write("chunk 2")
let compressed: Buffer = gz.finish()
gz.destroy()
```

## Choosing an Algorithm

| Algorithm | Compression | Speed | Best For |
|-----------|-------------|-------|----------|
| **Deflate** | Good | Fast | Raw compressed data, zip files |
| **Gzip** | Good | Fast | HTTP compression, file archiving |
| **Brotli** | Excellent | Slow | Web assets, static content |
| **Zstd** | Very Good | Very Fast | Real-time compression, databases, logs |

General guidelines:
- **Web/HTTP**: Use gzip for compatibility, brotli for best compression
- **Real-time/streaming**: Use zstd for best speed-to-ratio tradeoff
- **Archives**: Use gzip for compatibility, zstd for speed
- **Checksums only**: Use CRC32 for data integrity checks

## Error Handling

Compression operations can fail on corrupted data or invalid options:

```tml
use std::zlib::error::{ZlibError, ZlibErrorKind}
use std::zlib::gzip::{gunzip}

func decompress_safe(data: ref Buffer) -> Outcome[Str, ZlibError] {
    let result: Str = gunzip(data)
    Ok(result)
}
```

Common error kinds:

| Error Kind | Cause |
|------------|-------|
| `DataError` | Corrupted or invalid compressed data |
| `BufferError` | Output buffer too small |
| `StreamError` | Stream state error |
| `InvalidLevel` | Compression level out of range |

## See Also

- [Cryptography](ch16-00-crypto.md) — Hashing, encryption, and key management
- [std::zlib Package Reference](../packages/08-COMPRESS.md) — Complete API reference
- [Standard Library](ch10-00-standard-library.md) — Overview of all standard modules

---

*Previous: [ch16-00-crypto.md](ch16-00-crypto.md)*
*Next: (end of guide)*
