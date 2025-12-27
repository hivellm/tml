#ifndef TML_CLI_PARALLEL_BUILD_HPP
#define TML_CLI_PARALLEL_BUILD_HPP

#include <atomic>
#include <condition_variable>
#include <filesystem>
#include <memory>
#include <mutex>
#include <queue>
#include <set>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace tml::cli {

/**
 * Build job representing a single file to compile
 */
struct BuildJob {
    fs::path source_file;
    fs::path output_file;
    std::vector<std::string> dependencies; // Module import names
    bool completed = false;
    bool failed = false;
    std::string error_message;
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
 * Parallel build orchestrator
 * Manages compilation of multiple files using thread pool
 */
class ParallelBuilder {
public:
    ParallelBuilder(int num_threads = 0);

    // Add files to build
    void add_file(const fs::path& source_file, const fs::path& output_file);

    // Execute parallel build
    bool build(bool verbose = false);

    // Get build statistics
    const BuildStats& get_stats() const {
        return stats;
    }

private:
    int num_threads;
    std::vector<std::shared_ptr<BuildJob>> jobs;
    BuildQueue ready_queue;
    BuildStats stats;

    // Worker thread function
    void worker_thread(bool verbose);

    // Compile a single job
    bool compile_job(std::shared_ptr<BuildJob> job, bool verbose);

    // Resolve dependencies and populate ready queue
    void resolve_dependencies();
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
