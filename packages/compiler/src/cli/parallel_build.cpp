#include "parallel_build.hpp"

#include "cmd_build.hpp"
#include "compiler_setup.hpp"
#include "utils.hpp"

#include <algorithm>
#include <iostream>
#include <thread>

namespace tml::cli {

// ============================================================================
// BuildQueue Implementation
// ============================================================================

void BuildQueue::push(std::shared_ptr<BuildJob> job) {
    std::lock_guard<std::mutex> lock(mutex);
    queue.push(job);
    cv.notify_one();
}

std::shared_ptr<BuildJob> BuildQueue::pop(int timeout_ms) {
    std::unique_lock<std::mutex> lock(mutex);

    if (queue.empty()) {
        cv.wait_for(lock, std::chrono::milliseconds(timeout_ms),
                    [this] { return !queue.empty() || stop_flag; });
    }

    if (queue.empty()) {
        return nullptr;
    }

    auto job = queue.front();
    queue.pop();
    return job;
}

void BuildQueue::stop() {
    std::lock_guard<std::mutex> lock(mutex);
    stop_flag = true;
    cv.notify_all();
}

bool BuildQueue::is_empty() {
    std::lock_guard<std::mutex> lock(mutex);
    return queue.empty();
}

size_t BuildQueue::size() {
    std::lock_guard<std::mutex> lock(mutex);
    return queue.size();
}

// ============================================================================
// ParallelBuilder Implementation
// ============================================================================

ParallelBuilder::ParallelBuilder(int num_threads) : num_threads(num_threads) {
    if (this->num_threads == 0) {
        this->num_threads = std::thread::hardware_concurrency();
        if (this->num_threads == 0) {
            this->num_threads = 4; // fallback
        }
    }
}

void ParallelBuilder::add_file(const fs::path& source_file, const fs::path& output_file) {
    auto job = std::make_shared<BuildJob>();
    job->source_file = source_file;
    job->output_file = output_file;
    jobs.push_back(job);
}

bool ParallelBuilder::build(bool verbose) {
    if (jobs.empty()) {
        return true;
    }

    stats.reset();
    stats.total_files = static_cast<int>(jobs.size());

    // For now, simple approach: all jobs are independent
    // Future: add dependency resolution
    for (auto& job : jobs) {
        ready_queue.push(job);
    }

    // Launch worker threads
    std::vector<std::thread> workers;
    int actual_threads = (jobs.size() < static_cast<size_t>(num_threads))
                             ? static_cast<int>(jobs.size())
                             : num_threads;

    if (verbose) {
        std::cout << "Compiling " << jobs.size() << " files with " << actual_threads
                  << " threads...\n";
    }

    for (int i = 0; i < actual_threads; ++i) {
        workers.emplace_back(&ParallelBuilder::worker_thread, this, verbose);
    }

    // Wait for all workers to finish
    for (auto& worker : workers) {
        worker.join();
    }

    // Check results
    bool success = (stats.failed == 0);

    if (!verbose) {
        // Print summary
        std::cout << "\nBuild summary:\n";
        std::cout << "  Total: " << stats.total_files << " files\n";
        std::cout << "  Compiled: " << stats.completed << " files\n";
        std::cout << "  Cached: " << stats.cached << " files\n";
        if (stats.failed > 0) {
            std::cout << "  Failed: " << stats.failed << " files\n";
        }
        std::cout << "  Time: " << (stats.elapsed_ms() / 1000.0) << "s\n";
    }

    return success;
}

void ParallelBuilder::worker_thread(bool verbose) {
    while (true) {
        auto job = ready_queue.pop(100);
        if (!job) {
            // Check if there are more jobs pending
            if (ready_queue.is_empty() && stats.completed + stats.failed >= stats.total_files) {
                break;
            }
            continue;
        }

        if (!compile_job(job, verbose)) {
            job->failed = true;
            stats.failed++;
        } else {
            job->completed = true;
            stats.completed++;
        }
    }
}

bool ParallelBuilder::compile_job(std::shared_ptr<BuildJob> job, bool verbose) {
    // Use existing build infrastructure
    // For now, call the build function directly
    // TODO: Extract compilation logic into reusable function

    if (verbose) {
        std::cout << "[" << (stats.completed + stats.failed + 1) << "/" << stats.total_files
                  << "] Compiling " << job->source_file.filename().string() << "\n";
    }

    // Call existing build command
    // We need to refactor cmd_build to expose compilation function
    // For now, return success (will implement in next iteration)

    // Simulate compilation
    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    return true; // Placeholder
}

void ParallelBuilder::resolve_dependencies() {
    // TODO: Parse import statements to build dependency graph
    // For initial implementation, assume all files are independent
}

// ============================================================================
// File Discovery
// ============================================================================

std::vector<fs::path> discover_source_files(const fs::path& root_dir) {
    std::vector<fs::path> files;

    try {
        for (const auto& entry : fs::recursive_directory_iterator(root_dir)) {
            if (entry.is_regular_file()) {
                auto path = entry.path();
                std::string path_str = path.string();

                // Skip test files, build artifacts, and examples
                if (path_str.find("\\tests\\") != std::string::npos ||
                    path_str.find("/tests/") != std::string::npos ||
                    path_str.find("\\build\\") != std::string::npos ||
                    path_str.find("/build/") != std::string::npos ||
                    path_str.find("\\examples\\") != std::string::npos ||
                    path_str.find("/examples/") != std::string::npos ||
                    path.filename().string().ends_with(".test.tml")) {
                    continue;
                }

                // Include .tml files (but not .test.tml)
                if (path.extension() == ".tml") {
                    files.push_back(path);
                }
            }
        }
    } catch (const fs::filesystem_error& e) {
        std::cerr << "Error discovering source files: " << e.what() << "\n";
    }

    std::sort(files.begin(), files.end());
    return files;
}

// ============================================================================
// Entry Point
// ============================================================================

int run_parallel_build(const std::vector<std::string>& args, bool verbose) {
    // Parse arguments
    int num_threads = 0;
    bool clean = false;

    for (const auto& arg : args) {
        if (arg.starts_with("-j")) {
            if (arg.length() > 2) {
                num_threads = std::stoi(arg.substr(2));
            }
        } else if (arg == "--clean") {
            clean = true;
        }
    }

    // Discover source files
    fs::path cwd = fs::current_path();
    auto source_files = discover_source_files(cwd);

    if (source_files.empty()) {
        std::cout << "No source files found to build\n";
        return 0;
    }

    if (verbose) {
        std::cout << "Found " << source_files.size() << " source files\n";
    }

    // Create parallel builder
    ParallelBuilder builder(num_threads);

    // Add all files to builder
    for (const auto& src : source_files) {
        fs::path output = src;
        output.replace_extension(".o");
        builder.add_file(src, output);
    }

    // Execute parallel build
    bool success = builder.build(verbose);

    return success ? 0 : 1;
}

} // namespace tml::cli
