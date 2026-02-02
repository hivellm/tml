/**
 * TML Zlib Runtime - Internal Header
 *
 * Internal definitions shared between zlib implementation files.
 * Supports zlib, brotli, and zstd compression algorithms.
 */

#ifndef TML_ZLIB_INTERNAL_H
#define TML_ZLIB_INTERNAL_H

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

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
TmlBuffer* zlib_deflate(const char* data, int32_t level, int32_t window_bits, int32_t mem_level,
                        int32_t strategy);
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
TmlBuffer* gzip_compress(const char* data, int32_t level, int32_t window_bits, int32_t mem_level,
                         int32_t strategy);
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
TmlBuffer* brotli_compress(const char* data, int32_t quality, int32_t mode, int32_t lgwin,
                           int32_t lgblock, int64_t size_hint);
TmlBuffer* brotli_compress_buffer(TmlBuffer* data, int32_t quality, int32_t mode, int32_t lgwin,
                                  int32_t lgblock, int64_t size_hint);

// Brotli decompression
char* brotli_decompress(TmlBuffer* data, bool large_window);
TmlBuffer* brotli_decompress_buffer(TmlBuffer* data, bool large_window);

// Brotli error codes
int32_t brotli_get_error_code(TmlBuffer* buf);
int32_t brotli_last_error_code(void);

// Brotli streaming encoder (use _internal suffix to avoid brotli.h conflicts)
void* brotli_encoder_create_internal(int32_t quality, int32_t mode, int32_t lgwin, int32_t lgblock);
TmlBuffer* brotli_encoder_process_internal(void* state, const char* data, int32_t operation);
TmlBuffer* brotli_encoder_process_buffer_internal(void* state, TmlBuffer* data, int32_t operation);
bool brotli_encoder_is_finished_internal(void* state);
bool brotli_encoder_has_more_output_internal(void* state);
void brotli_encoder_destroy_internal(void* state);

// Brotli streaming decoder (use _internal suffix to avoid brotli.h conflicts)
void* brotli_decoder_create_internal(bool large_window);
TmlBuffer* brotli_decoder_process_internal(void* state, TmlBuffer* data);
bool brotli_decoder_is_finished_internal(void* state);
bool brotli_decoder_needs_more_input_internal(void* state);
bool brotli_decoder_has_more_output_internal(void* state);
int32_t brotli_decoder_get_error_code_internal(void* state);
void brotli_decoder_destroy_internal(void* state);

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
TmlBuffer* zstd_compress_context_process(ZstdCompressContext* ctx, const char* data,
                                         int32_t operation);
TmlBuffer* zstd_compress_context_process_buffer(ZstdCompressContext* ctx, TmlBuffer* data,
                                                int32_t operation);
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
// CRC32/Adler32 Functions (tml_ prefix to avoid collision with zlib.h)
// ============================================================================

uint32_t tml_crc32_compute(const char* data);
uint32_t tml_crc32_compute_buffer(TmlBuffer* data);
uint32_t tml_crc32_update(uint32_t crc, const char* data);
uint32_t tml_crc32_update_buffer(uint32_t crc, TmlBuffer* data);
uint32_t tml_crc32_combine(uint32_t crc1, uint32_t crc2, int64_t len2);

uint32_t tml_adler32_compute(const char* data);
uint32_t tml_adler32_compute_buffer(TmlBuffer* data);
uint32_t tml_adler32_update(uint32_t adler, const char* data);
uint32_t tml_adler32_update_buffer(uint32_t adler, TmlBuffer* data);
uint32_t tml_adler32_combine(uint32_t adler1, uint32_t adler2, int64_t len2);

// ============================================================================
// Streaming Classes (for Deflate, Inflate, Gzip, Gunzip, etc.)
// ============================================================================

// Deflate streaming compressor
typedef struct DeflateStream DeflateStream;
DeflateStream* deflate_stream_create(int32_t level, int32_t window_bits, int32_t mem_level,
                                     int32_t strategy);
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

// ============================================================================
// FFI Export Functions (zlib_exports.c)
// ============================================================================

// Zstd extended exports (matching TML signatures)
void* zstd_compress_ext(const char* data, int32_t level, int32_t strategy, int32_t window_log,
                        bool checksum, bool content_size);
void* zstd_compress_buffer_ext(void* handle, int32_t level, int32_t strategy, int32_t window_log,
                               bool checksum, bool content_size);
char* zstd_decompress_ext(void* handle, int32_t window_log);
void* zstd_decompress_buffer_ext(void* handle, int32_t window_log);

// Streaming deflate/inflate exports
void* zlib_deflate_stream_create(int32_t level, int32_t window_bits, int32_t mem_level,
                                 int32_t strategy);
void* zlib_deflate_stream_process(void* handle, const char* data, int32_t flush);
void* zlib_deflate_stream_process_buffer(void* handle, void* data_handle, int32_t flush);
void* zlib_deflate_stream_params(void* handle, int32_t level, int32_t strategy);
bool zlib_deflate_stream_reset(void* handle);
int64_t zlib_deflate_stream_bytes_written(void* handle);
void zlib_deflate_stream_destroy(void* handle);

void* zlib_inflate_stream_create(int32_t window_bits);
void* zlib_inflate_stream_process(void* handle, void* data_handle);
void* zlib_inflate_stream_flush(void* handle, int32_t flush);
bool zlib_inflate_stream_reset(void* handle);
bool zlib_inflate_stream_is_finished(void* handle);
int64_t zlib_inflate_stream_bytes_written(void* handle);
int32_t zlib_inflate_stream_error_code(void* handle);
void zlib_inflate_stream_destroy(void* handle);

// Gzip header exports
bool zlib_gzip_header_text(void* header_handle);
int32_t zlib_gzip_header_os(void* header_handle);
const char* zlib_gzip_header_name(void* header_handle);
const char* zlib_gzip_header_comment(void* header_handle);
bool zlib_gzip_header_hcrc(void* header_handle);
int64_t zlib_gzip_header_time(void* header_handle);

// Zstd streaming exports
void* zstd_cstream_create(int32_t level, int32_t strategy, int32_t window_log, bool checksum,
                          bool content_size, int32_t nb_workers);
void* zstd_cstream_create_with_dict(void* dict_handle, int32_t level);
void* zstd_cstream_process(void* handle, const char* data, int32_t end_op);
void* zstd_cstream_process_buffer(void* handle, void* data_handle, int32_t end_op);
bool zstd_cstream_reset(void* handle);
bool zstd_cstream_set_pledged_size(void* handle, int64_t size);
void zstd_cstream_destroy(void* handle);

void* zstd_dstream_create(int32_t window_log);
void* zstd_dstream_create_with_dict(void* dict_handle);
void* zstd_dstream_process(void* handle, void* data_handle);
bool zstd_dstream_reset(void* handle);
int64_t zstd_dstream_content_size(void* handle);
int32_t zstd_dstream_get_error_code(void* handle);
void zstd_dstream_destroy(void* handle);

// Zstd dictionary exports
int32_t zstd_dict_get_id(void* handle);
void* zstd_dict_to_buffer(void* handle);
void* zstd_dict_train_ext(void* samples_handle, int64_t dict_size);

// Zstd utility exports
int64_t zstd_get_frame_content_size(void* handle);
int64_t zstd_get_decompress_bound(void* handle);
int32_t zstd_get_frame_dict_id(void* handle);

// Gzip compression exports
void* gzip(const char* data, int32_t level, int32_t window_bits, int32_t mem_level,
           int32_t strategy);
char* gunzip(void* handle, int32_t window_bits);
void* gzip_buffer(void* handle, int32_t level, int32_t window_bits, int32_t mem_level,
                  int32_t strategy);
void* gunzip_buffer(void* handle, int32_t window_bits);
void* read_gzip_header(void* handle);
void gzip_header_destroy_wrapper(void* handle);

// Note: buffer_destroy is declared in collections.c (main runtime)

#ifdef __cplusplus
}
#endif

#endif // TML_ZLIB_INTERNAL_H
