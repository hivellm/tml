TML_MODULE("test")

//! # Cross-Platform Process Implementation
//!
//! Single-file implementation with platform-specific code gated by `#ifdef _WIN32`.
//!
//! ## Windows
//! - CreateProcessA with CREATE_SUSPENDED + CREATE_NEW_PROCESS_GROUP
//! - Job object with JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE for descendant termination
//! - PeekNamedPipe + ReadFile for deadlock-free pipe multiplexing
//!
//! ## Unix
//! - fork() + execvp() with setpgid(0,0) for process group
//! - poll() on stdout/stderr fds for deadlock-free pipe multiplexing
//! - SIGTERM + grace period + SIGKILL escalation (nextest pattern)

#include "testing/testing_process.hpp"

#include "log/log.hpp"

#include <algorithm>
#include <chrono>
#include <cstring>
#include <thread>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace tml::testing {

// ============================================================================
// Platform-specific Impl
// ============================================================================

struct Process::Impl {
#ifdef _WIN32
    HANDLE process = INVALID_HANDLE_VALUE;
    HANDLE job = INVALID_HANDLE_VALUE;
    HANDLE stdout_read = INVALID_HANDLE_VALUE;
    HANDLE stderr_read = INVALID_HANDLE_VALUE;
#else
    pid_t child_pid = -1;
    pid_t pgid = -1;
    int stdout_fd = -1;
    int stderr_fd = -1;
    bool reaped = false;
    int wait_status = -1;
#endif
    std::string exe;
    std::chrono::steady_clock::time_point start_time;
    std::chrono::milliseconds timeout{300'000};
    std::string stdout_buf;
    std::string stderr_buf;
    bool done = false;
    int cached_exit_code = -1;

    // Drain both pipes to prevent 64KB buffer deadlock.
    void drain_pipes();
};

// ============================================================================
// Constructor / Destructor / Move
// ============================================================================

Process::Process() : impl_(std::make_unique<Impl>()) {}

Process::Process(Process&& other) noexcept = default;
Process& Process::operator=(Process&& other) noexcept = default;

Process::~Process() {
    if (impl_ && valid() && !impl_->done) {
        kill();
    }
}

// ============================================================================
// Shared helpers
// ============================================================================

int64_t Process::pid() const {
    if (!impl_)
        return -1;
#ifdef _WIN32
    return (impl_->process != INVALID_HANDLE_VALUE) ? GetProcessId(impl_->process) : -1;
#else
    return impl_->child_pid;
#endif
}

const std::string& Process::exe_path() const {
    static const std::string empty;
    return impl_ ? impl_->exe : empty;
}

bool Process::valid() const {
    if (!impl_)
        return false;
#ifdef _WIN32
    return impl_->process != INVALID_HANDLE_VALUE;
#else
    return impl_->child_pid > 0;
#endif
}

std::chrono::microseconds Process::elapsed() const {
    if (!impl_)
        return std::chrono::microseconds(0);
    return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() -
                                                                 impl_->start_time);
}

ProcessResult Process::wait() {
    return wait(impl_ ? impl_->timeout : std::chrono::milliseconds(300'000));
}

std::optional<Process> Process::launch(const std::string& exe_path,
                                       const std::vector<std::string>& args,
                                       const std::map<std::string, std::string>& env) {
    ProcessOptions opts;
    opts.exe_path = exe_path;
    opts.args = args;
    opts.env = env;
    return launch(opts);
}

// ============================================================================
// WINDOWS IMPLEMENTATION
// ============================================================================

#ifdef _WIN32

// Build a double-null-terminated environment block merging current env with extras.
// If extra_env is empty, returns empty (caller should pass NULL to use inherited env).
static std::string build_environment_block(const std::map<std::string, std::string>& extra_env) {
    if (extra_env.empty())
        return {};

    char* env_strings = GetEnvironmentStringsA();
    if (!env_strings)
        return {};

    // Parse current env into a map (case-insensitive keys on Windows)
    std::map<std::string, std::string> merged;
    const char* p = env_strings;
    while (*p) {
        std::string entry(p);
        auto eq = entry.find('=');
        if (eq != std::string::npos && eq > 0) {
            std::string key = entry.substr(0, eq);
            std::string val = entry.substr(eq + 1);
            merged[key] = val;
        }
        p += entry.size() + 1;
    }
    FreeEnvironmentStringsA(env_strings);

    // Merge extra vars. Special handling for PATH: prepend instead of replace.
    for (const auto& [key, val] : extra_env) {
        if ((key == "PATH" || key == "Path" || key == "path") && merged.count("PATH")) {
            merged["PATH"] = val + ";" + merged["PATH"];
        } else if ((key == "PATH" || key == "Path" || key == "path") && merged.count("Path")) {
            merged["Path"] = val + ";" + merged["Path"];
        } else {
            merged[key] = val;
        }
    }

    // Build double-null-terminated block
    std::string block;
    for (const auto& [key, val] : merged) {
        block += key + "=" + val;
        block.push_back('\0');
    }
    block.push_back('\0'); // final null
    return block;
}

// Non-blocking read from a pipe handle. Returns available data or empty.
static std::string read_pipe_nonblocking(HANDLE pipe) {
    if (pipe == INVALID_HANDLE_VALUE)
        return {};

    DWORD available = 0;
    if (!PeekNamedPipe(pipe, NULL, 0, NULL, &available, NULL) || available == 0) {
        return {};
    }

    std::string result;
    result.resize(available);
    DWORD bytes_read = 0;
    if (ReadFile(pipe, result.data(), available, &bytes_read, NULL) && bytes_read > 0) {
        result.resize(bytes_read);
        return result;
    }
    return {};
}

// Read all remaining data from a pipe (blocking, used after process exits).
static std::string read_pipe_blocking(HANDLE pipe) {
    if (pipe == INVALID_HANDLE_VALUE)
        return {};

    std::string result;
    char buffer[4096];
    DWORD bytes_read;
    while (ReadFile(pipe, buffer, sizeof(buffer), &bytes_read, NULL) && bytes_read > 0) {
        result.append(buffer, bytes_read);
    }
    return result;
}

// Windows implementation of Impl::drain_pipes
void Process::Impl::drain_pipes() {
    auto out = read_pipe_nonblocking(stdout_read);
    if (!out.empty())
        stdout_buf += out;

    auto err = read_pipe_nonblocking(stderr_read);
    if (!err.empty())
        stderr_buf += err;
}

static void close_handle_safe(HANDLE& h) {
    if (h != INVALID_HANDLE_VALUE) {
        CloseHandle(h);
        h = INVALID_HANDLE_VALUE;
    }
}

std::optional<Process> Process::launch(const ProcessOptions& opts) {
    Process proc;
    proc.impl_->exe = opts.exe_path;
    proc.impl_->timeout = opts.timeout;
    proc.impl_->start_time = std::chrono::steady_clock::now();

    // 1. Create stdout/stderr pipes
    SECURITY_ATTRIBUTES sa;
    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.bInheritHandle = TRUE;
    sa.lpSecurityDescriptor = NULL;

    HANDLE stdout_read = INVALID_HANDLE_VALUE, stdout_write = INVALID_HANDLE_VALUE;
    HANDLE stderr_read = INVALID_HANDLE_VALUE, stderr_write = INVALID_HANDLE_VALUE;

    if (!CreatePipe(&stdout_read, &stdout_write, &sa, 0)) {
        TML_LOG_DEBUG("test", "Process::launch: CreatePipe(stdout) failed: " << GetLastError());
        return std::nullopt;
    }
    // Read ends must NOT be inherited by child
    SetHandleInformation(stdout_read, HANDLE_FLAG_INHERIT, 0);

    if (!CreatePipe(&stderr_read, &stderr_write, &sa, 0)) {
        CloseHandle(stdout_read);
        CloseHandle(stdout_write);
        TML_LOG_DEBUG("test", "Process::launch: CreatePipe(stderr) failed: " << GetLastError());
        return std::nullopt;
    }
    SetHandleInformation(stderr_read, HANDLE_FLAG_INHERIT, 0);

    // 2. Build command line: "exe_path" arg1 arg2 ...
    std::string cmd = "\"" + opts.exe_path + "\"";
    for (const auto& arg : opts.args) {
        cmd += " ";
        // Quote arguments that contain spaces
        if (arg.find(' ') != std::string::npos) {
            cmd += "\"" + arg + "\"";
        } else {
            cmd += arg;
        }
    }
    std::vector<char> cmd_buf(cmd.begin(), cmd.end());
    cmd_buf.push_back('\0');

    // 3. Build environment block
    std::string env_block = build_environment_block(opts.env);
    LPVOID env_ptr = env_block.empty() ? NULL : (LPVOID)env_block.data();

    // 4. Create Job Object with KILL_ON_JOB_CLOSE (nextest pattern)
    HANDLE job = CreateJobObjectA(NULL, NULL);
    if (job) {
        JOBOBJECT_EXTENDED_LIMIT_INFORMATION jeli = {};
        jeli.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
        SetInformationJobObject(job, JobObjectExtendedLimitInformation, &jeli, sizeof(jeli));
    }

    // 5. Create process SUSPENDED so we can assign to job before it runs
    STARTUPINFOA si = {};
    si.cb = sizeof(STARTUPINFOA);
    si.hStdOutput = stdout_write;
    si.hStdError = stderr_write;
    si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    si.dwFlags = STARTF_USESTDHANDLES;

    PROCESS_INFORMATION pi = {};
    DWORD flags = CREATE_SUSPENDED | CREATE_NEW_PROCESS_GROUP;

    LPCSTR working_dir = opts.working_dir.empty() ? NULL : opts.working_dir.c_str();

    BOOL created = CreateProcessA(NULL, cmd_buf.data(), NULL, NULL, TRUE, flags, env_ptr,
                                  working_dir, &si, &pi);

    // Close write ends immediately (child has them via inheritance)
    CloseHandle(stdout_write);
    CloseHandle(stderr_write);

    if (!created) {
        DWORD err = GetLastError();
        CloseHandle(stdout_read);
        CloseHandle(stderr_read);
        if (job)
            CloseHandle(job);
        TML_LOG_DEBUG("test",
                      "Process::launch: CreateProcessA failed: " << err << " (cmd: " << cmd << ")");
        return std::nullopt;
    }

    // 6. Assign to job object BEFORE resuming (prevents child escape)
    if (job) {
        if (!AssignProcessToJobObject(job, pi.hProcess)) {
            // Job nesting may not be supported (Windows < 8). Continue without job.
            TML_LOG_DEBUG("test", "Process::launch: AssignProcessToJobObject failed: "
                                      << GetLastError() << " (continuing without job)");
            CloseHandle(job);
            job = INVALID_HANDLE_VALUE;
        }
    }

    // 7. Resume the process
    ResumeThread(pi.hThread);
    CloseHandle(pi.hThread);

    proc.impl_->process = pi.hProcess;
    proc.impl_->job = job ? job : INVALID_HANDLE_VALUE;
    proc.impl_->stdout_read = stdout_read;
    proc.impl_->stderr_read = stderr_read;

    return proc;
}

bool Process::is_done() {
    if (!impl_ || !valid() || impl_->done)
        return impl_ ? impl_->done : true;

    impl_->drain_pipes();

    DWORD result = WaitForSingleObject(impl_->process, 0);
    if (result == WAIT_OBJECT_0) {
        DWORD exit_code = 0;
        GetExitCodeProcess(impl_->process, &exit_code);
        impl_->cached_exit_code = static_cast<int>(exit_code);
        impl_->done = true;

        // Final drain after process exits
        impl_->drain_pipes();
        impl_->stdout_buf += read_pipe_blocking(impl_->stdout_read);
        impl_->stderr_buf += read_pipe_blocking(impl_->stderr_read);
    }
    return impl_->done;
}

ProcessResult Process::wait(std::chrono::milliseconds timeout) {
    ProcessResult result;
    if (!impl_ || !valid())
        return result;
    result.launched = true;

    auto deadline = std::chrono::steady_clock::now() + timeout;

    // Interleaved drain + wait loop to prevent pipe deadlock
    while (!impl_->done) {
        impl_->drain_pipes();

        auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(
            deadline - std::chrono::steady_clock::now());
        if (remaining.count() <= 0) {
            // Timeout expired
            result.timed_out = true;
            kill();
            result.exit_code = -1;
            result.stdout_output = std::move(impl_->stdout_buf);
            result.stderr_output = std::move(impl_->stderr_buf);
            result.duration_us = std::chrono::duration_cast<std::chrono::microseconds>(
                                     std::chrono::steady_clock::now() - impl_->start_time)
                                     .count();
            return result;
        }

        DWORD wait_ms = static_cast<DWORD>(std::min(remaining.count(), (long long)50));
        DWORD wait_result = WaitForSingleObject(impl_->process, wait_ms);
        if (wait_result == WAIT_OBJECT_0) {
            DWORD exit_code = 0;
            GetExitCodeProcess(impl_->process, &exit_code);
            impl_->cached_exit_code = static_cast<int>(exit_code);
            impl_->done = true;
        }
    }

    // Final drain
    impl_->drain_pipes();
    impl_->stdout_buf += read_pipe_blocking(impl_->stdout_read);
    impl_->stderr_buf += read_pipe_blocking(impl_->stderr_read);

    result.exit_code = impl_->cached_exit_code;
    result.stdout_output = std::move(impl_->stdout_buf);
    result.stderr_output = std::move(impl_->stderr_buf);
    result.duration_us = std::chrono::duration_cast<std::chrono::microseconds>(
                             std::chrono::steady_clock::now() - impl_->start_time)
                             .count();
    return result;
}

std::string Process::read_stdout() {
    if (!impl_)
        return {};
    impl_->drain_pipes();
    std::string out;
    std::swap(out, impl_->stdout_buf);
    return out;
}

std::string Process::read_stderr() {
    if (!impl_)
        return {};
    impl_->drain_pipes();
    std::string out;
    std::swap(out, impl_->stderr_buf);
    return out;
}

void Process::kill() {
    if (!impl_ || !valid())
        return;

    // Kill entire process tree via job object (nextest pattern)
    if (impl_->job != INVALID_HANDLE_VALUE) {
        TerminateJobObject(impl_->job, 1);
    } else {
        // Fallback: kill just the main process
        TerminateProcess(impl_->process, 1);
    }

    // Wait for process to actually terminate (up to 5 seconds)
    WaitForSingleObject(impl_->process, 5000);

    DWORD exit_code = 0;
    GetExitCodeProcess(impl_->process, &exit_code);
    impl_->cached_exit_code = static_cast<int>(exit_code);
    impl_->done = true;

    // Final pipe drain
    impl_->drain_pipes();
    impl_->stdout_buf += read_pipe_blocking(impl_->stdout_read);
    impl_->stderr_buf += read_pipe_blocking(impl_->stderr_read);

    // Clean up handles
    close_handle_safe(impl_->stdout_read);
    close_handle_safe(impl_->stderr_read);
    close_handle_safe(impl_->process);
    close_handle_safe(impl_->job);
}

#else // UNIX

// Non-blocking read from a file descriptor. Returns available data or empty.
static std::string read_fd_nonblocking(int fd) {
    if (fd < 0)
        return {};

    std::string result;
    char buffer[4096];
    while (true) {
        ssize_t n = read(fd, buffer, sizeof(buffer));
        if (n > 0) {
            result.append(buffer, n);
        } else if (n == 0) {
            break; // EOF
        } else {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
                break; // no data available
            break;     // error
        }
    }
    return result;
}

// Read all remaining data from fd (blocking, used after process exits).
static std::string read_fd_blocking(int fd) {
    if (fd < 0)
        return {};

    // Clear non-blocking flag
    int flags = fcntl(fd, F_GETFL);
    if (flags != -1) {
        fcntl(fd, F_SETFL, flags & ~O_NONBLOCK);
    }

    std::string result;
    char buffer[4096];
    ssize_t n;
    while ((n = read(fd, buffer, sizeof(buffer))) > 0) {
        result.append(buffer, n);
    }
    return result;
}

// Unix implementation of Impl::drain_pipes
void Process::Impl::drain_pipes() {
    if (stdout_fd < 0 && stderr_fd < 0)
        return;

    struct pollfd fds[2];
    int nfds = 0;

    if (stdout_fd >= 0) {
        fds[nfds].fd = stdout_fd;
        fds[nfds].events = POLLIN;
        fds[nfds].revents = 0;
        nfds++;
    }
    if (stderr_fd >= 0) {
        fds[nfds].fd = stderr_fd;
        fds[nfds].events = POLLIN;
        fds[nfds].revents = 0;
        nfds++;
    }

    if (nfds == 0)
        return;

    // Poll with 0 timeout (non-blocking check)
    int ready = poll(fds, nfds, 0);
    if (ready <= 0)
        return;

    for (int i = 0; i < nfds; i++) {
        if (fds[i].revents & (POLLIN | POLLHUP)) {
            auto data = read_fd_nonblocking(fds[i].fd);
            if (!data.empty()) {
                if (fds[i].fd == stdout_fd) {
                    stdout_buf += data;
                } else {
                    stderr_buf += data;
                }
            }
        }
    }
}

static void close_fd_safe(int& fd) {
    if (fd >= 0) {
        close(fd);
        fd = -1;
    }
}

std::optional<Process> Process::launch(const ProcessOptions& opts) {
    Process proc;
    proc.impl_->exe = opts.exe_path;
    proc.impl_->timeout = opts.timeout;
    proc.impl_->start_time = std::chrono::steady_clock::now();

    // 1. Create stdout and stderr pipes
    int stdout_pipe[2], stderr_pipe[2];
    if (pipe(stdout_pipe) != 0) {
        TML_LOG_DEBUG("test", "Process::launch: pipe(stdout) failed: " << strerror(errno));
        return std::nullopt;
    }
    if (pipe(stderr_pipe) != 0) {
        close(stdout_pipe[0]);
        close(stdout_pipe[1]);
        TML_LOG_DEBUG("test", "Process::launch: pipe(stderr) failed: " << strerror(errno));
        return std::nullopt;
    }

    // 2. Fork
    pid_t child = fork();
    if (child < 0) {
        close(stdout_pipe[0]);
        close(stdout_pipe[1]);
        close(stderr_pipe[0]);
        close(stderr_pipe[1]);
        TML_LOG_DEBUG("test", "Process::launch: fork() failed: " << strerror(errno));
        return std::nullopt;
    }

    if (child == 0) {
        // === CHILD PROCESS ===

        // Create new process group for descendant termination
        setpgid(0, 0);

        // Redirect stdout/stderr
        close(stdout_pipe[0]); // close read ends
        close(stderr_pipe[0]);
        dup2(stdout_pipe[1], STDOUT_FILENO);
        dup2(stderr_pipe[1], STDERR_FILENO);
        close(stdout_pipe[1]);
        close(stderr_pipe[1]);

        // Set additional environment variables
        for (const auto& [key, val] : opts.env) {
            setenv(key.c_str(), val.c_str(), 1);
        }

        // Change working directory if specified
        if (!opts.working_dir.empty()) {
            if (chdir(opts.working_dir.c_str()) != 0) {
                _exit(126);
            }
        }

        // Build argv
        std::vector<char*> argv;
        argv.push_back(const_cast<char*>(opts.exe_path.c_str()));
        for (const auto& arg : opts.args) {
            argv.push_back(const_cast<char*>(arg.c_str()));
        }
        argv.push_back(nullptr);

        execvp(opts.exe_path.c_str(), argv.data());
        _exit(127); // execvp failed
    }

    // === PARENT PROCESS ===

    // Close write ends (child owns them now)
    close(stdout_pipe[1]);
    close(stderr_pipe[1]);

    // Set read ends to non-blocking
    fcntl(stdout_pipe[0], F_SETFL, O_NONBLOCK);
    fcntl(stderr_pipe[0], F_SETFL, O_NONBLOCK);

    // Also call setpgid in parent to avoid race condition
    setpgid(child, child);

    proc.impl_->child_pid = child;
    proc.impl_->pgid = child;
    proc.impl_->stdout_fd = stdout_pipe[0];
    proc.impl_->stderr_fd = stderr_pipe[0];

    return proc;
}

bool Process::is_done() {
    if (!impl_ || impl_->child_pid <= 0 || impl_->done)
        return impl_ ? impl_->done : true;

    impl_->drain_pipes();

    if (!impl_->reaped) {
        int status = 0;
        pid_t ret = waitpid(impl_->child_pid, &status, WNOHANG);
        if (ret > 0) {
            impl_->reaped = true;
            impl_->wait_status = status;
            if (WIFEXITED(status)) {
                impl_->cached_exit_code = WEXITSTATUS(status);
            } else if (WIFSIGNALED(status)) {
                impl_->cached_exit_code = 128 + WTERMSIG(status);
            } else {
                impl_->cached_exit_code = -1;
            }
            impl_->done = true;

            // Final blocking drain
            impl_->stdout_buf += read_fd_blocking(impl_->stdout_fd);
            impl_->stderr_buf += read_fd_blocking(impl_->stderr_fd);
        }
    }
    return impl_->done;
}

ProcessResult Process::wait(std::chrono::milliseconds timeout) {
    ProcessResult result;
    if (!impl_ || impl_->child_pid <= 0)
        return result;
    result.launched = true;

    auto deadline = std::chrono::steady_clock::now() + timeout;

    while (!impl_->done) {
        // Poll pipes with short timeout
        struct pollfd fds[2];
        int nfds = 0;
        if (impl_->stdout_fd >= 0) {
            fds[nfds].fd = impl_->stdout_fd;
            fds[nfds].events = POLLIN;
            fds[nfds].revents = 0;
            nfds++;
        }
        if (impl_->stderr_fd >= 0) {
            fds[nfds].fd = impl_->stderr_fd;
            fds[nfds].events = POLLIN;
            fds[nfds].revents = 0;
            nfds++;
        }

        if (nfds > 0) {
            poll(fds, nfds, 50); // 50ms poll
            for (int i = 0; i < nfds; i++) {
                if (fds[i].revents & (POLLIN | POLLHUP)) {
                    auto data = read_fd_nonblocking(fds[i].fd);
                    if (!data.empty()) {
                        if (fds[i].fd == impl_->stdout_fd) {
                            impl_->stdout_buf += data;
                        } else {
                            impl_->stderr_buf += data;
                        }
                    }
                }
            }
        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }

        // Check if child exited
        if (!impl_->reaped) {
            int status = 0;
            pid_t ret = waitpid(impl_->child_pid, &status, WNOHANG);
            if (ret > 0) {
                impl_->reaped = true;
                impl_->wait_status = status;
                if (WIFEXITED(status)) {
                    impl_->cached_exit_code = WEXITSTATUS(status);
                } else if (WIFSIGNALED(status)) {
                    impl_->cached_exit_code = 128 + WTERMSIG(status);
                } else {
                    impl_->cached_exit_code = -1;
                }
                impl_->done = true;
            }
        }

        // Check timeout
        if (!impl_->done && std::chrono::steady_clock::now() >= deadline) {
            result.timed_out = true;
            kill();
            result.exit_code = -1;
            result.stdout_output = std::move(impl_->stdout_buf);
            result.stderr_output = std::move(impl_->stderr_buf);
            result.duration_us = std::chrono::duration_cast<std::chrono::microseconds>(
                                     std::chrono::steady_clock::now() - impl_->start_time)
                                     .count();
            return result;
        }
    }

    // Final blocking drain
    impl_->stdout_buf += read_fd_blocking(impl_->stdout_fd);
    impl_->stderr_buf += read_fd_blocking(impl_->stderr_fd);

    result.exit_code = impl_->cached_exit_code;
    result.stdout_output = std::move(impl_->stdout_buf);
    result.stderr_output = std::move(impl_->stderr_buf);
    result.duration_us = std::chrono::duration_cast<std::chrono::microseconds>(
                             std::chrono::steady_clock::now() - impl_->start_time)
                             .count();
    return result;
}

std::string Process::read_stdout() {
    if (!impl_)
        return {};
    impl_->drain_pipes();
    std::string out;
    std::swap(out, impl_->stdout_buf);
    return out;
}

std::string Process::read_stderr() {
    if (!impl_)
        return {};
    impl_->drain_pipes();
    std::string out;
    std::swap(out, impl_->stderr_buf);
    return out;
}

void Process::kill() {
    if (!impl_ || impl_->child_pid <= 0)
        return;

    if (!impl_->reaped) {
        // SIGTERM to entire process group (nextest pattern)
        ::kill(-impl_->pgid, SIGTERM);

        // Grace period: wait up to 2 seconds
        auto grace_deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
        while (std::chrono::steady_clock::now() < grace_deadline) {
            int status = 0;
            pid_t ret = waitpid(impl_->child_pid, &status, WNOHANG);
            if (ret > 0) {
                impl_->reaped = true;
                impl_->wait_status = status;
                if (WIFEXITED(status)) {
                    impl_->cached_exit_code = WEXITSTATUS(status);
                } else if (WIFSIGNALED(status)) {
                    impl_->cached_exit_code = 128 + WTERMSIG(status);
                }
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }

        if (!impl_->reaped) {
            // Force kill
            ::kill(-impl_->pgid, SIGKILL);
            int status = 0;
            waitpid(impl_->child_pid, &status, 0); // blocking reap
            impl_->reaped = true;
            impl_->wait_status = status;
            impl_->cached_exit_code = -1;
        }
    }

    impl_->done = true;

    // Drain remaining pipe data
    impl_->drain_pipes();
    if (impl_->stdout_fd >= 0) {
        impl_->stdout_buf += read_fd_blocking(impl_->stdout_fd);
    }
    if (impl_->stderr_fd >= 0) {
        impl_->stderr_buf += read_fd_blocking(impl_->stderr_fd);
    }

    close_fd_safe(impl_->stdout_fd);
    close_fd_safe(impl_->stderr_fd);
}

#endif // _WIN32

} // namespace tml::testing
