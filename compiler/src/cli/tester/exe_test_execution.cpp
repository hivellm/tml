TML_MODULE("test")

//! # EXE Test Subprocess Execution
//!
//! Runs test functions by invoking the compiled suite EXE as a subprocess.
//! Supports two modes:
//! - `--test-index=N` — Run a single test (legacy, 1 process per test)
//! - `--run-all` — Run ALL tests in one process (optimized, 1 process per suite)
//!
//! ## Platform Support
//!
//! - **Windows**: CreateProcess with redirected stdout/stderr pipes
//! - **Unix**: fork + execvp with pipe-based output capture

#include "exe_test_runner.hpp"
#include "log/log.hpp"

#include <chrono>
#include <cstring>
#include <filesystem>
#include <sstream>
#include <string>
#include <vector>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <fcntl.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace fs = std::filesystem;

namespace tml::cli {

// ============================================================================
// Parse TML_RESULT lines from --run-all stdout
// Format: TML_RESULT:<index>:<PASS|FAIL>:<exit_code>
// ============================================================================

static std::vector<SuiteSubprocessResult::TestOutcome>
parse_run_all_output(const std::string& stdout_output) {
    std::vector<SuiteSubprocessResult::TestOutcome> outcomes;
    std::istringstream stream(stdout_output);
    std::string line;

    while (std::getline(stream, line)) {
        // Strip trailing \r
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }

        // Look for TML_RESULT: prefix
        const std::string prefix = "TML_RESULT:";
        if (line.size() < prefix.size() || line.substr(0, prefix.size()) != prefix) {
            continue;
        }

        // Parse: TML_RESULT:<index>:<PASS|FAIL>:<exit_code>
        std::string rest = line.substr(prefix.size());

        // Parse index
        auto colon1 = rest.find(':');
        if (colon1 == std::string::npos)
            continue;
        int index = std::atoi(rest.substr(0, colon1).c_str());

        // Parse PASS/FAIL
        std::string after_idx = rest.substr(colon1 + 1);
        auto colon2 = after_idx.find(':');
        if (colon2 == std::string::npos)
            continue;
        std::string status = after_idx.substr(0, colon2);
        bool passed = (status == "PASS");

        // Parse exit_code
        int exit_code = std::atoi(after_idx.substr(colon2 + 1).c_str());

        SuiteSubprocessResult::TestOutcome outcome;
        outcome.test_index = index;
        outcome.passed = passed;
        outcome.exit_code = exit_code;
        outcomes.push_back(outcome);
    }

    return outcomes;
}

#ifdef _WIN32

// Read all data from a pipe handle into a string
static std::string read_pipe(HANDLE pipe) {
    std::string result;
    char buffer[4096];
    DWORD bytes_read;
    while (ReadFile(pipe, buffer, sizeof(buffer) - 1, &bytes_read, NULL) && bytes_read > 0) {
        buffer[bytes_read] = '\0';
        result.append(buffer, bytes_read);
    }
    return result;
}

// Build environment block with vcpkg DLL paths prepended to PATH.
// Returns empty string if no vcpkg paths found (use default env).
static std::string build_env_with_dll_paths() {
    // Find vcpkg bin directories (relative to cwd)
    std::vector<std::string> vcpkg_candidates = {
        "src/x64-windows/bin",
        "../src/x64-windows/bin",
        "../../src/x64-windows/bin",
    };

    std::string extra_paths;
    for (const auto& candidate : vcpkg_candidates) {
        if (fs::exists(candidate)) {
            fs::path abs_path = fs::absolute(candidate);
            if (!extra_paths.empty())
                extra_paths += ";";
            extra_paths += abs_path.string();

            // Also add debug/bin for debug DLLs (zlibd1.dll etc.)
            fs::path debug_bin = fs::path(candidate).parent_path() / "debug" / "bin";
            if (fs::exists(debug_bin)) {
                extra_paths += ";" + fs::absolute(debug_bin).string();
            }
            break;
        }
    }

    if (extra_paths.empty()) {
        return {}; // No vcpkg found, use default environment
    }

    // Get current environment block
    char* env_block = GetEnvironmentStringsA();
    if (!env_block) {
        return {};
    }

    // Build new environment block with modified PATH
    std::string new_env;
    bool path_found = false;
    const char* p = env_block;
    while (*p) {
        std::string entry(p);
        // Case-insensitive check for PATH=
        if (entry.size() > 5 && (entry[0] == 'P' || entry[0] == 'p') &&
            (entry[1] == 'A' || entry[1] == 'a') && (entry[2] == 'T' || entry[2] == 't') &&
            (entry[3] == 'H' || entry[3] == 'h') && entry[4] == '=') {
            // Prepend vcpkg paths to existing PATH
            std::string old_value = entry.substr(5);
            new_env += "PATH=" + extra_paths + ";" + old_value;
            new_env.push_back('\0');
            path_found = true;
        } else {
            new_env += entry;
            new_env.push_back('\0');
        }
        p += entry.size() + 1;
    }

    if (!path_found) {
        new_env += "PATH=" + extra_paths;
        new_env.push_back('\0');
    }

    // Double null terminator
    new_env.push_back('\0');

    FreeEnvironmentStringsA(env_block);
    return new_env;
}

// Cache the environment block (computed once, reused for all subprocesses)
static const std::string& get_cached_env_block() {
    static std::string cached = build_env_with_dll_paths();
    return cached;
}

// ============================================================================
// Async subprocess launch (non-blocking)
// ============================================================================

AsyncSubprocessHandle launch_subprocess_async(const std::string& exe_path, int expected_tests,
                                               int timeout_seconds, const std::string& suite_name,
                                               const TestOptions& opts) {
    AsyncSubprocessHandle handle;
    handle.exe_path = exe_path;
    handle.expected_tests = expected_tests;
    handle.suite_name = suite_name;
    handle.timeout_seconds = timeout_seconds;
    handle.opts = &opts;
    handle.start_time_us = std::chrono::duration_cast<std::chrono::microseconds>(
                              std::chrono::high_resolution_clock::now().time_since_epoch())
                              .count();

#ifdef _WIN32
    // Create pipes for stdout/stderr capture (same as sync version)
    SECURITY_ATTRIBUTES sa;
    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.bInheritHandle = TRUE;
    sa.lpSecurityDescriptor = NULL;

    HANDLE stdout_read = NULL, stdout_write = NULL;
    HANDLE stderr_read = NULL, stderr_write = NULL;

    if (!CreatePipe(&stdout_read, &stdout_write, &sa, 0) ||
        !CreatePipe(&stderr_read, &stderr_write, &sa, 0)) {
        CloseHandle(stdout_read);
        CloseHandle(stdout_write);
        CloseHandle(stderr_read);
        CloseHandle(stderr_write);
        return handle; // Failed to create pipes
    }

    // Build command line
    std::string cmd = exe_path + " --run-all";
    std::vector<char> cmd_buf(cmd.begin(), cmd.end());
    cmd_buf.push_back('\0');

    // Build environment block
    std::string env_block = get_cached_env_block();
    if (opts.coverage && !suite_name.empty()) {
        fs::path cov_dir = fs::path("build") / "coverage";
        fs::create_directories(cov_dir);
        std::string cov_file_path = (cov_dir / ("cov_" + suite_name + ".txt")).string();
        env_block += "TML_COVERAGE_FILE=" + cov_file_path + "\0";
    }

    LPVOID env_ptr = env_block.empty() ? NULL : (LPVOID)env_block.data();

    STARTUPINFOA si = {};
    si.cb = sizeof(STARTUPINFOA);
    si.hStdOutput = stdout_write;
    si.hStdError = stderr_write;
    si.dwFlags = STARTF_USESTDHANDLES;

    PROCESS_INFORMATION pi = {};
    BOOL created = CreateProcessA(NULL, cmd_buf.data(), NULL, NULL, TRUE, 0, env_ptr, NULL, &si, &pi);

    CloseHandle(stdout_write);
    CloseHandle(stderr_write);

    if (!created) {
        CloseHandle(stdout_read);
        CloseHandle(stderr_read);
        return handle; // Failed to create process
    }

    // Store process handle and pipes for later
    // We'll store these in a temp structure and pass via handle
    // For now, just store the process handle
    handle.process_handle = (void*)pi.hProcess;
    CloseHandle(pi.hThread); // Don't need thread handle

#else // Unix
    // Similar for Unix with fork/pipe
    // For MVP, not implementing Unix async version yet
#endif

    return handle;
}

// Wait for async subprocess to complete
SuiteSubprocessResult wait_for_subprocess(const AsyncSubprocessHandle& handle) {
    SuiteSubprocessResult result;
    result.process_ok = false;

#ifdef _WIN32
    if (!handle.process_handle) {
        result.stderr_output = "Invalid process handle";
        return result;
    }

    DWORD timeout_ms = (handle.timeout_seconds > 0) ? (handle.timeout_seconds * 1000) : INFINITE;
    DWORD wait_result = WaitForSingleObject((HANDLE)handle.process_handle, timeout_ms);

    if (wait_result == WAIT_TIMEOUT) {
        result.timed_out = true;
        TerminateProcess((HANDLE)handle.process_handle, (UINT)-1);
        CloseHandle((HANDLE)handle.process_handle);
        return result;
    }

    if (wait_result != WAIT_OBJECT_0) {
        result.stderr_output = "WaitForSingleObject failed";
        CloseHandle((HANDLE)handle.process_handle);
        return result;
    }

    DWORD exit_code = 0;
    if (!GetExitCodeProcess((HANDLE)handle.process_handle, &exit_code)) {
        result.stderr_output = "GetExitCodeProcess failed";
        CloseHandle((HANDLE)handle.process_handle);
        return result;
    }

    CloseHandle((HANDLE)handle.process_handle);

    // TODO: Collect stdout/stderr from pipes and parse TML_RESULT lines
    // For MVP, just return success if exit_code was 0
    result.process_ok = (exit_code == 0);

#else // Unix
    // Similar for Unix
#endif

    auto elapsed_us = std::chrono::duration_cast<std::chrono::microseconds>(
                          std::chrono::high_resolution_clock::now().time_since_epoch())
                          .count() -
                      handle.start_time_us;
    result.total_duration_us = elapsed_us;

    return result;
}

// Check if a subprocess has completed (non-blocking)
bool subprocess_is_done(const AsyncSubprocessHandle& handle) {
#ifdef _WIN32
    if (!handle.process_handle) {
        return true; // Invalid handle means "done"
    }

    DWORD wait_result = WaitForSingleObject((HANDLE)handle.process_handle, 0); // 0 = non-blocking
    return (wait_result == WAIT_OBJECT_0); // WAIT_OBJECT_0 means process exited
#else
    // Unix: Similar with waitpid(..., WNOHANG)
    return false; // TODO: implement for Unix
#endif
}

// Helper: launch a subprocess with given arguments, capture stdout/stderr
struct RawSubprocessResult {
    bool launched = false;
    bool timed_out = false;
    int exit_code = 0;
    std::string stdout_output;
    std::string stderr_output;
    int64_t duration_us = 0;
};

static RawSubprocessResult launch_subprocess(const std::string& exe_path,
                                             const std::string& args_str, int timeout_seconds,
                                             const std::string& suite_name = "",
                                             const TestOptions& opts = {}) {
    using Clock = std::chrono::high_resolution_clock;
    auto start = Clock::now();

    RawSubprocessResult result;

    // Create pipes for stdout and stderr
    SECURITY_ATTRIBUTES sa;
    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.bInheritHandle = TRUE;
    sa.lpSecurityDescriptor = NULL;

    HANDLE stdout_read = NULL, stdout_write = NULL;
    HANDLE stderr_read = NULL, stderr_write = NULL;

    if (!CreatePipe(&stdout_read, &stdout_write, &sa, 0)) {
        result.stderr_output = "Failed to create stdout pipe";
        return result;
    }
    SetHandleInformation(stdout_read, HANDLE_FLAG_INHERIT, 0);

    if (!CreatePipe(&stderr_read, &stderr_write, &sa, 0)) {
        CloseHandle(stdout_read);
        CloseHandle(stdout_write);
        result.stderr_output = "Failed to create stderr pipe";
        return result;
    }
    SetHandleInformation(stderr_read, HANDLE_FLAG_INHERIT, 0);

    // Build command line
    std::string cmd = "\"" + exe_path + "\" " + args_str;

    // Create process
    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    si.hStdOutput = stdout_write;
    si.hStdError = stderr_write;
    si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    si.dwFlags |= STARTF_USESTDHANDLES;

    ZeroMemory(&pi, sizeof(pi));

    std::vector<char> cmd_buf(cmd.begin(), cmd.end());
    cmd_buf.push_back('\0');

    // Get base environment block (with DLL paths)
    std::string env_block = get_cached_env_block();

    // Add TML_COVERAGE_FILE env var if running with coverage and suite_name is provided
    if (opts.coverage && !suite_name.empty()) {
        fs::path cov_dir = fs::path("build") / "coverage";
        fs::create_directories(cov_dir);
        std::string cov_file_path =
            (cov_dir / ("cov_" + suite_name + ".txt")).string();
        env_block += "TML_COVERAGE_FILE=" + cov_file_path + "\0";
    }

    LPVOID env_ptr = env_block.empty() ? NULL : (LPVOID)env_block.data();

    BOOL created =
        CreateProcessA(NULL, cmd_buf.data(), NULL, NULL, TRUE, 0, env_ptr, NULL, &si, &pi);

    CloseHandle(stdout_write);
    CloseHandle(stderr_write);

    if (!created) {
        CloseHandle(stdout_read);
        CloseHandle(stderr_read);
        result.stderr_output = "Failed to create process: error " + std::to_string(GetLastError());
        return result;
    }

    result.launched = true;

    DWORD timeout_ms = (timeout_seconds > 0) ? (timeout_seconds * 1000) : INFINITE;
    DWORD wait_result = WaitForSingleObject(pi.hProcess, timeout_ms);

    if (wait_result == WAIT_TIMEOUT) {
        TerminateProcess(pi.hProcess, 1);
        WaitForSingleObject(pi.hProcess, 5000);
        result.timed_out = true;
        result.exit_code = -1;
        result.stderr_output = "Suite timed out after " + std::to_string(timeout_seconds) + "s";
    } else {
        DWORD exit_code = 0;
        GetExitCodeProcess(pi.hProcess, &exit_code);
        result.exit_code = static_cast<int>(exit_code);
    }

    result.stdout_output = read_pipe(stdout_read);
    result.stderr_output += read_pipe(stderr_read);

    CloseHandle(stdout_read);
    CloseHandle(stderr_read);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    auto end = Clock::now();
    result.duration_us = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();

    return result;
}

SubprocessTestResult run_test_subprocess(const std::string& exe_path, int test_index,
                                         int timeout_seconds, const std::string& test_name) {
    auto raw =
        launch_subprocess(exe_path, "--test-index=" + std::to_string(test_index), timeout_seconds);

    SubprocessTestResult result;
    result.success = raw.launched && !raw.timed_out && raw.exit_code == 0;
    result.exit_code = raw.exit_code;
    result.stdout_output = std::move(raw.stdout_output);
    result.stderr_output = std::move(raw.stderr_output);
    result.duration_us = raw.duration_us;
    result.timed_out = raw.timed_out;

    if (raw.timed_out && !test_name.empty()) {
        result.stderr_output =
            "Test timed out after " + std::to_string(timeout_seconds) + "s (" + test_name + ")";
    }

    return result;
}

SuiteSubprocessResult run_suite_all_subprocess(const std::string& exe_path, int expected_tests,
                                               int timeout_seconds, const std::string& suite_name,
                                               const TestOptions& opts) {
    auto raw = launch_subprocess(exe_path, "--run-all", timeout_seconds, suite_name, opts);

    SuiteSubprocessResult result;
    result.process_ok = raw.launched && !raw.timed_out;
    result.timed_out = raw.timed_out;
    result.stderr_output = std::move(raw.stderr_output);
    result.total_duration_us = raw.duration_us;

    if (result.process_ok) {
        result.outcomes = parse_run_all_output(raw.stdout_output);

        // If the process crashed mid-suite, some tests may not have results.
        // Mark missing tests as failed.
        if (static_cast<int>(result.outcomes.size()) < expected_tests) {
            std::vector<bool> seen(expected_tests, false);
            for (const auto& o : result.outcomes) {
                if (o.test_index >= 0 && o.test_index < expected_tests) {
                    seen[o.test_index] = true;
                }
            }
            for (int i = 0; i < expected_tests; ++i) {
                if (!seen[i]) {
                    SuiteSubprocessResult::TestOutcome missing;
                    missing.test_index = i;
                    missing.passed = false;
                    missing.exit_code = -1;
                    result.outcomes.push_back(missing);
                }
            }
        }
    }

    return result;
}

#else // Unix

static RawSubprocessResult launch_subprocess_unix(const std::string& exe_path,
                                                  const std::vector<std::string>& args,
                                                  int timeout_seconds) {
    using Clock = std::chrono::high_resolution_clock;
    auto start = Clock::now();

    RawSubprocessResult result;

    int stdout_pipe[2], stderr_pipe[2];
    if (pipe(stdout_pipe) != 0 || pipe(stderr_pipe) != 0) {
        result.stderr_output = "Failed to create pipes";
        return result;
    }

    pid_t pid = fork();
    if (pid < 0) {
        result.stderr_output = "Failed to fork";
        close(stdout_pipe[0]);
        close(stdout_pipe[1]);
        close(stderr_pipe[0]);
        close(stderr_pipe[1]);
        return result;
    }

    if (pid == 0) {
        // Child process
        close(stdout_pipe[0]);
        close(stderr_pipe[0]);

        dup2(stdout_pipe[1], STDOUT_FILENO);
        dup2(stderr_pipe[1], STDERR_FILENO);
        close(stdout_pipe[1]);
        close(stderr_pipe[1]);

        std::vector<char*> c_args;
        c_args.push_back(const_cast<char*>(exe_path.c_str()));
        for (const auto& a : args) {
            c_args.push_back(const_cast<char*>(a.c_str()));
        }
        c_args.push_back(nullptr);

        execvp(exe_path.c_str(), c_args.data());
        _exit(127);
    }

    result.launched = true;

    close(stdout_pipe[1]);
    close(stderr_pipe[1]);

    fcntl(stdout_pipe[0], F_SETFL, O_NONBLOCK);
    fcntl(stderr_pipe[0], F_SETFL, O_NONBLOCK);

    auto deadline = Clock::now() + std::chrono::seconds(timeout_seconds > 0 ? timeout_seconds : 60);

    int status = 0;
    bool finished = false;

    while (Clock::now() < deadline) {
        pid_t ret = waitpid(pid, &status, WNOHANG);
        if (ret > 0) {
            finished = true;
            break;
        }
        usleep(1000);
    }

    if (!finished) {
        kill(pid, SIGKILL);
        waitpid(pid, &status, 0);
        result.timed_out = true;
        result.exit_code = -1;
        result.stderr_output = "Suite timed out after " + std::to_string(timeout_seconds) + "s";
    } else {
        if (WIFEXITED(status)) {
            result.exit_code = WEXITSTATUS(status);
        } else {
            result.exit_code = -1;
        }
    }

    auto read_all = [](int fd) -> std::string {
        std::string out;
        char buf[4096];
        fcntl(fd, F_SETFL, 0);
        ssize_t n;
        while ((n = read(fd, buf, sizeof(buf))) > 0) {
            out.append(buf, n);
        }
        return out;
    };

    result.stdout_output = read_all(stdout_pipe[0]);
    result.stderr_output += read_all(stderr_pipe[0]);

    close(stdout_pipe[0]);
    close(stderr_pipe[0]);

    auto end = Clock::now();
    result.duration_us = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();

    return result;
}

SubprocessTestResult run_test_subprocess(const std::string& exe_path, int test_index,
                                         int timeout_seconds, const std::string& test_name) {
    std::vector<std::string> args = {"--test-index=" + std::to_string(test_index)};
    auto raw = launch_subprocess_unix(exe_path, args, timeout_seconds);

    SubprocessTestResult result;
    result.success = raw.launched && !raw.timed_out && raw.exit_code == 0;
    result.exit_code = raw.exit_code;
    result.stdout_output = std::move(raw.stdout_output);
    result.stderr_output = std::move(raw.stderr_output);
    result.duration_us = raw.duration_us;
    result.timed_out = raw.timed_out;

    if (raw.timed_out && !test_name.empty()) {
        result.stderr_output =
            "Test timed out after " + std::to_string(timeout_seconds) + "s (" + test_name + ")";
    }

    return result;
}

SuiteSubprocessResult run_suite_all_subprocess(const std::string& exe_path, int expected_tests,
                                               int timeout_seconds) {
    std::vector<std::string> args = {"--run-all"};
    auto raw = launch_subprocess_unix(exe_path, args, timeout_seconds);

    SuiteSubprocessResult result;
    result.process_ok = raw.launched && !raw.timed_out;
    result.timed_out = raw.timed_out;
    result.stderr_output = std::move(raw.stderr_output);
    result.total_duration_us = raw.duration_us;

    if (result.process_ok) {
        result.outcomes = parse_run_all_output(raw.stdout_output);

        if (static_cast<int>(result.outcomes.size()) < expected_tests) {
            std::vector<bool> seen(expected_tests, false);
            for (const auto& o : result.outcomes) {
                if (o.test_index >= 0 && o.test_index < expected_tests) {
                    seen[o.test_index] = true;
                }
            }
            for (int i = 0; i < expected_tests; ++i) {
                if (!seen[i]) {
                    SuiteSubprocessResult::TestOutcome missing;
                    missing.test_index = i;
                    missing.passed = false;
                    missing.exit_code = -1;
                    result.outcomes.push_back(missing);
                }
            }
        }
    }

    return result;
}

#endif // _WIN32

} // namespace tml::cli
