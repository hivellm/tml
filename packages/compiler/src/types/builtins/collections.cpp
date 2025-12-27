// Builtin collection functions: List, HashMap, Buffer
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
