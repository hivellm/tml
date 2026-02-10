//! # Parallel Build System
//!
//! This header defines the multi-threaded build infrastructure.
//!
//! ## Components
//!
//! | Class             | Description                              |
//! |-------------------|------------------------------------------|
//! | `BuildJob`        | Single file compilation task             |
//! | `BuildQueue`      | Thread-safe work queue                   |
//! | `BuildStats`      | Compilation statistics                   |
//! | `DependencyGraph` | Module dependency ordering               |
//! | `ParallelBuilder` | Orchestrates parallel compilation        |
//!
//! ## Build Pipeline
//!
//! ```text
//! source files → dependency analysis → topological sort → parallel compile → link
//! ```
//!
//! ## Caching
//!
//! Each `BuildJob` includes a content hash for cache lookup.
//! Cached object files are reused when source hasn't changed.

#ifndef TML_CLI_PARALLEL_BUILD_HPP
#define TML_CLI_PARALLEL_BUILD_HPP

#include <atomic>
#include <condition_variable>
#include <filesystem>
#include <functional>
#include <memory>
#include <mutex>
#include <queue>
#include <set>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace fs = std::filesystem;

namespace tml::cli {

/**
 * Build job representing a single file to compile
 */
struct BuildJob {
    fs::path source_file;
    fs::path output_file;
    std::vector<std::string> dependencies;    // Module import names
    std::vector<std::string> dependent_files; // Files that depend on this one
    int pending_deps = 0;                     // Number of unresolved dependencies
    bool completed = false;
    bool failed = false;
    bool cached = false; // True if we used cached object file
    bool queued = false; // True if already added to ready queue
    std::string error_message;
    std::string content_hash; // Hash of source content for caching
};

/**
 * Build statistics for reporting
 */
struct BuildStats {
    std::atomic<int> total_files{0};
    std::atomic<int> completed{0};
    std::atomic<int> failed{0};
    std::atomic<int> cached{0};
    std::chrono::high_resolution_clock::time_point start_time;

    void reset() {
        total_files = 0;
        completed = 0;
        failed = 0;
        cached = 0;
        start_time = std::chrono::high_resolution_clock::now();
    }

    int64_t elapsed_ms() const {
        auto now = std::chrono::high_resolution_clock::now();
        return std::chrono::duration_cast<std::chrono::milliseconds>(now - start_time).count();
    }
};

/**
 * Thread-safe work queue for parallel builds
 */
class BuildQueue {
public:
    BuildQueue() : stop_flag(false) {}

    void push(std::shared_ptr<BuildJob> job);
    std::shared_ptr<BuildJob> pop(int timeout_ms = 100);
    void stop();
    bool is_empty();
    size_t size();

private:
    std::queue<std::shared_ptr<BuildJob>> queue;
    std::mutex mutex;
    std::condition_variable cv;
    bool stop_flag;
};

/**
 * Build options for parallel builder
 */
struct ParallelBuildOptions {
    bool verbose = false;
    bool no_cache = false;
    bool lto = false;     // Enable Link-Time Optimization
    bool use_hir = false; // Use HIR pipeline (AST→HIR→MIR→Codegen)
    int optimization_level = 0;
    bool debug_info = false;
    std::string output_dir; // Output directory for build artifacts
    std::string cache_dir;  // Cache directory for object files
    bool polonius = false;  // Use Polonius borrow checker
};

/**
 * Dependency graph for build ordering
 */
class DependencyGraph {
public:
    // Add a file with its dependencies
    void add_file(const std::string& file, const std::vector<std::string>& deps);

    // Get files with no pending dependencies (ready to build)
    std::vector<std::string> get_ready_files() const;

    // Mark a file as complete and update dependents
    void mark_complete(const std::string& file);

    // Check if all files are complete
    bool all_complete() const;

    // Check for circular dependencies
    bool has_cycles() const;

    // Get topologically sorted order
    std::vector<std::string> topological_sort() const;

private:
    std::unordered_map<std::string, std::vector<std::string>> deps_;  // file -> dependencies
    std::unordered_map<std::string, std::vector<std::string>> rdeps_; // file -> dependents
    std::unordered_map<std::string, int> pending_count_;              // pending dep count
    std::unordered_set<std::string> completed_;                       // completed files
    mutable std::mutex mutex_;
};

/**
 * Parallel build orchestrator
 * Manages compilation of multiple files using thread pool
 */
class ParallelBuilder {
public:
    ParallelBuilder(int num_threads = 0);

    // Add files to build
    void add_file(const fs::path& source_file, const fs::path& output_file);

    // Set build options
    void set_options(const ParallelBuildOptions& opts) {
        options = opts;
    }

    // Execute parallel build
    bool build(bool verbose = false);

    // Get build statistics
    const BuildStats& get_stats() const {
        return stats;
    }

    // Get all compiled object files
    std::vector<fs::path> get_object_files() const;

private:
    int num_threads;
    std::vector<std::shared_ptr<BuildJob>> jobs;
    std::unordered_map<std::string, std::shared_ptr<BuildJob>> job_map; // path -> job
    BuildQueue ready_queue;
    BuildStats stats;
    ParallelBuildOptions options;
    DependencyGraph dep_graph;
    std::mutex job_mutex;

    // Worker thread function
    void worker_thread(bool verbose);

    // Compile a single job
    bool compile_job(std::shared_ptr<BuildJob> job, bool verbose);

    // Resolve dependencies and populate ready queue
    void resolve_dependencies();

    // Parse imports from a TML source file
    std::vector<std::string> parse_imports(const fs::path& source_file);

    // Notify dependent jobs that a dependency completed
    void notify_dependents(std::shared_ptr<BuildJob> job);

    // Check if object file is cached and valid
    bool is_cached(std::shared_ptr<BuildJob> job);

    // Generate content hash for a source file
    std::string generate_hash(const fs::path& source_file);
};

/**
 * Discover all .tml files in a directory recursively
 */
std::vector<fs::path> discover_source_files(const fs::path& root_dir);

/**
 * Parallel build entry point
 * Builds all .tml files in the current directory
 */
int run_parallel_build(const std::vector<std::string>& args, bool verbose);

} // namespace tml::cli

#endif // TML_CLI_PARALLEL_BUILD_HPP
