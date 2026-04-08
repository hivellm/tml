//! # TypeEnv Binary Deserializer Implementation
//!
//! Mirrors the writer in `lib/std/src/serial/typeenv.tml`. Every `write_X(w, ...)`
//! function in that file has a corresponding `read_X(c)` here that consumes
//! the same bytes in the same order.

#include "serial/typeenv_reader.hpp"

#include "types/env.hpp"
#include "types/type.hpp"

#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

namespace tml::serial {

namespace {

using namespace tml::types;

// ============================================================================
// Constants — must match lib/std/src/serial/typeenv.tml
// ============================================================================

constexpr uint32_t TYPEENV_MAGIC = 0x54454E56; // "TENV"
constexpr uint8_t TYPEENV_VERSION_MAJOR = 1;

// Semantic type tags
constexpr uint8_t TAG_PRIMITIVE = 0x00;
constexpr uint8_t TAG_NAMED = 0x01;
constexpr uint8_t TAG_REF = 0x02;
constexpr uint8_t TAG_PTR = 0x03;
constexpr uint8_t TAG_ARRAY = 0x04;
constexpr uint8_t TAG_SLICE = 0x05;
constexpr uint8_t TAG_TUPLE = 0x06;
constexpr uint8_t TAG_FUNC = 0x07;
constexpr uint8_t TAG_CLOSURE = 0x08;
constexpr uint8_t TAG_GENERIC = 0x09;
constexpr uint8_t TAG_CONST_GENERIC = 0x0A;
constexpr uint8_t TAG_DYN_BEHAVIOR = 0x0B;
constexpr uint8_t TAG_IMPL_BEHAVIOR = 0x0C;
constexpr uint8_t TAG_CLASS = 0x0D;
constexpr uint8_t TAG_INTERFACE = 0x0E;
constexpr uint8_t TAG_TYPEVAR = 0x0F;
constexpr uint8_t TAG_NULL = 0x10;

// Primitive kind values — must match PrimitiveKind enum order
constexpr uint8_t PRIM_I8 = 0;
constexpr uint8_t PRIM_I16 = 1;
constexpr uint8_t PRIM_I32 = 2;
constexpr uint8_t PRIM_I64 = 3;
constexpr uint8_t PRIM_I128 = 4;
constexpr uint8_t PRIM_U8 = 5;
constexpr uint8_t PRIM_U16 = 6;
constexpr uint8_t PRIM_U32 = 7;
constexpr uint8_t PRIM_U64 = 8;
constexpr uint8_t PRIM_U128 = 9;
constexpr uint8_t PRIM_F32 = 10;
constexpr uint8_t PRIM_F64 = 11;
constexpr uint8_t PRIM_BOOL = 12;
constexpr uint8_t PRIM_CHAR = 13;
constexpr uint8_t PRIM_STR = 14;
constexpr uint8_t PRIM_UNIT = 15;
constexpr uint8_t PRIM_NEVER = 16;

// Stability levels
constexpr uint8_t STABILITY_UNSTABLE = 0;
constexpr uint8_t STABILITY_STABLE = 1;
constexpr uint8_t STABILITY_DEPRECATED = 2;

// ============================================================================
// Cursor — primitive byte reader
// ============================================================================

struct Cursor {
    const uint8_t* data;
    size_t pos = 0;
    size_t len;

    explicit Cursor(const std::vector<uint8_t>& v) : data(v.data()), len(v.size()) {}

    [[noreturn]] void fail(const char* msg) const {
        throw std::runtime_error(std::string("typeenv_reader: ") + msg + " at offset " +
                                 std::to_string(pos));
    }

    void need(size_t n) const {
        if (pos + n > len) {
            fail("unexpected end of input");
        }
    }

    uint8_t read_u8() {
        need(1);
        return data[pos++];
    }

    uint32_t read_u32_le() {
        need(4);
        uint32_t v = static_cast<uint32_t>(data[pos]) |
                     (static_cast<uint32_t>(data[pos + 1]) << 8) |
                     (static_cast<uint32_t>(data[pos + 2]) << 16) |
                     (static_cast<uint32_t>(data[pos + 3]) << 24);
        pos += 4;
        return v;
    }

    uint64_t read_u64_le() {
        need(8);
        uint64_t v = 0;
        for (int i = 0; i < 8; i++) {
            v |= static_cast<uint64_t>(data[pos + i]) << (i * 8);
        }
        pos += 8;
        return v;
    }

    uint64_t read_varint() {
        uint64_t result = 0;
        int shift = 0;
        for (int i = 0; i < 10; i++) {
            need(1);
            uint8_t byte = data[pos++];
            result |= static_cast<uint64_t>(byte & 0x7F) << shift;
            if ((byte & 0x80) == 0) {
                return result;
            }
            shift += 7;
        }
        fail("varint too long");
    }

    bool read_bool() {
        return read_u8() != 0;
    }

    std::string read_str() {
        uint64_t n = read_varint();
        need(static_cast<size_t>(n));
        std::string s(reinterpret_cast<const char*>(data + pos), static_cast<size_t>(n));
        pos += static_cast<size_t>(n);
        return s;
    }

    std::optional<std::string> read_opt_str() {
        std::string s = read_str();
        if (s.empty()) {
            return std::nullopt;
        }
        return s;
    }
};

// ============================================================================
// Primitive kind conversion
// ============================================================================

PrimitiveKind to_primitive_kind(Cursor& c, uint8_t kind) {
    switch (kind) {
    case PRIM_I8:
        return PrimitiveKind::I8;
    case PRIM_I16:
        return PrimitiveKind::I16;
    case PRIM_I32:
        return PrimitiveKind::I32;
    case PRIM_I64:
        return PrimitiveKind::I64;
    case PRIM_I128:
        return PrimitiveKind::I128;
    case PRIM_U8:
        return PrimitiveKind::U8;
    case PRIM_U16:
        return PrimitiveKind::U16;
    case PRIM_U32:
        return PrimitiveKind::U32;
    case PRIM_U64:
        return PrimitiveKind::U64;
    case PRIM_U128:
        return PrimitiveKind::U128;
    case PRIM_F32:
        return PrimitiveKind::F32;
    case PRIM_F64:
        return PrimitiveKind::F64;
    case PRIM_BOOL:
        return PrimitiveKind::Bool;
    case PRIM_CHAR:
        return PrimitiveKind::Char;
    case PRIM_STR:
        return PrimitiveKind::Str;
    case PRIM_UNIT:
        return PrimitiveKind::Unit;
    case PRIM_NEVER:
        return PrimitiveKind::Never;
    default:
        c.fail("unknown primitive kind");
    }
}

StabilityLevel to_stability(uint8_t val) {
    switch (val) {
    case STABILITY_STABLE:
        return StabilityLevel::Stable;
    case STABILITY_DEPRECATED:
        return StabilityLevel::Deprecated;
    default:
        return StabilityLevel::Unstable;
    }
}

MemberVisibility to_visibility(uint8_t val) {
    switch (val) {
    case 1:
        return MemberVisibility::Protected;
    case 2:
        return MemberVisibility::Public;
    default:
        return MemberVisibility::Private;
    }
}

// ============================================================================
// Forward declarations
// ============================================================================

TypePtr read_sem_type(Cursor& c);
std::vector<TypePtr> read_sem_type_list(Cursor& c);
std::vector<std::string> read_str_list(Cursor& c);
ConstGenericParam read_const_generic_param(Cursor& c);
std::vector<ConstGenericParam> read_const_generic_param_list(Cursor& c);
WhereConstraint read_where_constraint(Cursor& c);
FuncSig read_func_sig(Cursor& c);
StructDef read_struct_def(Cursor& c);
EnumDef read_enum_def(Cursor& c);
BehaviorDef read_behavior_def(Cursor& c);
ClassDef read_class_def(Cursor& c);
InterfaceDef read_interface_def(Cursor& c);

// ============================================================================
// Semantic type deserialization
// ============================================================================

TypePtr read_sem_type(Cursor& c) {
    uint8_t tag = c.read_u8();
    switch (tag) {
    case TAG_PRIMITIVE: {
        uint8_t kind = c.read_u8();
        return make_primitive(to_primitive_kind(c, kind));
    }
    case TAG_NAMED: {
        std::string name = c.read_str();
        std::string module_path = c.read_str();
        uint64_t arg_count = c.read_varint();
        std::vector<TypePtr> type_args;
        type_args.reserve(arg_count);
        for (uint64_t i = 0; i < arg_count; i++) {
            type_args.push_back(read_sem_type(c));
        }
        return std::make_shared<Type>(
            Type{NamedType{std::move(name), std::move(module_path), std::move(type_args)}});
    }
    case TAG_REF: {
        bool is_mut = c.read_bool();
        TypePtr inner = read_sem_type(c);
        auto lifetime = c.read_opt_str();
        return std::make_shared<Type>(Type{RefType{is_mut, std::move(inner), std::move(lifetime)}});
    }
    case TAG_PTR: {
        bool is_mut = c.read_bool();
        TypePtr inner = read_sem_type(c);
        return std::make_shared<Type>(Type{PtrType{is_mut, std::move(inner)}});
    }
    case TAG_ARRAY: {
        TypePtr element = read_sem_type(c);
        uint64_t size = c.read_u64_le();
        auto const_param = c.read_opt_str();
        std::string param_str = const_param.value_or("");
        return std::make_shared<Type>(
            Type{ArrayType{std::move(element), static_cast<size_t>(size), std::move(param_str)}});
    }
    case TAG_SLICE: {
        TypePtr element = read_sem_type(c);
        return std::make_shared<Type>(Type{SliceType{std::move(element)}});
    }
    case TAG_TUPLE: {
        uint64_t count = c.read_varint();
        std::vector<TypePtr> elements;
        elements.reserve(count);
        for (uint64_t i = 0; i < count; i++) {
            elements.push_back(read_sem_type(c));
        }
        return make_tuple(std::move(elements));
    }
    case TAG_FUNC: {
        uint64_t param_count = c.read_varint();
        std::vector<TypePtr> params;
        params.reserve(param_count);
        for (uint64_t i = 0; i < param_count; i++) {
            params.push_back(read_sem_type(c));
        }
        TypePtr ret = read_sem_type(c);
        bool is_async = c.read_bool();
        auto type =
            std::make_shared<Type>(Type{FuncType{std::move(params), std::move(ret), is_async}});
        return type;
    }
    case TAG_CLOSURE: {
        uint64_t param_count = c.read_varint();
        std::vector<TypePtr> params;
        params.reserve(param_count);
        for (uint64_t i = 0; i < param_count; i++) {
            params.push_back(read_sem_type(c));
        }
        TypePtr ret = read_sem_type(c);
        uint64_t cap_count = c.read_varint();
        std::vector<CapturedVar> captures;
        captures.reserve(cap_count);
        for (uint64_t i = 0; i < cap_count; i++) {
            std::string name = c.read_str();
            TypePtr type = read_sem_type(c);
            bool is_mut = c.read_bool();
            captures.push_back(CapturedVar{std::move(name), std::move(type), is_mut});
        }
        return std::make_shared<Type>(
            Type{ClosureType{std::move(params), std::move(ret), std::move(captures)}});
    }
    case TAG_GENERIC: {
        std::string name = c.read_str();
        uint64_t bound_count = c.read_varint();
        std::vector<TypePtr> bounds;
        bounds.reserve(bound_count);
        for (uint64_t i = 0; i < bound_count; i++) {
            bounds.push_back(read_sem_type(c));
        }
        return std::make_shared<Type>(Type{GenericType{std::move(name), std::move(bounds)}});
    }
    case TAG_CONST_GENERIC: {
        std::string name = c.read_str();
        TypePtr value_type = read_sem_type(c);
        bool has_resolved = c.read_bool();
        std::optional<int64_t> resolved;
        if (has_resolved) {
            resolved = static_cast<int64_t>(c.read_u64_le());
        }
        return std::make_shared<Type>(
            Type{ConstGenericType{std::move(name), std::move(value_type), resolved}});
    }
    case TAG_DYN_BEHAVIOR: {
        std::string behavior_name = c.read_str();
        uint64_t arg_count = c.read_varint();
        std::vector<TypePtr> type_args;
        type_args.reserve(arg_count);
        for (uint64_t i = 0; i < arg_count; i++) {
            type_args.push_back(read_sem_type(c));
        }
        bool is_mut = c.read_bool();
        return std::make_shared<Type>(
            Type{DynBehaviorType{std::move(behavior_name), std::move(type_args), is_mut}});
    }
    case TAG_IMPL_BEHAVIOR: {
        std::string behavior_name = c.read_str();
        uint64_t arg_count = c.read_varint();
        std::vector<TypePtr> type_args;
        type_args.reserve(arg_count);
        for (uint64_t i = 0; i < arg_count; i++) {
            type_args.push_back(read_sem_type(c));
        }
        return std::make_shared<Type>(
            Type{ImplBehaviorType{std::move(behavior_name), std::move(type_args)}});
    }
    case TAG_CLASS: {
        std::string name = c.read_str();
        std::string module_path = c.read_str();
        uint64_t arg_count = c.read_varint();
        std::vector<TypePtr> type_args;
        type_args.reserve(arg_count);
        for (uint64_t i = 0; i < arg_count; i++) {
            type_args.push_back(read_sem_type(c));
        }
        return std::make_shared<Type>(
            Type{ClassType{std::move(name), std::move(module_path), std::move(type_args)}});
    }
    case TAG_INTERFACE: {
        std::string name = c.read_str();
        std::string module_path = c.read_str();
        uint64_t arg_count = c.read_varint();
        std::vector<TypePtr> type_args;
        type_args.reserve(arg_count);
        for (uint64_t i = 0; i < arg_count; i++) {
            type_args.push_back(read_sem_type(c));
        }
        return std::make_shared<Type>(
            Type{InterfaceType{std::move(name), std::move(module_path), std::move(type_args)}});
    }
    case TAG_TYPEVAR: {
        uint32_t id = c.read_u32_le();
        return std::make_shared<Type>(Type{TypeVar{id}});
    }
    case TAG_NULL: {
        return nullptr;
    }
    default:
        c.fail("unknown semantic type tag");
    }
}

std::vector<TypePtr> read_sem_type_list(Cursor& c) {
    uint64_t count = c.read_varint();
    std::vector<TypePtr> result;
    result.reserve(count);
    for (uint64_t i = 0; i < count; i++) {
        result.push_back(read_sem_type(c));
    }
    return result;
}

// ============================================================================
// String list
// ============================================================================

std::vector<std::string> read_str_list(Cursor& c) {
    uint64_t count = c.read_varint();
    std::vector<std::string> result;
    result.reserve(count);
    for (uint64_t i = 0; i < count; i++) {
        result.push_back(c.read_str());
    }
    return result;
}

// ============================================================================
// ConstGenericParam
// ============================================================================

ConstGenericParam read_const_generic_param(Cursor& c) {
    std::string name = c.read_str();
    TypePtr value_type = read_sem_type(c);
    return ConstGenericParam{std::move(name), std::move(value_type)};
}

std::vector<ConstGenericParam> read_const_generic_param_list(Cursor& c) {
    uint64_t count = c.read_varint();
    std::vector<ConstGenericParam> result;
    result.reserve(count);
    for (uint64_t i = 0; i < count; i++) {
        result.push_back(read_const_generic_param(c));
    }
    return result;
}

// ============================================================================
// WhereConstraint
// ============================================================================

BoundConstraint read_bound_constraint(Cursor& c) {
    std::string behavior_name = c.read_str();
    auto type_args = read_sem_type_list(c);
    return BoundConstraint{std::move(behavior_name), std::move(type_args)};
}

HigherRankedBound read_higher_ranked_bound(Cursor& c) {
    auto bound_type_params = read_str_list(c);
    std::string behavior_name = c.read_str();
    auto type_args = read_sem_type_list(c);
    return HigherRankedBound{std::move(bound_type_params), std::move(behavior_name),
                             std::move(type_args)};
}

WhereConstraint read_where_constraint(Cursor& c) {
    std::string type_param = c.read_str();
    auto required_behaviors = read_str_list(c);
    // Parameterized bounds
    uint64_t pb_count = c.read_varint();
    std::vector<BoundConstraint> param_bounds;
    param_bounds.reserve(pb_count);
    for (uint64_t i = 0; i < pb_count; i++) {
        param_bounds.push_back(read_bound_constraint(c));
    }
    // Higher-ranked bounds
    uint64_t hrb_count = c.read_varint();
    std::vector<HigherRankedBound> hr_bounds;
    hr_bounds.reserve(hrb_count);
    for (uint64_t i = 0; i < hrb_count; i++) {
        hr_bounds.push_back(read_higher_ranked_bound(c));
    }
    return WhereConstraint{std::move(type_param), std::move(required_behaviors),
                           std::move(param_bounds), std::move(hr_bounds)};
}

// ============================================================================
// FuncSig
// ============================================================================

FuncSig read_func_sig(Cursor& c) {
    FuncSig sig;
    sig.name = c.read_str();
    // Params as TypePtr list
    uint64_t param_count = c.read_varint();
    sig.params.reserve(param_count);
    for (uint64_t i = 0; i < param_count; i++) {
        sig.params.push_back(read_sem_type(c));
    }
    sig.return_type = read_sem_type(c);
    sig.type_params = read_str_list(c);
    sig.is_async = c.read_bool();
    sig.stability = to_stability(c.read_u8());
    sig.deprecated_message = c.read_str();
    sig.since_version = c.read_str();
    // Where constraints
    uint64_t wc_count = c.read_varint();
    sig.where_constraints.reserve(wc_count);
    for (uint64_t i = 0; i < wc_count; i++) {
        sig.where_constraints.push_back(read_where_constraint(c));
    }
    sig.is_lowlevel = c.read_bool();
    sig.is_intrinsic = c.read_bool();
    sig.extern_abi = c.read_opt_str();
    sig.extern_name = c.read_opt_str();
    sig.link_libs = read_str_list(c);
    sig.ffi_module = c.read_opt_str();
    sig.const_params = read_const_generic_param_list(c);
    return sig;
}

// ============================================================================
// StructDef
// ============================================================================

StructFieldDef read_struct_field(Cursor& c) {
    std::string name = c.read_str();
    TypePtr type = read_sem_type(c);
    bool has_default = c.read_bool();
    return StructFieldDef{std::move(name), std::move(type), has_default};
}

StructDef read_struct_def(Cursor& c) {
    StructDef def;
    def.name = c.read_str();
    def.type_params = read_str_list(c);
    def.const_params = read_const_generic_param_list(c);
    // Fields
    uint64_t field_count = c.read_varint();
    def.fields.reserve(field_count);
    for (uint64_t i = 0; i < field_count; i++) {
        def.fields.push_back(read_struct_field(c));
    }
    def.is_interior_mutable = c.read_bool();
    def.is_union = c.read_bool();
    def.is_simd = c.read_bool();
    return def;
}

// ============================================================================
// EnumDef
// ============================================================================

EnumDef read_enum_def(Cursor& c) {
    EnumDef def;
    def.name = c.read_str();
    def.type_params = read_str_list(c);
    def.const_params = read_const_generic_param_list(c);
    // Variants
    uint64_t var_count = c.read_varint();
    def.variants.reserve(var_count);
    for (uint64_t i = 0; i < var_count; i++) {
        std::string vname = c.read_str();
        auto payload_types = read_sem_type_list(c);
        def.variants.emplace_back(std::move(vname), std::move(payload_types));
    }
    def.is_flags = c.read_bool();
    def.flags_underlying_type = c.read_str();
    // Discriminant values
    uint64_t disc_count = c.read_varint();
    def.discriminant_values.reserve(disc_count);
    for (uint64_t i = 0; i < disc_count; i++) {
        def.discriminant_values.push_back(c.read_u64_le());
    }
    return def;
}

// ============================================================================
// BehaviorDef
// ============================================================================

AssociatedTypeDef read_associated_type(Cursor& c) {
    AssociatedTypeDef at;
    at.name = c.read_str();
    at.type_params = read_str_list(c);
    at.bounds = read_str_list(c);
    bool has_default = c.read_bool();
    if (has_default) {
        at.default_type = read_sem_type(c);
    }
    return at;
}

BehaviorDef read_behavior_def(Cursor& c) {
    BehaviorDef def;
    def.name = c.read_str();
    def.type_params = read_str_list(c);
    def.const_params = read_const_generic_param_list(c);
    // Associated types
    uint64_t at_count = c.read_varint();
    def.associated_types.reserve(at_count);
    for (uint64_t i = 0; i < at_count; i++) {
        def.associated_types.push_back(read_associated_type(c));
    }
    // Methods
    uint64_t method_count = c.read_varint();
    def.methods.reserve(method_count);
    for (uint64_t i = 0; i < method_count; i++) {
        def.methods.push_back(read_func_sig(c));
    }
    def.super_behaviors = read_str_list(c);
    auto mwd = read_str_list(c);
    def.methods_with_defaults = std::set<std::string>(mwd.begin(), mwd.end());
    return def;
}

// ============================================================================
// ClassDef
// ============================================================================

ClassFieldDef read_class_field(Cursor& c) {
    ClassFieldDef f;
    f.name = c.read_str();
    f.type = read_sem_type(c);
    f.vis = to_visibility(c.read_u8());
    f.is_static = c.read_bool();
    return f;
}

ClassMethodDef read_class_method(Cursor& c) {
    ClassMethodDef m;
    m.sig = read_func_sig(c);
    m.vis = to_visibility(c.read_u8());
    m.is_static = c.read_bool();
    m.is_virtual = c.read_bool();
    m.is_override = c.read_bool();
    m.is_abstract = c.read_bool();
    m.is_final = c.read_bool();
    m.vtable_index = static_cast<size_t>(c.read_varint());
    return m;
}

PropertyDef read_property(Cursor& c) {
    PropertyDef p;
    p.name = c.read_str();
    p.type = read_sem_type(c);
    p.vis = to_visibility(c.read_u8());
    p.is_static = c.read_bool();
    p.has_getter = c.read_bool();
    p.has_setter = c.read_bool();
    return p;
}

ConstructorDef read_constructor(Cursor& c) {
    ConstructorDef ctor;
    ctor.params = read_sem_type_list(c);
    ctor.vis = to_visibility(c.read_u8());
    ctor.calls_base = c.read_bool();
    return ctor;
}

ClassDef read_class_def(Cursor& c) {
    ClassDef def;
    def.name = c.read_str();
    def.type_params = read_str_list(c);
    def.const_params = read_const_generic_param_list(c);
    def.base_class = c.read_opt_str();
    def.interfaces = read_str_list(c);
    // Fields
    uint64_t f_count = c.read_varint();
    def.fields.reserve(f_count);
    for (uint64_t i = 0; i < f_count; i++) {
        def.fields.push_back(read_class_field(c));
    }
    // Methods
    uint64_t m_count = c.read_varint();
    def.methods.reserve(m_count);
    for (uint64_t i = 0; i < m_count; i++) {
        def.methods.push_back(read_class_method(c));
    }
    // Properties
    uint64_t p_count = c.read_varint();
    def.properties.reserve(p_count);
    for (uint64_t i = 0; i < p_count; i++) {
        def.properties.push_back(read_property(c));
    }
    // Constructors
    uint64_t ctor_count = c.read_varint();
    def.constructors.reserve(ctor_count);
    for (uint64_t i = 0; i < ctor_count; i++) {
        def.constructors.push_back(read_constructor(c));
    }
    def.is_abstract = c.read_bool();
    def.is_sealed = c.read_bool();
    def.is_value = c.read_bool();
    def.is_pooled = c.read_bool();
    def.stack_allocatable = c.read_bool();
    def.estimated_size = static_cast<size_t>(c.read_varint());
    def.inheritance_depth = static_cast<size_t>(c.read_varint());
    return def;
}

// ============================================================================
// InterfaceDef
// ============================================================================

InterfaceMethodDef read_interface_method(Cursor& c) {
    InterfaceMethodDef m;
    m.sig = read_func_sig(c);
    m.is_static = c.read_bool();
    m.has_default = c.read_bool();
    return m;
}

InterfaceDef read_interface_def(Cursor& c) {
    InterfaceDef def;
    def.name = c.read_str();
    def.type_params = read_str_list(c);
    def.const_params = read_const_generic_param_list(c);
    def.extends = read_str_list(c);
    // Methods
    uint64_t m_count = c.read_varint();
    def.methods.reserve(m_count);
    for (uint64_t i = 0; i < m_count; i++) {
        def.methods.push_back(read_interface_method(c));
    }
    return def;
}

} // anonymous namespace

// ============================================================================
// Public API
// ============================================================================

types::TypeEnv read_typeenv(const std::vector<uint8_t>& data) {
    Cursor c(data);

    // -- Header --
    uint32_t magic = c.read_u32_le();
    if (magic != TYPEENV_MAGIC) {
        c.fail("invalid magic (expected 0x54454E56 'TENV')");
    }
    uint8_t major = c.read_u8();
    if (major != TYPEENV_VERSION_MAJOR) {
        c.fail("unsupported major version");
    }
    // Minor version — forward compatible, just consume.
    c.read_u8();

    types::TypeEnv env;

    // -- Structs --
    {
        uint64_t count = c.read_varint();
        for (uint64_t i = 0; i < count; i++) {
            env.define_struct(read_struct_def(c));
        }
    }

    // -- Enums --
    {
        uint64_t count = c.read_varint();
        for (uint64_t i = 0; i < count; i++) {
            env.define_enum(read_enum_def(c));
        }
    }

    // -- Behaviors --
    {
        uint64_t count = c.read_varint();
        for (uint64_t i = 0; i < count; i++) {
            env.define_behavior(read_behavior_def(c));
        }
    }

    // -- Functions (grouped with overloads) --
    {
        uint64_t group_count = c.read_varint();
        for (uint64_t g = 0; g < group_count; g++) {
            c.read_str(); // name (redundant — FuncSig carries it)
            uint64_t overload_count = c.read_varint();
            for (uint64_t i = 0; i < overload_count; i++) {
                env.define_func(read_func_sig(c));
            }
        }
    }

    // -- Behavior impls --
    {
        uint64_t count = c.read_varint();
        for (uint64_t i = 0; i < count; i++) {
            std::string type_name = c.read_str();
            auto behaviors = read_str_list(c);
            for (const auto& bname : behaviors) {
                env.register_impl(type_name, bname);
            }
        }
    }

    // -- Type aliases --
    {
        uint64_t count = c.read_varint();
        for (uint64_t i = 0; i < count; i++) {
            std::string name = c.read_str();
            TypePtr type = read_sem_type(c);
            auto generics = read_str_list(c);
            env.define_type_alias(name, type, std::move(generics));
        }
    }

    // -- Classes --
    {
        uint64_t count = c.read_varint();
        for (uint64_t i = 0; i < count; i++) {
            env.define_class(read_class_def(c));
        }
    }

    // -- Interfaces --
    {
        uint64_t count = c.read_varint();
        for (uint64_t i = 0; i < count; i++) {
            env.define_interface(read_interface_def(c));
        }
    }

    // -- Class-interface impls --
    {
        uint64_t count = c.read_varint();
        for (uint64_t i = 0; i < count; i++) {
            std::string class_name = c.read_str();
            auto ifaces = read_str_list(c);
            for (const auto& iname : ifaces) {
                env.register_class_interface(class_name, iname);
            }
        }
    }

    // -- Builtins: skip, TypeEnv::init_builtins() handles these --
    {
        uint64_t count = c.read_varint();
        for (uint64_t i = 0; i < count; i++) {
            c.read_str();     // name
            read_sem_type(c); // type — consumed but not registered (builtins are pre-loaded)
        }
    }

    return env;
}

} // namespace tml::serial
