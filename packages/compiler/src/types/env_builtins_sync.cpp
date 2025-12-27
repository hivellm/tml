// Builtin synchronization primitives
#include "tml/types/env.hpp"

namespace tml::types {

void TypeEnv::init_builtin_sync() {
    SourceSpan builtin_span{};

    // ============ Spinlock Primitives ============

    // spin_lock(lock_ptr: Ptr[Unit]) -> Unit - Acquire spinlock (spins until acquired)
    functions_["spin_lock"].push_back(FuncSig{
        "spin_lock",
        {make_ptr(make_unit())},
        make_unit(),
        {},
        false,
        builtin_span
    });

    // spin_unlock(lock_ptr: Ptr[Unit]) -> Unit - Release spinlock
    functions_["spin_unlock"].push_back(FuncSig{
        "spin_unlock",
        {make_ptr(make_unit())},
        make_unit(),
        {},
        false,
        builtin_span
    });

    // spin_trylock(lock_ptr: Ptr[Unit]) -> Bool - Try to acquire, returns true if successful
    functions_["spin_trylock"].push_back(FuncSig{
        "spin_trylock",
        {make_ptr(make_unit())},
        make_bool(),
        {},
        false,
        builtin_span
    });

    // ============ Thread Primitives ============

    // thread_yield() -> Unit - Yield to other threads
    functions_["thread_yield"].push_back(FuncSig{
        "thread_yield",
        {},
        make_unit(),
        {},
        false,
        builtin_span
    });

    // thread_id() -> I64 - Get current thread ID
    functions_["thread_id"].push_back(FuncSig{
        "thread_id",
        {},
        make_i64(),
        {},
        false,
        builtin_span
    });

    // thread_sleep(ms: I32) -> Unit - Sleep for milliseconds
    functions_["thread_sleep"].push_back(FuncSig{
        "thread_sleep",
        {make_i32()},
        make_unit(),
        {},
        false,
        builtin_span
    });

    // ============ Channel Primitives (Go-style) ============

    // channel_create() -> Ptr[Unit]
    functions_["channel_create"].push_back(FuncSig{
        "channel_create",
        {},
        make_ptr(make_unit()),
        {},
        false,
        builtin_span
    });

    // channel_destroy(ch: Ptr[Unit]) -> Unit
    functions_["channel_destroy"].push_back(FuncSig{
        "channel_destroy",
        {make_ptr(make_unit())},
        make_unit(),
        {},
        false,
        builtin_span
    });

    // channel_send(ch: Ptr[Unit], value: I32) -> Bool
    functions_["channel_send"].push_back(FuncSig{
        "channel_send",
        {make_ptr(make_unit()), make_i32()},
        make_bool(),
        {},
        false,
        builtin_span
    });

    // channel_recv(ch: Ptr[Unit]) -> I32
    functions_["channel_recv"].push_back(FuncSig{
        "channel_recv",
        {make_ptr(make_unit())},
        make_i32(),
        {},
        false,
        builtin_span
    });

    // channel_try_send(ch: Ptr[Unit], value: I32) -> Bool
    functions_["channel_try_send"].push_back(FuncSig{
        "channel_try_send",
        {make_ptr(make_unit()), make_i32()},
        make_bool(),
        {},
        false,
        builtin_span
    });

    // channel_try_recv(ch: Ptr[Unit], out: Ptr[Unit]) -> Bool
    functions_["channel_try_recv"].push_back(FuncSig{
        "channel_try_recv",
        {make_ptr(make_unit()), make_ptr(make_unit())},
        make_bool(),
        {},
        false,
        builtin_span
    });

    // channel_close(ch: Ptr[Unit]) -> Unit
    functions_["channel_close"].push_back(FuncSig{
        "channel_close",
        {make_ptr(make_unit())},
        make_unit(),
        {},
        false,
        builtin_span
    });

    // channel_len(ch: Ptr[Unit]) -> I32
    functions_["channel_len"].push_back(FuncSig{
        "channel_len",
        {make_ptr(make_unit())},
        make_i32(),
        {},
        false,
        builtin_span
    });

    // ============ Mutex Primitives ============

    // mutex_create() -> Ptr[Unit]
    functions_["mutex_create"].push_back(FuncSig{
        "mutex_create",
        {},
        make_ptr(make_unit()),
        {},
        false,
        builtin_span
    });

    // mutex_lock(m: Ptr[Unit]) -> Unit
    functions_["mutex_lock"].push_back(FuncSig{
        "mutex_lock",
        {make_ptr(make_unit())},
        make_unit(),
        {},
        false,
        builtin_span
    });

    // mutex_unlock(m: Ptr[Unit]) -> Unit
    functions_["mutex_unlock"].push_back(FuncSig{
        "mutex_unlock",
        {make_ptr(make_unit())},
        make_unit(),
        {},
        false,
        builtin_span
    });

    // mutex_try_lock(m: Ptr[Unit]) -> Bool
    functions_["mutex_try_lock"].push_back(FuncSig{
        "mutex_try_lock",
        {make_ptr(make_unit())},
        make_bool(),
        {},
        false,
        builtin_span
    });

    // mutex_destroy(m: Ptr[Unit]) -> Unit
    functions_["mutex_destroy"].push_back(FuncSig{
        "mutex_destroy",
        {make_ptr(make_unit())},
        make_unit(),
        {},
        false,
        builtin_span
    });

    // ============ WaitGroup Primitives (Go-style) ============

    // waitgroup_create() -> Ptr[Unit]
    functions_["waitgroup_create"].push_back(FuncSig{
        "waitgroup_create",
        {},
        make_ptr(make_unit()),
        {},
        false,
        builtin_span
    });

    // waitgroup_add(wg: Ptr[Unit], delta: I32) -> Unit
    functions_["waitgroup_add"].push_back(FuncSig{
        "waitgroup_add",
        {make_ptr(make_unit()), make_i32()},
        make_unit(),
        {},
        false,
        builtin_span
    });

    // waitgroup_done(wg: Ptr[Unit]) -> Unit
    functions_["waitgroup_done"].push_back(FuncSig{
        "waitgroup_done",
        {make_ptr(make_unit())},
        make_unit(),
        {},
        false,
        builtin_span
    });

    // waitgroup_wait(wg: Ptr[Unit]) -> Unit
    functions_["waitgroup_wait"].push_back(FuncSig{
        "waitgroup_wait",
        {make_ptr(make_unit())},
        make_unit(),
        {},
        false,
        builtin_span
    });

    // waitgroup_destroy(wg: Ptr[Unit]) -> Unit
    functions_["waitgroup_destroy"].push_back(FuncSig{
        "waitgroup_destroy",
        {make_ptr(make_unit())},
        make_unit(),
        {},
        false,
        builtin_span
    });
}

} // namespace tml::types
