# Compression

TML provides bindings to industry-standard compression algorithms through the `std::compress` module. All algorithms support both one-shot and streaming modes.

## Deflate and Inflate

The foundational compression algorithm used by gzip and zip formats:

```tml
use std::compress::{deflate, inflate}

// Compress data
let original = "Hello, World! ".repeat(100)
let compressed = deflate(original.as_bytes())!
println("Compressed: " + compressed.len().to_string() + " bytes")

// Decompress data
let decompressed = inflate(compressed)!
assert_eq(decompressed, original.as_bytes())
```

## Gzip and Gunzip

The gzip format wraps deflate with headers and a CRC32 checksum:

```tml
use std::compress::{gzip, gunzip}
use std::compress::ZlibOptions

// Default compression
let compressed = gzip(data)!

// Custom compression level
let options = ZlibOptions { level: 9 }  // best compression
let compressed = gzip_with(data, options)!

// Decompress
let original = gunzip(compressed)!
```

## Brotli

Google's Brotli algorithm offers better compression ratios than gzip, particularly for text content:

```tml
use std::compress::{brotli_compress, brotli_decompress}

let compressed = brotli_compress(data)!
let original = brotli_decompress(compressed)!
```

Brotli is widely used for HTTP content encoding. The TML HTTP server middleware can apply it automatically.

## Zstandard

Facebook's Zstandard (zstd) provides high compression ratios with fast decompression:

```tml
use std::compress::{zstd_compress, zstd_decompress}

let compressed = zstd_compress(data)!
let original = zstd_decompress(compressed)!
```

Zstandard excels at compressing structured data like logs, databases, and serialized objects.

## Checksums

Verify data integrity with checksums:

```tml
use std::compress::{crc32, adler32}

let checksum = crc32(data)
println("CRC32: " + checksum.to_string())

let checksum = adler32(data)
println("Adler-32: " + checksum.to_string())
```

CRC32 is used by gzip and zip formats. Adler-32 is faster but less reliable for error detection.

## Algorithm Comparison

| Algorithm | Compression Ratio | Compress Speed | Decompress Speed | Use Case |
|-----------|-------------------|----------------|-------------------|----------|
| Deflate | Good | Medium | Fast | General purpose, zip files |
| Gzip | Good | Medium | Fast | HTTP content, file archives |
| Brotli | Excellent | Slow | Fast | Web content, static assets |
| Zstandard | Excellent | Fast | Very fast | Logs, databases, real-time |

**Guidelines:**
- Use **gzip** for HTTP responses and file archives (universal support).
- Use **brotli** for static web assets where compression time is not critical.
- Use **zstd** for application data where both speed and ratio matter.
- Use **deflate** directly only when working with zip-format archives.

## Error Handling

All compression functions return `Outcome[Bytes, CompressError]`. Common errors include corrupted input data and invalid compression parameters:

```tml
let result = gunzip(possibly_corrupt_data)
when result {
    Ok(data) => process(data),
    Err(e) => println("Decompression failed: " + e.to_string()),
}
```
