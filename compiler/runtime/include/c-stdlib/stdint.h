/* <stdint.h> — fixed-width integer typedefs.
 *
 * Minimal header for the TML self-hosted C frontend. Provides the
 * typedefs the runtime relies on; the full set of MIN/MAX limit
 * macros is intentionally omitted because TML's parser does not yet
 * accept the long-long suffix forms used in those expansions.
 */

#ifndef TML_C_STDLIB_STDINT_H
#define TML_C_STDLIB_STDINT_H

typedef signed char        int8_t;
typedef unsigned char      uint8_t;
typedef short              int16_t;
typedef unsigned short     uint16_t;
typedef int                int32_t;
typedef unsigned int       uint32_t;
typedef long long          int64_t;
typedef unsigned long long uint64_t;

typedef long long          intmax_t;
typedef unsigned long long uintmax_t;
typedef long long          intptr_t;
typedef unsigned long long uintptr_t;

#endif
