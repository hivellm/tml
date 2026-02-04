/**
 * TML Zlib Runtime - Deflate/Inflate Implementation
 *
 * Implements zlib compression using the system zlib library.
 */

#include "zlib_internal.h"

#include <zlib.h>

// Thread-local error code
#ifdef _WIN32
#include <windows.h>
static __declspec(thread) int32_t g_last_error = 0;
#else
static __thread int32_t g_last_error = 0;
#endif

// ============================================================================
// Error Handling
// ============================================================================

void zlib_set_last_error(int32_t code) {
    g_last_error = code;
}

int32_t zlib_get_last_error(void) {
    return g_last_error;
}

int32_t zlib_last_error_code(void) {
    return g_last_error;
}

int32_t zlib_get_error_code(TmlBuffer* buf) {
    (void)buf; // Buffer may store error code in future
    return g_last_error;
}

// ============================================================================
// Buffer Operations
// ============================================================================

TmlBuffer* tml_buffer_create(size_t capacity) {
    TmlBuffer* buf = (TmlBuffer*)malloc(sizeof(TmlBuffer));
    if (!buf)
        return NULL;

    buf->data = (uint8_t*)malloc(capacity);
    if (!buf->data) {
        free(buf);
        return NULL;
    }

    buf->len = 0;
    buf->capacity = capacity;
    buf->read_pos = 0;
    return buf;
}

TmlBuffer* tml_buffer_from_data(const uint8_t* data, size_t len) {
    TmlBuffer* buf = tml_buffer_create(len);
    if (!buf)
        return NULL;

    memcpy(buf->data, data, len);
    buf->len = len;
    return buf;
}

TmlBuffer* tml_buffer_from_string(const char* str) {
    size_t len = strlen(str);
    return tml_buffer_from_data((const uint8_t*)str, len);
}

void tml_buffer_destroy(TmlBuffer* buf) {
    if (buf) {
        free(buf->data);
        free(buf);
    }
}

// Note: buffer_destroy for TML is exported from zlib_exports.c

static void tml_buffer_ensure_capacity(TmlBuffer* buf, size_t additional) {
    size_t needed = buf->len + additional;
    if (needed <= buf->capacity)
        return;

    size_t new_cap = buf->capacity * 2;
    if (new_cap < needed)
        new_cap = needed;

    uint8_t* new_data = (uint8_t*)realloc(buf->data, new_cap);
    if (new_data) {
        buf->data = new_data;
        buf->capacity = new_cap;
    }
}

void tml_buffer_write(TmlBuffer* buf, const uint8_t* data, size_t len) {
    tml_buffer_ensure_capacity(buf, len);
    memcpy(buf->data + buf->len, data, len);
    buf->len += len;
}

// ============================================================================
// Deflate Compression
// ============================================================================

TmlBuffer* zlib_deflate(const char* data, int32_t level, int32_t window_bits, int32_t mem_level,
                        int32_t strategy) {
    size_t input_len = strlen(data);

    // Initialize zlib stream
    z_stream strm;
    memset(&strm, 0, sizeof(strm));

    int ret = deflateInit2(&strm, level, Z_DEFLATED, window_bits, mem_level, strategy);
    if (ret != Z_OK) {
        zlib_set_last_error(ret);
        return NULL;
    }

    // Allocate output buffer (worst case: slightly larger than input)
    size_t out_capacity = deflateBound(&strm, (uLong)input_len);
    TmlBuffer* output = tml_buffer_create(out_capacity);
    if (!output) {
        deflateEnd(&strm);
        zlib_set_last_error(Z_MEM_ERROR);
        return NULL;
    }

    // Set input
    strm.next_in = (Bytef*)data;
    strm.avail_in = (uInt)input_len;

    // Set output
    strm.next_out = output->data;
    strm.avail_out = (uInt)out_capacity;

    // Compress in one shot
    ret = deflate(&strm, Z_FINISH);
    if (ret != Z_STREAM_END) {
        deflateEnd(&strm);
        tml_buffer_destroy(output);
        zlib_set_last_error(ret == Z_OK ? Z_BUF_ERROR : ret);
        return NULL;
    }

    output->len = strm.total_out;
    deflateEnd(&strm);
    zlib_set_last_error(Z_OK);
    return output;
}

TmlBuffer* zlib_deflate_buffer(TmlBuffer* data, int32_t level, int32_t window_bits,
                               int32_t mem_level, int32_t strategy) {
    if (!data) {
        zlib_set_last_error(Z_STREAM_ERROR);
        return NULL;
    }

    z_stream strm;
    memset(&strm, 0, sizeof(strm));

    int ret = deflateInit2(&strm, level, Z_DEFLATED, window_bits, mem_level, strategy);
    if (ret != Z_OK) {
        zlib_set_last_error(ret);
        return NULL;
    }

    size_t out_capacity = deflateBound(&strm, (uLong)data->len);
    TmlBuffer* output = tml_buffer_create(out_capacity);
    if (!output) {
        deflateEnd(&strm);
        zlib_set_last_error(Z_MEM_ERROR);
        return NULL;
    }

    strm.next_in = data->data;
    strm.avail_in = (uInt)data->len;
    strm.next_out = output->data;
    strm.avail_out = (uInt)out_capacity;

    ret = deflate(&strm, Z_FINISH);
    if (ret != Z_STREAM_END) {
        deflateEnd(&strm);
        tml_buffer_destroy(output);
        zlib_set_last_error(ret == Z_OK ? Z_BUF_ERROR : ret);
        return NULL;
    }

    output->len = strm.total_out;
    deflateEnd(&strm);
    zlib_set_last_error(Z_OK);
    return output;
}

// ============================================================================
// Inflate Decompression
// ============================================================================

char* zlib_inflate(TmlBuffer* data, int32_t window_bits) {
    if (!data || data->len == 0) {
        zlib_set_last_error(Z_STREAM_ERROR);
        return NULL;
    }

    z_stream strm;
    memset(&strm, 0, sizeof(strm));

    int ret = inflateInit2(&strm, window_bits);
    if (ret != Z_OK) {
        zlib_set_last_error(ret);
        return NULL;
    }

    // Start with 4x input size estimate
    size_t out_capacity = data->len * 4;
    if (out_capacity < 256)
        out_capacity = 256;

    uint8_t* output = (uint8_t*)malloc(out_capacity);
    if (!output) {
        inflateEnd(&strm);
        zlib_set_last_error(Z_MEM_ERROR);
        return NULL;
    }

    strm.next_in = data->data;
    strm.avail_in = (uInt)data->len;

    size_t total_out = 0;

    do {
        // Expand buffer if needed
        if (total_out >= out_capacity - 1) {
            out_capacity *= 2;
            uint8_t* new_output = (uint8_t*)realloc(output, out_capacity);
            if (!new_output) {
                free(output);
                inflateEnd(&strm);
                zlib_set_last_error(Z_MEM_ERROR);
                return NULL;
            }
            output = new_output;
        }

        strm.next_out = output + total_out;
        strm.avail_out = (uInt)(out_capacity - total_out - 1);

        ret = inflate(&strm, Z_NO_FLUSH);
        if (ret == Z_NEED_DICT || ret == Z_DATA_ERROR || ret == Z_MEM_ERROR) {
            free(output);
            inflateEnd(&strm);
            zlib_set_last_error(ret);
            return NULL;
        }

        total_out = strm.total_out;

        // Prevent infinite loop: if input is exhausted AND output buffer has space
        // (meaning inflate has nothing more to output), then we're done
        if (strm.avail_in == 0 && strm.avail_out > 0 && ret != Z_STREAM_END) {
            break;
        }

    } while (ret != Z_STREAM_END);

    inflateEnd(&strm);

    // Null-terminate for string return
    output[total_out] = '\0';
    zlib_set_last_error(Z_OK);
    return (char*)output;
}

TmlBuffer* zlib_inflate_buffer(TmlBuffer* data, int32_t window_bits) {
    if (!data || data->len == 0) {
        zlib_set_last_error(Z_STREAM_ERROR);
        return NULL;
    }

    z_stream strm;
    memset(&strm, 0, sizeof(strm));

    int ret = inflateInit2(&strm, window_bits);
    if (ret != Z_OK) {
        zlib_set_last_error(ret);
        return NULL;
    }

    size_t out_capacity = data->len * 4;
    if (out_capacity < 256)
        out_capacity = 256;

    TmlBuffer* output = tml_buffer_create(out_capacity);
    if (!output) {
        inflateEnd(&strm);
        zlib_set_last_error(Z_MEM_ERROR);
        return NULL;
    }

    strm.next_in = data->data;
    strm.avail_in = (uInt)data->len;

    do {
        if (output->len >= output->capacity) {
            size_t new_cap = output->capacity * 2;
            uint8_t* new_data = (uint8_t*)realloc(output->data, new_cap);
            if (!new_data) {
                tml_buffer_destroy(output);
                inflateEnd(&strm);
                zlib_set_last_error(Z_MEM_ERROR);
                return NULL;
            }
            output->data = new_data;
            output->capacity = new_cap;
        }

        strm.next_out = output->data + output->len;
        strm.avail_out = (uInt)(output->capacity - output->len);

        ret = inflate(&strm, Z_NO_FLUSH);
        if (ret == Z_NEED_DICT || ret == Z_DATA_ERROR || ret == Z_MEM_ERROR) {
            tml_buffer_destroy(output);
            inflateEnd(&strm);
            zlib_set_last_error(ret);
            return NULL;
        }

        output->len = strm.total_out;

        // Prevent infinite loop: if input is exhausted AND output buffer has space
        // (meaning inflate has nothing more to output), then we're done
        if (strm.avail_in == 0 && strm.avail_out > 0 && ret != Z_STREAM_END) {
            break;
        }

    } while (ret != Z_STREAM_END);

    inflateEnd(&strm);
    zlib_set_last_error(Z_OK);
    return output;
}

// ============================================================================
// Gzip Compression (uses window_bits + 16)
// ============================================================================

TmlBuffer* gzip_compress(const char* data, int32_t level, int32_t window_bits, int32_t mem_level,
                         int32_t strategy) {
    // Add 16 to window_bits for gzip format
    int32_t gzip_window = window_bits > 0 ? window_bits + 16 : 15 + 16;
    return zlib_deflate(data, level, gzip_window, mem_level, strategy);
}

TmlBuffer* gzip_compress_buffer(TmlBuffer* data, int32_t level, int32_t window_bits,
                                int32_t mem_level, int32_t strategy) {
    int32_t gzip_window = window_bits > 0 ? window_bits + 16 : 15 + 16;
    return zlib_deflate_buffer(data, level, gzip_window, mem_level, strategy);
}

char* gzip_decompress(TmlBuffer* data, int32_t window_bits) {
    // Add 16 to window_bits for gzip format, or 32 for auto-detect
    int32_t gzip_window = window_bits > 0 ? window_bits + 16 : 15 + 32;
    return zlib_inflate(data, gzip_window);
}

TmlBuffer* gzip_decompress_buffer(TmlBuffer* data, int32_t window_bits) {
    int32_t gzip_window = window_bits > 0 ? window_bits + 16 : 15 + 32;
    return zlib_inflate_buffer(data, gzip_window);
}

// ============================================================================
// Gzip Header Reading
// ============================================================================

GzipHeaderInfo* gzip_read_header(TmlBuffer* data) {
    if (!data || data->len < 10) {
        return NULL;
    }

    // Check gzip magic number
    if (data->data[0] != 0x1f || data->data[1] != 0x8b) {
        return NULL;
    }

    GzipHeaderInfo* info = (GzipHeaderInfo*)calloc(1, sizeof(GzipHeaderInfo));
    if (!info)
        return NULL;

    // Compression method (should be 8 for deflate)
    if (data->data[2] != 8) {
        free(info);
        return NULL;
    }

    uint8_t flags = data->data[3];

    // Modification time (little-endian)
    info->mtime = (int64_t)(data->data[4] | (data->data[5] << 8) | (data->data[6] << 16) |
                            (data->data[7] << 24));

    // OS
    info->os = data->data[9];

    // Is text flag
    info->is_text = (flags & 0x01) != 0;

    // Skip extra field if present
    size_t pos = 10;
    if (flags & 0x04) { // FEXTRA
        if (pos + 2 > data->len) {
            free(info);
            return NULL;
        }
        uint16_t extra_len = data->data[pos] | (data->data[pos + 1] << 8);
        pos += 2 + extra_len;
    }

    // Read filename if present
    if (flags & 0x08) { // FNAME
        size_t name_start = pos;
        while (pos < data->len && data->data[pos] != 0)
            pos++;
        if (pos < data->len) {
            size_t name_len = pos - name_start;
            info->filename = (char*)malloc(name_len + 1);
            if (info->filename) {
                memcpy(info->filename, data->data + name_start, name_len);
                info->filename[name_len] = '\0';
            }
            pos++;
        }
    }

    // Read comment if present
    if (flags & 0x10) { // FCOMMENT
        size_t comment_start = pos;
        while (pos < data->len && data->data[pos] != 0)
            pos++;
        if (pos < data->len) {
            size_t comment_len = pos - comment_start;
            info->comment = (char*)malloc(comment_len + 1);
            if (info->comment) {
                memcpy(info->comment, data->data + comment_start, comment_len);
                info->comment[comment_len] = '\0';
            }
        }
    }

    return info;
}

void gzip_header_destroy(GzipHeaderInfo* header) {
    if (header) {
        free(header->filename);
        free(header->comment);
        free(header);
    }
}

// ============================================================================
// CRC32/Adler32 (tml_ prefix to avoid collision with zlib.h)
// ============================================================================

uint32_t tml_crc32_compute(const char* data) {
    if (!data)
        return 0;
    size_t len = strlen(data);
    return (uint32_t)crc32(0L, (const Bytef*)data, (uInt)len);
}

uint32_t tml_crc32_compute_buffer(TmlBuffer* data) {
    if (!data)
        return 0;
    return (uint32_t)crc32(0L, data->data, (uInt)data->len);
}

uint32_t tml_crc32_update(uint32_t crc, const char* data) {
    if (!data)
        return crc;
    size_t len = strlen(data);
    return (uint32_t)crc32((uLong)crc, (const Bytef*)data, (uInt)len);
}

uint32_t tml_crc32_update_buffer(uint32_t crc, TmlBuffer* data) {
    if (!data)
        return crc;
    return (uint32_t)crc32((uLong)crc, data->data, (uInt)data->len);
}

uint32_t tml_crc32_combine(uint32_t crc1, uint32_t crc2, int64_t len2) {
    // Use zlib's crc32_combine function
    return (uint32_t)crc32_combine((uLong)crc1, (uLong)crc2, (z_off_t)len2);
}

uint32_t tml_adler32_compute(const char* data) {
    if (!data)
        return 1; // Adler32 of empty is 1
    size_t len = strlen(data);
    return (uint32_t)adler32(1L, (const Bytef*)data, (uInt)len);
}

uint32_t tml_adler32_compute_buffer(TmlBuffer* data) {
    if (!data)
        return 1;
    return (uint32_t)adler32(1L, data->data, (uInt)data->len);
}

uint32_t tml_adler32_update(uint32_t adl, const char* data) {
    if (!data)
        return adl;
    size_t len = strlen(data);
    return (uint32_t)adler32((uLong)adl, (const Bytef*)data, (uInt)len);
}

uint32_t tml_adler32_update_buffer(uint32_t adl, TmlBuffer* data) {
    if (!data)
        return adl;
    return (uint32_t)adler32((uLong)adl, data->data, (uInt)data->len);
}

uint32_t tml_adler32_combine(uint32_t adler1, uint32_t adler2, int64_t len2) {
    // Use zlib's adler32_combine function
    return (uint32_t)adler32_combine((uLong)adler1, (uLong)adler2, (z_off_t)len2);
}

// ============================================================================
// Streaming Deflate
// ============================================================================

struct DeflateStream {
    z_stream strm;
    bool initialized;
};

DeflateStream* deflate_stream_create(int32_t level, int32_t window_bits, int32_t mem_level,
                                     int32_t strategy) {
    DeflateStream* stream = (DeflateStream*)calloc(1, sizeof(DeflateStream));
    if (!stream)
        return NULL;

    int ret = deflateInit2(&stream->strm, level, Z_DEFLATED, window_bits, mem_level, strategy);
    if (ret != Z_OK) {
        free(stream);
        zlib_set_last_error(ret);
        return NULL;
    }

    stream->initialized = true;
    return stream;
}

TmlBuffer* deflate_stream_write(DeflateStream* stream, const char* data, int32_t flush) {
    if (!stream || !stream->initialized || !data)
        return NULL;

    size_t input_len = strlen(data);
    size_t out_capacity = deflateBound(&stream->strm, (uLong)input_len);
    if (out_capacity < 64)
        out_capacity = 64;

    TmlBuffer* output = tml_buffer_create(out_capacity);
    if (!output)
        return NULL;

    stream->strm.next_in = (Bytef*)data;
    stream->strm.avail_in = (uInt)input_len;
    stream->strm.next_out = output->data;
    stream->strm.avail_out = (uInt)out_capacity;

    int ret = deflate(&stream->strm, flush);
    if (ret == Z_STREAM_ERROR) {
        tml_buffer_destroy(output);
        zlib_set_last_error(ret);
        return NULL;
    }

    output->len = out_capacity - stream->strm.avail_out;
    return output;
}

TmlBuffer* deflate_stream_write_buffer(DeflateStream* stream, TmlBuffer* data, int32_t flush) {
    if (!stream || !stream->initialized || !data)
        return NULL;

    size_t out_capacity = deflateBound(&stream->strm, (uLong)data->len);
    if (out_capacity < 64)
        out_capacity = 64;

    TmlBuffer* output = tml_buffer_create(out_capacity);
    if (!output)
        return NULL;

    stream->strm.next_in = data->data;
    stream->strm.avail_in = (uInt)data->len;
    stream->strm.next_out = output->data;
    stream->strm.avail_out = (uInt)out_capacity;

    int ret = deflate(&stream->strm, flush);
    if (ret == Z_STREAM_ERROR) {
        tml_buffer_destroy(output);
        zlib_set_last_error(ret);
        return NULL;
    }

    output->len = out_capacity - stream->strm.avail_out;
    return output;
}

TmlBuffer* deflate_stream_flush(DeflateStream* stream) {
    return deflate_stream_write(stream, "", Z_SYNC_FLUSH);
}

TmlBuffer* deflate_stream_finish(DeflateStream* stream) {
    return deflate_stream_write(stream, "", Z_FINISH);
}

void deflate_stream_destroy(DeflateStream* stream) {
    if (stream) {
        if (stream->initialized) {
            deflateEnd(&stream->strm);
        }
        free(stream);
    }
}

// ============================================================================
// Streaming Inflate
// ============================================================================

struct InflateStream {
    z_stream strm;
    bool initialized;
    bool finished;
};

InflateStream* inflate_stream_create(int32_t window_bits) {
    InflateStream* stream = (InflateStream*)calloc(1, sizeof(InflateStream));
    if (!stream)
        return NULL;

    int ret = inflateInit2(&stream->strm, window_bits);
    if (ret != Z_OK) {
        free(stream);
        zlib_set_last_error(ret);
        return NULL;
    }

    stream->initialized = true;
    stream->finished = false;
    return stream;
}

TmlBuffer* inflate_stream_write(InflateStream* stream, TmlBuffer* data) {
    if (!stream || !stream->initialized || !data)
        return NULL;

    size_t out_capacity = data->len * 4;
    if (out_capacity < 256)
        out_capacity = 256;

    TmlBuffer* output = tml_buffer_create(out_capacity);
    if (!output)
        return NULL;

    stream->strm.next_in = data->data;
    stream->strm.avail_in = (uInt)data->len;

    do {
        if (output->len >= output->capacity) {
            size_t new_cap = output->capacity * 2;
            uint8_t* new_data = (uint8_t*)realloc(output->data, new_cap);
            if (!new_data) {
                tml_buffer_destroy(output);
                return NULL;
            }
            output->data = new_data;
            output->capacity = new_cap;
        }

        stream->strm.next_out = output->data + output->len;
        stream->strm.avail_out = (uInt)(output->capacity - output->len);

        int ret = inflate(&stream->strm, Z_NO_FLUSH);
        if (ret == Z_STREAM_END) {
            stream->finished = true;
        } else if (ret != Z_OK && ret != Z_BUF_ERROR) {
            tml_buffer_destroy(output);
            zlib_set_last_error(ret);
            return NULL;
        }

        output->len = stream->strm.total_out;

        // Prevent infinite loop when input is exhausted
        if (stream->strm.avail_in == 0) {
            break;
        }

    } while (stream->strm.avail_out == 0 && !stream->finished);

    return output;
}

bool inflate_stream_is_finished(InflateStream* stream) {
    return stream ? stream->finished : true;
}

void inflate_stream_destroy(InflateStream* stream) {
    if (stream) {
        if (stream->initialized) {
            inflateEnd(&stream->strm);
        }
        free(stream);
    }
}
