// TML Standard Library - Collections Runtime
// Implements: Buffer
//
// Note: List and HashMap are now pure TML
// (see lib/std/src/collections/list.tml, hashmap.tml)

#ifndef STD_COLLECTIONS_H
#define STD_COLLECTIONS_H

#include <stdbool.h>
#include <stdint.h>

// ============================================================================
// Buffer - Byte buffer for binary data
// ============================================================================

typedef struct {
    uint8_t* data;
    int64_t len;
    int64_t capacity;
    int64_t read_pos;
} TmlBuffer;

TmlBuffer* buffer_create(int64_t initial_capacity);
void buffer_destroy(TmlBuffer* buf);
void buffer_write_byte(TmlBuffer* buf, int32_t byte);
void buffer_write_i32(TmlBuffer* buf, int32_t value);
void buffer_write_i64(TmlBuffer* buf, int64_t value);
int32_t buffer_read_byte(TmlBuffer* buf);
int32_t buffer_read_i32(TmlBuffer* buf);
int64_t buffer_read_i64(TmlBuffer* buf);
int64_t buffer_len(TmlBuffer* buf);
int64_t buffer_capacity(TmlBuffer* buf);
int64_t buffer_remaining(TmlBuffer* buf);
void buffer_clear(TmlBuffer* buf);
void buffer_reset_read(TmlBuffer* buf);

#endif // STD_COLLECTIONS_H
