TML_MODULE("compiler")

//! # Binary Module Metadata Deserialization — Implementation
//!
//! ModuleBinaryReader and convenience functions for loading/preloading
//! module metadata from .tml.meta cache files.
//!
//! Split from module_binary.cpp (which contains the Writer half).

#include "common/crc32c.hpp"
#include "log/log.hpp"
#include "types/env.hpp"
#include "types/module_binary.hpp"
#include "types/type.hpp"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <sstream>

namespace tml::types {

namespace fs = std::filesystem;

// ============================================================================
// Type Deserialization (string -> TypePtr)
// ============================================================================
//
// These static helpers are duplicated from module_binary.cpp because they are
// needed by ModuleBinaryReader::read_type() and are file-local (static).

// Helper to split comma-separated type arguments respecting bracket nesting
static std::vector<std::string> split_type_args(const std::string& str) {
    std::vector<std::string> args;
    int depth = 0;
    size_t start = 0;
    for (size_t i = 0; i < str.size(); ++i) {
        if (str[i] == '[' || str[i] == '(')
            depth++;
        else if (str[i] == ']' || str[i] == ')')
            depth--;
        else if (str[i] == ',' && depth == 0) {
            std::string arg = str.substr(start, i - start);
            // Trim whitespace
            size_t s = arg.find_first_not_of(' ');
            size_t e = arg.find_last_not_of(' ');
            if (s != std::string::npos) {
                args.push_back(arg.substr(s, e - s + 1));
            }
            start = i + 1;
        }
    }
    // Last argument
    if (start < str.size()) {
        std::string arg = str.substr(start);
        size_t s = arg.find_first_not_of(' ');
        size_t e = arg.find_last_not_of(' ');
        if (s != std::string::npos) {
            args.push_back(arg.substr(s, e - s + 1));
        }
    }
    return args;
}

static TypePtr deserialize_type_string(const std::string& str) {
    if (str.empty() || str == "<null>" || str == "Unit" || str == "()")
        return make_unit();

    // Primitive types
    if (str == "I8")
        return make_primitive(PrimitiveKind::I8);
    if (str == "I16")
        return make_primitive(PrimitiveKind::I16);
    if (str == "I32")
        return make_primitive(PrimitiveKind::I32);
    if (str == "I64")
        return make_primitive(PrimitiveKind::I64);
    if (str == "I128")
        return make_primitive(PrimitiveKind::I128);
    if (str == "U8")
        return make_primitive(PrimitiveKind::U8);
    if (str == "U16")
        return make_primitive(PrimitiveKind::U16);
    if (str == "U32")
        return make_primitive(PrimitiveKind::U32);
    if (str == "U64")
        return make_primitive(PrimitiveKind::U64);
    if (str == "U128")
        return make_primitive(PrimitiveKind::U128);
    if (str == "F32")
        return make_primitive(PrimitiveKind::F32);
    if (str == "F64")
        return make_primitive(PrimitiveKind::F64);
    if (str == "Bool")
        return make_primitive(PrimitiveKind::Bool);
    if (str == "Char")
        return make_primitive(PrimitiveKind::Char);
    if (str == "Str")
        return make_primitive(PrimitiveKind::Str);
    if (str == "Usize")
        return make_primitive(PrimitiveKind::U64);
    if (str == "Isize")
        return make_primitive(PrimitiveKind::I64);

    // Reference types
    if (str.starts_with("mut ref ")) {
        auto inner = deserialize_type_string(str.substr(8));
        return std::make_shared<Type>(Type{RefType{true, inner}});
    }
    if (str.starts_with("ref ")) {
        auto inner = deserialize_type_string(str.substr(4));
        return std::make_shared<Type>(Type{RefType{false, inner}});
    }

    // Dynamic behavior types: dyn Behavior, dyn mut Behavior, dyn Behavior[T]
    if (str.starts_with("dyn mut ")) {
        std::string rest = str.substr(8);
        // Check for generic args: dyn mut Behavior[T]
        std::string behavior_name = rest;
        std::vector<TypePtr> type_args;
        auto bracket_pos = rest.find('[');
        if (bracket_pos != std::string::npos && rest.back() == ']') {
            behavior_name = rest.substr(0, bracket_pos);
            std::string args_str = rest.substr(bracket_pos + 1, rest.size() - bracket_pos - 2);
            auto arg_strs = split_type_args(args_str);
            for (const auto& arg : arg_strs) {
                type_args.push_back(deserialize_type_string(arg));
            }
        }
        return std::make_shared<Type>(
            Type{DynBehaviorType{behavior_name, std::move(type_args), true}});
    }
    if (str.starts_with("dyn ")) {
        std::string rest = str.substr(4);
        // Check for generic args: dyn Behavior[T]
        std::string behavior_name = rest;
        std::vector<TypePtr> type_args;
        auto bracket_pos = rest.find('[');
        if (bracket_pos != std::string::npos && rest.back() == ']') {
            behavior_name = rest.substr(0, bracket_pos);
            std::string args_str = rest.substr(bracket_pos + 1, rest.size() - bracket_pos - 2);
            auto arg_strs = split_type_args(args_str);
            for (const auto& arg : arg_strs) {
                type_args.push_back(deserialize_type_string(arg));
            }
        }
        return std::make_shared<Type>(
            Type{DynBehaviorType{behavior_name, std::move(type_args), false}});
    }

    // Impl behavior types: impl Behavior, impl Behavior[T]
    if (str.starts_with("impl ")) {
        std::string rest = str.substr(5);
        std::string behavior_name = rest;
        std::vector<TypePtr> type_args;
        auto bracket_pos = rest.find('[');
        if (bracket_pos != std::string::npos && rest.back() == ']') {
            behavior_name = rest.substr(0, bracket_pos);
            std::string args_str = rest.substr(bracket_pos + 1, rest.size() - bracket_pos - 2);
            auto arg_strs = split_type_args(args_str);
            for (const auto& arg : arg_strs) {
                type_args.push_back(deserialize_type_string(arg));
            }
        }
        return std::make_shared<Type>(Type{ImplBehaviorType{behavior_name, std::move(type_args)}});
    }

    // Pointer types (raw pointer syntax: *T, *mut T)
    if (str.starts_with("*mut ")) {
        auto inner = deserialize_type_string(str.substr(5));
        return std::make_shared<Type>(Type{PtrType{true, inner}});
    }
    if (str.size() > 1 && str[0] == '*') {
        auto inner = deserialize_type_string(str.substr(1));
        return std::make_shared<Type>(Type{PtrType{false, inner}});
    }

    // Tuple types: (A, B, C)
    if (str.size() > 2 && str.front() == '(' && str.back() == ')') {
        std::string inner = str.substr(1, str.size() - 2);
        auto args = split_type_args(inner);
        std::vector<TypePtr> elements;
        for (const auto& arg : args) {
            elements.push_back(deserialize_type_string(arg));
        }
        return make_tuple(std::move(elements));
    }

    // Function types: func(A, B) -> C
    if (str.starts_with("func(")) {
        // Find the closing paren
        int depth = 0;
        size_t close_paren = std::string::npos;
        for (size_t i = 4; i < str.size(); ++i) {
            if (str[i] == '(')
                depth++;
            else if (str[i] == ')') {
                depth--;
                if (depth == 0) {
                    close_paren = i;
                    break;
                }
            }
        }
        if (close_paren != std::string::npos) {
            std::string params_str = str.substr(5, close_paren - 5);
            auto param_strs = split_type_args(params_str);
            std::vector<TypePtr> params;
            for (const auto& p : param_strs) {
                params.push_back(deserialize_type_string(p));
            }
            TypePtr ret = make_unit();
            if (close_paren + 4 < str.size() && str.substr(close_paren + 1, 4) == " -> ") {
                ret = deserialize_type_string(str.substr(close_paren + 5));
            }
            return std::make_shared<Type>(Type{FuncType{std::move(params), ret, false}});
        }
    }

    // Array types: [T; N]
    if (str.size() > 2 && str.front() == '[' && str.back() == ']') {
        std::string inner = str.substr(1, str.size() - 2);
        auto semi_pos = inner.find(';');
        if (semi_pos != std::string::npos) {
            std::string elem_str = inner.substr(0, semi_pos);
            // Trim
            while (!elem_str.empty() && elem_str.back() == ' ')
                elem_str.pop_back();
            std::string size_str = inner.substr(semi_pos + 1);
            while (!size_str.empty() && size_str.front() == ' ')
                size_str = size_str.substr(1);
            auto elem = deserialize_type_string(elem_str);
            size_t arr_size = 0;
            try {
                arr_size = std::stoull(size_str);
            } catch (...) {}
            return std::make_shared<Type>(Type{ArrayType{elem, arr_size}});
        }
        // Slice types: [T]
        auto elem = deserialize_type_string(inner);
        return std::make_shared<Type>(Type{SliceType{elem}});
    }

    // Named types with generic args: Name[Arg1, Arg2]
    // Find the first '[' that starts the type arguments
    auto bracket_pos = str.find('[');
    if (bracket_pos != std::string::npos && str.back() == ']') {
        std::string name = str.substr(0, bracket_pos);
        std::string args_str = str.substr(bracket_pos + 1, str.size() - bracket_pos - 2);
        auto arg_strs = split_type_args(args_str);
        std::vector<TypePtr> type_args;
        for (const auto& arg : arg_strs) {
            type_args.push_back(deserialize_type_string(arg));
        }

        // Special case: Ptr[T] -> PtrType (builtin pointer type)
        // Note: RawPtr[T] is NOT special-cased - it's a user-defined struct
        // with field {addr: I64}, and must remain as NamedType for correct codegen.
        if (name == "Ptr" && type_args.size() == 1) {
            return std::make_shared<Type>(Type{PtrType{false, type_args[0]}});
        }

        return std::make_shared<Type>(Type{NamedType{name, "", std::move(type_args)}});
    }

    // Simple named type (no generics)
    return std::make_shared<Type>(Type{NamedType{str, "", {}}});
}

// ============================================================================
// ModuleBinaryReader
// ============================================================================

ModuleBinaryReader::ModuleBinaryReader(std::istream& in) : in_(in) {}

void ModuleBinaryReader::set_error(const std::string& msg) {
    has_error_ = true;
    error_ = msg;
}

uint8_t ModuleBinaryReader::read_u8() {
    uint8_t value = 0;
    in_.read(reinterpret_cast<char*>(&value), 1);
    if (!in_)
        set_error("Unexpected end of data");
    return value;
}

uint16_t ModuleBinaryReader::read_u16() {
    uint16_t value = 0;
    in_.read(reinterpret_cast<char*>(&value), 2);
    if (!in_)
        set_error("Unexpected end of data");
    return value;
}

uint32_t ModuleBinaryReader::read_u32() {
    uint32_t value = 0;
    in_.read(reinterpret_cast<char*>(&value), 4);
    if (!in_)
        set_error("Unexpected end of data");
    return value;
}

uint64_t ModuleBinaryReader::read_u64() {
    uint64_t value = 0;
    in_.read(reinterpret_cast<char*>(&value), 8);
    if (!in_)
        set_error("Unexpected end of data");
    return value;
}

bool ModuleBinaryReader::read_bool() {
    return read_u8() != 0;
}

std::string ModuleBinaryReader::read_string() {
    uint32_t len = read_u32();
    if (has_error_)
        return "";
    if (len == 0)
        return "";
    if (len > 10 * 1024 * 1024) { // 10MB sanity check
        set_error("String length too large: " + std::to_string(len));
        return "";
    }
    std::string str(len, '\0');
    in_.read(str.data(), len);
    if (!in_)
        set_error("Unexpected end of data reading string");
    return str;
}

std::optional<std::string> ModuleBinaryReader::read_optional_string() {
    if (read_bool()) {
        return read_string();
    }
    return std::nullopt;
}

TypePtr ModuleBinaryReader::read_type() {
    std::string type_str = read_string();
    return deserialize_type_string(type_str);
}

bool ModuleBinaryReader::verify_header() {
    uint32_t magic = read_u32();
    if (has_error_)
        return false;
    if (magic != MODULE_META_MAGIC) {
        set_error("Invalid module meta magic number");
        return false;
    }

    uint16_t major = read_u16();
    uint16_t minor = read_u16();
    (void)minor; // Minor version differences are OK

    if (major != MODULE_META_VERSION_MAJOR) {
        set_error("Incompatible module meta version: " + std::to_string(major));
        return false;
    }

    return true;
}

uint64_t ModuleBinaryReader::read_header_hash() {
    if (!verify_header())
        return 0;
    uint64_t source_hash = read_u64();
    // Skip timestamp
    read_u64();
    return source_hash;
}

std::vector<std::string> ModuleBinaryReader::read_string_array() {
    uint32_t count = read_u32();
    if (has_error_)
        return {};
    std::vector<std::string> result;
    result.reserve(count);
    for (uint32_t i = 0; i < count && !has_error_; ++i) {
        result.push_back(read_string());
    }
    return result;
}

ConstGenericParam ModuleBinaryReader::read_const_generic_param() {
    ConstGenericParam param;
    param.name = read_string();
    param.value_type = read_type();
    return param;
}

WhereConstraint ModuleBinaryReader::read_where_constraint() {
    WhereConstraint wc;
    wc.type_param = read_string();
    wc.required_behaviors = read_string_array();

    uint32_t bound_count = read_u32();
    for (uint32_t i = 0; i < bound_count && !has_error_; ++i) {
        BoundConstraint bc;
        bc.behavior_name = read_string();
        uint32_t arg_count = read_u32();
        for (uint32_t j = 0; j < arg_count && !has_error_; ++j) {
            bc.type_args.push_back(read_type());
        }
        wc.parameterized_bounds.push_back(std::move(bc));
    }
    return wc;
}

AssociatedTypeDef ModuleBinaryReader::read_associated_type() {
    AssociatedTypeDef at;
    at.name = read_string();
    at.type_params = read_string_array();
    at.bounds = read_string_array();
    if (read_bool()) {
        at.default_type = read_type();
    }
    return at;
}

FuncSig ModuleBinaryReader::read_func_sig() {
    FuncSig sig;
    sig.name = read_string();

    // Parameters
    uint32_t param_count = read_u32();
    for (uint32_t i = 0; i < param_count && !has_error_; ++i) {
        sig.params.push_back(read_type());
    }

    // Return type
    sig.return_type = read_type();

    // Type params
    sig.type_params = read_string_array();

    // Const params
    uint32_t cp_count = read_u32();
    for (uint32_t i = 0; i < cp_count && !has_error_; ++i) {
        sig.const_params.push_back(read_const_generic_param());
    }

    // Flags
    sig.is_async = read_bool();
    sig.is_lowlevel = read_bool();
    sig.is_intrinsic = read_bool();
    sig.stability = static_cast<StabilityLevel>(read_u8());

    // Stability metadata
    sig.deprecated_message = read_string();
    sig.since_version = read_string();

    // Where constraints
    uint32_t wc_count = read_u32();
    for (uint32_t i = 0; i < wc_count && !has_error_; ++i) {
        sig.where_constraints.push_back(read_where_constraint());
    }

    // FFI
    sig.extern_abi = read_optional_string();
    sig.extern_name = read_optional_string();
    sig.link_libs = read_string_array();
    sig.ffi_module = read_optional_string();

    // Lifetime bounds
    uint32_t lb_count = read_u32();
    for (uint32_t i = 0; i < lb_count && !has_error_; ++i) {
        std::string param = read_string();
        std::string bound = read_string();
        sig.lifetime_bounds[param] = bound;
    }

    return sig;
}

StructFieldDef ModuleBinaryReader::read_struct_field() {
    StructFieldDef field;
    field.name = read_string();
    field.type = read_type();
    field.has_default = read_bool();
    return field;
}

StructDef ModuleBinaryReader::read_struct_def() {
    StructDef def;
    def.name = read_string();
    def.type_params = read_string_array();

    uint32_t cp_count = read_u32();
    for (uint32_t i = 0; i < cp_count && !has_error_; ++i) {
        def.const_params.push_back(read_const_generic_param());
    }

    uint32_t field_count = read_u32();
    for (uint32_t i = 0; i < field_count && !has_error_; ++i) {
        def.fields.push_back(read_struct_field());
    }

    def.is_interior_mutable = read_bool();
    def.is_union = read_bool();
    return def;
}

EnumDef ModuleBinaryReader::read_enum_def() {
    EnumDef def;
    def.name = read_string();
    def.type_params = read_string_array();

    uint32_t cp_count = read_u32();
    for (uint32_t i = 0; i < cp_count && !has_error_; ++i) {
        def.const_params.push_back(read_const_generic_param());
    }

    uint32_t variant_count = read_u32();
    for (uint32_t i = 0; i < variant_count && !has_error_; ++i) {
        std::string name = read_string();
        uint32_t payload_count = read_u32();
        std::vector<TypePtr> payload;
        for (uint32_t j = 0; j < payload_count && !has_error_; ++j) {
            payload.push_back(read_type());
        }
        def.variants.emplace_back(std::move(name), std::move(payload));
    }
    return def;
}

BehaviorDef ModuleBinaryReader::read_behavior_def() {
    BehaviorDef def;
    def.name = read_string();
    def.type_params = read_string_array();

    uint32_t cp_count = read_u32();
    for (uint32_t i = 0; i < cp_count && !has_error_; ++i) {
        def.const_params.push_back(read_const_generic_param());
    }

    // Associated types
    uint32_t at_count = read_u32();
    for (uint32_t i = 0; i < at_count && !has_error_; ++i) {
        def.associated_types.push_back(read_associated_type());
    }

    // Methods
    uint32_t method_count = read_u32();
    for (uint32_t i = 0; i < method_count && !has_error_; ++i) {
        def.methods.push_back(read_func_sig());
    }

    // Super behaviors
    def.super_behaviors = read_string_array();

    // Methods with defaults
    uint32_t default_count = read_u32();
    for (uint32_t i = 0; i < default_count && !has_error_; ++i) {
        def.methods_with_defaults.insert(read_string());
    }
    return def;
}

ClassDef ModuleBinaryReader::read_class_def() {
    ClassDef def;
    def.name = read_string();
    def.type_params = read_string_array();

    uint32_t cp_count = read_u32();
    for (uint32_t i = 0; i < cp_count && !has_error_; ++i) {
        def.const_params.push_back(read_const_generic_param());
    }

    def.base_class = read_optional_string();
    def.interfaces = read_string_array();

    // Fields (ClassFieldDef)
    uint32_t field_count = read_u32();
    for (uint32_t i = 0; i < field_count && !has_error_; ++i) {
        ClassFieldDef field;
        field.name = read_string();
        field.type = read_type();
        field.vis = static_cast<MemberVisibility>(read_u8());
        field.is_static = read_bool();
        if (read_bool()) {
            field.init_type = read_type();
        }
        def.fields.push_back(std::move(field));
    }

    // Methods (ClassMethodDef)
    uint32_t method_count = read_u32();
    for (uint32_t i = 0; i < method_count && !has_error_; ++i) {
        ClassMethodDef method;
        method.sig = read_func_sig();
        method.vis = static_cast<MemberVisibility>(read_u8());
        method.is_static = read_bool();
        method.is_virtual = read_bool();
        method.is_override = read_bool();
        method.is_abstract = read_bool();
        method.is_final = read_bool();
        method.vtable_index = read_u32();
        def.methods.push_back(std::move(method));
    }

    // Properties (PropertyDef)
    uint32_t prop_count = read_u32();
    for (uint32_t i = 0; i < prop_count && !has_error_; ++i) {
        PropertyDef prop;
        prop.name = read_string();
        prop.type = read_type();
        prop.vis = static_cast<MemberVisibility>(read_u8());
        prop.is_static = read_bool();
        prop.has_getter = read_bool();
        prop.has_setter = read_bool();
        def.properties.push_back(std::move(prop));
    }

    // Constructors (ConstructorDef)
    uint32_t ctor_count = read_u32();
    for (uint32_t i = 0; i < ctor_count && !has_error_; ++i) {
        ConstructorDef ctor;
        uint32_t p_count = read_u32();
        for (uint32_t j = 0; j < p_count && !has_error_; ++j) {
            ctor.params.push_back(read_type());
        }
        ctor.vis = static_cast<MemberVisibility>(read_u8());
        ctor.calls_base = read_bool();
        def.constructors.push_back(std::move(ctor));
    }

    // Class flags
    def.is_abstract = read_bool();
    def.is_sealed = read_bool();
    def.is_value = read_bool();
    def.is_pooled = read_bool();
    def.stack_allocatable = read_bool();
    def.estimated_size = read_u32();
    def.inheritance_depth = read_u32();
    return def;
}

InterfaceDef ModuleBinaryReader::read_interface_def() {
    InterfaceDef def;
    def.name = read_string();
    def.type_params = read_string_array();

    uint32_t cp_count = read_u32();
    for (uint32_t i = 0; i < cp_count && !has_error_; ++i) {
        def.const_params.push_back(read_const_generic_param());
    }

    def.extends = read_string_array();

    // Methods (InterfaceMethodDef)
    uint32_t method_count = read_u32();
    for (uint32_t i = 0; i < method_count && !has_error_; ++i) {
        InterfaceMethodDef method;
        method.sig = read_func_sig();
        method.is_static = read_bool();
        method.has_default = read_bool();
        def.methods.push_back(std::move(method));
    }
    return def;
}

ReExport ModuleBinaryReader::read_re_export() {
    ReExport re;
    re.source_path = read_string();
    re.is_glob = read_bool();
    re.symbols = read_string_array();
    re.alias = read_optional_string();
    return re;
}

Module ModuleBinaryReader::read_module() {
    Module module;

    if (!verify_header())
        return module;

    // Skip source_hash and timestamp (already read by verify_header or read_header_hash)
    // Actually verify_header only reads magic + version, so read the rest
    read_u64(); // source_hash
    read_u64(); // timestamp

    if (has_error_)
        return module;

    // Module metadata
    module.name = read_string();
    module.file_path = read_string();
    module.has_pure_tml_functions = read_bool();
    module.default_visibility = static_cast<parser::Visibility>(read_u8());

    // Functions
    uint32_t func_count = read_u32();
    for (uint32_t i = 0; i < func_count && !has_error_; ++i) {
        FuncSig sig = read_func_sig();
        module.functions[sig.name] = std::move(sig);
    }

    // Public structs
    uint32_t struct_count = read_u32();
    for (uint32_t i = 0; i < struct_count && !has_error_; ++i) {
        StructDef def = read_struct_def();
        module.structs[def.name] = std::move(def);
    }

    // Internal structs
    uint32_t internal_struct_count = read_u32();
    for (uint32_t i = 0; i < internal_struct_count && !has_error_; ++i) {
        StructDef def = read_struct_def();
        module.internal_structs[def.name] = std::move(def);
    }

    // Enums
    uint32_t enum_count = read_u32();
    for (uint32_t i = 0; i < enum_count && !has_error_; ++i) {
        EnumDef def = read_enum_def();
        module.enums[def.name] = std::move(def);
    }

    // Behaviors
    uint32_t behavior_count = read_u32();
    for (uint32_t i = 0; i < behavior_count && !has_error_; ++i) {
        BehaviorDef def = read_behavior_def();
        module.behaviors[def.name] = std::move(def);
    }

    // Type aliases
    uint32_t alias_count = read_u32();
    for (uint32_t i = 0; i < alias_count && !has_error_; ++i) {
        std::string name = read_string();
        TypePtr type = read_type();
        module.type_aliases[name] = type;
        // Read generic parameter names
        uint32_t gen_count = read_u32();
        if (gen_count > 0) {
            std::vector<std::string> params;
            params.reserve(gen_count);
            for (uint32_t j = 0; j < gen_count && !has_error_; ++j) {
                params.push_back(read_string());
            }
            module.type_alias_generics[name] = std::move(params);
        }
    }

    // Submodules
    uint32_t submod_count = read_u32();
    for (uint32_t i = 0; i < submod_count && !has_error_; ++i) {
        std::string name = read_string();
        std::string path = read_string();
        module.submodules[name] = path;
    }

    // Constants
    uint32_t const_count = read_u32();
    for (uint32_t i = 0; i < const_count && !has_error_; ++i) {
        std::string name = read_string();
        Module::ConstantInfo info;
        info.value = read_string();
        info.tml_type = read_string();
        module.constants[name] = info;
    }

    // Classes
    uint32_t class_count = read_u32();
    for (uint32_t i = 0; i < class_count && !has_error_; ++i) {
        ClassDef def = read_class_def();
        module.classes[def.name] = std::move(def);
    }

    // Interfaces
    uint32_t iface_count = read_u32();
    for (uint32_t i = 0; i < iface_count && !has_error_; ++i) {
        InterfaceDef def = read_interface_def();
        module.interfaces[def.name] = std::move(def);
    }

    // Re-exports
    uint32_t re_count = read_u32();
    for (uint32_t i = 0; i < re_count && !has_error_; ++i) {
        module.re_exports.push_back(read_re_export());
    }

    // Private imports
    module.private_imports = read_string_array();

    // Source code
    module.source_code = read_string();

    // Behavior implementations (v3.1+): type -> list of behavior names
    // Gracefully handle old format (v3.0) where this data doesn't exist
    if (!has_error_ && in_.peek() != std::char_traits<char>::eof()) {
        uint32_t bi_count = read_u32();
        for (uint32_t i = 0; i < bi_count && !has_error_; ++i) {
            std::string type_name = read_string();
            std::vector<std::string> behaviors = read_string_array();
            module.behavior_impls[type_name] = std::move(behaviors);
        }
    }

    return module;
}

// ============================================================================
// Convenience Functions
// ============================================================================

std::optional<Module> load_module_from_cache(const std::string& module_path,
                                             const std::string& source_file_path) {
    auto build_root = find_build_root();
    auto cache_path = get_module_cache_path(module_path, build_root);

    if (!fs::exists(cache_path)) {
        return std::nullopt;
    }

    // Compute current source hash
    uint64_t current_hash = compute_module_source_hash(source_file_path);
    if (current_hash == 0) {
        return std::nullopt;
    }

    // Read header to check hash
    std::ifstream in(cache_path, std::ios::binary);
    if (!in) {
        return std::nullopt;
    }

    ModuleBinaryReader header_reader(in);
    uint64_t cached_hash = header_reader.read_header_hash();

    if (header_reader.has_error() || cached_hash != current_hash) {
        TML_DEBUG_LN("[META] Cache miss for " << module_path << " (hash: " << current_hash << " vs "
                                              << cached_hash << ")");
        return std::nullopt;
    }

    // Hash matches - read full module from beginning
    in.seekg(0);
    ModuleBinaryReader reader(in);
    Module module = reader.read_module();

    if (reader.has_error()) {
        TML_LOG_WARN("types", "[META] Failed to read cache for " << module_path << ": "
                                                                 << reader.error_message());
        return std::nullopt;
    }

    TML_DEBUG_LN("[META] Cache hit for " << module_path);
    return module;
}

std::optional<Module> load_module_from_cache(const std::string& module_path) {
    auto build_root = find_build_root();
    auto cache_path = get_module_cache_path(module_path, build_root);

    if (!fs::exists(cache_path)) {
        return std::nullopt;
    }

    std::ifstream in(cache_path, std::ios::binary);
    if (!in) {
        return std::nullopt;
    }

    ModuleBinaryReader reader(in);
    Module module = reader.read_module();

    if (reader.has_error()) {
        TML_DEBUG_LN("[META] Failed to read cache for " << module_path << ": "
                                                        << reader.error_message());
        return std::nullopt;
    }

    TML_DEBUG_LN("[META] Cache hit for " << module_path << " (no hash check)");
    return module;
}

bool save_module_to_cache(const std::string& module_path, const Module& module,
                          const std::string& source_file_path) {
    auto build_root = find_build_root();
    auto cache_path = get_module_cache_path(module_path, build_root);

    // Create directories
    std::error_code ec;
    fs::create_directories(cache_path.parent_path(), ec);
    if (ec) {
        TML_DEBUG_LN("[META] Failed to create cache dir: " << ec.message());
        return false;
    }

    uint64_t source_hash = compute_module_source_hash(source_file_path);

    std::ofstream out(cache_path, std::ios::binary);
    if (!out) {
        TML_DEBUG_LN("[META] Failed to create cache file: " << cache_path);
        return false;
    }

    ModuleBinaryWriter writer(out);
    writer.write_module(module, source_hash);
    out.close();

    TML_DEBUG_LN("[META] Saved cache for " << module_path << " -> " << cache_path);
    return true;
}

// Helper: find the lib/ root directory (mirrors find_lib_root in env_module_support.cpp)
static fs::path find_lib_root_for_meta() {
    auto cwd = fs::current_path();
    std::vector<fs::path> candidates = {
        cwd / "lib",
        fs::path("lib"),
        fs::path("F:/Node/hivellm/tml/lib"),
        cwd.parent_path() / "lib",
        cwd.parent_path().parent_path() / "lib",
    };
    for (const auto& candidate : candidates) {
        if (fs::exists(candidate / "core" / "src") && fs::exists(candidate / "std" / "src")) {
            return fs::canonical(candidate);
        }
    }
    return {};
}

// Helper: extract "pub mod <name>" entries from a mod.tml file
static std::vector<std::string> extract_pub_mod_names(const fs::path& mod_file) {
    std::vector<std::string> names;
    std::ifstream in(mod_file);
    if (!in)
        return names;

    std::string line;
    while (std::getline(in, line)) {
        // Match "pub mod <name>" — skip comments
        size_t pos = line.find("pub mod ");
        if (pos == std::string::npos)
            continue;
        // Ensure it's not inside a comment
        size_t comment_pos = line.find("//");
        if (comment_pos != std::string::npos && comment_pos < pos)
            continue;

        std::string rest = line.substr(pos + 8);
        // Extract the module name (first word)
        std::string name;
        for (char c : rest) {
            if (std::isalnum(c) || c == '_') {
                name += c;
            } else {
                break;
            }
        }
        if (!name.empty()) {
            names.push_back(name);
        }
    }
    return names;
}

// Helper: resolve a module path (e.g. "core::str") to its source file path.
// Returns empty path if the source file cannot be found.
static fs::path resolve_module_source_path(const std::string& module_path,
                                           const fs::path& lib_root) {
    if (lib_root.empty())
        return {};

    // Split module_path on "::" — first segment is the library (core, std, test)
    std::string lib_name;
    std::string rest;
    auto sep = module_path.find("::");
    if (sep == std::string::npos) {
        lib_name = module_path;
    } else {
        lib_name = module_path.substr(0, sep);
        rest = module_path.substr(sep + 2);
    }

    // Determine source subdirectory
    std::string src_subdir = "src";

    // Build the base path: lib/<lib_name>/src/
    fs::path base = lib_root / lib_name / src_subdir;

    if (rest.empty()) {
        // Top-level module (e.g. "core" -> lib/core/src/mod.tml)
        fs::path candidate = base / "mod.tml";
        if (fs::exists(candidate))
            return candidate;
        return {};
    }

    // Replace "::" with "/" for nested modules
    std::string fs_path = rest;
    for (size_t i = 0; i < fs_path.size(); ++i) {
        if (fs_path[i] == ':' && i + 1 < fs_path.size() && fs_path[i + 1] == ':') {
            fs_path.replace(i, 2, "/");
        }
    }

    // Try name.tml first, then name/mod.tml
    fs::path candidate1 = base / (fs_path + ".tml");
    if (fs::exists(candidate1))
        return candidate1;

    fs::path candidate2 = base / fs_path / "mod.tml";
    if (fs::exists(candidate2))
        return candidate2;

    return {};
}

// Helper: load existing .tml.meta files from cache directory into GlobalModuleCache
// Validates source hash for each module to detect stale caches.
static int load_existing_meta_files(const fs::path& meta_dir) {
    TML_LOG_INFO("meta", "  Scanning " << meta_dir << " for .tml.meta files...");
    int loaded = 0;
    int stale = 0;
    std::error_code ec;

    // Find lib root for source hash validation
    auto lib_root = find_lib_root_for_meta();

    // Collect stale meta files to regenerate after loading valid ones
    std::vector<std::pair<std::string, fs::path>> stale_modules; // {module_path, source_path}

    for (auto& entry : fs::recursive_directory_iterator(meta_dir, ec)) {
        if (!entry.is_regular_file() || entry.path().extension() != ".meta") {
            continue;
        }

        // Convert file path back to module path:
        // build/debug/cache/meta/core/clone.tml.meta -> core::clone
        auto rel = fs::relative(entry.path(), meta_dir);
        std::string module_path;
        for (auto it = rel.begin(); it != rel.end(); ++it) {
            std::string part = it->string();
            auto next = it;
            ++next;
            if (next == rel.end()) {
                if (part.size() > 9 && part.substr(part.size() - 9) == ".tml.meta") {
                    part = part.substr(0, part.size() - 9);
                }
            }
            if (!module_path.empty()) {
                module_path += "::";
            }
            module_path += part;
        }

        if (GlobalModuleCache::instance().get(module_path)) {
            continue;
        }

        // Validate source hash before loading
        if (!lib_root.empty()) {
            auto source_path = resolve_module_source_path(module_path, lib_root);
            if (!source_path.empty()) {
                // Compute current source hash
                uint64_t current_hash = compute_module_source_hash(source_path.string());
                if (current_hash != 0) {
                    // Read just the header hash from the .tml.meta file
                    std::ifstream hash_in(entry.path(), std::ios::binary);
                    if (hash_in) {
                        ModuleBinaryReader hash_reader(hash_in);
                        uint64_t cached_hash = hash_reader.read_header_hash();
                        hash_in.close();

                        if (!hash_reader.has_error() && cached_hash != current_hash) {
                            TML_LOG_INFO("meta", "  [STALE] "
                                                     << module_path << " (source changed, hash "
                                                     << cached_hash << " -> " << current_hash
                                                     << ")");
                            stale_modules.push_back({module_path, source_path});
                            ++stale;
                            continue; // Skip loading stale cache
                        }
                    }
                }
            }
        }

        std::ifstream in(entry.path(), std::ios::binary);
        if (!in)
            continue;

        ModuleBinaryReader reader(in);
        Module module = reader.read_module();

        if (reader.has_error()) {
            TML_LOG_WARN("meta",
                         "  [LOAD FAILED] " << module_path << " - " << reader.error_message());
            continue;
        }

        GlobalModuleCache::instance().put(module_path, std::move(module));
        ++loaded;
        TML_LOG_INFO("meta", "  [LOADED] " << module_path);
    }

    // Regenerate stale modules from source
    if (!stale_modules.empty()) {
        TML_LOG_INFO("meta", "  Regenerating " << stale_modules.size()
                                               << " stale module(s) from source...");

        // Delete stale .tml.meta files from disk BEFORE regeneration.
        // Without this, load_native_module() would find the old binary via
        // load_module_from_cache() and load the stale version, defeating
        // the staleness detection above.
        for (const auto& [mod_path, source_path] : stale_modules) {
            std::string rel = mod_path;
            size_t pos = 0;
            while ((pos = rel.find("::", pos)) != std::string::npos) {
                rel.replace(pos, 2, "/");
                pos += 1;
            }
            fs::path stale_meta = meta_dir / (rel + ".tml.meta");
            std::error_code rm_ec;
            fs::remove(stale_meta, rm_ec);
            if (!rm_ec) {
                TML_LOG_INFO("meta", "  [DELETED STALE] " << stale_meta);
            }
        }

        auto registry = std::make_shared<ModuleRegistry>();
        TypeEnv env;
        env.set_module_registry(registry);
        env.set_abort_on_module_error(false);

        for (const auto& [mod_path, source_path] : stale_modules) {
            if (GlobalModuleCache::instance().get(mod_path)) {
                ++loaded; // Was loaded transitively
                continue;
            }

            bool ok = env.load_native_module(mod_path, /*silent=*/true);
            if (ok) {
                ++loaded;
                TML_LOG_INFO("meta", "  [REGENERATED] " << mod_path);
            } else {
                TML_LOG_WARN("meta", "  [REGEN FAILED] " << mod_path);
            }
        }
    }

    if (stale > 0) {
        TML_LOG_INFO("meta", "  Summary: " << loaded << " loaded, " << stale
                                           << " stale (regenerated from source)");
    }

    return loaded;
}

// Helper: generate all .tml.meta files by parsing library modules from source
static int generate_all_meta_from_source() {
    auto lib_root = find_lib_root_for_meta();
    if (lib_root.empty()) {
        TML_LOG_WARN("meta", "[META] Cannot find lib/ directory to generate meta caches");
        return 0;
    }

    TML_LOG_INFO("meta", "  lib/ root: " << lib_root);

    // Discover all library submodules from mod.tml files
    struct LibInfo {
        std::string prefix; // "core", "std"
        fs::path mod_file;
    };

    std::vector<LibInfo> libs = {
        {"core", lib_root / "core" / "src" / "mod.tml"},
        {"std", lib_root / "std" / "src" / "mod.tml"},
    };

    // Collect all module paths to load
    std::vector<std::string> module_paths;

    // Add top-level modules
    module_paths.push_back("test");

    for (const auto& lib : libs) {
        // Add the root module itself
        module_paths.push_back(lib.prefix);

        // Add submodules from mod.tml
        auto submodules = extract_pub_mod_names(lib.mod_file);
        for (const auto& submod : submodules) {
            module_paths.push_back(lib.prefix + "::" + submod);
        }
    }

    TML_LOG_INFO("meta", "  Discovered " << module_paths.size()
                                         << " library modules to generate from source:");
    for (const auto& mp : module_paths) {
        TML_LOG_INFO("meta", "    - " << mp);
    }

    // Create a TypeEnv to load all modules (this parses source and saves .tml.meta)
    auto registry = std::make_shared<ModuleRegistry>();
    TypeEnv env;
    env.set_module_registry(registry);
    env.set_abort_on_module_error(false); // Don't crash on parse errors

    int generated = 0;
    int skipped = 0;
    int failed = 0;
    for (const auto& mod_path : module_paths) {
        // Skip if already in GlobalModuleCache (might have been loaded transitively)
        if (GlobalModuleCache::instance().get(mod_path)) {
            ++generated;
            ++skipped;
            TML_LOG_INFO("meta", "  [CACHED] " << mod_path << " (already in GlobalModuleCache)");
            continue;
        }

        auto start = std::chrono::steady_clock::now();
        bool ok = env.load_native_module(mod_path, /*silent=*/true);
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                           std::chrono::steady_clock::now() - start)
                           .count();

        if (ok) {
            ++generated;
            TML_LOG_INFO("meta", "  [GENERATED] " << mod_path << " (" << elapsed << "ms)");
        } else {
            ++failed;
            TML_LOG_WARN("meta", "  [FAILED] " << mod_path << " (" << elapsed << "ms)");
        }
    }

    TML_LOG_INFO("meta", "  Summary: " << generated << " generated, " << skipped
                                       << " already cached, " << failed << " failed"
                                       << " (total: " << module_paths.size() << ")");

    return generated;
}

int preload_all_meta_caches() {
    // Thread-safe once-only initialization.
    // Called from suite_execution.cpp (main thread) before parallel compilation,
    // AND from compile_test_suite workers (multiple threads). std::call_once
    // guarantees the heavy work runs exactly once with proper memory barriers.
    static std::once_flag s_preload_flag;
    static int s_preload_result = 0;

    std::call_once(s_preload_flag, []() {
        auto preload_start = std::chrono::steady_clock::now();

        auto build_root = find_build_root();
        fs::path meta_dir = build_root / "cache" / "meta";

        TML_LOG_INFO("meta", "========================================");
        TML_LOG_INFO("meta", " META PRELOAD START");
        TML_LOG_INFO("meta", "  Cache dir: " << meta_dir);
        TML_LOG_INFO("meta", "========================================");

        // Phase 1: Try to load existing .tml.meta files
        int loaded = 0;
        if (fs::exists(meta_dir)) {
            loaded = load_existing_meta_files(meta_dir);
        }

        if (loaded > 0) {
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                               std::chrono::steady_clock::now() - preload_start)
                               .count();
            TML_LOG_INFO("meta", "========================================");
            TML_LOG_INFO("meta", " META PRELOAD COMPLETE (Phase 1: binary cache)");
            TML_LOG_INFO("meta", "  Loaded: " << loaded << " modules from .tml.meta files");
            TML_LOG_INFO("meta", "  Time: " << elapsed << "ms");
            TML_LOG_INFO("meta", "========================================");
            s_preload_result = loaded;
            return;
        }

        // Phase 2: No .tml.meta files found — generate them by parsing source files
        // This happens on first run or after cache clean. We MUST do this before
        // any test/build execution starts, so all library modules are available
        // in GlobalModuleCache when tests begin.
        TML_LOG_INFO("meta", "  No .tml.meta files found. Generating from source (first run)...");
        int generated = generate_all_meta_from_source();

        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                           std::chrono::steady_clock::now() - preload_start)
                           .count();
        TML_LOG_INFO("meta", "========================================");
        TML_LOG_INFO("meta", " META PRELOAD COMPLETE (Phase 2: generated from source)");
        TML_LOG_INFO("meta", "  Generated: " << generated << " modules");
        TML_LOG_INFO("meta", "  Time: " << elapsed << "ms");
        TML_LOG_INFO("meta", "========================================");

        s_preload_result = generated;
    });

    return s_preload_result;
}

} // namespace tml::types
