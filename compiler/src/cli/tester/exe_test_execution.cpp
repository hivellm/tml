//! # EXE Test Subprocess Execution
//!
//! Runs test functions by invoking the compiled suite EXE as a subprocess
//! with --test-index=N. Captures stdout/stderr via platform-specific pipes
//! and enforces timeouts.
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

    // Also add the exe's own directory
    // (handled by Windows automatically, but explicit is safer)

    if (extra_paths.empty()) {
        return {}; // No vcpkg found, use default environment
    }

    // Get current environment block
    // Format: VAR1=VALUE1\0VAR2=VALUE2\0...\0\0
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

SubprocessTestResult run_test_subprocess(const std::string& exe_path, int test_index,
                                         int timeout_seconds, const std::string& test_name) {
    using Clock = std::chrono::high_resolution_clock;
    auto start = Clock::now();

    SubprocessTestResult result;

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
    std::string cmd = "\"" + exe_path + "\" --test-index=" + std::to_string(test_index);

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

    // Need a mutable copy for CreateProcessA
    std::vector<char> cmd_buf(cmd.begin(), cmd.end());
    cmd_buf.push_back('\0');

    // Use cached environment block with vcpkg DLL paths in PATH
    const std::string& env_block = get_cached_env_block();
    LPVOID env_ptr = env_block.empty() ? NULL : (LPVOID)env_block.data();

    BOOL created = CreateProcessA(NULL,           // lpApplicationName
                                  cmd_buf.data(), // lpCommandLine
                                  NULL,           // lpProcessAttributes
                                  NULL,           // lpThreadAttributes
                                  TRUE,           // bInheritHandles
                                  0,              // dwCreationFlags
                                  env_ptr,        // lpEnvironment (with vcpkg PATH)
                                  NULL,           // lpCurrentDirectory
                                  &si,            // lpStartupInfo
                                  &pi             // lpProcessInformation
    );

    // Close write ends of pipes (parent doesn't need them)
    CloseHandle(stdout_write);
    CloseHandle(stderr_write);

    if (!created) {
        CloseHandle(stdout_read);
        CloseHandle(stderr_read);
        result.stderr_output = "Failed to create process: error " + std::to_string(GetLastError());
        return result;
    }

    // Wait for process with timeout
    DWORD timeout_ms = (timeout_seconds > 0) ? (timeout_seconds * 1000) : INFINITE;
    DWORD wait_result = WaitForSingleObject(pi.hProcess, timeout_ms);

    if (wait_result == WAIT_TIMEOUT) {
        // Kill the process
        TerminateProcess(pi.hProcess, 1);
        WaitForSingleObject(pi.hProcess, 5000); // Wait up to 5s for termination
        result.timed_out = true;
        result.exit_code = -1;
        result.stderr_output = "Test timed out after " + std::to_string(timeout_seconds) + "s" +
                               (test_name.empty() ? "" : " (" + test_name + ")");
    } else {
        // Get exit code
        DWORD exit_code = 0;
        GetExitCodeProcess(pi.hProcess, &exit_code);
        result.exit_code = static_cast<int>(exit_code);
        result.success = (exit_code == 0);
    }

    // Read captured output
    result.stdout_output = read_pipe(stdout_read);
    result.stderr_output += read_pipe(stderr_read);

    // Cleanup
    CloseHandle(stdout_read);
    CloseHandle(stderr_read);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    auto end = Clock::now();
    result.duration_us = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();

    return result;
}

#else // Unix

SubprocessTestResult run_test_subprocess(const std::string& exe_path, int test_index,
                                         int timeout_seconds, const std::string& test_name) {
    using Clock = std::chrono::high_resolution_clock;
    auto start = Clock::now();

    SubprocessTestResult result;

    // Create pipes for stdout and stderr
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
        close(stdout_pipe[0]); // Close read ends
        close(stderr_pipe[0]);

        dup2(stdout_pipe[1], STDOUT_FILENO);
        dup2(stderr_pipe[1], STDERR_FILENO);
        close(stdout_pipe[1]);
        close(stderr_pipe[1]);

        std::string index_arg = "--test-index=" + std::to_string(test_index);
        char* args[] = {const_cast<char*>(exe_path.c_str()), const_cast<char*>(index_arg.c_str()),
                        nullptr};
        execvp(exe_path.c_str(), args);
        _exit(127); // exec failed
    }

    // Parent process
    close(stdout_pipe[1]); // Close write ends
    close(stderr_pipe[1]);

    // Set pipes to non-blocking for timeout support
    fcntl(stdout_pipe[0], F_SETFL, O_NONBLOCK);
    fcntl(stderr_pipe[0], F_SETFL, O_NONBLOCK);

    // Wait with timeout
    auto deadline = Clock::now() + std::chrono::seconds(timeout_seconds > 0 ? timeout_seconds : 60);

    int status = 0;
    bool finished = false;

    while (Clock::now() < deadline) {
        pid_t ret = waitpid(pid, &status, WNOHANG);
        if (ret > 0) {
            finished = true;
            break;
        }
        // Sleep briefly between checks
        usleep(1000); // 1ms
    }

    if (!finished) {
        kill(pid, SIGKILL);
        waitpid(pid, &status, 0);
        result.timed_out = true;
        result.exit_code = -1;
        result.stderr_output = "Test timed out after " + std::to_string(timeout_seconds) + "s" +
                               (test_name.empty() ? "" : " (" + test_name + ")");
    } else {
        if (WIFEXITED(status)) {
            result.exit_code = WEXITSTATUS(status);
        } else {
            result.exit_code = -1;
        }
        result.success = (result.exit_code == 0);
    }

    // Read all output from pipes
    auto read_all = [](int fd) -> std::string {
        std::string out;
        char buf[4096];
        // Restore blocking mode for final read
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

#endif // _WIN32

} // namespace tml::cli
