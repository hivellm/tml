//! # Test Result Cache
//!
//! Suite-level test result caching for the coordinator. Answers one question:
//! "Can I skip running this suite because nothing changed and it passed last time?"
//!
//! Inspired by Go's test cache (cache-passing-only rule): only passing suites
//! are cached. Failing suites always re-run.
//!
//! ## Cache Key
//!
//! A suite is cache-valid when ALL of:
//! 1. Suite name matches (same files grouped the same way)
//! 2. All source file hashes match (no source changes)
//! 3. Compiler hash matches (no compiler update)
//! 4. Flags hash matches (same coverage/optimization settings)
//! 5. Previous result was PASS

#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace tml::testing {

struct TestConfig; // forward declaration

// ============================================================================
// Cache entry for a single suite
// ============================================================================

struct SuiteCacheEntry {
    std::vector<std::string> source_hashes; // CRC32C hex of each .tml file (sorted)
    std::string flags_hash;                 // Hash of coverage + optimization settings
    bool all_passed = false;                // Cache-passing-only: only true means cacheable
    int test_count = 0;
    int passed_count = 0;
    int failed_count = 0;
    int64_t duration_us = 0;     // Execution time of last run (microseconds)
    int64_t compile_time_us = 0; // Compilation time of last run (microseconds)
    std::string exe_path;        // Path to compiled EXE (for potential reuse)

    /// Phase 8.5 W5: the transitive source file paths (test files + imported
    /// library/package modules) whose content hashes produced `source_hashes`.
    /// Persisted so the early cache-check can re-hash the SAME set of files
    /// without needing to re-compile. Populated on the first real compilation
    /// and reused on subsequent runs.
    std::vector<std::string> source_paths;
};

// ============================================================================
// Test result cache
// ============================================================================

class TestResultCache {
public:
    /// Load cache from disk. Returns true if loaded successfully.
    bool load(const std::string& cache_file);

    /// Save cache to disk. Returns true if saved successfully.
    bool save(const std::string& cache_file) const;

    /// Check if a suite can be skipped (all hashes match + passed last time).
    bool is_cached(const std::string& suite_name, const std::vector<std::string>& source_hashes,
                   const std::string& flags_hash) const;

    /// Check if a previously compiled exe can be reused for a suite that failed.
    /// Returns the exe path if the source is unchanged and the exe still exists on disk.
    /// Unlike is_cached(), does NOT require all_passed == true.
    /// Use this to skip recompilation when only re-running a failing suite.
    std::string get_reusable_exe(const std::string& suite_name,
                                 const std::vector<std::string>& source_hashes,
                                 const std::string& flags_hash) const;

    /// Get cached entry for a suite (returns nullptr if not found).
    const SuiteCacheEntry* get(const std::string& suite_name) const;

    /// Update/insert a cache entry for a suite.
    void update(const std::string& suite_name, const SuiteCacheEntry& entry);

    /// Get the compiler hash stored in the cache.
    const std::string& compiler_hash() const {
        return compiler_hash_;
    }

    /// Set the compiler hash (call once before is_cached checks).
    void set_compiler_hash(const std::string& hash) {
        compiler_hash_ = hash;
    }

    /// Number of cached suites.
    size_t size() const {
        return entries_.size();
    }

    /// Clear all entries.
    void clear() {
        entries_.clear();
        compiler_hash_.clear();
    }

    /// Downgrade all entries: clear all_passed but keep exe_path for reuse.
    /// Used when compiler changes — exes need re-execution but may not need recompilation
    /// if source is unchanged and the exe still exists on disk.
    void downgrade_to_exe_reusable() {
        for (auto& [name, entry] : entries_) {
            entry.all_passed = false;
        }
    }

    /// Fully invalidate all entries: clear all_passed AND exe_path.
    /// Used when the runtime library changes — old EXEs have the old runtime baked in
    /// and must be recompiled, not just re-run.
    void invalidate_all_exes() {
        for (auto& [name, entry] : entries_) {
            entry.all_passed = false;
            entry.exe_path.clear();
        }
    }

    // ========================================================================
    // Static utility functions
    // ========================================================================

    /// Compute CRC32C hash of the compiler executable.
    /// Result is cached after first call (static).
    static std::string compute_compiler_hash();

    /// Compute a flags hash from the test configuration.
    static std::string compute_flags_hash(const TestConfig& config);

    /// Compute sorted source file hashes for a list of file paths.
    static std::vector<std::string>
    compute_source_hashes(const std::vector<std::string>& file_paths);

private:
    std::string compiler_hash_;
    std::map<std::string, SuiteCacheEntry> entries_;
};

} // namespace tml::testing
