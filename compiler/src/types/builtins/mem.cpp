TML_MODULE("compiler")

//! # Builtin Memory Functions
//!
//! This file registers low-level memory management intrinsics.
//!
//! ## Allocation
//!
//! | Function           | Signature                   | Description          |
//! |--------------------|-----------------------------|----------------------|
//! | `mem_alloc`        | `(I64) -> *Unit`            | Allocate bytes       |
//! | `mem_alloc_zeroed` | `(I64) -> *Unit`            | Allocate zeroed      |
//! | `mem_realloc`      | `(*Unit, I64) -> *Unit`     | Reallocate memory    |
//! | `mem_free`         | `(*Unit) -> Unit`           | Free memory          |
//!
//! ## Memory Operations
//!
//! | Function    | Signature                       | Description          |
//! |-------------|---------------------------------|----------------------|
//! | `mem_copy`  | `(*Unit, *Unit, I64) -> Unit`   | Copy (non-overlapping)|
//! | `mem_move`  | `(*Unit, *Unit, I64) -> Unit`   | Copy (overlapping OK)|
//! | `mem_set`   | `(*Unit, I32, I64) -> Unit`     | Fill with byte       |
//! | `mem_cmp`   | `(*Unit, *Unit, I64) -> I32`    | Compare memory       |
//!
//! These are `lowlevel` functions used by the allocator and collections.

#include "types/env.hpp"

namespace tml::types {

void TypeEnv::init_builtin_mem() {
    SourceSpan builtin_span{};

    // ============ Allocation ============

    // mem_alloc(size: I64) -> *Unit - Allocate memory
    functions_["mem_alloc"].push_back(FuncSig{"mem_alloc",
                                              {make_primitive(PrimitiveKind::I64)},
                                              make_ptr(make_unit()),
                                              {},
                                              false,
                                              builtin_span});

    // mem_alloc_zeroed(size: I64) -> *Unit - Allocate zeroed memory
    functions_["mem_alloc_zeroed"].push_back(FuncSig{"mem_alloc_zeroed",
                                                     {make_primitive(PrimitiveKind::I64)},
                                                     make_ptr(make_unit()),
                                                     {},
                                                     false,
                                                     builtin_span});

    // mem_realloc(ptr: *Unit, new_size: I64) -> *Unit - Reallocate memory
    functions_["mem_realloc"].push_back(
        FuncSig{"mem_realloc",
                {make_ptr(make_unit()), make_primitive(PrimitiveKind::I64)},
                make_ptr(make_unit()),
                {},
                false,
                builtin_span});

    // mem_free(ptr: *Unit) -> Unit - Free memory
    functions_["mem_free"].push_back(
        FuncSig{"mem_free", {make_ptr(make_unit())}, make_unit(), {}, false, builtin_span});

    // ============ Memory Operations ============

    // mem_copy(dest: *Unit, src: *Unit, size: I64) -> Unit
    functions_["mem_copy"].push_back(
        FuncSig{"mem_copy",
                {make_ptr(make_unit()), make_ptr(make_unit()), make_primitive(PrimitiveKind::I64)},
                make_unit(),
                {},
                false,
                builtin_span});

    // mem_move(dest: *Unit, src: *Unit, size: I64) -> Unit
    functions_["mem_move"].push_back(
        FuncSig{"mem_move",
                {make_ptr(make_unit()), make_ptr(make_unit()), make_primitive(PrimitiveKind::I64)},
                make_unit(),
                {},
                false,
                builtin_span});

    // mem_set(ptr: *Unit, value: I32, size: I64) -> Unit
    functions_["mem_set"].push_back(
        FuncSig{"mem_set",
                {make_ptr(make_unit()), make_primitive(PrimitiveKind::I32),
                 make_primitive(PrimitiveKind::I64)},
                make_unit(),
                {},
                false,
                builtin_span});

    // mem_zero(ptr: *Unit, size: I64) -> Unit
    functions_["mem_zero"].push_back(
        FuncSig{"mem_zero",
                {make_ptr(make_unit()), make_primitive(PrimitiveKind::I64)},
                make_unit(),
                {},
                false,
                builtin_span});

    // ============ Memory Comparison ============

    // mem_compare(a: *Unit, b: *Unit, size: I64) -> I32
    functions_["mem_compare"].push_back(
        FuncSig{"mem_compare",
                {make_ptr(make_unit()), make_ptr(make_unit()), make_primitive(PrimitiveKind::I64)},
                make_primitive(PrimitiveKind::I32),
                {},
                false,
                builtin_span});

    // mem_eq(a: *Unit, b: *Unit, size: I64) -> Bool
    functions_["mem_eq"].push_back(
        FuncSig{"mem_eq",
                {make_ptr(make_unit()), make_ptr(make_unit()), make_primitive(PrimitiveKind::I64)},
                make_primitive(PrimitiveKind::Bool),
                {},
                false,
                builtin_span});

    // ============ Simple Allocation (compatibility) ============

    // alloc(size: I64) -> *Unit - Simple allocation (for literal integers that default to I64)
    functions_["alloc"].push_back(FuncSig{"alloc",
                                          {make_primitive(PrimitiveKind::I64)},
                                          make_ptr(make_unit()),
                                          {},
                                          false,
                                          builtin_span});

    // alloc(size: I32) -> *Unit - Simple allocation for tests
    functions_["alloc"].push_back(FuncSig{"alloc",
                                          {make_primitive(PrimitiveKind::I32)},
                                          make_ptr(make_unit()),
                                          {},
                                          false,
                                          builtin_span});

    // dealloc(ptr: *Unit) -> Unit - Simple deallocation for tests
    functions_["dealloc"].push_back(
        FuncSig{"dealloc", {make_ptr(make_unit())}, make_unit(), {}, false, builtin_span});

    // ============ Simple I32 Memory Operations (for tests) ============

    // read_i32(ptr: *Unit) -> I32 - Read I32 from memory
    functions_["read_i32"].push_back(FuncSig{"read_i32",
                                             {make_ptr(make_unit())},
                                             make_primitive(PrimitiveKind::I32),
                                             {},
                                             false,
                                             builtin_span});

    // write_i32(ptr: *Unit, value: I32) -> Unit - Write I32 to memory
    functions_["write_i32"].push_back(
        FuncSig{"write_i32",
                {make_ptr(make_unit()), make_primitive(PrimitiveKind::I32)},
                make_unit(),
                {},
                false,
                builtin_span});

    // ptr_offset(ptr: *Unit, offset: I32) -> *Unit - Offset pointer by elements
    functions_["ptr_offset"].push_back(
        FuncSig{"ptr_offset",
                {make_ptr(make_unit()), make_primitive(PrimitiveKind::I32)},
                make_ptr(make_unit()),
                {},
                false,
                builtin_span});

    // ============ Raw Memory Intrinsics (lowlevel) ============

    // ptr_read[T](ptr: *Unit) -> T - Read value from pointer (generic, resolved at codegen)
    // ptr_write[T](ptr: *Unit, value: T) - Write value to pointer
    // ptr_read_volatile[T](ptr: *Unit) -> T - Volatile read
    // ptr_write_volatile[T](ptr: *Unit, value: T) - Volatile write
    // ptr_read_unaligned[T](ptr: *Unit) -> T - Unaligned read
    // ptr_write_unaligned[T](ptr: *Unit, value: T) - Unaligned write
    // These are generic intrinsics - the type checker registers them with *Unit params
    // but codegen resolves the actual type from type parameters.

    // ptr_read[T](ptr: *Unit) -> I32 (type resolved at codegen)
    functions_["ptr_read"].push_back(FuncSig{"ptr_read",
                                             {make_ptr(make_unit())},
                                             make_primitive(PrimitiveKind::I32),
                                             {},
                                             false,
                                             builtin_span});

    // ptr_write[T](ptr: *Unit, value: I32) -> Unit
    functions_["ptr_write"].push_back(
        FuncSig{"ptr_write",
                {make_ptr(make_unit()), make_primitive(PrimitiveKind::I32)},
                make_unit(),
                {},
                false,
                builtin_span});

    // ptr_read_volatile[T](ptr: *Unit) -> I32
    functions_["ptr_read_volatile"].push_back(FuncSig{"ptr_read_volatile",
                                                      {make_ptr(make_unit())},
                                                      make_primitive(PrimitiveKind::I32),
                                                      {},
                                                      false,
                                                      builtin_span});

    // ptr_write_volatile[T](ptr: *Unit, value: I32) -> Unit
    functions_["ptr_write_volatile"].push_back(
        FuncSig{"ptr_write_volatile",
                {make_ptr(make_unit()), make_primitive(PrimitiveKind::I32)},
                make_unit(),
                {},
                false,
                builtin_span});

    // ptr_read_unaligned[T](ptr: *Unit) -> I32
    functions_["ptr_read_unaligned"].push_back(FuncSig{"ptr_read_unaligned",
                                                       {make_ptr(make_unit())},
                                                       make_primitive(PrimitiveKind::I32),
                                                       {},
                                                       false,
                                                       builtin_span});

    // ptr_write_unaligned[T](ptr: *Unit, value: I32) -> Unit
    functions_["ptr_write_unaligned"].push_back(
        FuncSig{"ptr_write_unaligned",
                {make_ptr(make_unit()), make_primitive(PrimitiveKind::I32)},
                make_unit(),
                {},
                false,
                builtin_span});

    // memcpy(dst: *Unit, src: *Unit, size: I64) -> Unit
    functions_["memcpy"].push_back(
        FuncSig{"memcpy",
                {make_ptr(make_unit()), make_ptr(make_unit()), make_primitive(PrimitiveKind::I64)},
                make_unit(),
                {},
                false,
                builtin_span});

    // memmove(dst: *Unit, src: *Unit, size: I64) -> Unit
    functions_["memmove"].push_back(
        FuncSig{"memmove",
                {make_ptr(make_unit()), make_ptr(make_unit()), make_primitive(PrimitiveKind::I64)},
                make_unit(),
                {},
                false,
                builtin_span});

    // memset(dst: *Unit, value: I32, size: I64) -> Unit
    functions_["memset"].push_back(
        FuncSig{"memset",
                {make_ptr(make_unit()), make_primitive(PrimitiveKind::I32),
                 make_primitive(PrimitiveKind::I64)},
                make_unit(),
                {},
                false,
                builtin_span});

    // ============ Size/Alignment ============

    // size_of[T]() -> I64 - Get size of type (generic, resolved at compile time)
    // align_of[T]() -> I64 - Get alignment of type (generic, resolved at compile time)
    // These are handled specially by the type checker/codegen

    // ============ Direct Memory Store (optimized for hot loops) ============

    // store_byte(ptr: *U8, offset: I64, byte: I32) -> Unit
    // Store a byte at ptr+offset - combines GEP and store for hot loops
    functions_["store_byte"].push_back(
        FuncSig{"store_byte",
                {make_ptr(make_primitive(PrimitiveKind::U8)), make_primitive(PrimitiveKind::I64),
                 make_primitive(PrimitiveKind::I32)},
                make_unit(),
                {},
                false,
                builtin_span});
}

} // namespace tml::types
