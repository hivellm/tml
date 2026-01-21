//! # MIR Type Implementation
//!
//! This file implements MIR type queries and constructors.
//!
//! ## Type Queries
//!
//! - `is_integer()`: Check if type is signed/unsigned integer
//! - `is_signed()`: Check if integer type is signed
//! - `is_float()`: Check if type is F32 or F64
//! - `bit_width()`: Get bit width of numeric types
//!
//! ## Type Constructors
//!
//! Factory functions for creating MIR types:
//! - `make_unit_type()`, `make_bool_type()`
//! - `make_i8_type()` through `make_i64_type()`
//! - `make_f32_type()`, `make_f64_type()`
//! - `make_pointer_type()`, `make_array_type()`
//! - `make_struct_type()`, `make_enum_type()`

#include "mir/mir.hpp"

namespace tml::mir {

// ============================================================================
// MirType Methods
// ============================================================================

auto MirType::is_integer() const -> bool {
    if (auto* p = std::get_if<MirPrimitiveType>(&kind)) {
        switch (p->kind) {
        case PrimitiveType::I8:
        case PrimitiveType::I16:
        case PrimitiveType::I32:
        case PrimitiveType::I64:
        case PrimitiveType::I128:
        case PrimitiveType::U8:
        case PrimitiveType::U16:
        case PrimitiveType::U32:
        case PrimitiveType::U64:
        case PrimitiveType::U128:
            return true;
        default:
            return false;
        }
    }
    return false;
}

auto MirType::is_float() const -> bool {
    if (auto* p = std::get_if<MirPrimitiveType>(&kind)) {
        return p->kind == PrimitiveType::F32 || p->kind == PrimitiveType::F64;
    }
    return false;
}

auto MirType::is_signed() const -> bool {
    if (auto* p = std::get_if<MirPrimitiveType>(&kind)) {
        switch (p->kind) {
        case PrimitiveType::I8:
        case PrimitiveType::I16:
        case PrimitiveType::I32:
        case PrimitiveType::I64:
        case PrimitiveType::I128:
            return true;
        default:
            return false;
        }
    }
    return false;
}

auto MirType::bit_width() const -> int {
    if (auto* p = std::get_if<MirPrimitiveType>(&kind)) {
        switch (p->kind) {
        case PrimitiveType::Bool:
            return 1;
        case PrimitiveType::I8:
        case PrimitiveType::U8:
            return 8;
        case PrimitiveType::I16:
        case PrimitiveType::U16:
            return 16;
        case PrimitiveType::I32:
        case PrimitiveType::U32:
        case PrimitiveType::F32:
            return 32;
        case PrimitiveType::I64:
        case PrimitiveType::U64:
        case PrimitiveType::F64:
        case PrimitiveType::Ptr:
            return 64;
        case PrimitiveType::I128:
        case PrimitiveType::U128:
            return 128;
        default:
            return 0;
        }
    }
    return 0;
}

// ============================================================================
// Type Constructors
// ============================================================================

auto make_unit_type() -> MirTypePtr {
    auto type = std::make_shared<MirType>();
    type->kind = MirPrimitiveType{PrimitiveType::Unit};
    return type;
}

auto make_bool_type() -> MirTypePtr {
    auto type = std::make_shared<MirType>();
    type->kind = MirPrimitiveType{PrimitiveType::Bool};
    return type;
}

auto make_i8_type() -> MirTypePtr {
    auto type = std::make_shared<MirType>();
    type->kind = MirPrimitiveType{PrimitiveType::I8};
    return type;
}

auto make_i16_type() -> MirTypePtr {
    auto type = std::make_shared<MirType>();
    type->kind = MirPrimitiveType{PrimitiveType::I16};
    return type;
}

auto make_i32_type() -> MirTypePtr {
    auto type = std::make_shared<MirType>();
    type->kind = MirPrimitiveType{PrimitiveType::I32};
    return type;
}

auto make_i64_type() -> MirTypePtr {
    auto type = std::make_shared<MirType>();
    type->kind = MirPrimitiveType{PrimitiveType::I64};
    return type;
}

auto make_f32_type() -> MirTypePtr {
    auto type = std::make_shared<MirType>();
    type->kind = MirPrimitiveType{PrimitiveType::F32};
    return type;
}

auto make_f64_type() -> MirTypePtr {
    auto type = std::make_shared<MirType>();
    type->kind = MirPrimitiveType{PrimitiveType::F64};
    return type;
}

auto make_ptr_type() -> MirTypePtr {
    auto type = std::make_shared<MirType>();
    type->kind = MirPrimitiveType{PrimitiveType::Ptr};
    return type;
}

auto make_str_type() -> MirTypePtr {
    auto type = std::make_shared<MirType>();
    type->kind = MirPrimitiveType{PrimitiveType::Str};
    return type;
}

auto make_pointer_type(MirTypePtr pointee, bool is_mut) -> MirTypePtr {
    auto type = std::make_shared<MirType>();
    type->kind = MirPointerType{std::move(pointee), is_mut};
    return type;
}

auto make_array_type(MirTypePtr element, size_t size) -> MirTypePtr {
    auto type = std::make_shared<MirType>();
    type->kind = MirArrayType{std::move(element), size};
    return type;
}

auto make_tuple_type(std::vector<MirTypePtr> elements) -> MirTypePtr {
    auto type = std::make_shared<MirType>();
    type->kind = MirTupleType{std::move(elements)};
    return type;
}

auto make_struct_type(const std::string& name, std::vector<MirTypePtr> type_args) -> MirTypePtr {
    auto type = std::make_shared<MirType>();
    type->kind = MirStructType{name, std::move(type_args)};
    return type;
}

auto make_enum_type(const std::string& name, std::vector<MirTypePtr> type_args) -> MirTypePtr {
    auto type = std::make_shared<MirType>();
    type->kind = MirEnumType{name, std::move(type_args)};
    return type;
}

auto make_vector_type(MirTypePtr element, size_t width) -> MirTypePtr {
    auto type = std::make_shared<MirType>();
    type->kind = MirVectorType{std::move(element), width};
    return type;
}

} // namespace tml::mir
