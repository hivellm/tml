//! # RLIB Library Format Interface
//!
//! This header defines the TML library (.rlib) format API.
//!
//! ## RLIB Structure
//!
//! ```text
//! library.rlib (llvm-ar archive)
//!   ├─ metadata.json     # RlibMetadata serialized
//!   └─ <module>.obj      # Compiled object files
//! ```
//!
//! ## Metadata Types
//!
//! | Type            | Description                              |
//! |-----------------|------------------------------------------|
//! | `RlibExport`    | Public symbol from module                |
//! | `RlibModule`    | Compiled module with exports             |
//! | `RlibMetadata`  | Complete library metadata                |
//!
//! ## Key Functions
//!
//! - `create_rlib()`: Create .rlib from objects + metadata
//! - `read_rlib_metadata()`: Read metadata from .rlib
//! - `extract_rlib_objects()`: Extract objects for linking

#ifndef TML_CLI_RLIB_HPP
#define TML_CLI_RLIB_HPP

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace tml::cli {

/**
 * Represents a public export from a TML module
 */
struct RlibExport {
    std::string name;   // TML identifier (e.g., "add")
    std::string symbol; // Mangled symbol (e.g., "tml_add")
    std::string type;   // Type signature (e.g., "func(I32, I32) -> I32")
    bool is_public;     // Visibility (true for pub items)
};

/**
 * Represents a compiled module in an RLIB
 */
struct RlibModule {
    std::string name;                // Module name (e.g., "mylib")
    std::string file;                // Object file name (e.g., "mylib.obj")
    std::string hash;                // Content hash of source
    std::vector<RlibExport> exports; // Public symbols
};

/**
 * Represents a dependency of an RLIB
 */
struct RlibDependency {
    std::string name;    // Dependency name
    std::string version; // Required version (semver)
    std::string hash;    // Content hash of dependency .rlib
};

/**
 * Library information from RLIB metadata
 */
struct RlibLibraryInfo {
    std::string name;        // Library name
    std::string version;     // Library version (semver)
    std::string tml_version; // TML compiler version
};

/**
 * Complete RLIB metadata structure
 */
struct RlibMetadata {
    std::string format_version;               // Metadata format version
    RlibLibraryInfo library;                  // Library info
    std::vector<RlibModule> modules;          // Compiled modules
    std::vector<RlibDependency> dependencies; // Dependencies

    /**
     * Find an export by name
     */
    std::optional<RlibExport> find_export(const std::string& name) const;

    /**
     * Get all public exports across all modules
     */
    std::vector<RlibExport> get_all_exports() const;

    /**
     * Convert to JSON string for serialization
     */
    std::string to_json() const;

    /**
     * Parse from JSON string
     */
    static RlibMetadata from_json(const std::string& json_str);
};

/**
 * Options for RLIB creation
 */
struct RlibCreateOptions {
    bool verbose = false;
    std::string archiver = "lib.exe"; // "lib.exe" on Windows, "ar" on Linux
};

/**
 * Result of RLIB operation
 */
struct RlibResult {
    bool success;
    std::string message;
    int exit_code = 0;
};

/**
 * Create a .rlib file from object file(s) and metadata
 *
 * @param object_files Object files to include in archive
 * @param metadata RLIB metadata
 * @param output_rlib Output .rlib file path
 * @param options Creation options
 * @return Result of operation
 */
RlibResult create_rlib(const std::vector<fs::path>& object_files, const RlibMetadata& metadata,
                       const fs::path& output_rlib, const RlibCreateOptions& options = {});

/**
 * Read metadata from an existing .rlib file
 *
 * @param rlib_file Path to .rlib file
 * @return RLIB metadata, or std::nullopt on error
 */
std::optional<RlibMetadata> read_rlib_metadata(const fs::path& rlib_file);

/**
 * Extract object files from .rlib for linking
 *
 * @param rlib_file Path to .rlib file
 * @param temp_dir Temporary directory for extraction
 * @return List of extracted object file paths
 */
std::vector<fs::path> extract_rlib_objects(const fs::path& rlib_file, const fs::path& temp_dir);

/**
 * Extract a single file from .rlib archive
 *
 * @param rlib_file Path to .rlib file
 * @param member_name File to extract (e.g., "metadata.json")
 * @param output_path Where to write extracted file
 * @return True on success
 */
bool extract_rlib_member(const fs::path& rlib_file, const std::string& member_name,
                         const fs::path& output_path);

/**
 * Calculate SHA256 hash of a file
 *
 * @param file_path File to hash
 * @return Hex-encoded hash string, or empty string on error
 */
std::string calculate_file_hash(const fs::path& file_path);

/**
 * Validate RLIB format and metadata
 *
 * @param rlib_file Path to .rlib file
 * @return True if valid
 */
bool validate_rlib(const fs::path& rlib_file);

/**
 * Get list of all member files in .rlib archive
 *
 * @param rlib_file Path to .rlib file
 * @return List of member file names
 */
std::vector<std::string> list_rlib_members(const fs::path& rlib_file);

} // namespace tml::cli

#endif // TML_CLI_RLIB_HPP
