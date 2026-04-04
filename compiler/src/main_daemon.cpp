TML_MODULE("daemon")

//! # TML MCP Daemon — stdio proxy + subprocess supervisor
//!
//! Claude Code launches this via stdio (normal .mcp.json command).
//! The daemon spawns tml_mcp.exe as a subprocess and proxies:
//!   stdin (from Claude) → subprocess stdin
//!   subprocess stdout → stdout (to Claude)
//!
//! If tml_mcp.exe crashes, the daemon catches the error, returns a
//! JSON-RPC error response, and restarts the subprocess.
//!
//! Benefits:
//! - tml_mcp.exe can be rebuilt while daemon runs
//! - Crashes are recovered automatically
//! - Claude Code never sees "Connection closed"

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <mutex>
#include <string>
#include <thread>

#ifdef _WIN32
#include <windows.h>
#else
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace fs = std::filesystem;

// ============================================================================
// MCP Subprocess
// ============================================================================

class McpProcess {
public:
    bool start(const std::string& exe) {
        exe_ = exe;
#ifdef _WIN32
        SECURITY_ATTRIBUTES sa{};
        sa.nLength = sizeof(sa);
        sa.bInheritHandle = TRUE;

        if (!CreatePipe(&stdout_rd_, &stdout_wr_, &sa, 0))
            return false;
        if (!CreatePipe(&stdin_rd_, &stdin_wr_, &sa, 0))
            return false;
        if (!CreatePipe(&stderr_rd_, &stderr_wr_, &sa, 0))
            return false;

        SetHandleInformation(stdout_rd_, HANDLE_FLAG_INHERIT, 0);
        SetHandleInformation(stdin_wr_, HANDLE_FLAG_INHERIT, 0);
        SetHandleInformation(stderr_rd_, HANDLE_FLAG_INHERIT, 0);

        STARTUPINFOA si{};
        si.cb = sizeof(si);
        si.dwFlags = STARTF_USESTDHANDLES;
        si.hStdInput = stdin_rd_;
        si.hStdOutput = stdout_wr_;
        si.hStdError = stderr_wr_;

        PROCESS_INFORMATION pi{};
        std::string cmd = exe;

        if (!CreateProcessA(nullptr, cmd.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW, nullptr,
                            nullptr, &si, &pi)) {
            std::fprintf(stderr, "[daemon] Failed to start: %s (err %lu)\n", exe.c_str(),
                         GetLastError());
            return false;
        }

        proc_ = pi.hProcess;
        CloseHandle(pi.hThread);
        CloseHandle(stdout_wr_);
        stdout_wr_ = INVALID_HANDLE_VALUE;
        CloseHandle(stdin_rd_);
        stdin_rd_ = INVALID_HANDLE_VALUE;
        CloseHandle(stderr_wr_);
        stderr_wr_ = INVALID_HANDLE_VALUE;
#else
        int in_pipe[2], out_pipe[2];
        if (pipe(in_pipe) < 0 || pipe(out_pipe) < 0)
            return false;
        pid_t pid = fork();
        if (pid < 0)
            return false;
        if (pid == 0) {
            close(in_pipe[1]);
            close(out_pipe[0]);
            dup2(in_pipe[0], STDIN_FILENO);
            dup2(out_pipe[1], STDOUT_FILENO);
            close(in_pipe[0]);
            close(out_pipe[1]);
            execl(exe.c_str(), exe.c_str(), nullptr);
            _exit(1);
        }
        close(in_pipe[0]);
        close(out_pipe[1]);
        stdin_wr_fd_ = in_pipe[1];
        stdout_rd_fd_ = out_pipe[0];
        pid_ = pid;
#endif
        alive_ = true;
        std::fprintf(stderr, "[daemon] MCP started: %s\n", exe.c_str());
        return true;
    }

    void stop() {
        if (!alive_)
            return;
#ifdef _WIN32
        if (stdin_wr_ != INVALID_HANDLE_VALUE) {
            CloseHandle(stdin_wr_);
            stdin_wr_ = INVALID_HANDLE_VALUE;
        }
        if (stdout_rd_ != INVALID_HANDLE_VALUE) {
            CloseHandle(stdout_rd_);
            stdout_rd_ = INVALID_HANDLE_VALUE;
        }
        if (stderr_rd_ != INVALID_HANDLE_VALUE) {
            CloseHandle(stderr_rd_);
            stderr_rd_ = INVALID_HANDLE_VALUE;
        }
        if (proc_ != INVALID_HANDLE_VALUE) {
            TerminateProcess(proc_, 0);
            WaitForSingleObject(proc_, 2000);
            CloseHandle(proc_);
            proc_ = INVALID_HANDLE_VALUE;
        }
#else
        if (stdin_wr_fd_ >= 0) {
            close(stdin_wr_fd_);
            stdin_wr_fd_ = -1;
        }
        if (stdout_rd_fd_ >= 0) {
            close(stdout_rd_fd_);
            stdout_rd_fd_ = -1;
        }
        if (pid_ > 0) {
            kill(pid_, SIGTERM);
            waitpid(pid_, nullptr, WNOHANG);
            pid_ = 0;
        }
#endif
        alive_ = false;
    }

    // Send JSON line, read JSON line response
    std::string send(const std::string& json_line) {
        if (!alive_)
            return "";

        std::string line = json_line + "\n";
#ifdef _WIN32
        DWORD written;
        if (!WriteFile(stdin_wr_, line.c_str(), (DWORD)line.size(), &written, nullptr))
            return "";

        std::string resp;
        char buf[4096];
        for (int wait = 0; wait < 6000; wait++) { // 60s max
            DWORD avail = 0;
            PeekNamedPipe(stdout_rd_, nullptr, 0, nullptr, &avail, nullptr);
            if (avail > 0) {
                DWORD to_read = (avail < sizeof(buf) - 1) ? avail : (DWORD)(sizeof(buf) - 1);
                DWORD rd;
                if (ReadFile(stdout_rd_, buf, to_read, &rd, nullptr) && rd > 0) {
                    buf[rd] = '\0';
                    resp.append(buf, rd);
                    if (resp.find('\n') != std::string::npos)
                        break;
                }
            } else {
                // Check if process died
                DWORD ec;
                if (GetExitCodeProcess(proc_, &ec) && ec != STILL_ACTIVE) {
                    std::fprintf(stderr, "[daemon] MCP crashed (exit %lu)\n", ec);
                    alive_ = false;
                    return "";
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        }
        while (!resp.empty() && (resp.back() == '\n' || resp.back() == '\r'))
            resp.pop_back();
        return resp;
#else
        if (write(stdin_wr_fd_, line.c_str(), line.size()) < 0)
            return "";
        std::string resp;
        char buf[4096];
        while (true) {
            ssize_t r = read(stdout_rd_fd_, buf, sizeof(buf) - 1);
            if (r <= 0) {
                alive_ = false;
                return "";
            }
            buf[r] = '\0';
            resp.append(buf, r);
            if (resp.find('\n') != std::string::npos)
                break;
        }
        while (!resp.empty() && (resp.back() == '\n' || resp.back() == '\r'))
            resp.pop_back();
        return resp;
#endif
    }

    // Send without waiting for response (for notifications)
    void fire(const std::string& json_line) {
        if (!alive_)
            return;
        std::string line = json_line + "\n";
#ifdef _WIN32
        DWORD written;
        WriteFile(stdin_wr_, line.c_str(), (DWORD)line.size(), &written, nullptr);
#else
        (void)write(stdin_wr_fd_, line.c_str(), line.size());
#endif
    }

    // Drain any stderr output from subprocess (non-blocking)
    std::string drain_stderr() {
        std::string result;
#ifdef _WIN32
        if (stderr_rd_ == INVALID_HANDLE_VALUE)
            return result;
        char buf[4096];
        DWORD avail = 0;
        while (PeekNamedPipe(stderr_rd_, nullptr, 0, nullptr, &avail, nullptr) && avail > 0) {
            DWORD to_read = (avail < sizeof(buf) - 1) ? avail : (DWORD)(sizeof(buf) - 1);
            DWORD rd;
            if (ReadFile(stderr_rd_, buf, to_read, &rd, nullptr) && rd > 0) {
                buf[rd] = '\0';
                result.append(buf, rd);
            } else {
                break;
            }
            avail = 0;
        }
#else
        if (stderr_rd_fd_ < 0)
            return result;
        char buf[4096];
        // Non-blocking read
        int flags = fcntl(stderr_rd_fd_, F_GETFL);
        fcntl(stderr_rd_fd_, F_SETFL, flags | O_NONBLOCK);
        ssize_t r;
        while ((r = read(stderr_rd_fd_, buf, sizeof(buf) - 1)) > 0) {
            buf[r] = '\0';
            result.append(buf, r);
        }
        fcntl(stderr_rd_fd_, F_SETFL, flags);
#endif
        return result;
    }

    bool is_alive() const {
        return alive_;
    }

    bool restart() {
        stop();
        return start(exe_);
    }

    ~McpProcess() {
        stop();
    }

private:
    std::string exe_;
    std::atomic<bool> alive_{false};
#ifdef _WIN32
    HANDLE proc_ = INVALID_HANDLE_VALUE;
    HANDLE stdin_rd_ = INVALID_HANDLE_VALUE;
    HANDLE stdin_wr_ = INVALID_HANDLE_VALUE;
    HANDLE stdout_rd_ = INVALID_HANDLE_VALUE;
    HANDLE stdout_wr_ = INVALID_HANDLE_VALUE;
    HANDLE stderr_rd_ = INVALID_HANDLE_VALUE;
    HANDLE stderr_wr_ = INVALID_HANDLE_VALUE;
#else
    pid_t pid_ = 0;
    int stdin_wr_fd_ = -1;
    int stdout_rd_fd_ = -1;
    int stderr_rd_fd_ = -1;
#endif
};

// ============================================================================
// Main — stdio proxy
// ============================================================================

int main(int /*argc*/, char* /*argv*/[]) {
    // Find tml_mcp.exe next to daemon
    std::string mcp_exe;
#ifdef _WIN32
    char self_buf[MAX_PATH];
    DWORD len = GetModuleFileNameA(nullptr, self_buf, MAX_PATH);
    if (len > 0) {
        fs::path self = fs::path(std::string(self_buf, len));
        mcp_exe = (self.parent_path() / "tml_mcp.exe").string();
    }
#else
    {
        std::error_code ec;
        auto self = fs::read_symlink("/proc/self/exe", ec);
        if (!ec)
            mcp_exe = (self.parent_path() / "tml_mcp").string();
    }
#endif

    if (!fs::exists(mcp_exe)) {
        std::fprintf(stderr, "[daemon] tml_mcp.exe not found: %s\n", mcp_exe.c_str());
        return 1;
    }

    std::fprintf(stderr, "[daemon] Starting MCP supervisor\n");

    McpProcess mcp;
    if (!mcp.start(mcp_exe)) {
        std::fprintf(stderr, "[daemon] Failed to start MCP subprocess\n");
        return 1;
    }

    // Main loop: read from stdin, proxy to subprocess, write response to stdout
    std::string line;
    while (std::getline(std::cin, line)) {
        if (line.empty())
            continue;

        // Auto-restart if crashed
        if (!mcp.is_alive()) {
            std::fprintf(stderr, "[daemon] MCP crashed, restarting...\n");
            if (!mcp.restart()) {
                // Can't restart — send error response
                std::cout
                    << R"({"jsonrpc":"2.0","error":{"code":-32000,"message":"MCP subprocess failed to restart"},"id":null})"
                    << "\n"
                    << std::flush;
                continue;
            }
            // Re-initialize the restarted subprocess
            std::string init =
                R"({"jsonrpc":"2.0","method":"initialize","id":0,"params":{"protocolVersion":"2024-11-05","capabilities":{},"clientInfo":{"name":"daemon","version":"1.0"}}})";
            auto init_resp = mcp.send(init);
            (void)init_resp;
            mcp.send(R"({"jsonrpc":"2.0","method":"notifications/initialized","params":{}})");
            std::fprintf(stderr, "[daemon] MCP restarted and re-initialized\n");
        }

        // Check if this is a notification (no "id" field) — fire and forget
        bool is_notification = (line.find("\"id\"") == std::string::npos);
        if (is_notification) {
            // Send to subprocess but don't wait for response
            mcp.fire(line);
            continue;
        }

        // Proxy request to subprocess (waits for response)
        std::string response = mcp.send(line);

        if (response.empty()) {
            // Subprocess died — drain stderr for error details
            std::string stderr_output = mcp.drain_stderr();
            std::fprintf(stderr, "[daemon] MCP died during request\n");
            if (!stderr_output.empty()) {
                std::fprintf(stderr, "[daemon] stderr: %s\n", stderr_output.c_str());
            }

            // Extract request ID
            std::string id_str = "null";
            auto id_pos = line.find("\"id\":");
            if (id_pos != std::string::npos) {
                auto val_start = id_pos + 5;
                auto val_end = line.find_first_of(",}", val_start);
                if (val_end != std::string::npos)
                    id_str = line.substr(val_start, val_end - val_start);
            }

            if (line.find("\"id\"") == std::string::npos) {
                continue;
            }

            // Escape stderr for JSON (replace " and \ and newlines)
            std::string escaped_stderr;
            for (char c : stderr_output) {
                if (c == '"')
                    escaped_stderr += "\\\"";
                else if (c == '\\')
                    escaped_stderr += "\\\\";
                else if (c == '\n')
                    escaped_stderr += "\\n";
                else if (c == '\r')
                    continue;
                else if (c >= 32)
                    escaped_stderr += c;
            }
            if (escaped_stderr.size() > 500) {
                escaped_stderr = escaped_stderr.substr(escaped_stderr.size() - 500);
            }

            std::string error_msg = "MCP subprocess crashed";
            if (!escaped_stderr.empty()) {
                error_msg += ": " + escaped_stderr;
            }

            std::cout << "{\"jsonrpc\":\"2.0\",\"error\":{\"code\":-32000,\"message\":\""
                      << error_msg << "\"},\"id\":" << id_str << "}\n"
                      << std::flush;
            continue;
        }

        // Forward response to Claude Code
        std::cout << response << "\n" << std::flush;
    }

    mcp.stop();
    std::fprintf(stderr, "[daemon] Shutting down\n");
    return 0;
}
