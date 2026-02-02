/**
 * TML Zlib Runtime - Internal Header
 *
 * Internal definitions shared between zlib implementation files.
 * Supports zlib, brotli, and zstd compression algorithms.
 */

#ifndef TML_ZLIB_INTERNAL_H
#define TML_ZLIB_INTERNAL_H

#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Platform Detection
// ============================================================================

#if defined(_WIN32) || defined(_WIN64)
    #define TML_PLATFORM_WINDOWS 1
#elif defined(__APPLE__)
    #define TML_PLATFORM_MACOS 1
#elif defined(__linux__)
    #define TML_PLATFORM_LINUX 1
#else
    #define TML_PLATFORM_UNIX 1
#endif

// ============================================================================
// Buffer Structure (matches TML Buffer)
// ============================================================================

typedef struct TmlBuffer {
    uint8_t* data;
    size_t len;
    size_t capacity;
    size_t read_pos;
} TmlBuffer;

// Buffer creation/destruction
TmlBuffer* tml_buffer_create(size_t capacity);
TmlBuffer* tml_buffer_from_data(const uint8_t* data, size_t len);
TmlBuffer* tml_buffer_from_string(const char* str);
void tml_buffer_destroy(TmlBuffer* buf);

// Buffer operations
void tml_buffer_write(TmlBuffer* buf, const uint8_t* data, size_t len);
void tml_buffer_write_byte(TmlBuffer* buf, uint8_t byte);
size_t tml_buffer_read(TmlBuffer* buf, uint8_t* dest, size_t len);
void tml_buffer_reset_read(TmlBuffer* buf);
void tml_buffer_clear(TmlBuffer* buf);

// ============================================================================
// Error Handling
// ============================================================================

// Thread-local error code storage
void zlib_set_last_error(int32_t code);
int32_t zlib_get_last_error(void);

// ============================================================================
// Zlib Functions (RFC 1950/1951)
// ============================================================================

// Deflate compression
TmlBuffer* zlib_deflate(const char* data, int32_t level, int32_t window_bits,
                        int32_t mem_level, int32_t strategy);
TmlBuffer* zlib_deflate_buffer(TmlBuffer* data, int32_t level, int32_t window_bits,
                               int32_t mem_level, int32_t strategy);

// Inflate decompression
char* zlib_inflate(TmlBuffer* data, int32_t window_bits);
TmlBuffer* zlib_inflate_buffer(TmlBuffer* data, int32_t window_bits);

// Error code retrieval
int32_t zlib_get_error_code(TmlBuffer* buf);
int32_t zlib_last_error_code(void);

// ============================================================================
// Gzip Functions (RFC 1952)
// ============================================================================

// Gzip compression (adds gzip header/trailer)
TmlBuffer* gzip_compress(const char* data, int32_t level, int32_t window_bits,
                         int32_t mem_level, int32_t strategy);
TmlBuffer* gzip_compress_buffer(TmlBuffer* data, int32_t level, int32_t window_bits,
                                int32_t mem_level, int32_t strategy);

// Gzip decompression
char* gzip_decompress(TmlBuffer* data, int32_t window_bits);
TmlBuffer* gzip_decompress_buffer(TmlBuffer* data, int32_t window_bits);

// Gzip header operations
typedef struct GzipHeaderInfo {
    char* filename;
    char* comment;
    int64_t mtime;
    int32_t os;
    bool is_text;
} GzipHeaderInfo;

GzipHeaderInfo* gzip_read_header(TmlBuffer* data);
void gzip_header_destroy(GzipHeaderInfo* header);

// ============================================================================
// Brotli Functions
// ============================================================================

// Brotli compression
TmlBuffer* brotli_compress(const char* data, int32_t quality, int32_t mode,
                           int32_t lgwin, int32_t lgblock, int64_t size_hint);
TmlBuffer* brotli_compress_buffer(TmlBuffer* data, int32_t quality, int32_t mode,
                                  int32_t lgwin, int32_t lgblock, int64_t size_hint);

// Brotli decompression
char* brotli_decompress(TmlBuffer* data, bool large_window);
TmlBuffer* brotli_decompress_buffer(TmlBuffer* data, bool large_window);

// Brotli error codes
int32_t brotli_get_error_code(TmlBuffer* buf);
int32_t brotli_last_error_code(void);

// Brotli streaming encoder
typedef struct BrotliEncoderState BrotliEncoderState;
BrotliEncoderState* brotli_encoder_create(int32_t quality, int32_t mode,
                                          int32_t lgwin, int32_t lgblock);
TmlBuffer* brotli_encoder_process(BrotliEncoderState* state, const char* data, int32_t operation);
TmlBuffer* brotli_encoder_process_buffer(BrotliEncoderState* state, TmlBuffer* data, int32_t operation);
bool brotli_encoder_is_finished(BrotliEncoderState* state);
bool brotli_encoder_has_more_output(BrotliEncoderState* state);
void brotli_encoder_destroy(BrotliEncoderState* state);

// Brotli streaming decoder
typedef struct BrotliDecoderState BrotliDecoderState;
BrotliDecoderState* brotli_decoder_create(bool large_window);
TmlBuffer* brotli_decoder_process(BrotliDecoderState* state, TmlBuffer* data);
bool brotli_decoder_is_finished(BrotliDecoderState* state);
bool brotli_decoder_needs_more_input(BrotliDecoderState* state);
bool brotli_decoder_has_more_output(BrotliDecoderState* state);
int32_t brotli_decoder_get_error_code(BrotliDecoderState* state);
void brotli_decoder_destroy(BrotliDecoderState* state);

// ============================================================================
// Zstd Functions
// ============================================================================

// Zstd compression
TmlBuffer* zstd_compress(const char* data, int32_t level);
TmlBuffer* zstd_compress_buffer(TmlBuffer* data, int32_t level);
TmlBuffer* zstd_compress_with_dict(const char* data, int32_t level, TmlBuffer* dict);

// Zstd decompression
char* zstd_decompress(TmlBuffer* data);
TmlBuffer* zstd_decompress_buffer(TmlBuffer* data);
TmlBuffer* zstd_decompress_with_dict(TmlBuffer* data, TmlBuffer* dict);

// Zstd error codes
int32_t zstd_get_error_code(TmlBuffer* buf);
int32_t zstd_last_error_code(void);

// Zstd utilities
int64_t zstd_content_size(TmlBuffer* data);
int64_t zstd_decompress_bound(TmlBuffer* data);
int32_t zstd_frame_dict_id(TmlBuffer* data);
bool zstd_is_frame(TmlBuffer* data);
int32_t zstd_min_level(void);
int32_t zstd_max_level(void);
int32_t zstd_default_level(void);

// Zstd streaming compressor
typedef struct ZstdCompressContext ZstdCompressContext;
ZstdCompressContext* zstd_compress_context_create(int32_t level, bool checksum);
TmlBuffer* zstd_compress_context_process(ZstdCompressContext* ctx, const char* data, int32_t operation);
TmlBuffer* zstd_compress_context_process_buffer(ZstdCompressContext* ctx, TmlBuffer* data, int32_t operation);
void zstd_compress_context_destroy(ZstdCompressContext* ctx);

// Zstd streaming decompressor
typedef struct ZstdDecompressContext ZstdDecompressContext;
ZstdDecompressContext* zstd_decompress_context_create(void);
TmlBuffer* zstd_decompress_context_process(ZstdDecompressContext* ctx, TmlBuffer* data);
void zstd_decompress_context_destroy(ZstdDecompressContext* ctx);

// Zstd dictionary
typedef struct ZstdDict ZstdDict;
ZstdDict* zstd_dict_create(TmlBuffer* data);
ZstdDict* zstd_dict_train(TmlBuffer** samples, size_t num_samples, size_t dict_size);
int32_t zstd_dict_id(ZstdDict* dict);
void zstd_dict_destroy(ZstdDict* dict);

// ============================================================================
// CRC32/Adler32 Functions
// ============================================================================

uint32_t crc32_compute(const char* data);
uint32_t crc32_compute_buffer(TmlBuffer* data);
uint32_t crc32_update(uint32_t crc, const char* data);
uint32_t crc32_update_buffer(uint32_t crc, TmlBuffer* data);
uint32_t crc32_combine(uint32_t crc1, uint32_t crc2, int64_t len2);

uint32_t adler32_compute(const char* data);
uint32_t adler32_compute_buffer(TmlBuffer* data);
uint32_t adler32_update(uint32_t adler, const char* data);
uint32_t adler32_update_buffer(uint32_t adler, TmlBuffer* data);
uint32_t adler32_combine(uint32_t adler1, uint32_t adler2, int64_t len2);

// ============================================================================
// Streaming Classes (for Deflate, Inflate, Gzip, Gunzip, etc.)
// ============================================================================

// Deflate streaming compressor
typedef struct DeflateStream DeflateStream;
DeflateStream* deflate_stream_create(int32_t level, int32_t window_bits,
                                      int32_t mem_level, int32_t strategy);
TmlBuffer* deflate_stream_write(DeflateStream* stream, const char* data, int32_t flush);
TmlBuffer* deflate_stream_write_buffer(DeflateStream* stream, TmlBuffer* data, int32_t flush);
TmlBuffer* deflate_stream_flush(DeflateStream* stream);
TmlBuffer* deflate_stream_finish(DeflateStream* stream);
void deflate_stream_destroy(DeflateStream* stream);

// Inflate streaming decompressor
typedef struct InflateStream InflateStream;
InflateStream* inflate_stream_create(int32_t window_bits);
TmlBuffer* inflate_stream_write(InflateStream* stream, TmlBuffer* data);
bool inflate_stream_is_finished(InflateStream* stream);
void inflate_stream_destroy(InflateStream* stream);

#ifdef __cplusplus
}
#endif

#endif // TML_ZLIB_INTERNAL_H
