/**
 * @file compat.h
 * @brief Cross-platform compatibility macros for TML C runtime.
 *
 * Maps MSVC-specific functions to POSIX equivalents on Unix/macOS.
 */

#ifndef TML_RUNTIME_COMPAT_H
#define TML_RUNTIME_COMPAT_H

#ifndef _WIN32
#include <strings.h>  /* strcasecmp */
#define _strdup    strdup
#define _stricmp   strcasecmp
#define _strnicmp  strncasecmp
#endif

#endif /* TML_RUNTIME_COMPAT_H */
