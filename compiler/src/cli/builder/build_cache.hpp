//! # Build Cache Interface
//!
//! This header defines the incremental compilation cache.
//!
//! ## Cache Levels
//!
//! | Level      | Stores                    | Invalidated By          |
//! |------------|---------------------------|-------------------------|
//! | Module     | MIR binary, object file   | Source change, options  |
//! | Function   | Per-function MIR          | Function body change    |
//!
//! ## Cache Keys
//!
//! - **Source hash**: SHA256 of source content
//! - **Signature hash**: Hash of function parameters and return type
//! - **Body hash**: Hash of function instructions
//! - **Deps hash**: Hash of used types/constants
//!
//! ## Phase Timing
//!
//! `PhaseTimer` and `ScopedPhaseTimer` measure compilation phases
//! for profiling with `--time` flag.

#pragma once

#include "hir/hir_module.hpp"
#include "hir/hir_serialize.hpp"
#include "mir/mir.hpp"
#include "mir/mir_serialize.hpp"

#include <chrono>
#include <filesystem>
#include <functional>
#include <iomanip>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

namespace fs = std::filesystem;

namespace tml::cli {

// ============================================================================
// Compiler Phase Timing
// ============================================================================

/**
 * Timer for measuring compiler phase durations
 */
class PhaseTimer {
public:
    void start(const std::string& phase) {
        current_phase_ = phase;
        start_time_ = std::chrono::high_resolution_clock::now();
    }

    void stop() {
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration =
            std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time_);
        timings_[current_phase_] = duration.count();
        total_time_ += duration.count();
    }

    void report(std::ostream& out) const {
        out << "\n=== Compiler Phase Timings ===\n";
        for (const auto& [phase, us] : timings_) {
            double ms = us / 1000.0;
            double pct = (total_time_ > 0) ? (100.0 * us / total_time_) : 0;
            out << std::setw(20) << std::left << phase << ": " << std::setw(8) << std::right
                << std::fixed << std::setprecision(2) << ms << " ms"
                << " (" << std::setw(5) << std::fixed << std::setprecision(1) << pct << "%)\n";
        }
        out << std::string(40, '-') << "\n";
        out << std::setw(20) << std::left << "Total" << ": " << std::setw(8) << std::right
            << std::fixed << std::setprecision(2) << (total_time_ / 1000.0) << " ms\n";
    }

    int64_t get_timing(const std::string& phase) const {
        auto it = timings_.find(phase);
        return (it != timings_.end()) ? it->second : 0;
    }

    int64_t total_us() const {
        return total_time_;
    }

private:
    std::string current_phase_;
    std::chrono::high_resolution_clock::time_point start_time_;
    std::unordered_map<std::string, int64_t> timings_; // phase -> microseconds
    int64_t total_time_ = 0;
};

/**
 * RAII timer for automatic phase timing
 */
class ScopedPhaseTimer {
public:
    ScopedPhaseTimer(PhaseTimer& timer, const std::string& phase) : timer_(timer) {
        timer_.start(phase);
    }
    ~ScopedPhaseTimer() {
        timer_.stop();
    }

private:
    PhaseTimer& timer_;
};

// ============================================================================
// HIR Cache
// ============================================================================

/**
 * HIR cache for incremental compilation
 *
 * The HIR cache stores compiled HIR modules to avoid re-parsing and type
 * checking when source files haven't changed. This is the first level of
 * caching in the compilation pipeline:
 *
 * ```text
 * Source → [HIR Cache] → HIR → [MIR Cache] → MIR → Object
 * ```
 *
 * Benefits:
 * - Skip lexing, parsing, type checking for unchanged files
 * - Faster incremental builds
 * - Dependency tracking for proper invalidation
 */
class HirCache {
public:
    explicit HirCache(const fs::path& cache_dir);

    // Check if a valid cache entry exists for this source
    bool has_valid_cache(const std::string& source_path) const;

    // Load cached HIR module (returns nullopt if not cached or invalid)
    std::optional<hir::HirModule> load_hir(const std::string& source_path) const;

    // Save HIR module to cache with dependencies
    bool save_hir(const std::string& source_path, const hir::HirModule& module,
                  const std::vector<std::string>& dependencies = {});

    // Clear all cached entries
    void clear();

    // Clear cache for a specific source file
    void invalidate(const std::string& source_path);

    // Get cache statistics
    struct CacheStats {
        size_t total_entries;
        size_t valid_entries;
        size_t total_size_bytes;
    };
    CacheStats get_stats() const;

private:
    fs::path cache_dir_;
    fs::path index_file_;
    mutable std::unordered_map<std::string, hir::HirCacheInfo> entries_;
    mutable bool loaded_ = false;

    void load_index() const;
    void save_index() const;
    std::string compute_cache_key(const std::string& source_path) const;
    fs::path get_hir_path(const std::string& cache_key) const;
    fs::path get_info_path(const std::string& cache_key) const;
};

// ============================================================================
// MIR Cache
// ============================================================================

/**
 * Cache entry metadata (module-level)
 */
struct CacheEntry {
    std::string source_hash; // Hash of source file content
    std::string mir_file;    // Path to cached MIR binary
    std::string object_file; // Path to cached object file
    int64_t source_mtime;    // Source file modification time
    int optimization_level;  // Optimization level used
    bool debug_info;         // Debug info enabled
};

/**
 * Per-function cache entry metadata
 */
struct FunctionCacheEntry {
    std::string function_name;  // Fully qualified function name
    std::string signature_hash; // Hash of function signature (params + return type)
    std::string body_hash;      // Hash of function body (instructions)
    std::string deps_hash;      // Hash of dependencies (structs, enums, constants used)
    std::string mir_file;       // Path to cached function MIR binary
    int optimization_level;     // Optimization level used
};

/**
 * MIR cache for incremental compilation
 *
 * The cache stores:
 * - Pre-optimized MIR (after type checking)
 * - Optimized MIR (after pass pipeline)
 * - Object files (after codegen)
 *
 * Cache invalidation triggers:
 * - Source file content change (hash mismatch)
 * - Optimization level change
 * - Debug info setting change
 * - Compiler version change
 */
class MirCache {
public:
    explicit MirCache(const fs::path& cache_dir);

    // Check if a valid cache entry exists for this source
    bool has_valid_cache(const std::string& source_path, const std::string& content_hash,
                         int opt_level, bool debug_info) const;

    // Load cached MIR module (returns nullopt if not cached or invalid)
    std::optional<mir::Module> load_mir(const std::string& source_path) const;

    // Save MIR module to cache
    bool save_mir(const std::string& source_path, const std::string& content_hash,
                  const mir::Module& module, int opt_level, bool debug_info);

    // Get cached object file path (returns empty if not valid)
    fs::path get_cached_object(const std::string& source_path) const;

    // Save object file to cache
    bool save_object(const std::string& source_path, const fs::path& object_file);

    // Clear all cached entries
    void clear();

    // Clear cache for a specific source file
    void invalidate(const std::string& source_path);

    // Get cache statistics
    struct CacheStats {
        size_t total_entries;
        size_t valid_entries;
        size_t total_size_bytes;
        size_t function_entries;    // Per-function cache entries
        size_t function_cache_hits; // Functions loaded from cache
    };
    CacheStats get_stats() const;

    // ========================================================================
    // Per-Function Caching
    // ========================================================================

    // Check if a function has valid cached MIR
    bool has_valid_function_cache(const std::string& source_path, const std::string& function_name,
                                  const std::string& signature_hash, const std::string& body_hash,
                                  const std::string& deps_hash, int opt_level) const;

    // Load a single cached function
    std::optional<mir::Function> load_function(const std::string& source_path,
                                               const std::string& function_name) const;

    // Save a single function to cache
    bool save_function(const std::string& source_path, const std::string& function_name,
                       const std::string& signature_hash, const std::string& body_hash,
                       const std::string& deps_hash, const mir::Function& func, int opt_level);

    // Compute hash for function signature
    static std::string hash_function_signature(const mir::Function& func);

    // Compute hash for function body
    static std::string hash_function_body(const mir::Function& func);

    // Compute hash for function dependencies (used types)
    static std::string hash_function_deps(const mir::Function& func, const mir::Module& module);

    // Get function cache statistics
    struct FunctionCacheStats {
        size_t total_functions;
        size_t cached_functions;
        size_t cache_hits;
        size_t cache_misses;
    };
    FunctionCacheStats get_function_stats() const;

private:
    fs::path cache_dir_;
    fs::path index_file_;
    fs::path func_index_file_; // Separate index for per-function cache
    mutable std::unordered_map<std::string, CacheEntry> entries_;
    mutable std::unordered_map<std::string, FunctionCacheEntry>
        func_entries_; // key: source_path::func_name
    mutable bool loaded_ = false;
    mutable bool func_loaded_ = false;
    mutable FunctionCacheStats func_stats_ = {0, 0, 0, 0};

    void load_index() const;
    void save_index() const;
    void load_func_index() const;
    void save_func_index() const;
    std::string compute_cache_key(const std::string& source_path) const;
    std::string compute_func_cache_key(const std::string& source_path,
                                       const std::string& function_name) const;
    fs::path get_mir_path(const std::string& cache_key) const;
    fs::path get_obj_path(const std::string& cache_key) const;
    fs::path get_func_mir_path(const std::string& cache_key) const;
};

// ============================================================================
// Utility Functions
// ============================================================================

// Generate a content hash for a source file
std::string hash_file_content(const std::string& content);

// Get modification time as integer
int64_t get_mtime(const fs::path& path);

// Global phase timer (optional, enabled with --time flag)
extern thread_local PhaseTimer* g_phase_timer;

// Macro for scoped timing
#define TML_PHASE_TIME(timer, name) tml::cli::ScopedPhaseTimer _phase_timer_##__LINE__(timer, name)

} // namespace tml::cli
