//! # Builtin Synchronization Primitives
//!
//! This file registers concurrency and synchronization intrinsics.
//!
//! ## Spinlock
//!
//! | Function       | Signature            | Description              |
//! |----------------|----------------------|--------------------------|
//! | `spin_lock`    | `(*Unit) -> Unit`    | Acquire (spins)          |
//! | `spin_unlock`  | `(*Unit) -> Unit`    | Release                  |
//! | `spin_trylock` | `(*Unit) -> Bool`    | Try acquire, non-blocking|
//!
//! ## Thread
//!
//! | Function       | Signature         | Description              |
//! |----------------|-------------------|--------------------------|
//! | `thread_yield` | `() -> Unit`      | Yield to other threads   |
//! | `thread_id`    | `() -> I64`       | Get current thread ID    |
//! | `thread_sleep` | `(I32) -> Unit`   | Sleep for milliseconds   |
//!
//! ## Channel (Go-style)
//!
//! | Function         | Signature                  | Description         |
//! |------------------|----------------------------|---------------------|
//! | `channel_create` | `() -> *Unit`              | Create channel      |
//! | `channel_destroy`| `(*Unit) -> Unit`          | Destroy channel     |
//! | `channel_send`   | `(*Unit, I32) -> Bool`     | Blocking send       |
//! | `channel_recv`   | `(*Unit) -> I32`           | Blocking receive    |
//! | `channel_try_*`  | `...`                      | Non-blocking ops    |
//!
//! ## Mutex / WaitGroup
//!
//! Higher-level synchronization primitives built on atomics.

#include "types/env.hpp"

namespace tml::types {

void TypeEnv::init_builtin_sync() {
    SourceSpan builtin_span{};

    // ============ Spinlock Primitives ============

    // spin_lock(lock_ptr: Ptr[Unit]) -> Unit - Acquire spinlock (spins until acquired)
    functions_["spin_lock"].push_back(
        FuncSig{"spin_lock", {make_ptr(make_unit())}, make_unit(), {}, false, builtin_span});

    // spin_unlock(lock_ptr: Ptr[Unit]) -> Unit - Release spinlock
    functions_["spin_unlock"].push_back(
        FuncSig{"spin_unlock", {make_ptr(make_unit())}, make_unit(), {}, false, builtin_span});

    // spin_trylock(lock_ptr: Ptr[Unit]) -> Bool - Try to acquire, returns true if successful
    functions_["spin_trylock"].push_back(
        FuncSig{"spin_trylock", {make_ptr(make_unit())}, make_bool(), {}, false, builtin_span});

    // Thread, Channel, Mutex, WaitGroup primitives — REMOVED (Phase 24)
    // thread_yield/id/sleep → @extern("tml_thread_*") in std::thread
    // channel_* → MPSC channels built from Mutex+Condvar in std::sync::mpsc
    // mutex_* → @extern("tml_mutex_*") in std::sync::mutex
    // waitgroup_* → not used in TML library
}

} // namespace tml::types
