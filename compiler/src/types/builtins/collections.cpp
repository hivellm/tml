//! # Builtin Collection Functions
//!
//! This file registers intrinsics for standard collections.
//!
//! Note: List and HashMap builtins have been removed — now pure TML
//! (see lib/std/src/collections/list.tml, hashmap.tml)
//!
//! ## Buffer (Fixed-size)
//!
//! | Function        | Signature                     | Description        |
//! |-----------------|-------------------------------|--------------------|
//! | `buffer_create` | `(I32) -> *Unit`              | Create with size   |
//! | `buffer_*`      | `...`                         | Same as list ops   |

#include "types/env.hpp"

namespace tml::types {

void TypeEnv::init_builtin_collections() {
    SourceSpan builtin_span{};

    // Note: List and HashMap builtins removed — now pure TML

    // ============ Buffer Functions ============

    // buffer_create() -> Ptr[Unit]
    functions_["buffer_create"].push_back(
        FuncSig{"buffer_create", {}, make_ptr(make_unit()), {}, false, builtin_span});

    // buffer_create(capacity: I32) -> Ptr[Unit]
    functions_["buffer_create"].push_back(
        FuncSig{"buffer_create", {make_i32()}, make_ptr(make_unit()), {}, false, builtin_span});

    // buffer_destroy(buf: Ptr[Unit]) -> Unit
    functions_["buffer_destroy"].push_back(
        FuncSig{"buffer_destroy", {make_ptr(make_unit())}, make_unit(), {}, false, builtin_span});

    // buffer_write_byte(buf: Ptr[Unit], byte: I32) -> Unit
    functions_["buffer_write_byte"].push_back(FuncSig{"buffer_write_byte",
                                                      {make_ptr(make_unit()), make_i32()},
                                                      make_unit(),
                                                      {},
                                                      false,
                                                      builtin_span});

    // buffer_write_i32(buf: Ptr[Unit], value: I32) -> Unit
    functions_["buffer_write_i32"].push_back(FuncSig{"buffer_write_i32",
                                                     {make_ptr(make_unit()), make_i32()},
                                                     make_unit(),
                                                     {},
                                                     false,
                                                     builtin_span});

    // buffer_read_byte(buf: Ptr[Unit]) -> I32
    functions_["buffer_read_byte"].push_back(
        FuncSig{"buffer_read_byte", {make_ptr(make_unit())}, make_i32(), {}, false, builtin_span});

    // buffer_read_i32(buf: Ptr[Unit]) -> I32
    functions_["buffer_read_i32"].push_back(
        FuncSig{"buffer_read_i32", {make_ptr(make_unit())}, make_i32(), {}, false, builtin_span});

    // buffer_len(buf: Ptr[Unit]) -> I32
    functions_["buffer_len"].push_back(
        FuncSig{"buffer_len", {make_ptr(make_unit())}, make_i32(), {}, false, builtin_span});

    // buffer_capacity(buf: Ptr[Unit]) -> I32
    functions_["buffer_capacity"].push_back(
        FuncSig{"buffer_capacity", {make_ptr(make_unit())}, make_i32(), {}, false, builtin_span});

    // buffer_remaining(buf: Ptr[Unit]) -> I32
    functions_["buffer_remaining"].push_back(
        FuncSig{"buffer_remaining", {make_ptr(make_unit())}, make_i32(), {}, false, builtin_span});

    // buffer_clear(buf: Ptr[Unit]) -> Unit
    functions_["buffer_clear"].push_back(
        FuncSig{"buffer_clear", {make_ptr(make_unit())}, make_unit(), {}, false, builtin_span});

    // buffer_reset_read(buf: Ptr[Unit]) -> Unit
    functions_["buffer_reset_read"].push_back(FuncSig{
        "buffer_reset_read", {make_ptr(make_unit())}, make_unit(), {}, false, builtin_span});
}

} // namespace tml::types
