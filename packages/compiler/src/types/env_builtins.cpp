#include "tml/types/env.hpp"

namespace tml::types {

void TypeEnv::init_builtins() {
    // Builtin types
    builtins_["I8"] = make_primitive(PrimitiveKind::I8);
    builtins_["I16"] = make_primitive(PrimitiveKind::I16);
    builtins_["I32"] = make_primitive(PrimitiveKind::I32);
    builtins_["I64"] = make_primitive(PrimitiveKind::I64);
    builtins_["I128"] = make_primitive(PrimitiveKind::I128);
    builtins_["U8"] = make_primitive(PrimitiveKind::U8);
    builtins_["U16"] = make_primitive(PrimitiveKind::U16);
    builtins_["U32"] = make_primitive(PrimitiveKind::U32);
    builtins_["U64"] = make_primitive(PrimitiveKind::U64);
    builtins_["U128"] = make_primitive(PrimitiveKind::U128);
    builtins_["F32"] = make_primitive(PrimitiveKind::F32);
    builtins_["F64"] = make_primitive(PrimitiveKind::F64);
    builtins_["Bool"] = make_primitive(PrimitiveKind::Bool);
    builtins_["Char"] = make_primitive(PrimitiveKind::Char);
    builtins_["Str"] = make_primitive(PrimitiveKind::Str);
    builtins_["Unit"] = make_unit();

    // Builtin functions
    SourceSpan builtin_span{};

    // print(s: Str) -> Unit
    functions_["print"] = FuncSig{
        "print",
        {make_primitive(PrimitiveKind::Str)},
        make_unit(),
        {},
        false,
        builtin_span,
        StabilityLevel::Stable,
        "",
        "1.0"
    };

    // println(s: Str) -> Unit
    functions_["println"] = FuncSig{
        "println",
        {make_primitive(PrimitiveKind::Str)},
        make_unit(),
        {},
        false,
        builtin_span,
        StabilityLevel::Stable,
        "",
        "1.0"
    };

    // panic(msg: Str) -> Never (noreturn)
    // For now we return Unit since Never type is not implemented
    functions_["panic"] = FuncSig{
        "panic",
        {make_primitive(PrimitiveKind::Str)},
        make_unit(),  // TODO: Should be Never type
        {},
        false,
        builtin_span,
        StabilityLevel::Stable,
        "",
        "1.0"
    };

    // print_i32(n: I32) -> Unit
    functions_["print_i32"] = FuncSig{
        "print_i32",
        {make_primitive(PrimitiveKind::I32)},
        make_unit(),
        {},
        false,
        builtin_span,
        StabilityLevel::Deprecated,
        "Use toString(value) + print() instead for better type safety",
        "1.2"
    };

    // print_bool(b: Bool) -> Unit
    functions_["print_bool"] = FuncSig{
        "print_bool",
        {make_primitive(PrimitiveKind::Bool)},
        make_unit(),
        {},
        false,
        builtin_span,
        StabilityLevel::Deprecated,
        "Use toString(value) + print() instead for better type safety",
        "1.2"
    };

    // Memory allocation functions
    auto ptr_type = make_ref(make_primitive(PrimitiveKind::I32), true);

    // alloc(size: I32) -> ptr
    functions_["alloc"] = FuncSig{
        "alloc",
        {make_primitive(PrimitiveKind::I32)},
        ptr_type,
        {},
        false,
        builtin_span
    };

    // dealloc(ptr) -> Unit
    functions_["dealloc"] = FuncSig{
        "dealloc",
        {ptr_type},
        make_unit(),
        {},
        false,
        builtin_span
    };

    // read_i32(ptr) -> I32
    functions_["read_i32"] = FuncSig{
        "read_i32",
        {ptr_type},
        make_primitive(PrimitiveKind::I32),
        {},
        false,
        builtin_span
    };

    // write_i32(ptr, value: I32) -> Unit
    functions_["write_i32"] = FuncSig{
        "write_i32",
        {ptr_type, make_primitive(PrimitiveKind::I32)},
        make_unit(),
        {},
        false,
        builtin_span
    };

    // ptr_offset(ptr, offset: I32) -> ptr
    functions_["ptr_offset"] = FuncSig{
        "ptr_offset",
        {ptr_type, make_primitive(PrimitiveKind::I32)},
        ptr_type,
        {},
        false,
        builtin_span
    };

    // ============ ATOMIC OPERATIONS (Thread-Safe) ============

    // atomic_load(ptr) -> I32
    functions_["atomic_load"] = FuncSig{
        "atomic_load",
        {ptr_type},
        make_primitive(PrimitiveKind::I32),
        {},
        false,
        builtin_span
    };

    // atomic_store(ptr, value: I32) -> Unit
    functions_["atomic_store"] = FuncSig{
        "atomic_store",
        {ptr_type, make_primitive(PrimitiveKind::I32)},
        make_unit(),
        {},
        false,
        builtin_span
    };

    // atomic_add(ptr, value: I32) -> I32
    functions_["atomic_add"] = FuncSig{
        "atomic_add",
        {ptr_type, make_primitive(PrimitiveKind::I32)},
        make_primitive(PrimitiveKind::I32),
        {},
        false,
        builtin_span
    };

    // atomic_sub(ptr, value: I32) -> I32
    functions_["atomic_sub"] = FuncSig{
        "atomic_sub",
        {ptr_type, make_primitive(PrimitiveKind::I32)},
        make_primitive(PrimitiveKind::I32),
        {},
        false,
        builtin_span
    };

    // atomic_exchange(ptr, value: I32) -> I32
    functions_["atomic_exchange"] = FuncSig{
        "atomic_exchange",
        {ptr_type, make_primitive(PrimitiveKind::I32)},
        make_primitive(PrimitiveKind::I32),
        {},
        false,
        builtin_span
    };

    // atomic_cas(ptr, expected: I32, desired: I32) -> Bool
    functions_["atomic_cas"] = FuncSig{
        "atomic_cas",
        {ptr_type, make_primitive(PrimitiveKind::I32), make_primitive(PrimitiveKind::I32)},
        make_primitive(PrimitiveKind::Bool),
        {},
        false,
        builtin_span
    };

    // atomic_cas_val(ptr, expected: I32, desired: I32) -> I32
    functions_["atomic_cas_val"] = FuncSig{
        "atomic_cas_val",
        {ptr_type, make_primitive(PrimitiveKind::I32), make_primitive(PrimitiveKind::I32)},
        make_primitive(PrimitiveKind::I32),
        {},
        false,
        builtin_span
    };

    // atomic_and(ptr, value: I32) -> I32
    functions_["atomic_and"] = FuncSig{
        "atomic_and",
        {ptr_type, make_primitive(PrimitiveKind::I32)},
        make_primitive(PrimitiveKind::I32),
        {},
        false,
        builtin_span
    };

    // atomic_or(ptr, value: I32) -> I32
    functions_["atomic_or"] = FuncSig{
        "atomic_or",
        {ptr_type, make_primitive(PrimitiveKind::I32)},
        make_primitive(PrimitiveKind::I32),
        {},
        false,
        builtin_span
    };

    // fence() -> Unit
    functions_["fence"] = FuncSig{
        "fence",
        {},
        make_unit(),
        {},
        false,
        builtin_span
    };

    // fence_acquire() -> Unit
    functions_["fence_acquire"] = FuncSig{
        "fence_acquire",
        {},
        make_unit(),
        {},
        false,
        builtin_span
    };

    // fence_release() -> Unit
    functions_["fence_release"] = FuncSig{
        "fence_release",
        {},
        make_unit(),
        {},
        false,
        builtin_span
    };

    // ============ SPINLOCK PRIMITIVES ============

    // spin_lock(lock_ptr) -> Unit
    functions_["spin_lock"] = FuncSig{
        "spin_lock",
        {ptr_type},
        make_unit(),
        {},
        false,
        builtin_span
    };

    // spin_unlock(lock_ptr) -> Unit
    functions_["spin_unlock"] = FuncSig{
        "spin_unlock",
        {ptr_type},
        make_unit(),
        {},
        false,
        builtin_span
    };

    // spin_trylock(lock_ptr) -> Bool
    functions_["spin_trylock"] = FuncSig{
        "spin_trylock",
        {ptr_type},
        make_primitive(PrimitiveKind::Bool),
        {},
        false,
        builtin_span
    };

    // ============ THREADING PRIMITIVES ============

    // thread_spawn(func_ptr, arg_ptr) -> thread_handle (ptr)
    functions_["thread_spawn"] = FuncSig{
        "thread_spawn",
        {ptr_type, ptr_type},
        ptr_type,
        {},
        false,
        builtin_span
    };

    // thread_join(handle) -> Unit
    functions_["thread_join"] = FuncSig{
        "thread_join",
        {ptr_type},
        make_unit(),
        {},
        false,
        builtin_span
    };

    // thread_yield() -> Unit
    functions_["thread_yield"] = FuncSig{
        "thread_yield",
        {},
        make_unit(),
        {},
        false,
        builtin_span
    };

    // thread_sleep(ms: I32) -> Unit
    functions_["thread_sleep"] = FuncSig{
        "thread_sleep",
        {make_primitive(PrimitiveKind::I32)},
        make_unit(),
        {},
        false,
        builtin_span
    };

    // thread_id() -> I32
    functions_["thread_id"] = FuncSig{
        "thread_id",
        {},
        make_primitive(PrimitiveKind::I32),
        {},
        false,
        builtin_span
    };

    // ============ GO-STYLE CHANNELS ============

    // channel_create() -> channel_ptr
    functions_["channel_create"] = FuncSig{
        "channel_create",
        {},
        ptr_type,
        {},
        false,
        builtin_span
    };

    // channel_send(ch, value: I32) -> Bool (true if success)
    functions_["channel_send"] = FuncSig{
        "channel_send",
        {ptr_type, make_primitive(PrimitiveKind::I32)},
        make_primitive(PrimitiveKind::Bool),
        {},
        false,
        builtin_span
    };

    // channel_recv(ch) -> I32
    functions_["channel_recv"] = FuncSig{
        "channel_recv",
        {ptr_type},
        make_primitive(PrimitiveKind::I32),
        {},
        false,
        builtin_span
    };

    // channel_try_send(ch, value: I32) -> Bool
    functions_["channel_try_send"] = FuncSig{
        "channel_try_send",
        {ptr_type, make_primitive(PrimitiveKind::I32)},
        make_primitive(PrimitiveKind::Bool),
        {},
        false,
        builtin_span
    };

    // channel_try_recv(ch, out_ptr) -> Bool
    functions_["channel_try_recv"] = FuncSig{
        "channel_try_recv",
        {ptr_type, ptr_type},
        make_primitive(PrimitiveKind::Bool),
        {},
        false,
        builtin_span
    };

    // channel_close(ch) -> Unit
    functions_["channel_close"] = FuncSig{
        "channel_close",
        {ptr_type},
        make_unit(),
        {},
        false,
        builtin_span
    };

    // channel_destroy(ch) -> Unit
    functions_["channel_destroy"] = FuncSig{
        "channel_destroy",
        {ptr_type},
        make_unit(),
        {},
        false,
        builtin_span
    };

    // channel_len(ch) -> I32
    functions_["channel_len"] = FuncSig{
        "channel_len",
        {ptr_type},
        make_primitive(PrimitiveKind::I32),
        {},
        false,
        builtin_span
    };

    // ============ MUTEX PRIMITIVES ============

    // mutex_create() -> mutex_ptr
    functions_["mutex_create"] = FuncSig{
        "mutex_create",
        {},
        ptr_type,
        {},
        false,
        builtin_span
    };

    // mutex_lock(m) -> Unit
    functions_["mutex_lock"] = FuncSig{
        "mutex_lock",
        {ptr_type},
        make_unit(),
        {},
        false,
        builtin_span
    };

    // mutex_unlock(m) -> Unit
    functions_["mutex_unlock"] = FuncSig{
        "mutex_unlock",
        {ptr_type},
        make_unit(),
        {},
        false,
        builtin_span
    };

    // mutex_try_lock(m) -> Bool
    functions_["mutex_try_lock"] = FuncSig{
        "mutex_try_lock",
        {ptr_type},
        make_primitive(PrimitiveKind::Bool),
        {},
        false,
        builtin_span
    };

    // mutex_destroy(m) -> Unit
    functions_["mutex_destroy"] = FuncSig{
        "mutex_destroy",
        {ptr_type},
        make_unit(),
        {},
        false,
        builtin_span
    };

    // ============ WAITGROUP (GO-STYLE) ============

    // waitgroup_create() -> wg_ptr
    functions_["waitgroup_create"] = FuncSig{
        "waitgroup_create",
        {},
        ptr_type,
        {},
        false,
        builtin_span
    };

    // waitgroup_add(wg, delta: I32) -> Unit
    functions_["waitgroup_add"] = FuncSig{
        "waitgroup_add",
        {ptr_type, make_primitive(PrimitiveKind::I32)},
        make_unit(),
        {},
        false,
        builtin_span
    };

    // waitgroup_done(wg) -> Unit
    functions_["waitgroup_done"] = FuncSig{
        "waitgroup_done",
        {ptr_type},
        make_unit(),
        {},
        false,
        builtin_span
    };

    // waitgroup_wait(wg) -> Unit
    functions_["waitgroup_wait"] = FuncSig{
        "waitgroup_wait",
        {ptr_type},
        make_unit(),
        {},
        false,
        builtin_span
    };

    // waitgroup_destroy(wg) -> Unit
    functions_["waitgroup_destroy"] = FuncSig{
        "waitgroup_destroy",
        {ptr_type},
        make_unit(),
        {},
        false,
        builtin_span
    };

    // ============ LIST (Dynamic Array) ============

    // list_create(capacity: I32) -> list_ptr
    functions_["list_create"] = FuncSig{
        "list_create",
        {make_primitive(PrimitiveKind::I32)},
        ptr_type,
        {},
        false,
        builtin_span
    };

    // list_destroy(list) -> Unit
    functions_["list_destroy"] = FuncSig{
        "list_destroy",
        {ptr_type},
        make_unit(),
        {},
        false,
        builtin_span
    };

    // list_push(list, value: I32) -> Unit
    functions_["list_push"] = FuncSig{
        "list_push",
        {ptr_type, make_primitive(PrimitiveKind::I32)},
        make_unit(),
        {},
        false,
        builtin_span
    };

    // list_pop(list) -> I32
    functions_["list_pop"] = FuncSig{
        "list_pop",
        {ptr_type},
        make_primitive(PrimitiveKind::I32),
        {},
        false,
        builtin_span
    };

    // list_get(list, index: I32) -> I32
    functions_["list_get"] = FuncSig{
        "list_get",
        {ptr_type, make_primitive(PrimitiveKind::I32)},
        make_primitive(PrimitiveKind::I32),
        {},
        false,
        builtin_span
    };

    // list_set(list, index: I32, value: I32) -> Unit
    functions_["list_set"] = FuncSig{
        "list_set",
        {ptr_type, make_primitive(PrimitiveKind::I32), make_primitive(PrimitiveKind::I32)},
        make_unit(),
        {},
        false,
        builtin_span
    };

    // list_len(list) -> I32
    functions_["list_len"] = FuncSig{
        "list_len",
        {ptr_type},
        make_primitive(PrimitiveKind::I32),
        {},
        false,
        builtin_span
    };

    // list_capacity(list) -> I32
    functions_["list_capacity"] = FuncSig{
        "list_capacity",
        {ptr_type},
        make_primitive(PrimitiveKind::I32),
        {},
        false,
        builtin_span
    };

    // list_clear(list) -> Unit
    functions_["list_clear"] = FuncSig{
        "list_clear",
        {ptr_type},
        make_unit(),
        {},
        false,
        builtin_span
    };

    // list_is_empty(list) -> Bool
    functions_["list_is_empty"] = FuncSig{
        "list_is_empty",
        {ptr_type},
        make_primitive(PrimitiveKind::Bool),
        {},
        false,
        builtin_span
    };

    // ============ HASHMAP ============

    // hashmap_create(capacity: I32) -> map_ptr
    functions_["hashmap_create"] = FuncSig{
        "hashmap_create",
        {make_primitive(PrimitiveKind::I32)},
        ptr_type,
        {},
        false,
        builtin_span
    };

    // hashmap_destroy(map) -> Unit
    functions_["hashmap_destroy"] = FuncSig{
        "hashmap_destroy",
        {ptr_type},
        make_unit(),
        {},
        false,
        builtin_span
    };

    // hashmap_set(map, key: I32, value: I32) -> Unit
    functions_["hashmap_set"] = FuncSig{
        "hashmap_set",
        {ptr_type, make_primitive(PrimitiveKind::I32), make_primitive(PrimitiveKind::I32)},
        make_unit(),
        {},
        false,
        builtin_span
    };

    // hashmap_get(map, key: I32) -> I32
    functions_["hashmap_get"] = FuncSig{
        "hashmap_get",
        {ptr_type, make_primitive(PrimitiveKind::I32)},
        make_primitive(PrimitiveKind::I32),
        {},
        false,
        builtin_span
    };

    // hashmap_has(map, key: I32) -> Bool
    functions_["hashmap_has"] = FuncSig{
        "hashmap_has",
        {ptr_type, make_primitive(PrimitiveKind::I32)},
        make_primitive(PrimitiveKind::Bool),
        {},
        false,
        builtin_span
    };

    // hashmap_remove(map, key: I32) -> Bool
    functions_["hashmap_remove"] = FuncSig{
        "hashmap_remove",
        {ptr_type, make_primitive(PrimitiveKind::I32)},
        make_primitive(PrimitiveKind::Bool),
        {},
        false,
        builtin_span
    };

    // hashmap_len(map) -> I32
    functions_["hashmap_len"] = FuncSig{
        "hashmap_len",
        {ptr_type},
        make_primitive(PrimitiveKind::I32),
        {},
        false,
        builtin_span
    };

    // hashmap_clear(map) -> Unit
    functions_["hashmap_clear"] = FuncSig{
        "hashmap_clear",
        {ptr_type},
        make_unit(),
        {},
        false,
        builtin_span
    };

    // ============ BUFFER ============

    // buffer_create(capacity: I32) -> buf_ptr
    functions_["buffer_create"] = FuncSig{
        "buffer_create",
        {make_primitive(PrimitiveKind::I32)},
        ptr_type,
        {},
        false,
        builtin_span
    };

    // buffer_destroy(buf) -> Unit
    functions_["buffer_destroy"] = FuncSig{
        "buffer_destroy",
        {ptr_type},
        make_unit(),
        {},
        false,
        builtin_span
    };

    // buffer_write_byte(buf, byte: I32) -> Unit
    functions_["buffer_write_byte"] = FuncSig{
        "buffer_write_byte",
        {ptr_type, make_primitive(PrimitiveKind::I32)},
        make_unit(),
        {},
        false,
        builtin_span
    };

    // buffer_write_i32(buf, value: I32) -> Unit
    functions_["buffer_write_i32"] = FuncSig{
        "buffer_write_i32",
        {ptr_type, make_primitive(PrimitiveKind::I32)},
        make_unit(),
        {},
        false,
        builtin_span
    };

    // buffer_read_byte(buf) -> I32
    functions_["buffer_read_byte"] = FuncSig{
        "buffer_read_byte",
        {ptr_type},
        make_primitive(PrimitiveKind::I32),
        {},
        false,
        builtin_span
    };

    // buffer_read_i32(buf) -> I32
    functions_["buffer_read_i32"] = FuncSig{
        "buffer_read_i32",
        {ptr_type},
        make_primitive(PrimitiveKind::I32),
        {},
        false,
        builtin_span
    };

    // buffer_len(buf) -> I32
    functions_["buffer_len"] = FuncSig{
        "buffer_len",
        {ptr_type},
        make_primitive(PrimitiveKind::I32),
        {},
        false,
        builtin_span
    };

    // buffer_capacity(buf) -> I32
    functions_["buffer_capacity"] = FuncSig{
        "buffer_capacity",
        {ptr_type},
        make_primitive(PrimitiveKind::I32),
        {},
        false,
        builtin_span
    };

    // buffer_remaining(buf) -> I32
    functions_["buffer_remaining"] = FuncSig{
        "buffer_remaining",
        {ptr_type},
        make_primitive(PrimitiveKind::I32),
        {},
        false,
        builtin_span
    };

    // buffer_clear(buf) -> Unit
    functions_["buffer_clear"] = FuncSig{
        "buffer_clear",
        {ptr_type},
        make_unit(),
        {},
        false,
        builtin_span
    };

    // buffer_reset_read(buf) -> Unit
    functions_["buffer_reset_read"] = FuncSig{
        "buffer_reset_read",
        {ptr_type},
        make_unit(),
        {},
        false,
        builtin_span
    };

    // ============ STRING UTILITIES ============

    // str_len(s: Str) -> I32
    functions_["str_len"] = FuncSig{
        "str_len",
        {make_primitive(PrimitiveKind::Str)},
        make_primitive(PrimitiveKind::I32),
        {},
        false,
        builtin_span
    };

    // str_hash(s: Str) -> I32
    functions_["str_hash"] = FuncSig{
        "str_hash",
        {make_primitive(PrimitiveKind::Str)},
        make_primitive(PrimitiveKind::I32),
        {},
        false,
        builtin_span
    };

    // str_eq(a: Str, b: Str) -> Bool
    functions_["str_eq"] = FuncSig{
        "str_eq",
        {make_primitive(PrimitiveKind::Str), make_primitive(PrimitiveKind::Str)},
        make_primitive(PrimitiveKind::Bool),
        {},
        false,
        builtin_span
    };

    // ============ TIME FUNCTIONS ============

    // time_ms() -> I32 - Current time in milliseconds (for benchmarking)
    functions_["time_ms"] = FuncSig{
        "time_ms",
        {},
        make_primitive(PrimitiveKind::I32),
        {},
        false,
        builtin_span
    };

    // time_us() -> I64 - Current time in microseconds
    functions_["time_us"] = FuncSig{
        "time_us",
        {},
        make_primitive(PrimitiveKind::I64),
        {},
        false,
        builtin_span
    };

    // time_ns() -> I64 - Current time in nanoseconds
    functions_["time_ns"] = FuncSig{
        "time_ns",
        {},
        make_primitive(PrimitiveKind::I64),
        {},
        false,
        builtin_span
    };

    // elapsed_secs(start_ms: I32) -> Str - Get elapsed time as "X.XXX" seconds string
    functions_["elapsed_secs"] = FuncSig{
        "elapsed_secs",
        {make_primitive(PrimitiveKind::I32)},
        make_primitive(PrimitiveKind::Str),
        {},
        false,
        builtin_span
    };

    // elapsed_ms(start_ms: I32) -> I32 - Get elapsed milliseconds
    functions_["elapsed_ms"] = FuncSig{
        "elapsed_ms",
        {make_primitive(PrimitiveKind::I32)},
        make_primitive(PrimitiveKind::I32),
        {},
        false,
        builtin_span
    };

    // ============ INSTANT API (like Rust's std::time::Instant) ============

    // Instant::now() -> I64 - High-resolution timestamp
    functions_["Instant::now"] = FuncSig{
        "Instant::now",
        {},
        make_primitive(PrimitiveKind::I64),
        {},
        false,
        builtin_span
    };

    // Instant::elapsed(start: I64) -> I64 - Duration in microseconds
    functions_["Instant::elapsed"] = FuncSig{
        "Instant::elapsed",
        {make_primitive(PrimitiveKind::I64)},
        make_primitive(PrimitiveKind::I64),
        {},
        false,
        builtin_span
    };

    // Duration::as_secs_f64(us: I64) -> Str - Format as seconds
    functions_["Duration::as_secs_f64"] = FuncSig{
        "Duration::as_secs_f64",
        {make_primitive(PrimitiveKind::I64)},
        make_primitive(PrimitiveKind::Str),
        {},
        false,
        builtin_span
    };

    // Duration::as_millis_f64(us: I64) -> F64 - Duration in milliseconds as float
    functions_["Duration::as_millis_f64"] = FuncSig{
        "Duration::as_millis_f64",
        {make_primitive(PrimitiveKind::I64)},
        make_primitive(PrimitiveKind::F64),
        {},
        false,
        builtin_span
    };

    // ============ BLACK BOX (prevent optimization) ============

    // black_box(value: I32) -> I32 - Prevents LLVM from optimizing away
    functions_["black_box"] = FuncSig{
        "black_box",
        {make_primitive(PrimitiveKind::I32)},
        make_primitive(PrimitiveKind::I32),
        {},
        false,
        builtin_span
    };

    // black_box_i64(value: I64) -> I64 - Prevents LLVM from optimizing away
    functions_["black_box_i64"] = FuncSig{
        "black_box_i64",
        {make_primitive(PrimitiveKind::I64)},
        make_primitive(PrimitiveKind::I64),
        {},
        false,
        builtin_span
    };

    // ============ FLOAT FUNCTIONS ============

    // toFixed(value: I32, decimals: I32) -> Str
    functions_["toFixed"] = FuncSig{
        "toFixed",
        {make_primitive(PrimitiveKind::I32), make_primitive(PrimitiveKind::I32)},
        make_primitive(PrimitiveKind::Str),
        {},
        false,
        builtin_span
    };

    // toPrecision(value: I32, precision: I32) -> Str
    functions_["toPrecision"] = FuncSig{
        "toPrecision",
        {make_primitive(PrimitiveKind::I32), make_primitive(PrimitiveKind::I32)},
        make_primitive(PrimitiveKind::Str),
        {},
        false,
        builtin_span
    };

    // toString(value: I32) -> Str
    functions_["toString"] = FuncSig{
        "toString",
        {make_primitive(PrimitiveKind::I32)},
        make_primitive(PrimitiveKind::Str),
        {},
        false,
        builtin_span
    };

    // toFloat(value: I32) -> F64
    functions_["toFloat"] = FuncSig{
        "toFloat",
        {make_primitive(PrimitiveKind::I32)},
        make_primitive(PrimitiveKind::F64),
        {},
        false,
        builtin_span
    };

    // toInt(value: I32) -> I32 (for float-to-int conversion)
    functions_["toInt"] = FuncSig{
        "toInt",
        {make_primitive(PrimitiveKind::I32)},
        make_primitive(PrimitiveKind::I32),
        {},
        false,
        builtin_span
    };

    // round(value: I32) -> I32
    functions_["round"] = FuncSig{
        "round",
        {make_primitive(PrimitiveKind::I32)},
        make_primitive(PrimitiveKind::I32),
        {},
        false,
        builtin_span
    };

    // floor(value: I32) -> I32
    functions_["floor"] = FuncSig{
        "floor",
        {make_primitive(PrimitiveKind::I32)},
        make_primitive(PrimitiveKind::I32),
        {},
        false,
        builtin_span
    };

    // ceil(value: I32) -> I32
    functions_["ceil"] = FuncSig{
        "ceil",
        {make_primitive(PrimitiveKind::I32)},
        make_primitive(PrimitiveKind::I32),
        {},
        false,
        builtin_span
    };

    // abs(value: I32) -> I32
    functions_["abs"] = FuncSig{
        "abs",
        {make_primitive(PrimitiveKind::I32)},
        make_primitive(PrimitiveKind::I32),
        {},
        false,
        builtin_span
    };

    // sqrt(value: I32) -> I32 (truncated)
    functions_["sqrt"] = FuncSig{
        "sqrt",
        {make_primitive(PrimitiveKind::I32)},
        make_primitive(PrimitiveKind::I32),
        {},
        false,
        builtin_span
    };

    // pow(base: I32, exp: I32) -> I32 (truncated)
    functions_["pow"] = FuncSig{
        "pow",
        {make_primitive(PrimitiveKind::I32), make_primitive(PrimitiveKind::I32)},
        make_primitive(PrimitiveKind::I32),
        {},
        false,
        builtin_span
    };

    // ============ TEST ASSERTIONS ============

    // assert(condition: Bool, message: Str) -> Unit
    functions_["assert"] = FuncSig{
        "assert",
        {make_primitive(PrimitiveKind::Bool), make_primitive(PrimitiveKind::Str)},
        make_unit(),
        {},
        false,
        builtin_span,
        StabilityLevel::Stable,
        "",
        "1.0"
    };

    // assert_eq_i32(left: I32, right: I32, message: Str) -> Unit
    functions_["assert_eq_i32"] = FuncSig{
        "assert_eq_i32",
        {make_primitive(PrimitiveKind::I32), make_primitive(PrimitiveKind::I32), make_primitive(PrimitiveKind::Str)},
        make_unit(),
        {},
        false,
        builtin_span,
        StabilityLevel::Stable,
        "",
        "1.0"
    };

    // assert_ne_i32(left: I32, right: I32, message: Str) -> Unit
    functions_["assert_ne_i32"] = FuncSig{
        "assert_ne_i32",
        {make_primitive(PrimitiveKind::I32), make_primitive(PrimitiveKind::I32), make_primitive(PrimitiveKind::Str)},
        make_unit(),
        {},
        false,
        builtin_span,
        StabilityLevel::Stable,
        "",
        "1.0"
    };

    // assert_eq_str(left: Str, right: Str, message: Str) -> Unit
    functions_["assert_eq_str"] = FuncSig{
        "assert_eq_str",
        {make_primitive(PrimitiveKind::Str), make_primitive(PrimitiveKind::Str), make_primitive(PrimitiveKind::Str)},
        make_unit(),
        {},
        false,
        builtin_span,
        StabilityLevel::Stable,
        "",
        "1.0"
    };

    // assert_eq_bool(left: Bool, right: Bool, message: Str) -> Unit
    functions_["assert_eq_bool"] = FuncSig{
        "assert_eq_bool",
        {make_primitive(PrimitiveKind::Bool), make_primitive(PrimitiveKind::Bool), make_primitive(PrimitiveKind::Str)},
        make_unit(),
        {},
        false,
        builtin_span,
        StabilityLevel::Stable,
        "",
        "1.0"
    };
}

} // namespace tml::types
