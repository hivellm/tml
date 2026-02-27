//! # Cross-Platform Process Management
//!
//! RAII-based subprocess abstraction for the TML test system.
//! Supports launch with pipe capture, non-blocking poll, timeout wait, and
//! graceful kill with descendant process termination.
//!
//! ## Platform Support
//!
//! - Windows: CreateProcessA + Job Objects (KILL_ON_JOB_CLOSE) + Named Pipes
//! - Unix: fork/execvp + Process Groups (setpgid) + pipe/poll
//!
//! ## Usage
//!
//! ```cpp
//! auto proc = Process::launch("/path/to/exe", {"--run-all"}, {{"TML_COVERAGE_FILE", "cov.txt"}});
//! if (!proc) { /* handle error */ }
//!
//! while (!proc->is_done()) {
//!     auto out = proc->read_stdout();
//!     // process output incrementally...
//! }
//!
//! auto result = proc->wait(std::chrono::seconds(60));
//! ```

#pragma once

#include <chrono>
#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace tml::testing {

/// Result of waiting for a process to complete.
struct ProcessResult {
    int exit_code = -1;
    bool timed_out = false;
    bool launched = false;
    std::string stdout_output;
    std::string stderr_output;
    int64_t duration_us = 0;
};

/// Options for launching a process.
struct ProcessOptions {
    std::string exe_path;
    std::vector<std::string> args;
    std::map<std::string, std::string> env;
    std::chrono::milliseconds timeout{300'000};
    std::string working_dir;
};

/// Cross-platform subprocess with pipe I/O and process tree management.
///
/// Owns all platform handles (pipes, process, job object / process group).
/// Non-copyable, movable. Destructor kills the process if still running.
class Process {
public:
    Process(Process&& other) noexcept;
    Process& operator=(Process&& other) noexcept;
    ~Process();

    Process(const Process&) = delete;
    Process& operator=(const Process&) = delete;

    /// Launch a subprocess with pipe capture and process tree management.
    static std::optional<Process> launch(const ProcessOptions& opts);

    /// Convenience: launch(exe_path, args, env) with default timeout.
    static std::optional<Process> launch(const std::string& exe_path,
                                         const std::vector<std::string>& args = {},
                                         const std::map<std::string, std::string>& env = {});

    /// Check if the process has exited (non-blocking). Also drains pipes.
    bool is_done();

    /// Block until process exits or timeout expires. Drains all pipe data.
    /// On timeout, kills the process and sets timed_out=true.
    ProcessResult wait(std::chrono::milliseconds timeout);

    /// Wait using the timeout specified at launch.
    ProcessResult wait();

    /// Read available stdout data (non-blocking).
    std::string read_stdout();

    /// Read available stderr data (non-blocking).
    std::string read_stderr();

    /// Force-terminate the process and all descendants.
    void kill();

    /// Get the process ID (platform-native).
    int64_t pid() const;

    /// Get the executable path.
    const std::string& exe_path() const;

    /// True if the process was successfully launched.
    bool valid() const;

    /// Elapsed time since launch.
    std::chrono::microseconds elapsed() const;

private:
    Process();

    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace tml::testing
