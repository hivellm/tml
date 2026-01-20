//! # HIR Serialization
//!
//! This module provides serialization and deserialization of HIR (High-level
//! Intermediate Representation) modules. It supports both binary and text
//! formats for different use cases.
//!
//! ## Overview
//!
//! HIR serialization enables:
//! - **Incremental compilation**: Cache compiled HIR to avoid recompilation
//! - **Fast loading**: Binary format for minimal I/O overhead
//! - **Debugging**: Text format for human inspection
//! - **Change detection**: Content hashing for cache invalidation
//!
//! ## Binary Format
//!
//! The binary format is a compact representation optimized for fast I/O:
//!
//! ```text
//! +----------------+------------------+
//! | Header (16B)   | Module Data      |
//! +----------------+------------------+
//!
//! Header:
//!   [0..4)   magic: u32 = 0x52494854 ("THIR")
//!   [4..6)   version_major: u16
//!   [6..8)   version_minor: u16
//!   [8..16)  content_hash: u64
//!
//! Module Data:
//!   - name: length-prefixed string
//!   - source_path: length-prefixed string
//!   - structs: count + [HirStruct...]
//!   - enums: count + [HirEnum...]
//!   - behaviors: count + [HirBehavior...]
//!   - impls: count + [HirImpl...]
//!   - functions: count + [HirFunction...]
//!   - constants: count + [HirConst...]
//!   - imports: count + [string...]
//! ```
//!
//! ### String Encoding
//!
//! All strings use length-prefixed encoding: `u32 length` + `bytes[length]`
//!
//! ### Type Encoding
//!
//! Types are serialized as: `u8 tag` + `string type_name`
//! The tag indicates Unknown (null) vs Named types.
//!
//! ## Text Format
//!
//! The text format is human-readable, resembling TML source syntax with
//! additional annotations for resolved types and HIR IDs:
//!
//! ```text
//! ; HIR Module: main
//! ; Source: src/main.tml
//! ; Hash: 12345678901234567890
//!
//! pub type Point {
//!     x: I32
//!     y: I32
//! }
//!
//! pub func add(a: I32, b: I32) -> I32 {
//!     return (a + b)
//! }
//! ```
//!
//! **Note**: The text format is NOT designed for round-trip serialization.
//! Use binary format for cache persistence.
//!
//! ## Content Hashing
//!
//! Content hashes use FNV-1a algorithm for fast, reliable change detection.
//! The hash covers:
//! - Function signatures (name, params, return type)
//! - Struct/enum definitions
//! - Type information
//! - Source file path
//!
//! ## Incremental Compilation Flow
//!
//! ```text
//! 1. compute_source_hash(source_path)
//! 2. if is_hir_cache_valid(cache_path, source_hash):
//!      return read_hir_file(cache_path)  // Fast path
//! 3. else:
//!      hir = compile_fresh(source)
//!      write_hir_file(hir, cache_path)
//!      return hir
//! ```
//!
//! ## Thread Safety
//!
//! - Writers are NOT thread-safe (single writer per stream)
//! - Readers are NOT thread-safe (single reader per stream)
//! - Hash functions ARE thread-safe (pure functions)
//!
//! ## See Also
//!
//! - `hir.hpp` - HIR data structures
//! - `hir_module.hpp` - Module container
//! - `mir_serialize.hpp` - Similar serialization for MIR

#pragma once

#include "hir/hir.hpp"
#include "hir/hir_module.hpp"

#include <cstdint>
#include <istream>
#include <optional>
#include <ostream>
#include <string>
#include <vector>

namespace tml::hir {

// ============================================================================
// Serialization Options
// ============================================================================

/// Options controlling HIR serialization behavior.
///
/// These options allow customization of the serialization output for
/// different use cases (debugging, caching, minimal size).
///
/// ## Example
///
/// ```cpp
/// HirSerializeOptions opts;
/// opts.include_spans = false;  // Smaller output
/// opts.compact = true;         // Minimize whitespace
/// auto bytes = serialize_hir_binary(module, opts);
/// ```
struct HirSerializeOptions {
    /// Include debug comments in text format output.
    /// When true, adds extra annotations showing HIR IDs and other metadata.
    bool include_comments = false;

    /// Minimize whitespace in text format output.
    /// When true, reduces indentation and newlines for smaller output.
    bool compact = false;

    /// Include source location spans in binary output.
    /// Disabling this reduces file size but loses source mapping.
    /// Recommended to keep enabled for debugging support.
    bool include_spans = true;
};

// ============================================================================
// Binary Format Constants
// ============================================================================

/// Magic number for HIR binary format header.
///
/// The value 0x52494854 reads as "THIR" (TML HIR) in ASCII when viewed
/// in little-endian byte order. This allows quick identification of
/// HIR binary files.
constexpr uint32_t HIR_MAGIC = 0x52494854;

/// HIR binary format major version.
///
/// Increment when making breaking changes to the binary format.
/// Files with different major versions are incompatible.
constexpr uint16_t HIR_VERSION_MAJOR = 1;

/// HIR binary format minor version.
///
/// Increment when adding backward-compatible features.
/// Readers should handle files with higher minor versions gracefully.
constexpr uint16_t HIR_VERSION_MINOR = 0;

// ============================================================================
// Content Hash
// ============================================================================

/// A 64-bit content hash for change detection.
///
/// Used to detect whether source files have changed since the HIR was cached.
/// The hash is computed using FNV-1a algorithm for good distribution and speed.
///
/// ## Properties
///
/// - **Deterministic**: Same input always produces same hash
/// - **Fast**: O(n) where n is content size
/// - **Good distribution**: Low collision probability for similar inputs
///
/// ## Usage
///
/// ```cpp
/// ContentHash source_hash = compute_source_hash("main.tml");
/// ContentHash hir_hash = compute_hir_hash(module);
///
/// if (cached_hash != source_hash) {
///     // Source changed, recompile needed
/// }
/// ```
using ContentHash = uint64_t;

/// Compute content hash for a source file.
///
/// Hashes the file content and path to detect changes. The hash includes:
/// - Full file path (to differentiate files with same content)
/// - File content (byte-by-byte)
///
/// @param source_path Path to the source file
/// @return Hash of the file content and metadata, or 0 if file not found
auto compute_source_hash(const std::string& source_path) -> ContentHash;

/// Compute content hash for HIR module content.
///
/// Hashes the module structure to detect semantic changes. The hash covers:
/// - Module name and source path
/// - All function signatures and metadata
/// - All struct and enum definitions
/// - Type information
///
/// **Note**: Does not hash function bodies deeply (only structure).
/// This is intentional for incremental compilation - internal changes
/// don't affect dependents unless signatures change.
///
/// @param module The HIR module to hash
/// @return Hash of the module structure
auto compute_hir_hash(const HirModule& module) -> ContentHash;

// ============================================================================
// Binary Writer
// ============================================================================

/// Writes HIR modules to compact binary format.
///
/// The binary writer produces a self-describing format with version info
/// and content hash for cache validation. The format is optimized for
/// fast sequential I/O.
///
/// ## Binary Format Structure
///
/// ```text
/// Header (16 bytes):
///   magic:         u32  - 0x52494854 ("THIR")
///   version_major: u16  - Format major version
///   version_minor: u16  - Format minor version
///   content_hash:  u64  - FNV-1a hash of module content
///
/// Module:
///   name:        string - Module name
///   source_path: string - Original source file path
///   structs:     [HirStruct...]
///   enums:       [HirEnum...]
///   behaviors:   [HirBehavior...]
///   impls:       [HirImpl...]
///   functions:   [HirFunction...]
///   constants:   [HirConst...]
///   imports:     [string...]
/// ```
///
/// ## Example
///
/// ```cpp
/// std::ofstream file("module.hir", std::ios::binary);
/// HirBinaryWriter writer(file);
/// writer.write_module(module);
///
/// // Get hash for cache info
/// ContentHash hash = writer.content_hash();
/// ```
///
/// ## Error Handling
///
/// Write errors are propagated through the underlying ostream.
/// Check `out.good()` after writing to detect errors.
class HirBinaryWriter {
public:
    /// Creates a binary writer for the given output stream.
    ///
    /// @param out Output stream (should be opened in binary mode)
    /// @param options Serialization options
    explicit HirBinaryWriter(std::ostream& out, HirSerializeOptions options = {});

    /// Writes a complete HIR module to the output stream.
    ///
    /// This writes the header (including computed content hash) followed
    /// by all module data. The content hash is computed before writing.
    ///
    /// @param module The module to serialize
    void write_module(const HirModule& module);

    /// Returns the content hash of the written module.
    ///
    /// This is computed during write_module() and can be used for
    /// cache info files or validation.
    ///
    /// @return Content hash, or 0 if write_module() not called yet
    [[nodiscard]] auto content_hash() const -> ContentHash {
        return content_hash_;
    }

private:
    std::ostream& out_;
    HirSerializeOptions options_;
    ContentHash content_hash_ = 0;

    // Header writing
    void write_header(ContentHash hash);

    // Primitive type writing (little-endian)
    void write_u8(uint8_t value);
    void write_u16(uint16_t value);
    void write_u32(uint32_t value);
    void write_u64(uint64_t value);
    void write_i64(int64_t value);
    void write_f64(double value);
    void write_bool(bool value);

    // String writing (length-prefixed)
    void write_string(const std::string& str);

    // Source location writing
    void write_span(const SourceSpan& span);

    // Type serialization (tag + string representation)
    void write_type(const HirType& type);

    // Expression tree serialization
    void write_expr(const HirExpr& expr);
    void write_expr_ptr(const HirExprPtr& expr);
    void write_optional_expr(const std::optional<HirExprPtr>& expr);

    // Pattern serialization
    void write_pattern(const HirPattern& pattern);
    void write_pattern_ptr(const HirPatternPtr& pattern);

    // Statement serialization
    void write_stmt(const HirStmt& stmt);
    void write_stmt_ptr(const HirStmtPtr& stmt);

    // Declaration serialization
    void write_param(const HirParam& param);
    void write_field(const HirField& field);
    void write_variant(const HirVariant& variant);
    void write_function(const HirFunction& func);
    void write_struct(const HirStruct& s);
    void write_enum(const HirEnum& e);
    void write_behavior_method(const HirBehaviorMethod& method);
    void write_behavior(const HirBehavior& b);
    void write_impl(const HirImpl& impl);
    void write_const(const HirConst& c);
};

// ============================================================================
// Binary Reader
// ============================================================================

/// Reads HIR modules from binary format.
///
/// The reader validates the file format and version before loading data.
/// It uses a soft error model - errors set a flag but reading continues
/// to collect as much data as possible.
///
/// ## Error Handling
///
/// ```cpp
/// HirBinaryReader reader(file);
/// HirModule module = reader.read_module();
///
/// if (reader.has_error()) {
///     std::cerr << "Load failed: " << reader.error_message() << "\n";
///     return;
/// }
/// ```
///
/// ## Version Compatibility
///
/// - Different major version: Error, file incompatible
/// - Higher minor version: Warning, may miss some data
/// - Same version: Full compatibility
///
/// ## Type Reconstruction
///
/// Types are stored as strings and reconstructed on load. Primitive types
/// (I32, Bool, etc.) are fully reconstructed. Complex types (generics,
/// user-defined) are created as NamedType placeholders.
class HirBinaryReader {
public:
    /// Creates a binary reader for the given input stream.
    ///
    /// @param in Input stream (should be opened in binary mode)
    explicit HirBinaryReader(std::istream& in);

    /// Reads a complete HIR module from the input stream.
    ///
    /// Validates header, then reads all module data. On error, returns
    /// a partial module - check has_error() for success.
    ///
    /// @return Loaded module (may be partial on error)
    auto read_module() -> HirModule;

    /// Check if an error occurred during reading.
    [[nodiscard]] auto has_error() const -> bool {
        return has_error_;
    }

    /// Get the error message if any.
    [[nodiscard]] auto error_message() const -> std::string {
        return error_;
    }

    /// Get the content hash from the file header.
    ///
    /// Available after read_module() completes header validation.
    [[nodiscard]] auto content_hash() const -> ContentHash {
        return content_hash_;
    }

private:
    std::istream& in_;
    bool has_error_ = false;
    std::string error_;
    ContentHash content_hash_ = 0;
    HirIdGenerator id_gen_;

    void set_error(const std::string& msg);
    auto verify_header() -> bool;

    // Primitive type reading
    auto read_u8() -> uint8_t;
    auto read_u16() -> uint16_t;
    auto read_u32() -> uint32_t;
    auto read_u64() -> uint64_t;
    auto read_i64() -> int64_t;
    auto read_f64() -> double;
    auto read_bool() -> bool;
    auto read_string() -> std::string;
    auto read_span() -> SourceSpan;

    // Type deserialization
    auto read_type() -> HirType;

    // Expression deserialization
    auto read_expr() -> HirExprPtr;
    auto read_optional_expr() -> std::optional<HirExprPtr>;

    // Pattern deserialization
    auto read_pattern() -> HirPatternPtr;

    // Statement deserialization
    auto read_stmt() -> HirStmtPtr;

    // Declaration deserialization
    auto read_param() -> HirParam;
    auto read_field() -> HirField;
    auto read_variant() -> HirVariant;
    auto read_function() -> HirFunction;
    auto read_struct() -> HirStruct;
    auto read_enum() -> HirEnum;
    auto read_behavior_method() -> HirBehaviorMethod;
    auto read_behavior() -> HirBehavior;
    auto read_impl() -> HirImpl;
    auto read_const() -> HirConst;
};

// ============================================================================
// Text Writer (Debugging)
// ============================================================================

/// Writes HIR modules to human-readable text format.
///
/// This writer produces output resembling TML source code with additional
/// annotations for debugging. The output is NOT designed for round-trip
/// serialization - use binary format for that.
///
/// ## Output Format
///
/// ```text
/// ; HIR Module: main
/// ; Source: src/main.tml
/// ; Hash: 12345678901234567890
///
/// ; Structs
/// pub type Point {
///     x: I32
///     y: I32
/// }
///
/// ; Functions
/// pub func add(a: I32, b: I32) -> I32 {
///     return (a + b)
/// }
/// ```
///
/// ## Usage
///
/// ```cpp
/// std::cout << serialize_hir_text(module);
/// // Or with more control:
/// HirTextWriter writer(std::cout);
/// writer.write_module(module);
/// ```
class HirTextWriter {
public:
    /// Creates a text writer for the given output stream.
    explicit HirTextWriter(std::ostream& out, HirSerializeOptions options = {});

    /// Writes a complete HIR module as formatted text.
    void write_module(const HirModule& module);

private:
    std::ostream& out_;
    HirSerializeOptions options_;
    int indent_ = 0; ///< Current indentation level

    void write_indent();
    void write_line(const std::string& line);
    void write_type(const HirType& type);
    void write_expr(const HirExpr& expr);
    void write_pattern(const HirPattern& pattern);
    void write_stmt(const HirStmt& stmt);
    void write_function(const HirFunction& func);
    void write_struct(const HirStruct& s);
    void write_enum(const HirEnum& e);
    void write_behavior(const HirBehavior& b);
    void write_impl(const HirImpl& impl);
    void write_const(const HirConst& c);
};

// ============================================================================
// Text Reader (Testing)
// ============================================================================

/// Reads HIR modules from text format.
///
/// **Note**: This is a minimal implementation for testing purposes.
/// The text format is not designed for full round-trip serialization.
/// Use binary format for production caching.
class HirTextReader {
public:
    explicit HirTextReader(std::istream& in);

    auto read_module() -> HirModule;
    [[nodiscard]] auto has_error() const -> bool {
        return has_error_;
    }
    [[nodiscard]] auto error_message() const -> std::string {
        return error_;
    }

private:
    std::istream& in_;
    std::string current_line_;
    size_t line_num_ = 0;
    size_t pos_ = 0;
    bool has_error_ = false;
    std::string error_;
    HirIdGenerator id_gen_;

    void set_error(const std::string& msg);
    auto next_line() -> bool;
    void skip_whitespace();
    auto peek_char() -> char;
    auto read_char() -> char;
    auto read_identifier() -> std::string;
    auto read_number() -> int64_t;
    auto read_string_literal() -> std::string;
    auto expect(char c) -> bool;
    auto expect(const std::string& s) -> bool;
};

// ============================================================================
// Convenience Functions
// ============================================================================

/// Serialize HIR module to binary bytes.
///
/// Convenience function that handles stream creation internally.
///
/// @param module The module to serialize
/// @param options Serialization options
/// @return Binary data as byte vector
auto serialize_hir_binary(const HirModule& module, HirSerializeOptions options = {})
    -> std::vector<uint8_t>;

/// Deserialize HIR module from binary bytes.
///
/// @param data Binary data from serialize_hir_binary()
/// @return Loaded module (check validity by inspecting content)
auto deserialize_hir_binary(const std::vector<uint8_t>& data) -> HirModule;

/// Serialize HIR module to text string.
///
/// Useful for debugging and logging.
///
/// @param module The module to serialize
/// @param options Serialization options
/// @return Human-readable text representation
auto serialize_hir_text(const HirModule& module, HirSerializeOptions options = {}) -> std::string;

/// Deserialize HIR module from text string.
///
/// **Note**: Limited functionality, for testing only.
auto deserialize_hir_text(const std::string& text) -> HirModule;

/// Write HIR module to file.
///
/// Creates the file and writes the module in the specified format.
///
/// @param module The module to write
/// @param path Output file path (will be created/overwritten)
/// @param binary If true, use binary format; otherwise text
/// @return true on success, false on I/O error
auto write_hir_file(const HirModule& module, const std::string& path, bool binary = true) -> bool;

/// Read HIR module from file.
///
/// Automatically detects format by checking magic number.
///
/// @param path Input file path
/// @return Loaded module
/// @throws May return empty module on read error
auto read_hir_file(const std::string& path) -> HirModule;

/// Check if a cached HIR file is valid.
///
/// Validates that:
/// 1. Cache file exists and is readable
/// 2. Format version is compatible
/// 3. Source hash matches (if cache info available)
/// 4. All dependencies are still valid
///
/// @param cache_path Path to the cached .hir file
/// @param source_hash Current hash of the source file
/// @return true if cache can be used
auto is_hir_cache_valid(const std::string& cache_path, ContentHash source_hash) -> bool;

/// Get the content hash from a cached HIR file header.
///
/// Reads only the header, not the full module. Useful for quick
/// cache validation without loading everything.
///
/// @param cache_path Path to the cached .hir file
/// @return Content hash from header, or 0 if file invalid
auto get_hir_cache_hash(const std::string& cache_path) -> ContentHash;

// ============================================================================
// Dependency Tracking
// ============================================================================

/// Tracks a dependency on another HIR module.
///
/// Used by incremental compilation to detect when dependencies change.
/// If any dependency's hash differs from the recorded value, the
/// dependent module must be recompiled.
struct HirDependency {
    std::string module_name;  ///< Imported module name (e.g., "std::io")
    std::string source_path;  ///< Path to dependency's source file
    ContentHash content_hash; ///< Hash when this module was compiled
};

/// Cache metadata for incremental compilation.
///
/// Stored alongside the HIR binary file (e.g., as `module.hir.info`).
/// Contains all information needed to validate cache without loading
/// the full HIR.
///
/// ## Example Cache Info File
///
/// ```text
/// module_name: main
/// source_path: src/main.tml
/// source_hash: 12345678901234567890
/// hir_hash: 98765432109876543210
/// compile_timestamp: 1704067200000
/// deps:
///   - std::io: 11111111111111111111
///   - mylib::utils: 22222222222222222222
/// ```
struct HirCacheInfo {
    std::string module_name;         ///< Name of this module
    std::string source_path;         ///< Original source file path
    ContentHash source_hash;         ///< Hash of source at compile time
    ContentHash hir_hash;            ///< Hash of compiled HIR
    std::vector<HirDependency> deps; ///< All module dependencies
    uint64_t compile_timestamp;      ///< Unix timestamp of compilation
};

/// Write cache info to a file.
///
/// Typically written as `module.hir.info` alongside `module.hir`.
///
/// @param info Cache metadata to write
/// @param path Output file path
/// @return true on success
auto write_hir_cache_info(const HirCacheInfo& info, const std::string& path) -> bool;

/// Read cache info from a file.
///
/// @param path Path to cache info file
/// @return Cache info if file valid, nullopt otherwise
auto read_hir_cache_info(const std::string& path) -> std::optional<HirCacheInfo>;

/// Check if all dependencies in cache info are still valid.
///
/// Recomputes each dependency's source hash and compares to recorded value.
///
/// @param info Cache info with dependencies to check
/// @return true if all dependencies unchanged
auto are_dependencies_valid(const HirCacheInfo& info) -> bool;

} // namespace tml::hir
