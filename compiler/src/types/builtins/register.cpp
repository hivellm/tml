//! # Builtin Registration
//!
//! This file is the entry point for registering all builtin functions.
//!
//! ## Initialization Order
//!
//! `init_builtins()` calls specialized initializers in this order:
//!
//! | Initializer               | Registers                          |
//! |---------------------------|------------------------------------|
//! | `init_builtin_types`      | Primitive types, behavior impls    |
//! | `init_builtin_io`         | print, println, panic, assert      |
//! | `init_builtin_string`     | str_len, str_eq, str_hash, etc.    |
//! | `init_builtin_time`       | time_ms, sleep, elapsed, etc.      |
//! | `init_builtin_mem`        | mem_alloc, mem_free, mem_copy      |
//! | `init_builtin_atomic`     | atomic_load, atomic_store, fence   |
//! | `init_builtin_sync`       | spin_lock, thread_*, mutex_*, etc. |
//! | `init_builtin_math`       | sqrt, pow, abs, floor, ceil, round |
//! | `init_builtin_async`      | block_on                           |
//!
//! Each initializer is implemented in its own file for organization.

#include "types/env.hpp"

namespace tml::types {

void TypeEnv::init_builtins() {
    // Initialize builtins from specialized files
    init_builtin_types();  // Primitive types and behavior impls
    init_builtin_io();     // print, println, panic, assert
    init_builtin_string(); // str_len, str_eq, str_hash, etc.
    init_builtin_time();   // time_ms, time_us, time_ns, sleep, elapsed
    init_builtin_mem();    // mem_alloc, mem_free, mem_copy, etc.
    init_builtin_atomic(); // atomic_load, atomic_store, atomic_add, fence, etc.
    init_builtin_sync();   // spin_lock, spin_unlock, spin_trylock, thread_*, channel_*, mutex_*,
                           // waitgroup_*
    init_builtin_math();   // sqrt, pow, abs, floor, ceil, round, black_box
    init_builtin_async();  // block_on
}

} // namespace tml::types