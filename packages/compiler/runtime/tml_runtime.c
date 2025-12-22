// TML Runtime - Main Entry Point
// Includes all runtime modules for single-file compilation
//
// Modules:
//   - tml_core.c      : Black box, SIMD operations
//   - tml_thread.c    : Threads, Channels, Mutex, WaitGroup, Atomic
//   - tml_collections.c : List, HashMap, Buffer, String utilities
//   - tml_time.c      : Time functions, Instant API, Float formatting

#include "tml_core.c"
#include "tml_thread.c"
#include "tml_collections.c"
#include "tml_time.c"
