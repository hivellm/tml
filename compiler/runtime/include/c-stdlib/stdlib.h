/* <stdlib.h> — process control + memory + parsing helpers. */

#ifndef TML_C_STDLIB_STDLIB_H
#define TML_C_STDLIB_STDLIB_H

#include <stddef.h>

#define EXIT_SUCCESS 0
#define EXIT_FAILURE 1
#define RAND_MAX 2147483647

void   exit(int status);
void   abort(void);
void   _exit(int status);
char*  getenv(const char* name);
int    setenv(const char* name, const char* value, int overwrite);
int    atexit(void (*fn)(void));

void*  malloc(size_t size);
void*  calloc(size_t count, size_t size);
void*  realloc(void* ptr, size_t size);
void   free(void* ptr);

int    atoi(const char* s);
long   atol(const char* s);
long long atoll(const char* s);
double atof(const char* s);

int    rand(void);
void   srand(unsigned seed);

void   qsort(void* base, size_t count, size_t size,
             int (*cmp)(const void*, const void*));

#endif
