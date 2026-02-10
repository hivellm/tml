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

// Helper: load existing .tml.meta files from cache directory into GlobalModuleCache
static int load_existing_meta_files(const fs::path& meta_dir) {
    TML_LOG_INFO("meta", "  Scanning " << meta_dir << " for .tml.meta files...");
    int loaded = 0;
    std::error_code ec;

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
    // Only run once per process
    static bool s_preloaded = false;
    if (s_preloaded) {
        return 0;
    }
    s_preloaded = true;

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
        return loaded;
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

    return generated;
}

} // namespace tml::types
