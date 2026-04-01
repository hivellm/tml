/**
 * @file essential_utf8.c
 * @brief TML Runtime - UTF-8 String Encoding Functions
 *
 * Extracted from essential.c. All statics remain static (translation-unit scope).
 */

#include <stdint.h>

#ifdef _WIN32
#define TML_EXPORT __declspec(dllexport)
#else
#define TML_EXPORT __attribute__((visibility("default")))
#endif

// ============================================================================
// UTF-8 String Encoding Functions
// ============================================================================

/** @brief Buffer for UTF-8 character output (max 4 bytes + null terminator). */
static char utf8_char_buffer[8];

/**
 * @brief Converts a 2-byte UTF-8 sequence to a string.
 *
 * Used for Unicode code points U+0080 to U+07FF.
 *
 * @param b1 First byte (110xxxxx).
 * @param b2 Second byte (10xxxxxx).
 * @return A static buffer containing the 2-byte UTF-8 string.
 */
TML_EXPORT const char* utf8_2byte_to_string(uint8_t b1, uint8_t b2) {
    utf8_char_buffer[0] = (char)b1;
    utf8_char_buffer[1] = (char)b2;
    utf8_char_buffer[2] = '\0';
    return utf8_char_buffer;
}

/**
 * @brief Converts a 3-byte UTF-8 sequence to a string.
 *
 * Used for Unicode code points U+0800 to U+FFFF.
 *
 * @param b1 First byte (1110xxxx).
 * @param b2 Second byte (10xxxxxx).
 * @param b3 Third byte (10xxxxxx).
 * @return A static buffer containing the 3-byte UTF-8 string.
 */
TML_EXPORT const char* utf8_3byte_to_string(uint8_t b1, uint8_t b2, uint8_t b3) {
    utf8_char_buffer[0] = (char)b1;
    utf8_char_buffer[1] = (char)b2;
    utf8_char_buffer[2] = (char)b3;
    utf8_char_buffer[3] = '\0';
    return utf8_char_buffer;
}

/**
 * @brief Converts a 4-byte UTF-8 sequence to a string.
 *
 * Used for Unicode code points U+10000 to U+10FFFF.
 *
 * @param b1 First byte (11110xxx).
 * @param b2 Second byte (10xxxxxx).
 * @param b3 Third byte (10xxxxxx).
 * @param b4 Fourth byte (10xxxxxx).
 * @return A static buffer containing the 4-byte UTF-8 string.
 */
TML_EXPORT const char* utf8_4byte_to_string(uint8_t b1, uint8_t b2, uint8_t b3, uint8_t b4) {
    utf8_char_buffer[0] = (char)b1;
    utf8_char_buffer[1] = (char)b2;
    utf8_char_buffer[2] = (char)b3;
    utf8_char_buffer[3] = (char)b4;
    utf8_char_buffer[4] = '\0';
    return utf8_char_buffer;
}
