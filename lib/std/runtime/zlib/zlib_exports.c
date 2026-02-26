/**
 * TML Zlib Runtime - FFI Export Wrappers
 *
 * This file provides wrapper functions that match the @extern names
 * used in the TML zlib module. It maps TML FFI names to internal C functions.
 *
 * IMPORTANT: Do not include external library headers (zlib.h, brotli, zstd.h)
 * in this file to avoid symbol conflicts. All external types are treated as opaque void*.
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>   // For debug fprintf
#include <stdlib.h>
#include <string.h>

// Forward declare TmlBuffer without including zlib_internal.h (which includes zlib.h)
typedef struct TmlBuffer {
    uint8_t* data;
    size_t len;
    size_t capacity;
    size_t read_pos;
} TmlBuffer;

typedef struct GzipHeaderInfo {
    char* filename;
    char* comment;
    int64_t mtime;
    int32_t os;
    bool is_text;
} GzipHeaderInfo;

// Forward declare TmlList to access list elements for zstd_dict_train
typedef struct TmlList {
    void* data;
    int64_t len;
    int64_t capacity;
    int64_t elem_size;
} TmlList;

// ============================================================================
// Forward declarations for internal types and functions (defined in other .c files)
// ============================================================================

// Deflate/Inflate streaming (opaque types)
typedef struct DeflateStream DeflateStream;
typedef struct InflateStream InflateStream;

extern DeflateStream* deflate_stream_create(int32_t level, int32_t window_bits, int32_t mem_level,
                                            int32_t strategy);
extern TmlBuffer* deflate_stream_write(DeflateStream* stream, const char* data, int32_t flush);
extern TmlBuffer* deflate_stream_write_buffer(DeflateStream* stream, TmlBuffer* data,
                                              int32_t flush);
extern void deflate_stream_destroy(DeflateStream* stream);

extern InflateStream* inflate_stream_create(int32_t window_bits);
extern TmlBuffer* inflate_stream_write(InflateStream* stream, TmlBuffer* data);
extern bool inflate_stream_is_finished(InflateStream* stream);
extern void inflate_stream_destroy(InflateStream* stream);

// Zstd streaming (opaque types)
typedef struct ZstdCompressContext ZstdCompressContext;
typedef struct ZstdDecompressContext ZstdDecompressContext;
typedef struct ZstdDict ZstdDict;

extern ZstdCompressContext* zstd_compress_context_create(int32_t level, bool checksum);
extern TmlBuffer* zstd_compress_context_process(ZstdCompressContext* ctx, const char* data,
                                                int32_t operation);
extern TmlBuffer* zstd_compress_context_process_buffer(ZstdCompressContext* ctx, TmlBuffer* data,
                                                       int32_t operation);
extern void zstd_compress_context_destroy(ZstdCompressContext* ctx);

extern ZstdDecompressContext* zstd_decompress_context_create(void);
extern TmlBuffer* zstd_decompress_context_process(ZstdDecompressContext* ctx, TmlBuffer* data);
extern void zstd_decompress_context_destroy(ZstdDecompressContext* ctx);

extern ZstdDict* zstd_dict_create(TmlBuffer* data);
extern ZstdDict* zstd_dict_train_impl(TmlBuffer** samples, size_t num_samples, size_t dict_size);
extern int32_t zstd_dict_id(ZstdDict* dict);
extern TmlBuffer* zstd_dict_export(ZstdDict* dict);
extern void zstd_dict_destroy(ZstdDict* dict);

extern int64_t zstd_content_size(TmlBuffer* data);
extern int64_t zstd_decompress_bound(TmlBuffer* data);
extern int32_t zstd_frame_dict_id(TmlBuffer* data);
extern bool zstd_is_frame(TmlBuffer* data);
extern int32_t zstd_last_error_code(void);

// Brotli streaming (void* to avoid type conflicts with brotli headers)
extern void* brotli_encoder_create_internal(int32_t quality, int32_t mode, int32_t lgwin,
                                            int32_t lgblock);
extern TmlBuffer* brotli_encoder_process_internal(void* state, const char* data, int32_t operation);
extern TmlBuffer* brotli_encoder_process_buffer_internal(void* state, TmlBuffer* data,
                                                         int32_t operation);
extern bool brotli_encoder_is_finished_internal(void* state);
extern bool brotli_encoder_has_more_output_internal(void* state);
extern void brotli_encoder_destroy_internal(void* state);

extern void* brotli_decoder_create_internal(bool large_window);
extern TmlBuffer* brotli_decoder_process_internal(void* state, TmlBuffer* data);
extern bool brotli_decoder_is_finished_internal(void* state);
extern bool brotli_decoder_needs_more_input_internal(void* state);
extern bool brotli_decoder_has_more_output_internal(void* state);
extern int32_t brotli_decoder_get_error_code_internal(void* state);
extern void brotli_decoder_destroy_internal(void* state);

// Core compression functions
extern TmlBuffer* brotli_compress(const char* data, int32_t quality, int32_t mode, int32_t lgwin,
                                  int32_t lgblock, int64_t size_hint);
extern TmlBuffer* brotli_compress_buffer(TmlBuffer* data, int32_t quality, int32_t mode,
                                         int32_t lgwin, int32_t lgblock, int64_t size_hint);
extern char* brotli_decompress(TmlBuffer* data, bool large_window);
extern TmlBuffer* brotli_decompress_buffer(TmlBuffer* data, bool large_window);
extern int32_t brotli_last_error_code(void);
extern int32_t brotli_get_error_code(TmlBuffer* buf);

extern TmlBuffer* zstd_compress(const char* data, int32_t level);
extern TmlBuffer* zstd_compress_buffer(TmlBuffer* data, int32_t level);
extern char* zstd_decompress(TmlBuffer* data);
extern TmlBuffer* zstd_decompress_buffer(TmlBuffer* data);
extern int32_t zstd_get_error_code(TmlBuffer* buf);

extern TmlBuffer* zlib_deflate(const char* data, int32_t level, int32_t window_bits,
                               int32_t mem_level, int32_t strategy);
extern TmlBuffer* zlib_deflate_buffer(TmlBuffer* data, int32_t level, int32_t window_bits,
                                      int32_t mem_level, int32_t strategy);
extern char* zlib_inflate(TmlBuffer* data, int32_t window_bits);
extern TmlBuffer* zlib_inflate_buffer(TmlBuffer* data, int32_t window_bits);
extern int32_t zlib_get_error_code(TmlBuffer* buf);
extern int32_t zlib_last_error_code(void);

extern TmlBuffer* gzip_compress(const char* data, int32_t level, int32_t window_bits,
                                int32_t mem_level, int32_t strategy);
extern TmlBuffer* gzip_compress_buffer(TmlBuffer* data, int32_t level, int32_t window_bits,
                                       int32_t mem_level, int32_t strategy);
extern char* gzip_decompress(TmlBuffer* data, int32_t window_bits);
extern TmlBuffer* gzip_decompress_buffer(TmlBuffer* data, int32_t window_bits);
extern GzipHeaderInfo* gzip_read_header(TmlBuffer* data);
extern void gzip_header_destroy(GzipHeaderInfo* header);

// CRC32/Adler32 with tml_ prefix to avoid zlib.h conflicts
extern uint32_t tml_crc32_compute(const char* data);
extern uint32_t tml_crc32_compute_buffer(TmlBuffer* data);
extern uint32_t tml_crc32_update(uint32_t crc, const char* data);
extern uint32_t tml_crc32_update_buffer(uint32_t crc, TmlBuffer* data);
extern uint32_t tml_crc32_combine(uint32_t crc1, uint32_t crc2, int64_t len2);

extern uint32_t tml_adler32_compute(const char* data);
extern uint32_t tml_adler32_compute_buffer(TmlBuffer* data);
extern uint32_t tml_adler32_update(uint32_t adl, const char* data);
extern uint32_t tml_adler32_update_buffer(uint32_t adl, TmlBuffer* data);
extern uint32_t tml_adler32_combine(uint32_t adler1, uint32_t adler2, int64_t len2);

// ============================================================================
// Zstd Extended Exports - wrappers for TML signatures
// ============================================================================

// TML signature: zstd_compress(data: Str, level: I32, strategy: I32, window_log: I32, checksum:
// Bool, content_size: Bool) -> *Unit C signature: zstd_compress(data: const char*, level: int32_t)
// -> TmlBuffer* We need a wrapper that accepts all params but only uses level
void* zstd_compress_ext(const char* data, int32_t level, int32_t strategy, int32_t window_log,
                        bool checksum, bool content_size) {
    (void)strategy;
    (void)window_log;
    (void)checksum;
    (void)content_size;
    return (void*)zstd_compress(data, level);
}

// TML signature: zstd_compress_buffer(handle: *Unit, level: I32, strategy: I32, window_log: I32,
// checksum: Bool, content_size: Bool) -> *Unit
void* zstd_compress_buffer_ext(void* handle, int32_t level, int32_t strategy, int32_t window_log,
                               bool checksum, bool content_size) {
    (void)strategy;
    (void)window_log;
    (void)checksum;
    (void)content_size;
    return (void*)zstd_compress_buffer((TmlBuffer*)handle, level);
}

// TML signature: zstd_decompress(handle: *Unit, window_log: I32) -> Str
char* zstd_decompress_ext(void* handle, int32_t window_log) {
    (void)window_log;
    return zstd_decompress((TmlBuffer*)handle);
}

// TML signature: zstd_decompress_buffer(handle: *Unit, window_log: I32) -> *Unit
void* zstd_decompress_buffer_ext(void* handle, int32_t window_log) {
    (void)window_log;
    return (void*)zstd_decompress_buffer((TmlBuffer*)handle);
}

// ============================================================================
// Streaming Deflate/Inflate Exports
// ============================================================================

#define Z_SYNC_FLUSH 2

// TML: zlib_deflate_stream_create(level, window_bits, mem_level, strategy) -> *Unit
void* zlib_deflate_stream_create(int32_t level, int32_t window_bits, int32_t mem_level,
                                 int32_t strategy) {
    return (void*)deflate_stream_create(level, window_bits, mem_level, strategy);
}

// TML: zlib_deflate_stream_process(handle, data, flush) -> *Unit
void* zlib_deflate_stream_process(void* handle, const char* data, int32_t flush) {
    return (void*)deflate_stream_write((DeflateStream*)handle, data, flush);
}

// TML: zlib_deflate_stream_process_buffer(handle, data_handle, flush) -> *Unit
void* zlib_deflate_stream_process_buffer(void* handle, void* data_handle, int32_t flush) {
    return (void*)deflate_stream_write_buffer((DeflateStream*)handle, (TmlBuffer*)data_handle,
                                              flush);
}

// TML: zlib_deflate_stream_params(handle, level, strategy) -> *Unit
void* zlib_deflate_stream_params(void* handle, int32_t level, int32_t strategy) {
    DeflateStream* stream = (DeflateStream*)handle;
    (void)level;
    (void)strategy;
    if (!stream)
        return NULL;
    // Just flush with current params for now - deflateParams would need strm access
    return (void*)deflate_stream_write(stream, "", Z_SYNC_FLUSH);
}

// TML: zlib_deflate_stream_reset(handle) -> Bool
bool zlib_deflate_stream_reset(void* handle) {
    // Reset not directly supported without reimplementing
    return handle != NULL;
}

// TML: zlib_deflate_stream_bytes_written(handle) -> I64
int64_t zlib_deflate_stream_bytes_written(void* handle) {
    (void)handle;
    // Would need to track in stream structure
    return 0;
}

// TML: zlib_deflate_stream_destroy(handle)
void zlib_deflate_stream_destroy(void* handle) {
    deflate_stream_destroy((DeflateStream*)handle);
}

// TML: zlib_inflate_stream_create(window_bits) -> *Unit
void* zlib_inflate_stream_create(int32_t window_bits) {
    return (void*)inflate_stream_create(window_bits);
}

// TML: zlib_inflate_stream_process(handle, data_handle) -> *Unit
void* zlib_inflate_stream_process(void* handle, void* data_handle) {
    return (void*)inflate_stream_write((InflateStream*)handle, (TmlBuffer*)data_handle);
}

// TML: zlib_inflate_stream_flush(handle, flush) -> *Unit
void* zlib_inflate_stream_flush(void* handle, int32_t flush) {
    (void)handle;
    (void)flush;
    // Inflate doesn't have flush concept like deflate
    return NULL;
}

// TML: zlib_inflate_stream_reset(handle) -> Bool
bool zlib_inflate_stream_reset(void* handle) {
    return handle != NULL;
}

// TML: zlib_inflate_stream_is_finished(handle) -> Bool
bool zlib_inflate_stream_is_finished(void* handle) {
    return inflate_stream_is_finished((InflateStream*)handle);
}

// TML: zlib_inflate_stream_bytes_written(handle) -> I64
int64_t zlib_inflate_stream_bytes_written(void* handle) {
    (void)handle;
    return 0;
}

// TML: zlib_inflate_stream_error_code(handle) -> I32
int32_t zlib_inflate_stream_error_code(void* handle) {
    (void)handle;
    return zlib_last_error_code();
}

// TML: zlib_inflate_stream_destroy(handle)
void zlib_inflate_stream_destroy(void* handle) {
    inflate_stream_destroy((InflateStream*)handle);
}

// ============================================================================
// Gzip Header Exports
// ============================================================================

// TML: zlib_gzip_header_text(header_handle) -> Bool
bool zlib_gzip_header_text(void* header_handle) {
    GzipHeaderInfo* header = (GzipHeaderInfo*)header_handle;
    return header ? header->is_text : false;
}

// TML: zlib_gzip_header_os(header_handle) -> I32
int32_t zlib_gzip_header_os(void* header_handle) {
    GzipHeaderInfo* header = (GzipHeaderInfo*)header_handle;
    return header ? header->os : 255; // 255 = unknown
}

// TML: zlib_gzip_header_name(header_handle) -> Str
const char* zlib_gzip_header_name(void* header_handle) {
    GzipHeaderInfo* header = (GzipHeaderInfo*)header_handle;
    return (header && header->filename) ? header->filename : "";
}

// TML: zlib_gzip_header_comment(header_handle) -> Str
const char* zlib_gzip_header_comment(void* header_handle) {
    GzipHeaderInfo* header = (GzipHeaderInfo*)header_handle;
    return (header && header->comment) ? header->comment : "";
}

// TML: zlib_gzip_header_hcrc(header_handle) -> Bool
bool zlib_gzip_header_hcrc(void* header_handle) {
    (void)header_handle;
    return false; // Not stored in current implementation
}

// TML: zlib_gzip_header_time(header_handle) -> I64
int64_t zlib_gzip_header_time(void* header_handle) {
    GzipHeaderInfo* header = (GzipHeaderInfo*)header_handle;
    return header ? header->mtime : 0;
}

// ============================================================================
// Zstd Streaming Exports
// ============================================================================

// TML: zstd_cstream_create(level, strategy, window_log, checksum, content_size, nb_workers) ->
// *Unit
void* zstd_cstream_create(int32_t level, int32_t strategy, int32_t window_log, bool checksum,
                          bool content_size, int32_t nb_workers) {
    (void)strategy;
    (void)window_log;
    (void)content_size;
    (void)nb_workers;
    return (void*)zstd_compress_context_create(level, checksum);
}

// TML: zstd_cstream_create_with_dict(dict_handle, level) -> *Unit
void* zstd_cstream_create_with_dict(void* dict_handle, int32_t level) {
    (void)dict_handle;
    // TODO: implement dictionary-based streaming
    return (void*)zstd_compress_context_create(level, false);
}

// TML: zstd_cstream_process(handle, data, end_op) -> *Unit
void* zstd_cstream_process(void* handle, const char* data, int32_t end_op) {
    return (void*)zstd_compress_context_process((ZstdCompressContext*)handle, data, end_op);
}

// TML: zstd_cstream_process_buffer(handle, data_handle, end_op) -> *Unit
void* zstd_cstream_process_buffer(void* handle, void* data_handle, int32_t end_op) {
    return (void*)zstd_compress_context_process_buffer((ZstdCompressContext*)handle,
                                                       (TmlBuffer*)data_handle, end_op);
}

// TML: zstd_cstream_reset(handle) -> Bool
bool zstd_cstream_reset(void* handle) {
    // TODO: implement ZSTD_CCtx_reset
    return handle != NULL;
}

// TML: zstd_cstream_set_pledged_size(handle, size) -> Bool
bool zstd_cstream_set_pledged_size(void* handle, int64_t size) {
    (void)handle;
    (void)size;
    // TODO: implement ZSTD_CCtx_setPledgedSrcSize
    return handle != NULL;
}

// TML: zstd_cstream_destroy(handle)
void zstd_cstream_destroy(void* handle) {
    zstd_compress_context_destroy((ZstdCompressContext*)handle);
}

// TML: zstd_dstream_create(window_log) -> *Unit
void* zstd_dstream_create(int32_t window_log) {
    (void)window_log;
    return (void*)zstd_decompress_context_create();
}

// TML: zstd_dstream_create_with_dict(dict_handle) -> *Unit
void* zstd_dstream_create_with_dict(void* dict_handle) {
    (void)dict_handle;
    // TODO: implement dictionary-based streaming
    return (void*)zstd_decompress_context_create();
}

// TML: zstd_dstream_process(handle, data_handle) -> *Unit
void* zstd_dstream_process(void* handle, void* data_handle) {
    return (void*)zstd_decompress_context_process((ZstdDecompressContext*)handle,
                                                  (TmlBuffer*)data_handle);
}

// TML: zstd_dstream_reset(handle) -> Bool
bool zstd_dstream_reset(void* handle) {
    return handle != NULL;
}

// TML: zstd_dstream_content_size(handle) -> I64
int64_t zstd_dstream_content_size(void* handle) {
    (void)handle;
    return -1; // Unknown in streaming mode
}

// TML: zstd_dstream_get_error_code(handle) -> I32
int32_t zstd_dstream_get_error_code(void* handle) {
    (void)handle;
    return zstd_last_error_code();
}

// TML: zstd_dstream_destroy(handle)
void zstd_dstream_destroy(void* handle) {
    zstd_decompress_context_destroy((ZstdDecompressContext*)handle);
}

// ============================================================================
// Zstd Dictionary Exports
// ============================================================================

// TML: zstd_dict_create(data_handle) -> *Unit
// (already has correct signature in zlib_zstd.c)

// TML: zstd_dict_get_id(handle) -> I32
int32_t zstd_dict_get_id(void* handle) {
    return zstd_dict_id((ZstdDict*)handle);
}

// TML: zstd_dict_to_buffer(handle) -> *Unit
void* zstd_dict_to_buffer(void* handle) {
    return (void*)zstd_dict_export((ZstdDict*)handle);
}

// Debug function to verify runtime linking
void* zstd_dict_train_test(void) {
    fprintf(stderr, "[ZSTD] zstd_dict_train_test called!\n");
    fflush(stderr);
    return NULL;
}

// TML: zstd_dict_train(samples_handle, dict_size) -> *Unit
// The samples_handle is a TmlList* header. List[Buffer] stores Buffer values inline.
// Buffer = { handle: *Unit } is a single-field struct (8 bytes), so each element
// in the list data array IS the TmlBuffer* handle directly (no extra indirection).
void* zstd_dict_train(void* samples_handle, int64_t dict_size) {
    if (!samples_handle || dict_size <= 0)
        return NULL;

    TmlList* list = (TmlList*)samples_handle;
    if (!list->data || list->len == 0)
        return NULL;

    // List[Buffer] stores elements inline at stride=8. Each element is Buffer.handle
    // which is a TmlBuffer* directly — no pointer-to-struct indirection.
    void** data = (void**)list->data;
    size_t num_samples = (size_t)list->len;

    TmlBuffer** samples = (TmlBuffer**)malloc(num_samples * sizeof(TmlBuffer*));
    if (!samples)
        return NULL;

    for (size_t i = 0; i < num_samples; i++) {
        TmlBuffer* buf = (TmlBuffer*)data[i];
        if (!buf || !buf->data || buf->len == 0) {
            free(samples);
            return NULL;
        }
        samples[i] = buf;
    }

    ZstdDict* dict = zstd_dict_train_impl(samples, num_samples, (size_t)dict_size);

    free(samples);
    return (void*)dict;
}

// ============================================================================
// Zstd Utility Exports
// ============================================================================

// TML: zstd_get_frame_content_size(handle) -> I64
int64_t zstd_get_frame_content_size(void* handle) {
    return zstd_content_size((TmlBuffer*)handle);
}

// TML: zstd_get_decompress_bound(handle) -> I64
int64_t zstd_get_decompress_bound(void* handle) {
    return zstd_decompress_bound((TmlBuffer*)handle);
}

// TML: zstd_get_frame_dict_id(handle) -> I32
int32_t zstd_get_frame_dict_id(void* handle) {
    return zstd_frame_dict_id((TmlBuffer*)handle);
}

// Note: zstd_is_frame already has correct signature in zlib_zstd.c

// ============================================================================
// Gzip Compression Exports (direct mappings)
// ============================================================================

// The TML gzip functions map directly to gzip_compress/gzip_decompress
// which are already exported in zlib_deflate.c

// TML: gzip(data, level, window_bits, mem_level, strategy) -> *Unit
void* gzip(const char* data, int32_t level, int32_t window_bits, int32_t mem_level,
           int32_t strategy) {
    return (void*)gzip_compress(data, level, window_bits, mem_level, strategy);
}

// TML: gunzip(handle, window_bits) -> Str
char* gunzip(void* handle, int32_t window_bits) {
    return gzip_decompress((TmlBuffer*)handle, window_bits);
}

// TML: gzip_buffer(handle, level, window_bits, mem_level, strategy) -> *Unit
void* gzip_buffer(void* handle, int32_t level, int32_t window_bits, int32_t mem_level,
                  int32_t strategy) {
    return (void*)gzip_compress_buffer((TmlBuffer*)handle, level, window_bits, mem_level, strategy);
}

// TML: gunzip_buffer(handle, window_bits) -> *Unit
void* gunzip_buffer(void* handle, int32_t window_bits) {
    return (void*)gzip_decompress_buffer((TmlBuffer*)handle, window_bits);
}

// TML: read_gzip_header(handle) -> *Unit
void* read_gzip_header_wrapper(void* handle) {
    return (void*)gzip_read_header((TmlBuffer*)handle);
}

// TML: gzip_header_destroy(handle)
void gzip_header_destroy_wrapper(void* handle) {
    gzip_header_destroy((GzipHeaderInfo*)handle);
}

// ============================================================================
// Brotli Streaming Exports
// ============================================================================

// TML: brotli_encoder_create(quality, mode, lgwin, lgblock) -> *Unit
void* brotli_encoder_create(int32_t quality, int32_t mode, int32_t lgwin, int32_t lgblock) {
    return brotli_encoder_create_internal(quality, mode, lgwin, lgblock);
}

// TML: brotli_encoder_process(state, data, operation) -> *Unit
void* brotli_encoder_process(void* state, const char* data, int32_t operation) {
    return (void*)brotli_encoder_process_internal(state, data, operation);
}

// TML: brotli_encoder_process_buffer(state, data, operation) -> *Unit
void* brotli_encoder_process_buffer(void* state, void* data, int32_t operation) {
    return (void*)brotli_encoder_process_buffer_internal(state, (TmlBuffer*)data, operation);
}

// TML: brotli_encoder_is_finished(state) -> Bool
bool brotli_encoder_is_finished(void* state) {
    return brotli_encoder_is_finished_internal(state);
}

// TML: brotli_encoder_has_more_output(state) -> Bool
bool brotli_encoder_has_more_output(void* state) {
    return brotli_encoder_has_more_output_internal(state);
}

// TML: brotli_encoder_destroy(state)
void brotli_encoder_destroy(void* state) {
    brotli_encoder_destroy_internal(state);
}

// TML: brotli_decoder_create(large_window) -> *Unit
void* brotli_decoder_create(bool large_window) {
    return brotli_decoder_create_internal(large_window);
}

// TML: brotli_decoder_process(state, data) -> *Unit
void* brotli_decoder_process(void* state, void* data) {
    return (void*)brotli_decoder_process_internal(state, (TmlBuffer*)data);
}

// TML: brotli_decoder_is_finished(state) -> Bool
bool brotli_decoder_is_finished(void* state) {
    return brotli_decoder_is_finished_internal(state);
}

// TML: brotli_decoder_needs_more_input(state) -> Bool
bool brotli_decoder_needs_more_input(void* state) {
    return brotli_decoder_needs_more_input_internal(state);
}

// TML: brotli_decoder_has_more_output(state) -> Bool
bool brotli_decoder_has_more_output(void* state) {
    return brotli_decoder_has_more_output_internal(state);
}

// TML: brotli_decoder_get_error_code(state) -> I32
int32_t brotli_decoder_get_error_code(void* state) {
    return brotli_decoder_get_error_code_internal(state);
}

// TML: brotli_decoder_destroy(state)
void brotli_decoder_destroy(void* state) {
    brotli_decoder_destroy_internal(state);
}

// Note: buffer_destroy is provided by collections.c in the main runtime
// Do not define here to avoid duplicate symbol errors
