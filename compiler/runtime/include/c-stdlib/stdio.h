/* <stdio.h> — I/O declarations used by the TML runtime. */

#ifndef TML_C_STDLIB_STDIO_H
#define TML_C_STDLIB_STDIO_H

#include <stddef.h>
#include <stdarg.h>

typedef struct _FILE FILE;

extern FILE* stdin;
extern FILE* stdout;
extern FILE* stderr;

#define EOF (-1)

int   printf(const char* fmt, ...);
int   fprintf(FILE* stream, const char* fmt, ...);
int   sprintf(char* buf, const char* fmt, ...);
int   snprintf(char* buf, size_t size, const char* fmt, ...);
int   vfprintf(FILE* stream, const char* fmt, va_list ap);
int   vprintf(const char* fmt, va_list ap);
int   vsnprintf(char* buf, size_t size, const char* fmt, va_list ap);

int   fputs(const char* s, FILE* stream);
int   puts(const char* s);
int   fputc(int c, FILE* stream);
int   putc(int c, FILE* stream);
int   putchar(int c);
int   fflush(FILE* stream);

FILE* fopen(const char* path, const char* mode);
int   fclose(FILE* stream);
size_t fread(void* ptr, size_t size, size_t count, FILE* stream);
size_t fwrite(const void* ptr, size_t size, size_t count, FILE* stream);
int   fseek(FILE* stream, long offset, int whence);
long  ftell(FILE* stream);

#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

#endif
