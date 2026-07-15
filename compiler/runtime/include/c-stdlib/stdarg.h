/* <stdarg.h> — variable-argument access. */

#ifndef TML_C_STDLIB_STDARG_H
#define TML_C_STDLIB_STDARG_H

typedef char* va_list;

#define va_start(ap, last) ((void)((ap) = (char*)&(last) + sizeof(last)))
#define va_arg(ap, type)   (*(type*)(((ap) += sizeof(type)) - sizeof(type)))
#define va_end(ap)         ((void)((ap) = (char*)0))
#define va_copy(dst, src)  ((dst) = (src))

#endif
