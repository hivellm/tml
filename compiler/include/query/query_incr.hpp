//! # Incremental Compilation Cache
//!
//! Persists query fingerprints and dependency edges to disk between
//! compilation sessions. Enables red-green incremental reuse:
//! - GREEN: query inputs unchanged from previous session → reuse result
//! - RED: inputs changed → must recompute
//!
//! ## Cache Directory Structure
//!
//! ```text
//! build/{debug|release}/.incr-cache/
//!   ├─ incr.bin         # Binary fingerprint/dep cache
//!   └─ ir/
//!       ├─ <hash>.ll    # Cached LLVM IR per compilation unit
//!       └─ <hash>.libs  # Cached link libraries per compilation unit
//! ```

#pragma once

#include "query/query_fingerprint.hpp"
#include "query/query_key.hpp"

#include <filesystem>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

namespace tml::query {

/// Magic number for incremental cache files: "TMIC" (TML Incremental Cache)
constexpr uint32_t INCR_CACHE_MAGIC = 0x544D4943;
constexpr uint16_t INCR_CACHE_VERSION_MAJOR = 2;
constexpr uint16_t INCR_CACHE_VERSION_MINOR = 0;

/// Compile-time build hash — changes every time the compiler is recompiled.
/// When the compiler binary changes, the old incremental cache is invalidated.
uint32_t compiler_build_hash();

/// Color for a query in the red-green system.
enum class QueryColor : uint8_t {
    Unknown = 0,
    Green = 1,
    Red = 2,
};

/// Stored entry from a previous compilation session.
struct PrevSessionEntry {
    QueryKey key;
    Fingerprint input_fingerprint;
    Fingerprint output_fingerprint;
    std::vector<QueryKey> dependencies;
};

// ============================================================================
// Previous Session Cache (read-only, loaded from disk)
// ============================================================================

/// Previous session cache — loaded from disk at startup.
/// Read-only during compilation.
class PrevSessionCache {
public:
    /// Load from binary file. Returns false if file doesn't exist or is corrupt.
    bool load(const std::filesystem::path& cache_file);

    /// Look up a previous session entry by key.
    [[nodiscard]] const PrevSessionEntry* lookup(const QueryKey& key) const;

    /// Get the options hash from the previous session.
    [[nodiscard]] uint32_t options_hash() const {
        return options_hash_;
    }

    /// Get the session timestamp.
    [[nodiscard]] uint64_t session_timestamp() const {
        return session_timestamp_;
    }

    /// Get total number of entries.
    [[nodiscard]] size_t entry_count() const {
        return entries_.size();
    }

private:
    std::unordered_map<QueryKey, PrevSessionEntry, QueryKeyHash, QueryKeyEqual> entries_;
    uint32_t options_hash_ = 0;
    uint32_t build_hash_ = 0;
    uint64_t session_timestamp_ = 0;
};

// ============================================================================
// Incremental Cache Writer (accumulates entries, writes at session end)
// ============================================================================

/// Current session cache writer — writes the new cache to disk at the end.
class IncrCacheWriter {
public:
    /// Record a completed query's fingerprints and dependencies.
    void record(const QueryKey& key, Fingerprint input_fp, Fingerprint output_fp,
                std::vector<QueryKey> deps);

    /// Save a CodegenUnit's LLVM IR to the cache directory.
    bool save_ir(const QueryKey& key, const std::string& llvm_ir,
                 const std::filesystem::path& cache_dir);

    /// Save link_libs for a CodegenUnit.
    bool save_link_libs(const QueryKey& key, const std::set<std::string>& link_libs,
                        const std::filesystem::path& cache_dir);

    /// Write all entries to the binary cache file.
    bool write(const std::filesystem::path& cache_file, uint32_t options_hash);

    /// Get number of recorded entries.
    [[nodiscard]] size_t entry_count() const {
        return entries_.size();
    }

private:
    std::vector<PrevSessionEntry> entries_;
};

// ============================================================================
// Free Functions
// ============================================================================

/// Load cached LLVM IR for a CodegenUnit query.
[[nodiscard]] std::optional<std::string> load_cached_ir(const QueryKey& key,
                                                        const std::filesystem::path& cache_dir);

/// Load cached link_libs for a CodegenUnit query.
[[nodiscard]] std::set<std::string> load_cached_link_libs(const QueryKey& key,
                                                          const std::filesystem::path& cache_dir);

/// Compute a hash of build options that affect code generation.
/// If this changes between sessions, the entire cache is invalidated.
[[nodiscard]] uint32_t compute_options_hash(int opt_level, bool debug_info,
                                            const std::string& target_triple,
                                            const std::vector<std::string>& defines, bool coverage);

/// Compute the library environment fingerprint — a combined hash of all
/// .tml.meta files in the build cache directory.
[[nodiscard]] Fingerprint compute_library_env_fingerprint(const std::filesystem::path& build_dir);

/// Serialize a QueryKey to bytes (for binary cache format).
[[nodiscard]] std::vector<uint8_t> serialize_query_key(const QueryKey& key);

/// Deserialize a QueryKey from bytes.
[[nodiscard]] std::optional<QueryKey> deserialize_query_key(const uint8_t* data, size_t len,
                                                            QueryKind kind);

/// Get the IR cache filename for a codegen key (hash-based).
[[nodiscard]] std::string get_ir_cache_filename(const QueryKey& key);

} // namespace tml::query
