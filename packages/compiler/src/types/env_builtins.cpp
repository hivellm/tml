// Main builtin initialization
// Delegates to specialized files for organization
#include "tml/types/env.hpp"

namespace tml::types {

void TypeEnv::init_builtins() {
    // Initialize builtins from specialized files
    init_builtin_types();   // Primitive types and behavior impls
    init_builtin_io();      // print, println, panic, assert
    init_builtin_string();  // str_len, str_eq, str_hash, etc.
    init_builtin_time();    // time_ms, time_us, time_ns, sleep, elapsed
    init_builtin_mem();     // mem_alloc, mem_free, mem_copy, etc.
}

} // namespace tml::types