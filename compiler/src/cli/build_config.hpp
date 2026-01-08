//! # Build Configuration Interface
//!
//! This header defines tml.toml manifest parsing and project configuration.
//!
//! ## Manifest Sections
//!
//! | Section          | Type          | Description                   |
//! |------------------|---------------|-------------------------------|
//! | `[package]`      | `PackageInfo` | Name, version, authors        |
//! | `[lib]`          | `LibConfig`   | Library output configuration  |
//! | `[[bin]]`        | `BinConfig`   | Binary targets                |
//! | `[dependencies]` | `Dependency`  | Package dependencies          |
//! | `[build]`        | `BuildSettings` | Build options               |
//! | `[profile.*]`    | `ProfileConfig` | Profile-specific settings   |
//!
//! ## TOML Parser
//!
//! `SimpleTomlParser` handles a subset of TOML for manifest parsing.

#ifndef TML_CLI_BUILD_CONFIG_HPP
#define TML_CLI_BUILD_CONFIG_HPP

#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace tml::cli {

/**
 * Package metadata from [package] section
 */
struct PackageInfo {
    std::string name;
    std::string version;
    std::vector<std::string> authors;
    std::string edition = "2024";
    std::string description;
    std::string license;
    std::string repository;

    bool validate() const;
};

/**
 * Library configuration from [lib] section
 */
struct LibConfig {
    std::string path = "src/lib.tml";
    std::vector<std::string> crate_types = {"rlib"}; // rlib, lib, dylib
    std::string name;                                // Optional override (defaults to package name)
    bool emit_header = false;

    bool validate() const;
};

/**
 * Binary configuration from [[bin]] section
 */
struct BinConfig {
    std::string name;
    std::string path;

    bool validate() const;
};

/**
 * Dependency specification from [dependencies] section
 */
struct Dependency {
    std::string name;
    std::string version; // Semver constraint (e.g., "^1.2.0")
    std::string path;    // For path dependencies
    std::string git;     // For git dependencies
    std::string tag;     // Git tag
    std::string branch;  // Git branch
    std::string rev;     // Git commit hash

    bool is_path_dependency() const {
        return !path.empty();
    }
    bool is_version_dependency() const {
        return !version.empty();
    }
    bool is_git_dependency() const {
        return !git.empty();
    }

    bool validate() const;
};

/**
 * Build settings from [build] section
 */
struct BuildSettings {
    int optimization_level = 0; // 0-3
    bool emit_ir = false;
    bool emit_header = false;
    bool verbose = false;
    bool cache = true;
    bool parallel = true;

    bool validate() const;
};

/**
 * Profile-specific configuration from [profile.debug] or [profile.release]
 */
struct ProfileConfig {
    std::string name; // "debug" or "release"
    BuildSettings settings;

    bool validate() const;
};

/**
 * Complete manifest structure
 */
struct Manifest {
    PackageInfo package;
    std::optional<LibConfig> lib;
    std::vector<BinConfig> bins;
    std::map<std::string, Dependency> dependencies;
    BuildSettings build;
    std::map<std::string, ProfileConfig> profiles;

    /**
     * Load manifest from tml.toml file
     * @param path Path to tml.toml file
     * @return Manifest if successful, std::nullopt on error
     */
    static std::optional<Manifest> load(const fs::path& path);

    /**
     * Load manifest from current directory
     * Looks for tml.toml in current directory
     */
    static std::optional<Manifest> load_from_current_dir();

    /**
     * Validate entire manifest
     * @return true if valid
     */
    bool validate() const;

    /**
     * Get build settings for a specific profile
     * @param profile_name "debug" or "release"
     * @return Build settings (from profile if exists, otherwise from [build])
     */
    BuildSettings get_build_settings(const std::string& profile_name) const;
};

/**
 * Simple TOML parser (subset of TOML spec)
 * Handles:
 * - Sections: [section]
 * - Array sections: [[array]]
 * - Key-value pairs: key = "value"
 * - Numbers: key = 123
 * - Booleans: key = true
 * - Arrays: key = ["value1", "value2"]
 */
class SimpleTomlParser {
public:
    explicit SimpleTomlParser(const std::string& content);

    /**
     * Parse TOML content into manifest
     */
    std::optional<Manifest> parse();

    /**
     * Get error message if parsing failed
     */
    std::string get_error() const {
        return error_message_;
    }

private:
    std::string content_;
    std::string error_message_;
    size_t pos_ = 0;
    int line_ = 1;

    // Helper methods
    void skip_whitespace();
    void skip_comment();
    bool is_eof() const {
        return pos_ >= content_.size();
    }
    char peek() const {
        return is_eof() ? '\0' : content_[pos_];
    }
    char advance();

    std::string parse_identifier();
    std::string parse_string();
    int parse_number();
    bool parse_boolean();
    std::vector<std::string> parse_string_array();

    std::optional<PackageInfo> parse_package_section();
    std::optional<LibConfig> parse_lib_section();
    std::optional<BinConfig> parse_bin_section();
    std::map<std::string, Dependency> parse_dependencies_section();
    std::optional<BuildSettings> parse_build_section();
    std::optional<ProfileConfig> parse_profile_section(const std::string& profile_name);

    void set_error(const std::string& message);
};

/**
 * Validate semantic version string
 * @param version Version string (e.g., "1.2.3")
 * @return true if valid semver
 */
bool is_valid_semver(const std::string& version);

/**
 * Validate package name
 * @param name Package name
 * @return true if valid
 */
bool is_valid_package_name(const std::string& name);

} // namespace tml::cli

#endif // TML_CLI_BUILD_CONFIG_HPP
