//! # Binary Module Metadata Serialization
//!
//! Compact binary serialization of Module structs for fast loading.
//! Eliminates the need to re-lex/parse/extract library modules on every
//! compiler invocation. Cache files are stored in `build/cache/meta/`.
//!
//! ## Binary Format
//!
//! ```text
//! Header (24 bytes):
//!   [0..4)    magic: u32 = 0x544D4D54 ("TMMT")
//!   [4..6)    version_major: u16 = 1
//!   [6..8)    version_minor: u16 = 0
//!   [8..16)   source_hash: u64 (CRC32C of source files)
//!   [16..24)  timestamp: u64 (write time)
//!
//! Module Data:
//!   Length-prefixed strings, count-prefixed collections.
//!   Types serialized as strings via type_to_string().
//! ```
//!
//! ## Cache Invalidation
//!
//! Source hash (CRC32C) is stored in the header. On load, the hash is
//! recomputed from source files and compared. If different, the cache
//! file is ignored and re-written after parsing.

#ifndef TML_TYPES_MODULE_BINARY_HPP
#define TML_TYPES_MODULE_BINARY_HPP

#include "types/env.hpp"
#include "types/module.hpp"

#include <cstdint>
#include <filesystem>
#include <istream>
#include <optional>
#include <ostream>
#include <string>

namespace tml::types {

// ============================================================================
// Binary Format Constants
// ============================================================================

/// Magic number: "TMMT" (TML Module MeTadata) in little-endian.
constexpr uint32_t MODULE_META_MAGIC = 0x544D4D54;

/// Format version.
constexpr uint16_t MODULE_META_VERSION_MAJOR = 2;
constexpr uint16_t MODULE_META_VERSION_MINOR = 0;

// ============================================================================
// Source Hash
// ============================================================================

/// Computes CRC32C hash of all source files for a module.
/// For directory modules (mod.tml), hashes all .tml files sorted by name.
/// Returns the hash zero-extended to 64 bits.
uint64_t compute_module_source_hash(const std::string& file_path);

// ============================================================================
// Cache Path
// ============================================================================

/// Computes the cache file path for a module.
/// "core::mem" -> "<build_root>/cache/meta/core/mem.tml.meta"
/// "test"      -> "<build_root>/cache/meta/test.tml.meta"
std::filesystem::path get_module_cache_path(const std::string& module_path,
                                            const std::filesystem::path& build_root);

/// Discovers the build root directory (e.g., "build/debug" or "build/release").
/// Walks up from CWD looking for build/ directory structure.
std::filesystem::path find_build_root();

// ============================================================================
// Binary Writer
// ============================================================================

/// Writes a Module struct to compact binary format.
class ModuleBinaryWriter {
public:
    explicit ModuleBinaryWriter(std::ostream& out);

    /// Writes a complete module with header.
    void write_module(const Module& module, uint64_t source_hash);

private:
    std::ostream& out_;

    // Primitive writers
    void write_u8(uint8_t value);
    void write_u16(uint16_t value);
    void write_u32(uint32_t value);
    void write_u64(uint64_t value);
    void write_bool(bool value);
    void write_string(const std::string& str);
    void write_optional_string(const std::optional<std::string>& str);
    void write_type(const TypePtr& type);

    // Header
    void write_header(uint64_t source_hash);

    // Compound writers
    void write_string_array(const std::vector<std::string>& arr);
    void write_func_sig(const FuncSig& sig);
    void write_struct_field(const StructFieldDef& field);
    void write_struct_def(const StructDef& def);
    void write_enum_def(const EnumDef& def);
    void write_behavior_def(const BehaviorDef& def);
    void write_class_def(const ClassDef& def);
    void write_interface_def(const InterfaceDef& def);
    void write_re_export(const ReExport& re);
    void write_const_generic_param(const ConstGenericParam& param);
    void write_where_constraint(const WhereConstraint& wc);
    void write_associated_type(const AssociatedTypeDef& at);
};

// ============================================================================
// Binary Reader
// ============================================================================

/// Reads a Module struct from binary format.
class ModuleBinaryReader {
public:
    explicit ModuleBinaryReader(std::istream& in);

    /// Reads a complete module. Check has_error() after.
    Module read_module();

    /// Reads only the header to check source hash without loading full module.
    /// Returns the source hash from header, or 0 on error.
    uint64_t read_header_hash();

    [[nodiscard]] bool has_error() const {
        return has_error_;
    }
    [[nodiscard]] const std::string& error_message() const {
        return error_;
    }

private:
    std::istream& in_;
    bool has_error_ = false;
    std::string error_;

    void set_error(const std::string& msg);
    bool verify_header();

    // Primitive readers
    uint8_t read_u8();
    uint16_t read_u16();
    uint32_t read_u32();
    uint64_t read_u64();
    bool read_bool();
    std::string read_string();
    std::optional<std::string> read_optional_string();
    TypePtr read_type();

    // Compound readers
    std::vector<std::string> read_string_array();
    FuncSig read_func_sig();
    StructFieldDef read_struct_field();
    StructDef read_struct_def();
    EnumDef read_enum_def();
    BehaviorDef read_behavior_def();
    ClassDef read_class_def();
    InterfaceDef read_interface_def();
    ReExport read_re_export();
    ConstGenericParam read_const_generic_param();
    WhereConstraint read_where_constraint();
    AssociatedTypeDef read_associated_type();
};

// ============================================================================
// Convenience Functions
// ============================================================================

/// Try to load a module from its binary cache file (with hash validation).
/// Returns nullopt if cache doesn't exist, is invalid, or hash mismatches.
std::optional<Module> load_module_from_cache(const std::string& module_path,
                                             const std::string& source_file_path);

/// Try to load a module from its binary cache file (no hash validation).
/// Used in load_native_module() where the source file path is not yet resolved.
/// Only checks magic + version validity.
std::optional<Module> load_module_from_cache(const std::string& module_path);

/// Save a module to its binary cache file.
/// Creates directories as needed. Returns true on success.
bool save_module_to_cache(const std::string& module_path, const Module& module,
                          const std::string& source_file_path);

/// Pre-load ALL .tml.meta files from the cache directory into GlobalModuleCache.
/// Must be called BEFORE any test/build execution so all library modules are available.
/// Returns the number of modules loaded.
int preload_all_meta_caches();

} // namespace tml::types

#endif // TML_TYPES_MODULE_BINARY_HPP
