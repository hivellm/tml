// TML Essential Runtime - Minimal Core Implementation
// Only print, println, panic - everything else goes in std

#include "tml_essential.h"

void tml_panic(const char* msg) {
    fprintf(stderr, "panic: %s\n", msg);
    exit(1);
}

void tml_print(const char* str) {
    printf("%s", str);
}

void tml_println(const char* str) {
    printf("%s\n", str);
}
