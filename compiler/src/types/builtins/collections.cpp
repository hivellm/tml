//! # Builtin Collection Functions
//!
//! This file registers intrinsics for standard collections.
//!
//! ## List (Dynamic Array)
//!
//! | Function        | Signature                     | Description        |
//! |-----------------|-------------------------------|--------------------|
//! | `list_create`   | `() -> *Unit`                 | Create empty list  |
//! | `list_destroy`  | `(*Unit) -> Unit`             | Free list memory   |
//! | `list_push`     | `(*Unit, I32) -> Unit`        | Append element     |
//! | `list_pop`      | `(*Unit) -> I32`              | Remove last        |
//! | `list_get`      | `(*Unit, I32) -> I32`         | Get at index       |
//! | `list_set`      | `(*Unit, I32, I32) -> Unit`   | Set at index       |
//! | `list_len`      | `(*Unit) -> I32`              | Get length         |
//! | `list_capacity` | `(*Unit) -> I32`              | Get capacity       |
//! | `list_clear`    | `(*Unit) -> Unit`             | Remove all         |
//!
//! ## HashMap
//!
//! | Function          | Signature                     | Description       |
//! |-------------------|-------------------------------|-------------------|
//! | `hashmap_create`  | `() -> *Unit`                 | Create empty map  |
//! | `hashmap_insert`  | `(*Unit, I32, I32) -> Unit`   | Insert key-value  |
//! | `hashmap_get`     | `(*Unit, I32) -> I32`         | Get by key        |
//! | `hashmap_remove`  | `(*Unit, I32) -> Bool`        | Remove by key     |
//! | `hashmap_contains`| `(*Unit, I32) -> Bool`        | Check key exists  |
//!
//! ## HashMap Iterator
//!
//! | Function               | Signature                  | Description       |
//! |------------------------|----------------------------|-------------------|
//! | `hashmap_iter_create`  | `(*Unit) -> *Unit`         | Create iterator   |
//! | `hashmap_iter_destroy` | `(*Unit) -> Unit`          | Free iterator     |
//! | `hashmap_iter_has_next`| `(*Unit) -> Bool`          | Check if more     |
//! | `hashmap_iter_next`    | `(*Unit) -> Unit`          | Advance iterator  |
//! | `hashmap_iter_key`     | `(*Unit) -> I64`           | Get current key   |
//! | `hashmap_iter_value`   | `(*Unit) -> I64`           | Get current value |
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

    // ============ List Functions ============

    // list_create() -> Ptr[Unit]
    functions_["list_create"].push_back(
        FuncSig{"list_create", {}, make_ptr(make_unit()), {}, false, builtin_span});

    // list_create(capacity: I32) -> Ptr[Unit]
    functions_["list_create"].push_back(
        FuncSig{"list_create", {make_i32()}, make_ptr(make_unit()), {}, false, builtin_span});

    // list_destroy(list: Ptr[Unit]) -> Unit
    functions_["list_destroy"].push_back(
        FuncSig{"list_destroy", {make_ptr(make_unit())}, make_unit(), {}, false, builtin_span});

    // list_push(list: Ptr[Unit], value: I32) -> Unit
    functions_["list_push"].push_back(FuncSig{
        "list_push", {make_ptr(make_unit()), make_i32()}, make_unit(), {}, false, builtin_span});

    // list_pop(list: Ptr[Unit]) -> I32
    functions_["list_pop"].push_back(
        FuncSig{"list_pop", {make_ptr(make_unit())}, make_i32(), {}, false, builtin_span});

    // list_get(list: Ptr[Unit], index: I32) -> I32
    functions_["list_get"].push_back(FuncSig{
        "list_get", {make_ptr(make_unit()), make_i32()}, make_i32(), {}, false, builtin_span});

    // list_set(list: Ptr[Unit], index: I32, value: I32) -> Unit
    functions_["list_set"].push_back(FuncSig{"list_set",
                                             {make_ptr(make_unit()), make_i32(), make_i32()},
                                             make_unit(),
                                             {},
                                             false,
                                             builtin_span});

    // list_len(list: Ptr[Unit]) -> I32
    functions_["list_len"].push_back(
        FuncSig{"list_len", {make_ptr(make_unit())}, make_i32(), {}, false, builtin_span});

    // list_capacity(list: Ptr[Unit]) -> I32
    functions_["list_capacity"].push_back(
        FuncSig{"list_capacity", {make_ptr(make_unit())}, make_i32(), {}, false, builtin_span});

    // list_clear(list: Ptr[Unit]) -> Unit
    functions_["list_clear"].push_back(
        FuncSig{"list_clear", {make_ptr(make_unit())}, make_unit(), {}, false, builtin_span});

    // list_is_empty(list: Ptr[Unit]) -> Bool
    functions_["list_is_empty"].push_back(
        FuncSig{"list_is_empty", {make_ptr(make_unit())}, make_bool(), {}, false, builtin_span});

    // ============ HashMap Functions ============

    // hashmap_create() -> Ptr[Unit]
    functions_["hashmap_create"].push_back(
        FuncSig{"hashmap_create", {}, make_ptr(make_unit()), {}, false, builtin_span});

    // hashmap_create(capacity: I32) -> Ptr[Unit]
    functions_["hashmap_create"].push_back(
        FuncSig{"hashmap_create", {make_i32()}, make_ptr(make_unit()), {}, false, builtin_span});

    // hashmap_destroy(map: Ptr[Unit]) -> Unit
    functions_["hashmap_destroy"].push_back(
        FuncSig{"hashmap_destroy", {make_ptr(make_unit())}, make_unit(), {}, false, builtin_span});

    // hashmap_set(map: Ptr[Unit], key: I32, value: I32) -> Unit
    functions_["hashmap_set"].push_back(FuncSig{"hashmap_set",
                                                {make_ptr(make_unit()), make_i32(), make_i32()},
                                                make_unit(),
                                                {},
                                                false,
                                                builtin_span});

    // hashmap_get(map: Ptr[Unit], key: I32) -> I32
    functions_["hashmap_get"].push_back(FuncSig{
        "hashmap_get", {make_ptr(make_unit()), make_i32()}, make_i32(), {}, false, builtin_span});

    // hashmap_has(map: Ptr[Unit], key: I32) -> Bool
    functions_["hashmap_has"].push_back(FuncSig{
        "hashmap_has", {make_ptr(make_unit()), make_i32()}, make_bool(), {}, false, builtin_span});

    // hashmap_remove(map: Ptr[Unit], key: I32) -> Bool
    functions_["hashmap_remove"].push_back(FuncSig{"hashmap_remove",
                                                   {make_ptr(make_unit()), make_i32()},
                                                   make_bool(),
                                                   {},
                                                   false,
                                                   builtin_span});

    // hashmap_len(map: Ptr[Unit]) -> I32
    functions_["hashmap_len"].push_back(
        FuncSig{"hashmap_len", {make_ptr(make_unit())}, make_i32(), {}, false, builtin_span});

    // hashmap_clear(map: Ptr[Unit]) -> Unit
    functions_["hashmap_clear"].push_back(
        FuncSig{"hashmap_clear", {make_ptr(make_unit())}, make_unit(), {}, false, builtin_span});

    // ============ HashMap Iterator Functions ============

    // hashmap_iter_create(map: Ptr[Unit]) -> Ptr[Unit]
    functions_["hashmap_iter_create"].push_back(FuncSig{"hashmap_iter_create",
                                                        {make_ptr(make_unit())},
                                                        make_ptr(make_unit()),
                                                        {},
                                                        false,
                                                        builtin_span});

    // hashmap_iter_destroy(iter: Ptr[Unit]) -> Unit
    functions_["hashmap_iter_destroy"].push_back(FuncSig{
        "hashmap_iter_destroy", {make_ptr(make_unit())}, make_unit(), {}, false, builtin_span});

    // hashmap_iter_has_next(iter: Ptr[Unit]) -> Bool
    functions_["hashmap_iter_has_next"].push_back(FuncSig{
        "hashmap_iter_has_next", {make_ptr(make_unit())}, make_bool(), {}, false, builtin_span});

    // hashmap_iter_next(iter: Ptr[Unit]) -> Unit
    functions_["hashmap_iter_next"].push_back(FuncSig{
        "hashmap_iter_next", {make_ptr(make_unit())}, make_unit(), {}, false, builtin_span});

    // hashmap_iter_key(iter: Ptr[Unit]) -> I64
    functions_["hashmap_iter_key"].push_back(
        FuncSig{"hashmap_iter_key", {make_ptr(make_unit())}, make_i64(), {}, false, builtin_span});

    // hashmap_iter_value(iter: Ptr[Unit]) -> I64
    functions_["hashmap_iter_value"].push_back(FuncSig{
        "hashmap_iter_value", {make_ptr(make_unit())}, make_i64(), {}, false, builtin_span});

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
