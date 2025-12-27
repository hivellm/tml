// Main builtin initialization
// Delegates to specialized files for organization
#include "tml/types/env.hpp"

namespace tml::types {

void TypeEnv::init_builtins() {
    // Initialize builtins from specialized files
    init_builtin_types();       // Primitive types and behavior impls
    init_builtin_io();          // print, println, panic, assert
    init_builtin_string();      // str_len, str_eq, str_hash, etc.
    init_builtin_time();        // time_ms, time_us, time_ns, sleep, elapsed
    init_builtin_mem();         // mem_alloc, mem_free, mem_copy, etc.
    init_builtin_atomic();      // atomic_load, atomic_store, atomic_add, fence, etc.
    init_builtin_sync();        // spin_lock, spin_unlock, spin_trylock, thread_*, channel_*, mutex_*, waitgroup_*
    init_builtin_math();        // sqrt, pow, abs, floor, ceil, round, black_box
    init_builtin_collections(); // list_*, hashmap_*, buffer_*
}

} // namespace tml::types