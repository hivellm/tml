TML_MODULE("compiler")

//! # Binary Module Metadata Serialization — Implementation
//!
//! Compact binary serialization of Module structs following the
//! HIR/MIR binary format patterns (length-prefixed strings, count-prefixed
//! collections, little-endian primitives).

#include "types/module_binary.hpp"

#include "common/crc32c.hpp"
#include "log/log.hpp"
#include "types/env.hpp"
#include "types/type.hpp"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace tml::types {

namespace fs = std::filesystem;

// ============================================================================
// Source Hash
// ============================================================================

uint64_t compute_module_source_hash(const std::string& file_path) {
    auto fs_path = fs::path(file_path);

    if (fs_path.stem() == "mod") {
        // Directory module: hash all .tml files in directory sorted by name
        auto dir = fs_path.parent_path();
        std::vector<std::string> files;

        std::error_code ec;
        for (const auto& entry : fs::directory_iterator(dir, ec)) {
            if (entry.is_regular_file() && entry.path().extension() == ".tml") {
                files.push_back(entry.path().string());
            }
        }
        std::sort(files.begin(), files.end());

        // Concatenate all file contents and hash
        std::string combined;
        for (const auto& f : files) {
            std::ifstream in(f, std::ios::binary);
            if (in) {
                std::ostringstream oss;
                oss << in.rdbuf();
                combined += oss.str();
            }
        }
        return static_cast<uint64_t>(crc32c(combined));
    }

    // Single file module
    std::ifstream in(file_path, std::ios::binary);
    if (!in) {
        return 0;
    }
    std::ostringstream oss;
    oss << in.rdbuf();
    std::string content = oss.str();
    return static_cast<uint64_t>(crc32c(content));
}

// ============================================================================
// Cache Path
// ============================================================================

static std::mutex s_build_root_mutex;
static fs::path s_build_root;
static bool s_build_root_resolved = false;

fs::path find_build_root() {
    std::lock_guard<std::mutex> lock(s_build_root_mutex);
    if (s_build_root_resolved) {
        return s_build_root;
    }
    s_build_root_resolved = true;

    auto cwd = fs::current_path();

    // Look for build/ directory walking up from CWD
    for (auto dir = cwd; !dir.empty() && dir.has_parent_path(); dir = dir.parent_path()) {
        if (fs::exists(dir / "build" / "debug")) {
            s_build_root = dir / "build" / "debug";
            return s_build_root;
        }
        if (fs::exists(dir / "build" / "release")) {
            s_build_root = dir / "build" / "release";
            return s_build_root;
        }
        // If we're inside build/debug or build/release
        if (dir.filename() == "debug" || dir.filename() == "release") {
            if (dir.parent_path().filename() == "build") {
                s_build_root = dir;
                return s_build_root;
            }
        }
    }

    // Fallback: use build/debug relative to CWD
    s_build_root = cwd / "build" / "debug";
    return s_build_root;
}

fs::path get_module_cache_path(const std::string& module_path, const fs::path& build_root) {
    // Convert module_path "core::mem" to "core/mem"
    std::string rel = module_path;
    size_t pos = 0;
    while ((pos = rel.find("::", pos)) != std::string::npos) {
        rel.replace(pos, 2, "/");
        pos += 1;
    }
    return build_root / "cache" / "meta" / (rel + ".tml.meta");
}

// ============================================================================
// Type Deserialization (string -> TypePtr)
// ============================================================================

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
// ModuleBinaryWriter
// ============================================================================

ModuleBinaryWriter::ModuleBinaryWriter(std::ostream& out) : out_(out) {}

void ModuleBinaryWriter::write_u8(uint8_t value) {
    out_.write(reinterpret_cast<const char*>(&value), 1);
}

void ModuleBinaryWriter::write_u16(uint16_t value) {
    out_.write(reinterpret_cast<const char*>(&value), 2);
}

void ModuleBinaryWriter::write_u32(uint32_t value) {
    out_.write(reinterpret_cast<const char*>(&value), 4);
}

void ModuleBinaryWriter::write_u64(uint64_t value) {
    out_.write(reinterpret_cast<const char*>(&value), 8);
}

void ModuleBinaryWriter::write_bool(bool value) {
    write_u8(value ? 1 : 0);
}

void ModuleBinaryWriter::write_string(const std::string& str) {
    write_u32(static_cast<uint32_t>(str.size()));
    if (!str.empty()) {
        out_.write(str.data(), static_cast<std::streamsize>(str.size()));
    }
}

void ModuleBinaryWriter::write_optional_string(const std::optional<std::string>& str) {
    if (str.has_value()) {
        write_bool(true);
        write_string(*str);
    } else {
        write_bool(false);
    }
}

void ModuleBinaryWriter::write_type(const TypePtr& type) {
    write_string(type_to_string(type));
}

void ModuleBinaryWriter::write_header(uint64_t source_hash) {
    write_u32(MODULE_META_MAGIC);
    write_u16(MODULE_META_VERSION_MAJOR);
    write_u16(MODULE_META_VERSION_MINOR);
    write_u64(source_hash);
    auto now = std::chrono::system_clock::now();
    auto timestamp = static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count());
    write_u64(timestamp);
}

void ModuleBinaryWriter::write_string_array(const std::vector<std::string>& arr) {
    write_u32(static_cast<uint32_t>(arr.size()));
    for (const auto& s : arr) {
        write_string(s);
    }
}

void ModuleBinaryWriter::write_const_generic_param(const ConstGenericParam& param) {
    write_string(param.name);
    write_type(param.value_type);
}

void ModuleBinaryWriter::write_where_constraint(const WhereConstraint& wc) {
    write_string(wc.type_param);
    write_string_array(wc.required_behaviors);
    // Parameterized bounds
    write_u32(static_cast<uint32_t>(wc.parameterized_bounds.size()));
    for (const auto& bound : wc.parameterized_bounds) {
        write_string(bound.behavior_name);
        write_u32(static_cast<uint32_t>(bound.type_args.size()));
        for (const auto& arg : bound.type_args) {
            write_type(arg);
        }
    }
}

void ModuleBinaryWriter::write_associated_type(const AssociatedTypeDef& at) {
    write_string(at.name);
    write_string_array(at.type_params);
    write_string_array(at.bounds);
    if (at.default_type.has_value()) {
        write_bool(true);
        write_type(*at.default_type);
    } else {
        write_bool(false);
    }
}

void ModuleBinaryWriter::write_func_sig(const FuncSig& sig) {
    write_string(sig.name);

    // Parameters
    write_u32(static_cast<uint32_t>(sig.params.size()));
    for (const auto& param : sig.params) {
        write_type(param);
    }

    // Return type
    write_type(sig.return_type);

    // Type params
    write_string_array(sig.type_params);

    // Const params
    write_u32(static_cast<uint32_t>(sig.const_params.size()));
    for (const auto& cp : sig.const_params) {
        write_const_generic_param(cp);
    }

    // Flags
    write_bool(sig.is_async);
    write_bool(sig.is_lowlevel);
    write_bool(sig.is_intrinsic);
    write_u8(static_cast<uint8_t>(sig.stability));

    // Stability metadata
    write_string(sig.deprecated_message);
    write_string(sig.since_version);

    // Where constraints
    write_u32(static_cast<uint32_t>(sig.where_constraints.size()));
    for (const auto& wc : sig.where_constraints) {
        write_where_constraint(wc);
    }

    // FFI
    write_optional_string(sig.extern_abi);
    write_optional_string(sig.extern_name);
    write_string_array(sig.link_libs);
    write_optional_string(sig.ffi_module);

    // Lifetime bounds
    write_u32(static_cast<uint32_t>(sig.lifetime_bounds.size()));
    for (const auto& [param, bound] : sig.lifetime_bounds) {
        write_string(param);
        write_string(bound);
    }
}

void ModuleBinaryWriter::write_struct_field(const StructFieldDef& field) {
    write_string(field.name);
    write_type(field.type);
    write_bool(field.has_default);
}

void ModuleBinaryWriter::write_struct_def(const StructDef& def) {
    write_string(def.name);
    write_string_array(def.type_params);

    write_u32(static_cast<uint32_t>(def.const_params.size()));
    for (const auto& cp : def.const_params) {
        write_const_generic_param(cp);
    }

    write_u32(static_cast<uint32_t>(def.fields.size()));
    for (const auto& field : def.fields) {
        write_struct_field(field);
    }

    write_bool(def.is_interior_mutable);
    write_bool(def.is_union);
}

void ModuleBinaryWriter::write_enum_def(const EnumDef& def) {
    write_string(def.name);
    write_string_array(def.type_params);

    write_u32(static_cast<uint32_t>(def.const_params.size()));
    for (const auto& cp : def.const_params) {
        write_const_generic_param(cp);
    }

    write_u32(static_cast<uint32_t>(def.variants.size()));
    for (const auto& [name, payload] : def.variants) {
        write_string(name);
        write_u32(static_cast<uint32_t>(payload.size()));
        for (const auto& t : payload) {
            write_type(t);
        }
    }
}

void ModuleBinaryWriter::write_behavior_def(const BehaviorDef& def) {
    write_string(def.name);
    write_string_array(def.type_params);

    write_u32(static_cast<uint32_t>(def.const_params.size()));
    for (const auto& cp : def.const_params) {
        write_const_generic_param(cp);
    }

    // Associated types
    write_u32(static_cast<uint32_t>(def.associated_types.size()));
    for (const auto& at : def.associated_types) {
        write_associated_type(at);
    }

    // Methods
    write_u32(static_cast<uint32_t>(def.methods.size()));
    for (const auto& method : def.methods) {
        write_func_sig(method);
    }

    // Super behaviors
    write_string_array(def.super_behaviors);

    // Methods with defaults (std::set<std::string>)
    write_u32(static_cast<uint32_t>(def.methods_with_defaults.size()));
    for (const auto& name : def.methods_with_defaults) {
        write_string(name);
    }
}

void ModuleBinaryWriter::write_class_def(const ClassDef& def) {
    write_string(def.name);
    write_string_array(def.type_params);

    write_u32(static_cast<uint32_t>(def.const_params.size()));
    for (const auto& cp : def.const_params) {
        write_const_generic_param(cp);
    }

    write_optional_string(def.base_class);
    write_string_array(def.interfaces);

    // Fields (ClassFieldDef)
    write_u32(static_cast<uint32_t>(def.fields.size()));
    for (const auto& field : def.fields) {
        write_string(field.name);
        write_type(field.type);
        write_u8(static_cast<uint8_t>(field.vis));
        write_bool(field.is_static);
        if (field.init_type.has_value()) {
            write_bool(true);
            write_type(*field.init_type);
        } else {
            write_bool(false);
        }
    }

    // Methods (ClassMethodDef)
    write_u32(static_cast<uint32_t>(def.methods.size()));
    for (const auto& method : def.methods) {
        write_func_sig(method.sig);
        write_u8(static_cast<uint8_t>(method.vis));
        write_bool(method.is_static);
        write_bool(method.is_virtual);
        write_bool(method.is_override);
        write_bool(method.is_abstract);
        write_bool(method.is_final);
        write_u32(static_cast<uint32_t>(method.vtable_index));
    }

    // Properties (PropertyDef)
    write_u32(static_cast<uint32_t>(def.properties.size()));
    for (const auto& prop : def.properties) {
        write_string(prop.name);
        write_type(prop.type);
        write_u8(static_cast<uint8_t>(prop.vis));
        write_bool(prop.is_static);
        write_bool(prop.has_getter);
        write_bool(prop.has_setter);
    }

    // Constructors (ConstructorDef)
    write_u32(static_cast<uint32_t>(def.constructors.size()));
    for (const auto& ctor : def.constructors) {
        write_u32(static_cast<uint32_t>(ctor.params.size()));
        for (const auto& p : ctor.params) {
            write_type(p);
        }
        write_u8(static_cast<uint8_t>(ctor.vis));
        write_bool(ctor.calls_base);
    }

    // Class flags
    write_bool(def.is_abstract);
    write_bool(def.is_sealed);
    write_bool(def.is_value);
    write_bool(def.is_pooled);
    write_bool(def.stack_allocatable);
    write_u32(static_cast<uint32_t>(def.estimated_size));
    write_u32(static_cast<uint32_t>(def.inheritance_depth));
}

void ModuleBinaryWriter::write_interface_def(const InterfaceDef& def) {
    write_string(def.name);
    write_string_array(def.type_params);

    write_u32(static_cast<uint32_t>(def.const_params.size()));
    for (const auto& cp : def.const_params) {
        write_const_generic_param(cp);
    }

    write_string_array(def.extends);

    // Methods (InterfaceMethodDef)
    write_u32(static_cast<uint32_t>(def.methods.size()));
    for (const auto& method : def.methods) {
        write_func_sig(method.sig);
        write_bool(method.is_static);
        write_bool(method.has_default);
    }
}

void ModuleBinaryWriter::write_re_export(const ReExport& re) {
    write_string(re.source_path);
    write_bool(re.is_glob);
    write_string_array(re.symbols);
    write_optional_string(re.alias);
}

void ModuleBinaryWriter::write_module(const Module& module, uint64_t source_hash) {
    write_header(source_hash);

    // Module metadata
    write_string(module.name);
    write_string(module.file_path);
    write_bool(module.has_pure_tml_functions);
    write_u8(static_cast<uint8_t>(module.default_visibility));

    // Functions
    write_u32(static_cast<uint32_t>(module.functions.size()));
    for (const auto& [name, sig] : module.functions) {
        write_func_sig(sig);
    }

    // Public structs
    write_u32(static_cast<uint32_t>(module.structs.size()));
    for (const auto& [name, def] : module.structs) {
        write_struct_def(def);
    }

    // Internal structs
    write_u32(static_cast<uint32_t>(module.internal_structs.size()));
    for (const auto& [name, def] : module.internal_structs) {
        write_struct_def(def);
    }

    // Enums
    write_u32(static_cast<uint32_t>(module.enums.size()));
    for (const auto& [name, def] : module.enums) {
        write_enum_def(def);
    }

    // Behaviors
    write_u32(static_cast<uint32_t>(module.behaviors.size()));
    for (const auto& [name, def] : module.behaviors) {
        write_behavior_def(def);
    }

    // Type aliases
    write_u32(static_cast<uint32_t>(module.type_aliases.size()));
    for (const auto& [name, type] : module.type_aliases) {
        write_string(name);
        write_type(type);
        // Write generic parameter names for this alias
        auto gen_it = module.type_alias_generics.find(name);
        if (gen_it != module.type_alias_generics.end()) {
            write_u32(static_cast<uint32_t>(gen_it->second.size()));
            for (const auto& param : gen_it->second) {
                write_string(param);
            }
        } else {
            write_u32(0);
        }
    }

    // Submodules
    write_u32(static_cast<uint32_t>(module.submodules.size()));
    for (const auto& [name, path] : module.submodules) {
        write_string(name);
        write_string(path);
    }

    // Constants
    write_u32(static_cast<uint32_t>(module.constants.size()));
    for (const auto& [name, info] : module.constants) {
        write_string(name);
        write_string(info.value);
        write_string(info.tml_type);
    }

    // Classes
    write_u32(static_cast<uint32_t>(module.classes.size()));
    for (const auto& [name, def] : module.classes) {
        write_class_def(def);
    }

    // Interfaces
    write_u32(static_cast<uint32_t>(module.interfaces.size()));
    for (const auto& [name, def] : module.interfaces) {
        write_interface_def(def);
    }

    // Re-exports
    write_u32(static_cast<uint32_t>(module.re_exports.size()));
    for (const auto& re : module.re_exports) {
        write_re_export(re);
    }

    // Private imports
    write_string_array(module.private_imports);

    // Source code (for pure TML modules that need it for codegen)
    write_string(module.source_code);

    // Behavior implementations (v3.1+): type -> list of behavior names
    // Critical for is_trivially_destructible() to detect Drop impls on imported types
    write_u32(static_cast<uint32_t>(module.behavior_impls.size()));
    for (const auto& [type_name, behaviors] : module.behavior_impls) {
        write_string(type_name);
        write_string_array(behaviors);
    }
}

} // namespace tml::types
