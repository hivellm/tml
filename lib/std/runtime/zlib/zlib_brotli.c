/**
 * TML Zlib Runtime - Brotli Implementation
 *
 * Implements Brotli compression using the brotli library.
 */

#include "zlib_internal.h"
#include <brotli/encode.h>
#include <brotli/decode.h>

// Thread-local error code for brotli
#ifdef _WIN32
    static __declspec(thread) int32_t g_brotli_last_error = 0;
#else
    static __thread int32_t g_brotli_last_error = 0;
#endif

static void brotli_set_last_error(int32_t code) {
    g_brotli_last_error = code;
}

int32_t brotli_last_error_code(void) {
    return g_brotli_last_error;
}

int32_t brotli_get_error_code(TmlBuffer* buf) {
    (void)buf;
    return g_brotli_last_error;
}

// ============================================================================
// Brotli Compression
// ============================================================================

TmlBuffer* brotli_compress(const char* data, int32_t quality, int32_t mode,
                           int32_t lgwin, int32_t lgblock, int64_t size_hint) {
    if (!data) {
        brotli_set_last_error(BROTLI_DECODER_ERROR_INVALID_ARGUMENTS);
        return NULL;
    }

    size_t input_len = strlen(data);

    // Clamp parameters to valid ranges
    if (quality < BROTLI_MIN_QUALITY) quality = BROTLI_MIN_QUALITY;
    if (quality > BROTLI_MAX_QUALITY) quality = BROTLI_MAX_QUALITY;
    if (mode < BROTLI_MODE_GENERIC) mode = BROTLI_MODE_GENERIC;
    if (mode > BROTLI_MODE_FONT) mode = BROTLI_MODE_GENERIC;
    if (lgwin < BROTLI_MIN_WINDOW_BITS) lgwin = BROTLI_DEFAULT_WINDOW;
    if (lgwin > BROTLI_MAX_WINDOW_BITS) lgwin = BROTLI_MAX_WINDOW_BITS;

    // Calculate max compressed size
    size_t max_compressed = BrotliEncoderMaxCompressedSize(input_len);
    if (max_compressed == 0) {
        max_compressed = input_len + 1024;  // Fallback estimate
    }

    TmlBuffer* output = tml_buffer_create(max_compressed);
    if (!output) {
        brotli_set_last_error(BROTLI_DECODER_ERROR_ALLOC_CONTEXT_MODE_MEMORY);
        return NULL;
    }

    size_t encoded_size = max_compressed;
    BROTLI_BOOL success = BrotliEncoderCompress(
        quality,
        lgwin,
        (BrotliEncoderMode)mode,
        input_len,
        (const uint8_t*)data,
        &encoded_size,
        output->data
    );

    if (!success) {
        tml_buffer_destroy(output);
        brotli_set_last_error(1);  // Generic compression error
        return NULL;
    }

    output->len = encoded_size;
    brotli_set_last_error(0);
    return output;
}

TmlBuffer* brotli_compress_buffer(TmlBuffer* data, int32_t quality, int32_t mode,
                                  int32_t lgwin, int32_t lgblock, int64_t size_hint) {
    if (!data) {
        brotli_set_last_error(BROTLI_DECODER_ERROR_INVALID_ARGUMENTS);
        return NULL;
    }

    if (quality < BROTLI_MIN_QUALITY) quality = BROTLI_MIN_QUALITY;
    if (quality > BROTLI_MAX_QUALITY) quality = BROTLI_MAX_QUALITY;
    if (mode < BROTLI_MODE_GENERIC) mode = BROTLI_MODE_GENERIC;
    if (mode > BROTLI_MODE_FONT) mode = BROTLI_MODE_GENERIC;
    if (lgwin < BROTLI_MIN_WINDOW_BITS) lgwin = BROTLI_DEFAULT_WINDOW;
    if (lgwin > BROTLI_MAX_WINDOW_BITS) lgwin = BROTLI_MAX_WINDOW_BITS;

    size_t max_compressed = BrotliEncoderMaxCompressedSize(data->len);
    if (max_compressed == 0) {
        max_compressed = data->len + 1024;
    }

    TmlBuffer* output = tml_buffer_create(max_compressed);
    if (!output) {
        brotli_set_last_error(BROTLI_DECODER_ERROR_ALLOC_CONTEXT_MODE_MEMORY);
        return NULL;
    }

    size_t encoded_size = max_compressed;
    BROTLI_BOOL success = BrotliEncoderCompress(
        quality,
        lgwin,
        (BrotliEncoderMode)mode,
        data->len,
        data->data,
        &encoded_size,
        output->data
    );

    if (!success) {
        tml_buffer_destroy(output);
        brotli_set_last_error(1);
        return NULL;
    }

    output->len = encoded_size;
    brotli_set_last_error(0);
    return output;
}

// ============================================================================
// Brotli Decompression
// ============================================================================

char* brotli_decompress(TmlBuffer* data, bool large_window) {
    if (!data || data->len == 0) {
        brotli_set_last_error(BROTLI_DECODER_ERROR_INVALID_ARGUMENTS);
        return NULL;
    }

    // Start with 4x estimate
    size_t out_capacity = data->len * 4;
    if (out_capacity < 256) out_capacity = 256;

    uint8_t* output = (uint8_t*)malloc(out_capacity);
    if (!output) {
        brotli_set_last_error(BROTLI_DECODER_ERROR_ALLOC_CONTEXT_MODE_MEMORY);
        return NULL;
    }

    size_t decoded_size = out_capacity - 1;  // Leave room for null terminator
    BrotliDecoderResult result = BrotliDecoderDecompress(
        data->len,
        data->data,
        &decoded_size,
        output
    );

    // If buffer too small, retry with larger buffer
    while (result == BROTLI_DECODER_RESULT_NEEDS_MORE_OUTPUT) {
        out_capacity *= 2;
        uint8_t* new_output = (uint8_t*)realloc(output, out_capacity);
        if (!new_output) {
            free(output);
            brotli_set_last_error(BROTLI_DECODER_ERROR_ALLOC_CONTEXT_MODE_MEMORY);
            return NULL;
        }
        output = new_output;
        decoded_size = out_capacity - 1;

        result = BrotliDecoderDecompress(
            data->len,
            data->data,
            &decoded_size,
            output
        );
    }

    if (result != BROTLI_DECODER_RESULT_SUCCESS) {
        free(output);
        brotli_set_last_error((int32_t)result);
        return NULL;
    }

    output[decoded_size] = '\0';
    brotli_set_last_error(0);
    return (char*)output;
}

TmlBuffer* brotli_decompress_buffer(TmlBuffer* data, bool large_window) {
    if (!data || data->len == 0) {
        brotli_set_last_error(BROTLI_DECODER_ERROR_INVALID_ARGUMENTS);
        return NULL;
    }

    size_t out_capacity = data->len * 4;
    if (out_capacity < 256) out_capacity = 256;

    TmlBuffer* output = tml_buffer_create(out_capacity);
    if (!output) {
        brotli_set_last_error(BROTLI_DECODER_ERROR_ALLOC_CONTEXT_MODE_MEMORY);
        return NULL;
    }

    size_t decoded_size = out_capacity;
    BrotliDecoderResult result = BrotliDecoderDecompress(
        data->len,
        data->data,
        &decoded_size,
        output->data
    );

    while (result == BROTLI_DECODER_RESULT_NEEDS_MORE_OUTPUT) {
        size_t new_cap = output->capacity * 2;
        uint8_t* new_data = (uint8_t*)realloc(output->data, new_cap);
        if (!new_data) {
            tml_buffer_destroy(output);
            brotli_set_last_error(BROTLI_DECODER_ERROR_ALLOC_CONTEXT_MODE_MEMORY);
            return NULL;
        }
        output->data = new_data;
        output->capacity = new_cap;
        decoded_size = new_cap;

        result = BrotliDecoderDecompress(
            data->len,
            data->data,
            &decoded_size,
            output->data
        );
    }

    if (result != BROTLI_DECODER_RESULT_SUCCESS) {
        tml_buffer_destroy(output);
        brotli_set_last_error((int32_t)result);
        return NULL;
    }

    output->len = decoded_size;
    brotli_set_last_error(0);
    return output;
}

// ============================================================================
// Brotli Streaming Encoder
// ============================================================================

struct BrotliEncoderState {
    BrotliEncoderState* encoder;
};

BrotliEncoderState* brotli_encoder_create(int32_t quality, int32_t mode,
                                          int32_t lgwin, int32_t lgblock) {
    BrotliEncoderState* state = BrotliEncoderCreateInstance(NULL, NULL, NULL);
    if (!state) return NULL;

    BrotliEncoderSetParameter(state, BROTLI_PARAM_QUALITY, quality);
    BrotliEncoderSetParameter(state, BROTLI_PARAM_MODE, mode);
    BrotliEncoderSetParameter(state, BROTLI_PARAM_LGWIN, lgwin);
    if (lgblock > 0) {
        BrotliEncoderSetParameter(state, BROTLI_PARAM_LGBLOCK, lgblock);
    }

    return state;
}

TmlBuffer* brotli_encoder_process(BrotliEncoderState* state, const char* data, int32_t operation) {
    if (!state) return NULL;

    size_t available_in = data ? strlen(data) : 0;
    const uint8_t* next_in = (const uint8_t*)data;

    size_t out_capacity = available_in > 0 ? BrotliEncoderMaxCompressedSize(available_in) : 1024;
    if (out_capacity < 64) out_capacity = 64;

    TmlBuffer* output = tml_buffer_create(out_capacity);
    if (!output) return NULL;

    size_t available_out = out_capacity;
    uint8_t* next_out = output->data;

    BrotliEncoderOperation op;
    switch (operation) {
        case 0: op = BROTLI_OPERATION_PROCESS; break;
        case 1: op = BROTLI_OPERATION_FLUSH; break;
        case 2: op = BROTLI_OPERATION_FINISH; break;
        default: op = BROTLI_OPERATION_PROCESS; break;
    }

    while (available_in > 0 || BrotliEncoderHasMoreOutput(state)) {
        if (available_out == 0) {
            // Expand output buffer
            size_t used = next_out - output->data;
            size_t new_cap = output->capacity * 2;
            uint8_t* new_data = (uint8_t*)realloc(output->data, new_cap);
            if (!new_data) {
                tml_buffer_destroy(output);
                return NULL;
            }
            output->data = new_data;
            output->capacity = new_cap;
            next_out = output->data + used;
            available_out = new_cap - used;
        }

        if (!BrotliEncoderCompressStream(state, op, &available_in, &next_in, &available_out, &next_out, NULL)) {
            tml_buffer_destroy(output);
            return NULL;
        }
    }

    output->len = next_out - output->data;
    return output;
}

TmlBuffer* brotli_encoder_process_buffer(BrotliEncoderState* state, TmlBuffer* data, int32_t operation) {
    if (!state || !data) return NULL;

    size_t available_in = data->len;
    const uint8_t* next_in = data->data;

    size_t out_capacity = available_in > 0 ? BrotliEncoderMaxCompressedSize(available_in) : 1024;
    if (out_capacity < 64) out_capacity = 64;

    TmlBuffer* output = tml_buffer_create(out_capacity);
    if (!output) return NULL;

    size_t available_out = out_capacity;
    uint8_t* next_out = output->data;

    BrotliEncoderOperation op;
    switch (operation) {
        case 0: op = BROTLI_OPERATION_PROCESS; break;
        case 1: op = BROTLI_OPERATION_FLUSH; break;
        case 2: op = BROTLI_OPERATION_FINISH; break;
        default: op = BROTLI_OPERATION_PROCESS; break;
    }

    while (available_in > 0 || BrotliEncoderHasMoreOutput(state)) {
        if (available_out == 0) {
            size_t used = next_out - output->data;
            size_t new_cap = output->capacity * 2;
            uint8_t* new_data = (uint8_t*)realloc(output->data, new_cap);
            if (!new_data) {
                tml_buffer_destroy(output);
                return NULL;
            }
            output->data = new_data;
            output->capacity = new_cap;
            next_out = output->data + used;
            available_out = new_cap - used;
        }

        if (!BrotliEncoderCompressStream(state, op, &available_in, &next_in, &available_out, &next_out, NULL)) {
            tml_buffer_destroy(output);
            return NULL;
        }
    }

    output->len = next_out - output->data;
    return output;
}

bool brotli_encoder_is_finished(BrotliEncoderState* state) {
    return state ? BrotliEncoderIsFinished(state) : true;
}

bool brotli_encoder_has_more_output(BrotliEncoderState* state) {
    return state ? BrotliEncoderHasMoreOutput(state) : false;
}

void brotli_encoder_destroy(BrotliEncoderState* state) {
    if (state) {
        BrotliEncoderDestroyInstance(state);
    }
}

// ============================================================================
// Brotli Streaming Decoder
// ============================================================================

struct BrotliDecoderState {
    BrotliDecoderState* decoder;
};

BrotliDecoderState* brotli_decoder_create(bool large_window) {
    BrotliDecoderState* state = BrotliDecoderCreateInstance(NULL, NULL, NULL);
    if (state && large_window) {
        BrotliDecoderSetParameter(state, BROTLI_DECODER_PARAM_LARGE_WINDOW, 1);
    }
    return state;
}

TmlBuffer* brotli_decoder_process(BrotliDecoderState* state, TmlBuffer* data) {
    if (!state || !data) return NULL;

    size_t available_in = data->len;
    const uint8_t* next_in = data->data;

    size_t out_capacity = data->len * 4;
    if (out_capacity < 256) out_capacity = 256;

    TmlBuffer* output = tml_buffer_create(out_capacity);
    if (!output) return NULL;

    size_t available_out = out_capacity;
    uint8_t* next_out = output->data;

    BrotliDecoderResult result;
    do {
        result = BrotliDecoderDecompressStream(state, &available_in, &next_in, &available_out, &next_out, NULL);

        if (result == BROTLI_DECODER_RESULT_NEEDS_MORE_OUTPUT) {
            size_t used = next_out - output->data;
            size_t new_cap = output->capacity * 2;
            uint8_t* new_data = (uint8_t*)realloc(output->data, new_cap);
            if (!new_data) {
                tml_buffer_destroy(output);
                brotli_set_last_error(BROTLI_DECODER_ERROR_ALLOC_CONTEXT_MODE_MEMORY);
                return NULL;
            }
            output->data = new_data;
            output->capacity = new_cap;
            next_out = output->data + used;
            available_out = new_cap - used;
        }
    } while (result == BROTLI_DECODER_RESULT_NEEDS_MORE_OUTPUT);

    if (result == BROTLI_DECODER_RESULT_ERROR) {
        tml_buffer_destroy(output);
        brotli_set_last_error((int32_t)BrotliDecoderGetErrorCode(state));
        return NULL;
    }

    output->len = next_out - output->data;
    return output;
}

bool brotli_decoder_is_finished(BrotliDecoderState* state) {
    return state ? BrotliDecoderIsFinished(state) : true;
}

bool brotli_decoder_needs_more_input(BrotliDecoderState* state) {
    return state ? !BrotliDecoderIsFinished(state) && !BrotliDecoderHasMoreOutput(state) : false;
}

bool brotli_decoder_has_more_output(BrotliDecoderState* state) {
    return state ? BrotliDecoderHasMoreOutput(state) : false;
}

int32_t brotli_decoder_get_error_code(BrotliDecoderState* state) {
    return state ? (int32_t)BrotliDecoderGetErrorCode(state) : 0;
}

void brotli_decoder_destroy(BrotliDecoderState* state) {
    if (state) {
        BrotliDecoderDestroyInstance(state);
    }
}
