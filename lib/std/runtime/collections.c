// TML Standard Library - Collections Runtime Implementation
// Implements: Buffer
//
// Note: List and HashMap are now pure TML
// (see lib/std/src/collections/list.tml, hashmap.tml)

#include "collections.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ============================================================================
// Buffer - Byte buffer for binary data
// ============================================================================

TmlBuffer* buffer_create(int64_t initial_capacity) {
    TmlBuffer* buf = (TmlBuffer*)malloc(sizeof(TmlBuffer));
    if (!buf)
        return NULL;

    if (initial_capacity < 64)
        initial_capacity = 64;

    buf->data = (uint8_t*)malloc(initial_capacity);
    if (!buf->data) {
        free(buf);
        return NULL;
    }

    buf->len = 0;
    buf->capacity = initial_capacity;
    buf->read_pos = 0;
    return buf;
}

void buffer_destroy(TmlBuffer* buf) {
    if (!buf)
        return;
    free(buf->data);
    free(buf);
}

static void buffer_grow(TmlBuffer* buf, int64_t min_capacity) {
    int64_t new_capacity = buf->capacity * 2;
    if (new_capacity < min_capacity)
        new_capacity = min_capacity;

    uint8_t* new_data = (uint8_t*)realloc(buf->data, new_capacity);
    if (new_data) {
        buf->data = new_data;
        buf->capacity = new_capacity;
    }
}

void buffer_write_byte(TmlBuffer* buf, int32_t byte) {
    if (!buf)
        return;
    if (buf->len >= buf->capacity) {
        buffer_grow(buf, buf->capacity + 1);
    }
    buf->data[buf->len++] = (uint8_t)(byte & 0xFF);
}

void buffer_write_i32(TmlBuffer* buf, int32_t value) {
    if (!buf)
        return;
    if (buf->len + 4 > buf->capacity) {
        buffer_grow(buf, buf->len + 4);
    }
    // Little-endian
    buf->data[buf->len++] = (uint8_t)(value & 0xFF);
    buf->data[buf->len++] = (uint8_t)((value >> 8) & 0xFF);
    buf->data[buf->len++] = (uint8_t)((value >> 16) & 0xFF);
    buf->data[buf->len++] = (uint8_t)((value >> 24) & 0xFF);
}

void buffer_write_i64(TmlBuffer* buf, int64_t value) {
    if (!buf)
        return;
    if (buf->len + 8 > buf->capacity) {
        buffer_grow(buf, buf->len + 8);
    }
    // Little-endian
    buf->data[buf->len++] = (uint8_t)(value & 0xFF);
    buf->data[buf->len++] = (uint8_t)((value >> 8) & 0xFF);
    buf->data[buf->len++] = (uint8_t)((value >> 16) & 0xFF);
    buf->data[buf->len++] = (uint8_t)((value >> 24) & 0xFF);
    buf->data[buf->len++] = (uint8_t)((value >> 32) & 0xFF);
    buf->data[buf->len++] = (uint8_t)((value >> 40) & 0xFF);
    buf->data[buf->len++] = (uint8_t)((value >> 48) & 0xFF);
    buf->data[buf->len++] = (uint8_t)((value >> 56) & 0xFF);
}

int32_t buffer_read_byte(TmlBuffer* buf) {
    if (!buf || buf->read_pos >= buf->len)
        return -1;
    return buf->data[buf->read_pos++];
}

int32_t buffer_read_i32(TmlBuffer* buf) {
    if (!buf || buf->read_pos + 4 > buf->len)
        return 0;
    int32_t value = 0;
    value |= buf->data[buf->read_pos++];
    value |= buf->data[buf->read_pos++] << 8;
    value |= buf->data[buf->read_pos++] << 16;
    value |= buf->data[buf->read_pos++] << 24;
    return value;
}

int64_t buffer_read_i64(TmlBuffer* buf) {
    if (!buf || buf->read_pos + 8 > buf->len)
        return 0;
    int64_t value = 0;
    value |= (int64_t)buf->data[buf->read_pos++];
    value |= (int64_t)buf->data[buf->read_pos++] << 8;
    value |= (int64_t)buf->data[buf->read_pos++] << 16;
    value |= (int64_t)buf->data[buf->read_pos++] << 24;
    value |= (int64_t)buf->data[buf->read_pos++] << 32;
    value |= (int64_t)buf->data[buf->read_pos++] << 40;
    value |= (int64_t)buf->data[buf->read_pos++] << 48;
    value |= (int64_t)buf->data[buf->read_pos++] << 56;
    return value;
}

int64_t buffer_len(TmlBuffer* buf) {
    return buf ? buf->len : 0;
}

int64_t buffer_capacity(TmlBuffer* buf) {
    return buf ? buf->capacity : 0;
}

int64_t buffer_remaining(TmlBuffer* buf) {
    if (!buf)
        return 0;
    return buf->len - buf->read_pos;
}

void buffer_clear(TmlBuffer* buf) {
    if (!buf)
        return;
    buf->len = 0;
    buf->read_pos = 0;
}

void buffer_reset_read(TmlBuffer* buf) {
    if (buf)
        buf->read_pos = 0;
}
