/* <stddef.h> — common type definitions and NULL. */

#ifndef TML_C_STDLIB_STDDEF_H
#define TML_C_STDLIB_STDDEF_H

typedef unsigned long long size_t;
typedef long long          ptrdiff_t;
typedef long long          ssize_t;

#define NULL ((void*)0)

#define offsetof(type, member) ((size_t)&(((type*)0)->member))

#endif
