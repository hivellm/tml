// TML Essential Runtime - Minimal Core Functions
// Only print, println, panic - everything else goes in std

#ifndef TML_ESSENTIAL_H
#define TML_ESSENTIAL_H

#include <stdio.h>
#include <stdlib.h>

void tml_panic(const char* msg);
void tml_print(const char* str);
void tml_println(const char* str);

#endif // TML_ESSENTIAL_H
