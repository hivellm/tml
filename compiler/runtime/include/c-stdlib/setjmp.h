/* <setjmp.h> — non-local jump support used by panic interception. */

#ifndef TML_C_STDLIB_SETJMP_H
#define TML_C_STDLIB_SETJMP_H

/* Opaque buffer; size mirrors the largest platform jmp_buf in use
 * (Windows x64 _JBLEN * 16 bytes = 256). The runtime only
 * memcpy()s this around so the exact field layout does not matter
 * to the C frontend. */
typedef long long jmp_buf[32];

int  setjmp(jmp_buf env);
int  _setjmp(jmp_buf env);
void longjmp(jmp_buf env, int val);

#endif
