/*
 * Minimal zstd decompression header.
 *
 * Declares only the functions used by the plugin loader.
 * The actual implementation is in zstddeclib.c (single-file decoder).
 */

#ifndef ZSTD_H_MINIMAL
#define ZSTD_H_MINIMAL

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ZSTD_CONTENTSIZE_UNKNOWN (0ULL - 1)
#define ZSTD_CONTENTSIZE_ERROR (0ULL - 2)

unsigned long long ZSTD_getFrameContentSize(const void* src, size_t srcSize);
size_t ZSTD_decompress(void* dst, size_t dstCapacity, const void* src, size_t srcSize);
unsigned ZSTD_isError(size_t code);
const char* ZSTD_getErrorName(size_t code);

#ifdef __cplusplus
}
#endif

#endif /* ZSTD_H_MINIMAL */
