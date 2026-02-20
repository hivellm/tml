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
//! | ~~`init_builtin_time`~~   | Removed Phase 39 — @extern FFI     |
//! | `init_builtin_mem`        | mem_alloc, mem_free, mem_copy      |
//! | `init_builtin_atomic`     | atomic_load, atomic_store, fence   |
//! | `init_builtin_sync`       | spin_lock, thread_*, mutex_*, etc. |
//! | `init_builtin_math`       | sqrt, pow, abs, floor, ceil, round |
//! | `init_builtin_async`      | block_on                           |
//!
//! Each initializer is implemented in its own file for organization.
//!
//! ## Removed Initializers
//!
//! | Removed                   | Reason                                    |
//! |---------------------------|-------------------------------------------|
//! | `init_builtin_string`     | Phase 29: all 29 FuncSig dead — str ops   |
//! |                           | go through try_gen_builtin_string()        |
//! | `init_builtin_time`       | Phase 39: all 8 FuncSig dead — time_ns,   |
//! |                           | sleep_ms now @extern("c") in std::time     |

#include "types/env.hpp"

namespace tml::types {

void TypeEnv::init_builtins() {
    // Initialize builtins from specialized files
    init_builtin_types(); // Primitive types and behavior impls
    init_builtin_io();    // print, println, panic, assert
    // init_builtin_string removed (Phase 29) — 29 dead FuncSig entries
    // String ops: codegen uses try_gen_builtin_string() inline, not functions_ lookup
    // Char ops: migrated to pure TML in lib/core/src/char/methods.tml
    // init_builtin_time removed (Phase 39) — 8 dead FuncSig entries
    // time_ns and sleep_ms now @extern("c") FFI in lib/std/src/time.tml
    init_builtin_mem();    // mem_alloc, mem_free, mem_copy, etc.
    init_builtin_atomic(); // atomic_load, atomic_store, atomic_add, fence, etc.
    init_builtin_sync();   // spin_lock, spin_unlock, spin_trylock, thread_*, channel_*, mutex_*,
                           // waitgroup_*
    init_builtin_math();   // sqrt, pow, abs, floor, ceil, round, black_box
    init_builtin_async();  // block_on
}

} // namespace tml::types