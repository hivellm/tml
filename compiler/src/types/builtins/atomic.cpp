//! # Builtin Atomic Functions
//!
//! This file registers atomic operations for thread-safe programming.
//!
//! ## Atomic Load/Store
//!
//! | Function       | Signature                 | Description          |
//! |----------------|---------------------------|----------------------|
//! | `atomic_load`  | `(*Unit) -> I32`          | Thread-safe read     |
//! | `atomic_store` | `(*Unit, I32) -> Unit`    | Thread-safe write    |
//!
//! ## Atomic Arithmetic
//!
//! | Function     | Signature                  | Description          |
//! |--------------|----------------------------|----------------------|
//! | `atomic_add` | `(*Unit, I32) -> I32`      | Fetch-and-add        |
//! | `atomic_sub` | `(*Unit, I32) -> I32`      | Fetch-and-subtract   |
//!
//! ## Atomic Exchange
//!
//! | Function          | Signature                     | Description          |
//! |-------------------|-------------------------------|----------------------|
//! | `atomic_exchange` | `(*Unit, I32) -> I32`         | Swap, return old     |
//! | `atomic_cas`      | `(*Unit, I32, I32) -> Bool`   | Compare-and-swap     |
//!
//! ## Memory Fences
//!
//! | Function        | Signature    | Description             |
//! |-----------------|--------------|-------------------------|
//! | `fence_acquire` | `() -> Unit` | Acquire memory barrier  |
//! | `fence_release` | `() -> Unit` | Release memory barrier  |
//! | `fence_seqcst`  | `() -> Unit` | Sequentially consistent |

#include "types/env.hpp"

namespace tml::types {

void TypeEnv::init_builtin_atomic() {
    SourceSpan builtin_span{};

    // Note: All atomic functions use Ptr[Unit] for flexibility
    // The codegen uses opaque pointers, so any pointer type works

    // ============ Atomic Load/Store ============

    // atomic_load(ptr: Ptr[Unit]) -> I32 - Thread-safe read
    functions_["atomic_load"].push_back(
        FuncSig{"atomic_load", {make_ptr(make_unit())}, make_i32(), {}, false, builtin_span});

    // atomic_store(ptr: Ptr[Unit], value: I32) -> Unit - Thread-safe write
    functions_["atomic_store"].push_back(FuncSig{
        "atomic_store", {make_ptr(make_unit()), make_i32()}, make_unit(), {}, false, builtin_span});

    // ============ Atomic Arithmetic ============

    // atomic_add(ptr: Ptr[Unit], value: I32) -> I32 - Fetch-and-add, returns old value
    functions_["atomic_add"].push_back(FuncSig{
        "atomic_add", {make_ptr(make_unit()), make_i32()}, make_i32(), {}, false, builtin_span});

    // atomic_sub(ptr: Ptr[Unit], value: I32) -> I32 - Fetch-and-sub, returns old value
    functions_["atomic_sub"].push_back(FuncSig{
        "atomic_sub", {make_ptr(make_unit()), make_i32()}, make_i32(), {}, false, builtin_span});

    // ============ Atomic Exchange ============

    // atomic_exchange(ptr: Ptr[Unit], value: I32) -> I32 - Exchange, returns old value
    functions_["atomic_exchange"].push_back(FuncSig{"atomic_exchange",
                                                    {make_ptr(make_unit()), make_i32()},
                                                    make_i32(),
                                                    {},
                                                    false,
                                                    builtin_span});

    // ============ Compare-and-Swap ============

    // atomic_cas(ptr: Ptr[Unit], expected: I32, desired: I32) -> Bool
    // Returns true if exchange happened (old value == expected)
    functions_["atomic_cas"].push_back(FuncSig{"atomic_cas",
                                               {make_ptr(make_unit()), make_i32(), make_i32()},
                                               make_bool(),
                                               {},
                                               false,
                                               builtin_span});

    // atomic_cas_val(ptr: Ptr[Unit], expected: I32, desired: I32) -> I32
    // Returns old value (for compare-exchange-weak patterns)
    functions_["atomic_cas_val"].push_back(FuncSig{"atomic_cas_val",
                                                   {make_ptr(make_unit()), make_i32(), make_i32()},
                                                   make_i32(),
                                                   {},
                                                   false,
                                                   builtin_span});

    // ============ Atomic Bitwise ============

    // atomic_and(ptr: Ptr[Unit], value: I32) -> I32 - Fetch-and-and, returns old value
    functions_["atomic_and"].push_back(FuncSig{
        "atomic_and", {make_ptr(make_unit()), make_i32()}, make_i32(), {}, false, builtin_span});

    // atomic_or(ptr: Ptr[Unit], value: I32) -> I32 - Fetch-and-or, returns old value
    functions_["atomic_or"].push_back(FuncSig{
        "atomic_or", {make_ptr(make_unit()), make_i32()}, make_i32(), {}, false, builtin_span});

    // atomic_xor(ptr: Ptr[Unit], value: I32) -> I32 - Fetch-and-xor, returns old value
    functions_["atomic_xor"].push_back(FuncSig{
        "atomic_xor", {make_ptr(make_unit()), make_i32()}, make_i32(), {}, false, builtin_span});

    // ============ Memory Fences ============

    // fence() -> Unit - Full memory barrier (seq_cst)
    functions_["fence"].push_back(FuncSig{"fence", {}, make_unit(), {}, false, builtin_span});

    // fence_acquire() -> Unit - Acquire fence
    functions_["fence_acquire"].push_back(
        FuncSig{"fence_acquire", {}, make_unit(), {}, false, builtin_span});

    // fence_release() -> Unit - Release fence
    functions_["fence_release"].push_back(
        FuncSig{"fence_release", {}, make_unit(), {}, false, builtin_span});
}

} // namespace tml::types
