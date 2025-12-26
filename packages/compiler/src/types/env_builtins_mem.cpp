// Builtin memory functions
#include "tml/types/env.hpp"

namespace tml::types {

void TypeEnv::init_builtin_mem() {
    SourceSpan builtin_span{};

    // ============ Allocation ============

    // mem_alloc(size: I64) -> *Unit - Allocate memory
    functions_["mem_alloc"].push_back(FuncSig{
        "mem_alloc",
        {make_primitive(PrimitiveKind::I64)},
        make_ptr(make_unit()),
        {},
        false,
        builtin_span
    });

    // mem_alloc_zeroed(size: I64) -> *Unit - Allocate zeroed memory
    functions_["mem_alloc_zeroed"].push_back(FuncSig{
        "mem_alloc_zeroed",
        {make_primitive(PrimitiveKind::I64)},
        make_ptr(make_unit()),
        {},
        false,
        builtin_span
    });

    // mem_realloc(ptr: *Unit, new_size: I64) -> *Unit - Reallocate memory
    functions_["mem_realloc"].push_back(FuncSig{
        "mem_realloc",
        {make_ptr(make_unit()), make_primitive(PrimitiveKind::I64)},
        make_ptr(make_unit()),
        {},
        false,
        builtin_span
    });

    // mem_free(ptr: *Unit) -> Unit - Free memory
    functions_["mem_free"].push_back(FuncSig{
        "mem_free",
        {make_ptr(make_unit())},
        make_unit(),
        {},
        false,
        builtin_span
    });

    // ============ Memory Operations ============

    // mem_copy(dest: *Unit, src: *Unit, size: I64) -> Unit
    functions_["mem_copy"].push_back(FuncSig{
        "mem_copy",
        {make_ptr(make_unit()), make_ptr(make_unit()), make_primitive(PrimitiveKind::I64)},
        make_unit(),
        {},
        false,
        builtin_span
    });

    // mem_move(dest: *Unit, src: *Unit, size: I64) -> Unit
    functions_["mem_move"].push_back(FuncSig{
        "mem_move",
        {make_ptr(make_unit()), make_ptr(make_unit()), make_primitive(PrimitiveKind::I64)},
        make_unit(),
        {},
        false,
        builtin_span
    });

    // mem_set(ptr: *Unit, value: I32, size: I64) -> Unit
    functions_["mem_set"].push_back(FuncSig{
        "mem_set",
        {make_ptr(make_unit()), make_primitive(PrimitiveKind::I32), make_primitive(PrimitiveKind::I64)},
        make_unit(),
        {},
        false,
        builtin_span
    });

    // mem_zero(ptr: *Unit, size: I64) -> Unit
    functions_["mem_zero"].push_back(FuncSig{
        "mem_zero",
        {make_ptr(make_unit()), make_primitive(PrimitiveKind::I64)},
        make_unit(),
        {},
        false,
        builtin_span
    });

    // ============ Memory Comparison ============

    // mem_compare(a: *Unit, b: *Unit, size: I64) -> I32
    functions_["mem_compare"].push_back(FuncSig{
        "mem_compare",
        {make_ptr(make_unit()), make_ptr(make_unit()), make_primitive(PrimitiveKind::I64)},
        make_primitive(PrimitiveKind::I32),
        {},
        false,
        builtin_span
    });

    // mem_eq(a: *Unit, b: *Unit, size: I64) -> Bool
    functions_["mem_eq"].push_back(FuncSig{
        "mem_eq",
        {make_ptr(make_unit()), make_ptr(make_unit()), make_primitive(PrimitiveKind::I64)},
        make_primitive(PrimitiveKind::Bool),
        {},
        false,
        builtin_span
    });

    // ============ Size/Alignment ============

    // size_of[T]() -> I64 - Get size of type (generic, resolved at compile time)
    // align_of[T]() -> I64 - Get alignment of type (generic, resolved at compile time)
    // These are handled specially by the type checker/codegen
}

} // namespace tml::types
