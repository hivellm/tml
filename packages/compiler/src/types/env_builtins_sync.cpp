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
}

} // namespace tml::types
