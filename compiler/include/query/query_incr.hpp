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

#include <atomic>
#include <filesystem>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace tml::query {

/// Magic number for incremental cache files: "TMIC" (TML Incremental Cache)
constexpr uint32_t INCR_CACHE_MAGIC = 0x544D4943;
/// Format version. Bumped 2→3 in phase42a: the CodegenUnit key no longer
/// serializes `test_entry_index`/`has_cached_library_state` (F-023). Old (v2)
/// caches are rejected on load and regenerate once — expected and harmless.
constexpr uint16_t INCR_CACHE_VERSION_MAJOR = 3;
constexpr uint16_t INCR_CACHE_VERSION_MINOR = 0;

/// F-021 aging cap: max entries written to a single partition. Generous — the
/// old cliff was 10k; with per-config partitioning (F-025) each file holds one
/// config's working set. Oldest (least-recently-used) entries age out at save.
constexpr size_t MAX_INCR_ENTRIES = 100'000;

/// F-025: max `incr.<hash>.bin` partitions kept on disk (newest by mtime).
constexpr size_t MAX_INCR_PARTITIONS = 4;

/// Compiler build hash — F-024 (phase42a): content-addressed. This is the
/// CRC32C of the compiler DLL/EXE *content* (not its mtime), memoized in a
/// `<binary>.bhash` sidecar keyed by (mtime,size) so the 100+MB module is
/// hashed at most once per actual rebuild. When the compiler binary content
/// changes the old incremental cache is invalidated; a no-op relink that
/// produces byte-identical output keeps the cache GREEN (raw mtime wiped it).
uint32_t compiler_build_hash();

/// F-023: a file-stable symbol id (31-bit, non-zero) derived from a test file's
/// path. Used as the `s{id}_` internal-symbol prefix and the `tml_test_<id>`
/// entry-wrapper name so the emitted IR is independent of the file's position
/// in the suite. Codegen (query_core) and the dispatcher generator
/// (testing_compile) call this on the SAME file_path so their symbol names
/// agree. Collision within a suite is astronomically unlikely (~N²/2³² per
/// suite) and no worse than the existing 16-hex obj-cache truncation tolerance.
[[nodiscard]] uint32_t stable_test_symbol_id(const std::string& file_path);

// ============================================================================
// F-019: Incremental cache telemetry (per run)
// ============================================================================

/// Process-global GREEN/RED and load/save counters for the incremental cache.
/// Reset at the start of a run, reported at the end (`incr_telemetry_report`).
struct IncrTelemetry {
    std::atomic<uint64_t> green_hits{0};   ///< CodegenUnit results reused from disk.
    std::atomic<uint64_t> red_misses{0};   ///< CodegenUnit results recomputed.
    std::atomic<uint64_t> cache_loads{0};  ///< incr.bin load operations.
    std::atomic<uint64_t> cache_saves{0};  ///< incr.bin save operations.
    std::atomic<uint64_t> load_us{0};      ///< Total time parsing incr.bin.
    std::atomic<uint64_t> save_us{0};      ///< Total time writing incr.bin.
    std::atomic<uint64_t> ir_bytes_gc{0};  ///< Bytes reclaimed by ir/ GC.
    std::atomic<uint64_t> ir_files_gc{0};  ///< ir/ files deleted by GC.
};

/// Access the process-global telemetry instance.
IncrTelemetry& incr_telemetry();

/// Reset all telemetry counters (call at run start).
void reset_incr_telemetry();

/// Human-readable one-line telemetry summary (call at run end).
[[nodiscard]] std::string incr_telemetry_report();

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

    /// Get all entries (for merging into writer).
    [[nodiscard]] const auto& entries() const {
        return entries_;
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

    /// Save link_search_paths for a CodegenUnit.
    bool save_link_search_paths(const QueryKey& key,
                                const std::vector<std::filesystem::path>& search_paths,
                                const std::filesystem::path& cache_dir);

    /// Write all entries to the binary cache file.
    ///
    /// F-021 (phase42a): `max_entries` caps the written set (0 = unlimited).
    /// Entries are written in recorded order — current-session records first,
    /// then merged previous-session entries (see `merge_from`) — so capping
    /// keeps the most-recently-used entries and ages out the oldest. This
    /// replaces the hard 10,000-entry *load* rejection (a silent total-reset
    /// cliff) with graceful session-recency aging at *save*.
    bool write(const std::filesystem::path& cache_file, uint32_t options_hash,
               size_t max_entries = 0);

    /// Merge entries from a previous session (entries not already recorded).
    void merge_from(const PrevSessionCache& prev);

    /// phase41c / F-010: merge entries recorded by another writer (entries not
    /// already recorded here). Used to fold many per-file test-compilation
    /// writers into a single per-suite accumulator so the suite flushes
    /// `incr.bin` once instead of once per file. Format-preserving — this does
    /// not change the on-disk cache layout or red-green semantics.
    void merge_from_writer(const IncrCacheWriter& other);

    /// Get number of recorded entries.
    [[nodiscard]] size_t entry_count() const {
        return entries_.size();
    }

private:
    std::vector<PrevSessionEntry> entries_;
    std::unordered_set<QueryKey, QueryKeyHash, QueryKeyEqual> recorded_keys_;
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

/// Load cached link_search_paths for a CodegenUnit query.
[[nodiscard]] std::vector<std::filesystem::path>
load_cached_link_search_paths(const QueryKey& key, const std::filesystem::path& cache_dir);

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

// ============================================================================
// F-025: config-partitioned cache files
// ============================================================================

/// The incr.bin path for a given options_hash: `incr.<hash>.bin`. Alternating
/// build configs (coverage↔normal, O0↔O2, defines, cached-library-state) each
/// get their own partition instead of mutually evicting one shared file.
[[nodiscard]] std::filesystem::path incr_cache_file_for(const std::filesystem::path& incr_dir,
                                                        uint32_t options_hash);

/// Keep only the `keep_newest` most-recently-modified `incr.*.bin` partitions
/// (plus the current one, which the caller just wrote); delete older ones. Also
/// migrates a legacy unpartitioned `incr.bin` out of the way. Returns the number
/// of partition files removed.
size_t prune_incr_partitions(const std::filesystem::path& incr_dir, size_t keep_newest);

// ============================================================================
// F-020: session-scoped shared previous-session cache (load once per run)
// ============================================================================

/// Return a shared, read-only previous-session cache for `cache_file`, loading
/// it from disk at most once per run. All per-file QueryContexts in a run share
/// the one parsed copy instead of each re-reading the whole file (the O(N²) read
/// churn of F-020). The load is memoized by (path, mtime): if the file is
/// rewritten (new run), the next call reloads. Returns nullptr when the cache is
/// missing/corrupt, was built by a different compiler, or its options_hash does
/// not match `options_hash`.
[[nodiscard]] std::shared_ptr<const PrevSessionCache>
get_shared_prev_session(const std::filesystem::path& cache_file, uint32_t options_hash);

/// Drop the shared prev-session memo (call at run teardown so a subsequent run
/// in the same process re-reads a freshly written cache).
void reset_shared_prev_session();

// ============================================================================
// F-022: IR store garbage collection
// ============================================================================

/// Delete `ir/<stem>.ll|.libs|.search_paths` files whose stem is NOT in
/// `surviving_stems`. Called once at run teardown after the final cache write so
/// orphaned full-stdlib IR dumps (2.2 GB, never GC'd before) are reclaimed and
/// the store stays bounded. Returns bytes reclaimed and sets file/byte
/// telemetry. Safe to call concurrently with nothing else touching ir/.
uint64_t gc_ir_store(const std::filesystem::path& cache_dir,
                     const std::unordered_set<std::string>& surviving_stems);

/// Run-teardown maintenance for the incremental cache, called ONCE after all
/// compilation in a run has flushed its entries (F-021/F-022/F-025): prune old
/// config partitions, then GC the ir/ store down to the keys still referenced by
/// the surviving partitions, then drop the shared prev-session memo. `incr_dir`
/// is `<build>/cache/incr`. Returns bytes reclaimed from ir/.
uint64_t incr_run_teardown(const std::filesystem::path& incr_dir);

} // namespace tml::query
