// MIR Serializer Internal Header
// Shared types and utilities for MIR serialization

#pragma once

#include "mir/mir.hpp"
#include "mir/mir_serialize.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace tml::mir {

// ============================================================================
// Binary Format Type Tags
// ============================================================================

enum class TypeTag : uint8_t {
    Primitive = 0,
    Pointer = 1,
    Array = 2,
    Slice = 3,
    Tuple = 4,
    Struct = 5,
    Enum = 6,
    Function = 7,
};

enum class InstTag : uint8_t {
    Binary = 0,
    Unary = 1,
    Load = 2,
    Store = 3,
    Alloca = 4,
    Gep = 5,
    ExtractValue = 6,
    InsertValue = 7,
    Call = 8,
    MethodCall = 9,
    Cast = 10,
    Phi = 11,
    Constant = 12,
    Select = 13,
    StructInit = 14,
    EnumInit = 15,
    TupleInit = 16,
    ArrayInit = 17,
};

enum class TermTag : uint8_t {
    Return = 0,
    Branch = 1,
    CondBranch = 2,
    Switch = 3,
    Unreachable = 4,
};

enum class ConstTag : uint8_t {
    Int = 0,
    Float = 1,
    Bool = 2,
    String = 3,
    Unit = 4,
};

} // namespace tml::mir
