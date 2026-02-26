/**
 * TML Zlib Runtime - Zstd Implementation
 *
 * Implements Zstd compression using the zstd library.
 */

#include "zlib_internal.h"

#include <zdict.h>
#include <zstd.h>
#include <zstd_errors.h>

// Thread-local error code for zstd
#ifdef _WIN32
static __declspec(thread) int32_t g_zstd_last_error = 0;
#else
static __thread int32_t g_zstd_last_error = 0;
#endif

static void zstd_set_last_error(int32_t code) {
    g_zstd_last_error = code;
}

int32_t zstd_last_error_code(void) {
    return g_zstd_last_error;
}

int32_t zstd_get_error_code(TmlBuffer* buf) {
    (void)buf;
    return g_zstd_last_error;
}

// ============================================================================
// Zstd Compression
// ============================================================================

TmlBuffer* zstd_compress(const char* data, int32_t level) {
    if (!data) {
        zstd_set_last_error(-1);
        return NULL;
    }

    size_t input_len = strlen(data);

    // Clamp level to valid range
    if (level < ZSTD_minCLevel())
        level = ZSTD_defaultCLevel();
    if (level > ZSTD_maxCLevel())
        level = ZSTD_maxCLevel();

    // Calculate max compressed size
    size_t max_compressed = ZSTD_compressBound(input_len);

    TmlBuffer* output = tml_buffer_create(max_compressed);
    if (!output) {
        zstd_set_last_error(-2); // Memory error
        return NULL;
    }

    size_t compressed_size = ZSTD_compress(output->data, max_compressed, data, input_len, level);

    if (ZSTD_isError(compressed_size)) {
        tml_buffer_destroy(output);
        zstd_set_last_error((int32_t)ZSTD_getErrorCode(compressed_size));
        return NULL;
    }

    output->len = compressed_size;
    zstd_set_last_error(0);
    return output;
}

TmlBuffer* zstd_compress_buffer(TmlBuffer* data, int32_t level) {
    if (!data) {
        zstd_set_last_error(-1);
        return NULL;
    }

    if (level < ZSTD_minCLevel())
        level = ZSTD_defaultCLevel();
    if (level > ZSTD_maxCLevel())
        level = ZSTD_maxCLevel();

    size_t max_compressed = ZSTD_compressBound(data->len);

    TmlBuffer* output = tml_buffer_create(max_compressed);
    if (!output) {
        zstd_set_last_error(-2);
        return NULL;
    }

    size_t compressed_size =
        ZSTD_compress(output->data, max_compressed, data->data, data->len, level);

    if (ZSTD_isError(compressed_size)) {
        tml_buffer_destroy(output);
        zstd_set_last_error((int32_t)ZSTD_getErrorCode(compressed_size));
        return NULL;
    }

    output->len = compressed_size;
    zstd_set_last_error(0);
    return output;
}

TmlBuffer* zstd_compress_with_dict(const char* data, int32_t level, TmlBuffer* dict) {
    if (!data) {
        zstd_set_last_error(-1);
        return NULL;
    }

    size_t input_len = strlen(data);

    if (level < ZSTD_minCLevel())
        level = ZSTD_defaultCLevel();
    if (level > ZSTD_maxCLevel())
        level = ZSTD_maxCLevel();

    size_t max_compressed = ZSTD_compressBound(input_len);

    TmlBuffer* output = tml_buffer_create(max_compressed);
    if (!output) {
        zstd_set_last_error(-2);
        return NULL;
    }

    size_t compressed_size;
    if (dict && dict->len > 0) {
        compressed_size = ZSTD_compress_usingDict(ZSTD_createCCtx(), // TODO: reuse context
                                                  output->data, max_compressed, data, input_len,
                                                  dict->data, dict->len, level);
    } else {
        compressed_size = ZSTD_compress(output->data, max_compressed, data, input_len, level);
    }

    if (ZSTD_isError(compressed_size)) {
        tml_buffer_destroy(output);
        zstd_set_last_error((int32_t)ZSTD_getErrorCode(compressed_size));
        return NULL;
    }

    output->len = compressed_size;
    zstd_set_last_error(0);
    return output;
}

// ============================================================================
// Zstd Decompression
// ============================================================================

char* zstd_decompress(TmlBuffer* data) {
    if (!data || data->len == 0) {
        zstd_set_last_error(-1);
        return NULL;
    }

    // Get decompressed size if known
    unsigned long long decompressed_size = ZSTD_getFrameContentSize(data->data, data->len);

    size_t out_capacity;
    if (decompressed_size == ZSTD_CONTENTSIZE_UNKNOWN ||
        decompressed_size == ZSTD_CONTENTSIZE_ERROR) {
        // Unknown size, start with 4x estimate
        out_capacity = data->len * 4;
        if (out_capacity < 256)
            out_capacity = 256;
    } else {
        out_capacity = (size_t)decompressed_size + 1; // +1 for null terminator
    }

    uint8_t* output = (uint8_t*)malloc(out_capacity);
    if (!output) {
        zstd_set_last_error(-2);
        return NULL;
    }

    size_t result = ZSTD_decompress(output, out_capacity - 1, data->data, data->len);

    if (ZSTD_isError(result)) {
        // Try with streaming if simple decompress fails
        free(output);
        zstd_set_last_error((int32_t)ZSTD_getErrorCode(result));
        return NULL;
    }

    output[result] = '\0';
    zstd_set_last_error(0);
    return (char*)output;
}

TmlBuffer* zstd_decompress_buffer(TmlBuffer* data) {
    if (!data || data->len == 0) {
        zstd_set_last_error(-1);
        return NULL;
    }

    unsigned long long decompressed_size = ZSTD_getFrameContentSize(data->data, data->len);

    size_t out_capacity;
    if (decompressed_size == ZSTD_CONTENTSIZE_UNKNOWN ||
        decompressed_size == ZSTD_CONTENTSIZE_ERROR) {
        out_capacity = data->len * 4;
        if (out_capacity < 256)
            out_capacity = 256;
    } else {
        out_capacity = (size_t)decompressed_size;
    }

    TmlBuffer* output = tml_buffer_create(out_capacity);
    if (!output) {
        zstd_set_last_error(-2);
        return NULL;
    }

    size_t result = ZSTD_decompress(output->data, out_capacity, data->data, data->len);

    if (ZSTD_isError(result)) {
        tml_buffer_destroy(output);
        zstd_set_last_error((int32_t)ZSTD_getErrorCode(result));
        return NULL;
    }

    output->len = result;
    zstd_set_last_error(0);
    return output;
}

TmlBuffer* zstd_decompress_with_dict(TmlBuffer* data, TmlBuffer* dict) {
    if (!data || data->len == 0) {
        zstd_set_last_error(-1);
        return NULL;
    }

    unsigned long long decompressed_size = ZSTD_getFrameContentSize(data->data, data->len);

    size_t out_capacity;
    if (decompressed_size == ZSTD_CONTENTSIZE_UNKNOWN ||
        decompressed_size == ZSTD_CONTENTSIZE_ERROR) {
        out_capacity = data->len * 4;
        if (out_capacity < 256)
            out_capacity = 256;
    } else {
        out_capacity = (size_t)decompressed_size;
    }

    TmlBuffer* output = tml_buffer_create(out_capacity);
    if (!output) {
        zstd_set_last_error(-2);
        return NULL;
    }

    size_t result;
    if (dict && dict->len > 0) {
        result = ZSTD_decompress_usingDict(ZSTD_createDCtx(), // TODO: reuse context
                                           output->data, out_capacity, data->data, data->len,
                                           dict->data, dict->len);
    } else {
        result = ZSTD_decompress(output->data, out_capacity, data->data, data->len);
    }

    if (ZSTD_isError(result)) {
        tml_buffer_destroy(output);
        zstd_set_last_error((int32_t)ZSTD_getErrorCode(result));
        return NULL;
    }

    output->len = result;
    zstd_set_last_error(0);
    return output;
}

// ============================================================================
// Zstd Utilities
// ============================================================================

int64_t zstd_content_size(TmlBuffer* data) {
    if (!data || data->len == 0)
        return -1;

    unsigned long long size = ZSTD_getFrameContentSize(data->data, data->len);
    if (size == ZSTD_CONTENTSIZE_UNKNOWN || size == ZSTD_CONTENTSIZE_ERROR) {
        return -1;
    }
    return (int64_t)size;
}

int64_t zstd_decompress_bound(TmlBuffer* data) {
    if (!data || data->len == 0)
        return -1;

    unsigned long long bound = ZSTD_getFrameContentSize(data->data, data->len);
    if (bound == ZSTD_CONTENTSIZE_UNKNOWN || bound == ZSTD_CONTENTSIZE_ERROR) {
        // Unknown or error, return estimate
        return (int64_t)(data->len * 4);
    }
    return (int64_t)bound;
}

int32_t zstd_frame_dict_id(TmlBuffer* data) {
    if (!data || data->len == 0)
        return 0;
    return (int32_t)ZSTD_getDictID_fromFrame(data->data, data->len);
}

bool zstd_is_frame(TmlBuffer* data) {
    if (!data || data->len < 4)
        return false;
    // Check for zstd magic number: 0xFD2FB528
    return data->data[0] == 0x28 && data->data[1] == 0xB5 && data->data[2] == 0x2F &&
           data->data[3] == 0xFD;
}

int32_t zstd_min_level(void) {
    return (int32_t)ZSTD_minCLevel();
}

int32_t zstd_max_level(void) {
    return (int32_t)ZSTD_maxCLevel();
}

int32_t zstd_default_level(void) {
    return (int32_t)ZSTD_defaultCLevel();
}

// ============================================================================
// Zstd Streaming Compressor
// ============================================================================

struct ZstdCompressContext {
    ZSTD_CStream* stream;
    bool checksum;
};

ZstdCompressContext* zstd_compress_context_create(int32_t level, bool checksum) {
    ZstdCompressContext* ctx = (ZstdCompressContext*)calloc(1, sizeof(ZstdCompressContext));
    if (!ctx)
        return NULL;

    ctx->stream = ZSTD_createCStream();
    if (!ctx->stream) {
        free(ctx);
        return NULL;
    }

    if (level < ZSTD_minCLevel())
        level = ZSTD_defaultCLevel();
    if (level > ZSTD_maxCLevel())
        level = ZSTD_maxCLevel();

    size_t init_result = ZSTD_initCStream(ctx->stream, level);
    if (ZSTD_isError(init_result)) {
        ZSTD_freeCStream(ctx->stream);
        free(ctx);
        return NULL;
    }

    if (checksum) {
        ZSTD_CCtx_setParameter(ctx->stream, ZSTD_c_checksumFlag, 1);
    }

    ctx->checksum = checksum;
    return ctx;
}

TmlBuffer* zstd_compress_context_process(ZstdCompressContext* ctx, const char* data,
                                         int32_t operation) {
    if (!ctx || !ctx->stream)
        return NULL;

    size_t input_len = data ? strlen(data) : 0;

    ZSTD_inBuffer input = {.src = data, .size = input_len, .pos = 0};

    size_t out_capacity = ZSTD_CStreamOutSize();
    if (input_len > 0) {
        size_t bound = ZSTD_compressBound(input_len);
        if (bound > out_capacity)
            out_capacity = bound;
    }

    TmlBuffer* output = tml_buffer_create(out_capacity);
    if (!output)
        return NULL;

    ZSTD_outBuffer out_buf = {.dst = output->data, .size = out_capacity, .pos = 0};

    ZSTD_EndDirective mode;
    switch (operation) {
    case 0:
        mode = ZSTD_e_continue;
        break;
    case 1:
        mode = ZSTD_e_flush;
        break;
    case 2:
        mode = ZSTD_e_end;
        break;
    default:
        mode = ZSTD_e_continue;
        break;
    }

    size_t remaining;
    do {
        remaining = ZSTD_compressStream2(ctx->stream, &out_buf, &input, mode);
        if (ZSTD_isError(remaining)) {
            tml_buffer_destroy(output);
            zstd_set_last_error((int32_t)ZSTD_getErrorCode(remaining));
            return NULL;
        }

        // Expand buffer if needed
        if (out_buf.pos == out_buf.size && remaining > 0) {
            size_t new_cap = output->capacity * 2;
            uint8_t* new_data = (uint8_t*)realloc(output->data, new_cap);
            if (!new_data) {
                tml_buffer_destroy(output);
                return NULL;
            }
            output->data = new_data;
            output->capacity = new_cap;
            out_buf.dst = output->data;
            out_buf.size = new_cap;
        }
    } while (remaining > 0);

    output->len = out_buf.pos;
    return output;
}

TmlBuffer* zstd_compress_context_process_buffer(ZstdCompressContext* ctx, TmlBuffer* data,
                                                int32_t operation) {
    if (!ctx || !ctx->stream || !data)
        return NULL;

    ZSTD_inBuffer input = {.src = data->data, .size = data->len, .pos = 0};

    size_t out_capacity = ZSTD_CStreamOutSize();
    size_t bound = ZSTD_compressBound(data->len);
    if (bound > out_capacity)
        out_capacity = bound;

    TmlBuffer* output = tml_buffer_create(out_capacity);
    if (!output)
        return NULL;

    ZSTD_outBuffer out_buf = {.dst = output->data, .size = out_capacity, .pos = 0};

    ZSTD_EndDirective mode;
    switch (operation) {
    case 0:
        mode = ZSTD_e_continue;
        break;
    case 1:
        mode = ZSTD_e_flush;
        break;
    case 2:
        mode = ZSTD_e_end;
        break;
    default:
        mode = ZSTD_e_continue;
        break;
    }

    size_t remaining;
    do {
        remaining = ZSTD_compressStream2(ctx->stream, &out_buf, &input, mode);
        if (ZSTD_isError(remaining)) {
            tml_buffer_destroy(output);
            zstd_set_last_error((int32_t)ZSTD_getErrorCode(remaining));
            return NULL;
        }

        if (out_buf.pos == out_buf.size && remaining > 0) {
            size_t new_cap = output->capacity * 2;
            uint8_t* new_data = (uint8_t*)realloc(output->data, new_cap);
            if (!new_data) {
                tml_buffer_destroy(output);
                return NULL;
            }
            output->data = new_data;
            output->capacity = new_cap;
            out_buf.dst = output->data;
            out_buf.size = new_cap;
        }
    } while (remaining > 0);

    output->len = out_buf.pos;
    return output;
}

void zstd_compress_context_destroy(ZstdCompressContext* ctx) {
    if (ctx) {
        if (ctx->stream) {
            ZSTD_freeCStream(ctx->stream);
        }
        free(ctx);
    }
}

// ============================================================================
// Zstd Streaming Decompressor
// ============================================================================

struct ZstdDecompressContext {
    ZSTD_DStream* stream;
};

ZstdDecompressContext* zstd_decompress_context_create(void) {
    ZstdDecompressContext* ctx = (ZstdDecompressContext*)calloc(1, sizeof(ZstdDecompressContext));
    if (!ctx)
        return NULL;

    ctx->stream = ZSTD_createDStream();
    if (!ctx->stream) {
        free(ctx);
        return NULL;
    }

    size_t init_result = ZSTD_initDStream(ctx->stream);
    if (ZSTD_isError(init_result)) {
        ZSTD_freeDStream(ctx->stream);
        free(ctx);
        return NULL;
    }

    return ctx;
}

TmlBuffer* zstd_decompress_context_process(ZstdDecompressContext* ctx, TmlBuffer* data) {
    if (!ctx || !ctx->stream || !data)
        return NULL;

    ZSTD_inBuffer input = {.src = data->data, .size = data->len, .pos = 0};

    size_t out_capacity = ZSTD_DStreamOutSize();
    if (data->len * 4 > out_capacity)
        out_capacity = data->len * 4;

    TmlBuffer* output = tml_buffer_create(out_capacity);
    if (!output)
        return NULL;

    ZSTD_outBuffer out_buf = {.dst = output->data, .size = out_capacity, .pos = 0};

    while (input.pos < input.size) {
        size_t result = ZSTD_decompressStream(ctx->stream, &out_buf, &input);
        if (ZSTD_isError(result)) {
            tml_buffer_destroy(output);
            zstd_set_last_error((int32_t)ZSTD_getErrorCode(result));
            return NULL;
        }

        // Expand buffer if needed
        if (out_buf.pos == out_buf.size) {
            size_t new_cap = output->capacity * 2;
            uint8_t* new_data = (uint8_t*)realloc(output->data, new_cap);
            if (!new_data) {
                tml_buffer_destroy(output);
                return NULL;
            }
            output->data = new_data;
            output->capacity = new_cap;
            out_buf.dst = output->data;
            out_buf.size = new_cap;
        }
    }

    output->len = out_buf.pos;
    return output;
}

void zstd_decompress_context_destroy(ZstdDecompressContext* ctx) {
    if (ctx) {
        if (ctx->stream) {
            ZSTD_freeDStream(ctx->stream);
        }
        free(ctx);
    }
}

// ============================================================================
// Zstd Dictionary
// ============================================================================

struct ZstdDict {
    ZSTD_CDict* cdict;
    ZSTD_DDict* ddict;
    uint32_t dict_id;
    uint8_t* raw_data;     // Store raw dictionary data for to_buffer()
    size_t raw_data_len;
};

ZstdDict* zstd_dict_create(TmlBuffer* data) {
    if (!data || data->len == 0)
        return NULL;

    ZstdDict* dict = (ZstdDict*)calloc(1, sizeof(ZstdDict));
    if (!dict)
        return NULL;

    // Store a copy of the raw dictionary data for to_buffer()
    dict->raw_data = (uint8_t*)malloc(data->len);
    if (!dict->raw_data) {
        free(dict);
        return NULL;
    }
    memcpy(dict->raw_data, data->data, data->len);
    dict->raw_data_len = data->len;

    dict->cdict = ZSTD_createCDict(data->data, data->len, ZSTD_defaultCLevel());
    dict->ddict = ZSTD_createDDict(data->data, data->len);
    dict->dict_id = ZSTD_getDictID_fromDict(data->data, data->len);

    if (!dict->cdict || !dict->ddict) {
        if (dict->cdict)
            ZSTD_freeCDict(dict->cdict);
        if (dict->ddict)
            ZSTD_freeDDict(dict->ddict);
        free(dict->raw_data);
        free(dict);
        return NULL;
    }

    return dict;
}

TmlBuffer* zstd_dict_export(ZstdDict* dict) {
    if (!dict || !dict->raw_data || dict->raw_data_len == 0)
        return NULL;

    TmlBuffer* buf = tml_buffer_create(dict->raw_data_len);
    if (!buf)
        return NULL;

    memcpy(buf->data, dict->raw_data, dict->raw_data_len);
    buf->len = dict->raw_data_len;
    return buf;
}

ZstdDict* zstd_dict_train_impl(TmlBuffer** samples, size_t num_samples, size_t dict_size) {
    if (!samples || num_samples == 0 || dict_size == 0)
        return NULL;

    // Calculate total samples size and build sizes array
    size_t* sample_sizes = (size_t*)malloc(num_samples * sizeof(size_t));
    if (!sample_sizes)
        return NULL;

    size_t total_size = 0;
    for (size_t i = 0; i < num_samples; i++) {
        sample_sizes[i] = samples[i] ? samples[i]->len : 0;
        total_size += sample_sizes[i];
    }

    // Concatenate all samples
    uint8_t* all_samples = (uint8_t*)malloc(total_size);
    if (!all_samples) {
        free(sample_sizes);
        return NULL;
    }

    size_t offset = 0;
    for (size_t i = 0; i < num_samples; i++) {
        if (samples[i] && samples[i]->len > 0) {
            memcpy(all_samples + offset, samples[i]->data, samples[i]->len);
            offset += samples[i]->len;
        }
    }

    // Create dictionary buffer
    TmlBuffer* dict_buf = tml_buffer_create(dict_size);
    if (!dict_buf) {
        free(all_samples);
        free(sample_sizes);
        return NULL;
    }

    // Train the dictionary
    size_t result = ZDICT_trainFromBuffer(dict_buf->data, dict_size, all_samples, sample_sizes,
                                          (unsigned)num_samples);

    free(all_samples);
    free(sample_sizes);

    if (ZDICT_isError(result)) {
        tml_buffer_destroy(dict_buf);
        return NULL;
    }

    dict_buf->len = result;

    // Create ZstdDict from trained data
    ZstdDict* dict = zstd_dict_create(dict_buf);
    tml_buffer_destroy(dict_buf);

    return dict;
}

int32_t zstd_dict_id(ZstdDict* dict) {
    return dict ? (int32_t)dict->dict_id : 0;
}

void zstd_dict_destroy(ZstdDict* dict) {
    if (dict) {
        if (dict->cdict)
            ZSTD_freeCDict(dict->cdict);
        if (dict->ddict)
            ZSTD_freeDDict(dict->ddict);
        if (dict->raw_data)
            free(dict->raw_data);
        free(dict);
    }
}
