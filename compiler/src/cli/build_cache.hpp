#pragma once

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
// MIR Cache
// ============================================================================

/**
 * Cache entry metadata
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
    };
    CacheStats get_stats() const;

private:
    fs::path cache_dir_;
    fs::path index_file_;
    mutable std::unordered_map<std::string, CacheEntry> entries_;
    mutable bool loaded_ = false;

    void load_index() const;
    void save_index() const;
    std::string compute_cache_key(const std::string& source_path) const;
    fs::path get_mir_path(const std::string& cache_key) const;
    fs::path get_obj_path(const std::string& cache_key) const;
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
