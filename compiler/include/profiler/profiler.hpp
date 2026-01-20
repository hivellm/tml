#pragma once

/**
 * @file profiler.hpp
 * @brief TML Native Profiler - Chrome DevTools compatible profiling
 *
 * This profiler generates .cpuprofile files that can be loaded in:
 * - Chrome DevTools (Performance tab)
 * - VS Code (JavaScript Profiler extension)
 * - Any tool that supports the V8 CPU profile format
 *
 * Usage:
 *   tml build program.tml --profile          # Enable profiling
 *   tml run program.tml --profile            # Run with profiling
 *   tml run program.tml --profile=output.cpuprofile  # Custom output
 *
 * The profiler tracks:
 * - Function entry/exit times
 * - Call stacks
 * - Time spent in each function
 * - Call counts
 */

#include <atomic>
#include <chrono>
#include <cstdint>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace tml::profiler {

// ============================================================================
// Data Structures
// ============================================================================

/**
 * @brief Represents a single call frame in the profile
 */
struct CallFrame {
    uint32_t id;              // Unique node ID
    std::string function_name;
    std::string file_name;
    uint32_t line_number;
    uint32_t column_number;
    uint32_t parent_id;       // Parent node ID (0 for root)
    uint64_t self_time_us;    // Time spent in this function (excluding children)
    uint64_t total_time_us;   // Total time (including children)
    uint64_t hit_count;       // Number of times this was sampled/called
    std::vector<uint32_t> children;  // Child node IDs
};

/**
 * @brief A sample point in the profile (for sampling profiler)
 */
struct Sample {
    uint32_t node_id;         // Which node was active
    int64_t timestamp_us;     // Microseconds since profile start
};

/**
 * @brief Stack frame for tracking call hierarchy
 */
struct StackFrame {
    uint32_t node_id;
    int64_t enter_time_us;
};

/**
 * @brief Profile data that will be exported
 */
struct ProfileData {
    std::vector<CallFrame> nodes;
    std::vector<Sample> samples;
    std::vector<int64_t> time_deltas;  // Delta between samples in microseconds
    int64_t start_time;       // Profile start (microseconds since epoch)
    int64_t end_time;         // Profile end (microseconds since epoch)
};

// ============================================================================
// Profiler Class
// ============================================================================

/**
 * @brief Main profiler class - singleton
 *
 * Thread-safe profiling with minimal overhead when disabled.
 */
class Profiler {
public:
    /**
     * @brief Get the singleton profiler instance
     */
    static auto instance() -> Profiler&;

    /**
     * @brief Initialize the profiler
     * @param output_path Path for the .cpuprofile output file
     * @param sampling_interval_us Sampling interval in microseconds (0 = instrumentation only)
     */
    void initialize(const std::string& output_path = "profile.cpuprofile",
                    uint64_t sampling_interval_us = 0);

    /**
     * @brief Start profiling
     */
    void start();

    /**
     * @brief Stop profiling and write output
     */
    void stop();

    /**
     * @brief Check if profiling is active
     */
    [[nodiscard]] auto is_active() const -> bool { return active_.load(std::memory_order_relaxed); }

    /**
     * @brief Record function entry (called by instrumented code)
     * @param func_name Function name
     * @param file_name Source file name
     * @param line Line number
     */
    void enter_function(const char* func_name, const char* file_name, uint32_t line);

    /**
     * @brief Record function exit (called by instrumented code)
     */
    void exit_function();

    /**
     * @brief Add a sample at current position (for sampling profiler)
     */
    void add_sample();

    /**
     * @brief Register a function (for pre-registration during codegen)
     * @param func_name Function name
     * @param file_name Source file name
     * @param line Line number
     * @return Node ID for this function
     */
    auto register_function(const std::string& func_name,
                           const std::string& file_name,
                           uint32_t line) -> uint32_t;

    /**
     * @brief Get or create a node for a function call
     */
    auto get_or_create_node(const std::string& func_name,
                            const std::string& file_name,
                            uint32_t line,
                            uint32_t parent_id) -> uint32_t;

    /**
     * @brief Export profile data to .cpuprofile format
     */
    void export_cpuprofile(const std::string& path);

    /**
     * @brief Get current timestamp in microseconds
     */
    static auto now_us() -> int64_t;

    // Prevent copying
    Profiler(const Profiler&) = delete;
    auto operator=(const Profiler&) -> Profiler& = delete;

private:
    Profiler() = default;
    ~Profiler();

    // Thread-local call stack
    static thread_local std::vector<StackFrame> call_stack_;

    // Profile data (protected by mutex)
    std::mutex mutex_;
    ProfileData data_;
    std::unordered_map<std::string, uint32_t> node_map_;  // Key: "parent_id:func:file:line"

    // Configuration
    std::string output_path_{"profile.cpuprofile"};
    uint64_t sampling_interval_us_{0};

    // State
    std::atomic<bool> active_{false};
    std::atomic<bool> initialized_{false};

    // Helper to generate unique node key
    static auto make_node_key(uint32_t parent_id,
                              const std::string& func_name,
                              const std::string& file_name,
                              uint32_t line) -> std::string;

    // Generate cpuprofile JSON
    [[nodiscard]] auto to_cpuprofile_json() const -> std::string;
};

// ============================================================================
// C API for Runtime Integration
// ============================================================================

extern "C" {

/**
 * @brief Initialize profiler from TML runtime
 * @param output_path Output file path (can be NULL for default)
 */
void tml_profiler_init(const char* output_path);

/**
 * @brief Start profiling
 */
void tml_profiler_start(void);

/**
 * @brief Stop profiling and write output
 */
void tml_profiler_stop(void);

/**
 * @brief Record function entry
 * @param func_name Function name (null-terminated)
 * @param file_name Source file (null-terminated)
 * @param line Line number
 */
void tml_profiler_enter(const char* func_name, const char* file_name, uint32_t line);

/**
 * @brief Record function exit
 */
void tml_profiler_exit(void);

/**
 * @brief Check if profiler is active (fast check for instrumented code)
 * @return 1 if active, 0 otherwise
 */
int32_t tml_profiler_is_active(void);

/**
 * @brief Add a manual sample point
 */
void tml_profiler_sample(void);

}  // extern "C"

// ============================================================================
// Instrumentation Macros (for C++ code using the profiler)
// ============================================================================

#define TML_PROFILE_FUNCTION() \
    tml::profiler::ScopedProfiler _profiler_scope_(__FUNCTION__, __FILE__, __LINE__)

#define TML_PROFILE_SCOPE(name) \
    tml::profiler::ScopedProfiler _profiler_scope_(name, __FILE__, __LINE__)

/**
 * @brief RAII helper for scoped profiling
 */
class ScopedProfiler {
public:
    ScopedProfiler(const char* func_name, const char* file_name, uint32_t line) {
        if (Profiler::instance().is_active()) {
            Profiler::instance().enter_function(func_name, file_name, line);
            active_ = true;
        }
    }

    ~ScopedProfiler() {
        if (active_) {
            Profiler::instance().exit_function();
        }
    }

    // Prevent copying
    ScopedProfiler(const ScopedProfiler&) = delete;
    auto operator=(const ScopedProfiler&) -> ScopedProfiler& = delete;

private:
    bool active_ = false;
};

}  // namespace tml::profiler
