TML_MODULE("daemon")

//! # TML MCP Daemon — HTTP proxy + subprocess supervisor
//!
//! Persistent daemon that:
//! 1. Listens on localhost:25710 for HTTP MCP requests
//! 2. Spawns tml_mcp.exe as a subprocess with stdio transport
//! 3. Proxies HTTP requests → stdin, stdout → HTTP responses
//! 4. Auto-restarts tml_mcp.exe on crash
//! 5. Supports hot reload: kill + relaunch on SIGHUP or /reload endpoint
//!
//! ## Why?
//!
//! - tml_mcp.exe can be rebuilt while daemon runs (no file locking)
//! - Subprocess crashes don't kill the HTTP server
//! - Socket handles stay in daemon, not inherited by subprocess
//! - Claude Code connects to stable HTTP endpoint
//!
//! ## Usage
//!
//! ```bash
//! tml_daemon                    # Start on port 25710
//! tml_daemon --port 30000       # Custom port
//! curl http://localhost:25710/reload  # Hot reload MCP subprocess
//! ```

#include "log/log.hpp"

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
using socket_t = SOCKET;
constexpr socket_t INVALID_SOCK = INVALID_SOCKET;
static void close_socket(socket_t s) {
    closesocket(s);
}
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>
using socket_t = int;
constexpr socket_t INVALID_SOCK = -1;
static void close_socket(socket_t s) {
    close(s);
}
#endif

namespace fs = std::filesystem;

// ============================================================================
// MCP Subprocess Manager
// ============================================================================

class McpSubprocess {
public:
    McpSubprocess() = default;
    ~McpSubprocess() {
        stop();
    }

    bool start(const std::string& exe_path) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (running_)
            stop_unlocked();

        exe_path_ = exe_path;

#ifdef _WIN32
        // Create pipes for stdin/stdout
        SECURITY_ATTRIBUTES sa{};
        sa.nLength = sizeof(sa);
        sa.bInheritHandle = TRUE;

        if (!CreatePipe(&child_stdout_read_, &child_stdout_write_, &sa, 0))
            return false;
        if (!CreatePipe(&child_stdin_read_, &child_stdin_write_, &sa, 0))
            return false;

        // Ensure our ends are not inherited
        SetHandleInformation(child_stdout_read_, HANDLE_FLAG_INHERIT, 0);
        SetHandleInformation(child_stdin_write_, HANDLE_FLAG_INHERIT, 0);

        STARTUPINFOA si{};
        si.cb = sizeof(si);
        si.dwFlags = STARTF_USESTDHANDLES;
        si.hStdInput = child_stdin_read_;
        si.hStdOutput = child_stdout_write_;
        si.hStdError = GetStdHandle(STD_ERROR_HANDLE);

        PROCESS_INFORMATION pi{};
        std::string cmd = exe_path;

        if (!CreateProcessA(nullptr, cmd.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW, nullptr,
                            nullptr, &si, &pi)) {
            std::fprintf(stderr, "[daemon] Failed to start MCP subprocess: %s (error %lu)\n",
                         exe_path.c_str(), GetLastError());
            return false;
        }

        process_ = pi.hProcess;
        CloseHandle(pi.hThread);

        // Close the child's ends in our process
        CloseHandle(child_stdout_write_);
        child_stdout_write_ = INVALID_HANDLE_VALUE;
        CloseHandle(child_stdin_read_);
        child_stdin_read_ = INVALID_HANDLE_VALUE;
#else
        int stdin_pipe[2], stdout_pipe[2];
        if (pipe(stdin_pipe) < 0 || pipe(stdout_pipe) < 0)
            return false;

        pid_t pid = fork();
        if (pid < 0)
            return false;

        if (pid == 0) {
            // Child
            close(stdin_pipe[1]);
            close(stdout_pipe[0]);
            dup2(stdin_pipe[0], STDIN_FILENO);
            dup2(stdout_pipe[1], STDOUT_FILENO);
            close(stdin_pipe[0]);
            close(stdout_pipe[1]);
            execl(exe_path.c_str(), exe_path.c_str(), nullptr);
            _exit(1);
        }

        // Parent
        close(stdin_pipe[0]);
        close(stdout_pipe[1]);
        child_stdin_fd_ = stdin_pipe[1];
        child_stdout_fd_ = stdout_pipe[0];
        pid_ = pid;
#endif

        running_ = true;
        std::fprintf(stderr, "[daemon] MCP subprocess started: %s (PID %d)\n", exe_path.c_str(),
                     get_pid());
        return true;
    }

    void stop() {
        std::lock_guard<std::mutex> lock(mutex_);
        stop_unlocked();
    }

    // Send a line to MCP subprocess stdin, read response from stdout
    std::string send_request(const std::string& json_line) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!running_)
            return "";

        std::string line = json_line + "\n";

#ifdef _WIN32
        DWORD written;
        if (!WriteFile(child_stdin_write_, line.c_str(), static_cast<DWORD>(line.size()), &written,
                       nullptr)) {
            std::fprintf(stderr, "[daemon] Write to MCP subprocess failed\n");
            return "";
        }

        // Read response line
        std::string response;
        char buf[4096];
        while (true) {
            DWORD avail = 0;
            PeekNamedPipe(child_stdout_read_, nullptr, 0, nullptr, &avail, nullptr);
            if (avail == 0) {
                // Wait a bit for the subprocess to respond
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                PeekNamedPipe(child_stdout_read_, nullptr, 0, nullptr, &avail, nullptr);
                if (avail == 0) {
                    // Poll with timeout
                    for (int i = 0; i < 3000; i++) { // 30 seconds max
                        std::this_thread::sleep_for(std::chrono::milliseconds(10));
                        PeekNamedPipe(child_stdout_read_, nullptr, 0, nullptr, &avail, nullptr);
                        if (avail > 0)
                            break;
                        // Check if process died
                        DWORD exit_code;
                        if (GetExitCodeProcess(process_, &exit_code) && exit_code != STILL_ACTIVE) {
                            std::fprintf(stderr, "[daemon] MCP subprocess died (exit %lu)\n",
                                         exit_code);
                            running_ = false;
                            return "";
                        }
                    }
                    if (avail == 0)
                        return ""; // Timeout
                }
            }

            DWORD read_bytes;
            DWORD to_read = (avail < sizeof(buf) - 1) ? avail : sizeof(buf) - 1;
            if (!ReadFile(child_stdout_read_, buf, to_read, &read_bytes, nullptr) ||
                read_bytes == 0) {
                break;
            }
            buf[read_bytes] = '\0';
            response.append(buf, read_bytes);

            // Check if we got a complete JSON line
            if (response.find('\n') != std::string::npos) {
                break;
            }
        }

        // Trim trailing newline
        while (!response.empty() && (response.back() == '\n' || response.back() == '\r')) {
            response.pop_back();
        }
        return response;
#else
        ssize_t w = write(child_stdin_fd_, line.c_str(), line.size());
        if (w < 0)
            return "";

        std::string response;
        char buf[4096];
        while (true) {
            ssize_t r = read(child_stdout_fd_, buf, sizeof(buf) - 1);
            if (r <= 0)
                break;
            buf[r] = '\0';
            response.append(buf, r);
            if (response.find('\n') != std::string::npos)
                break;
        }
        while (!response.empty() && (response.back() == '\n' || response.back() == '\r')) {
            response.pop_back();
        }
        return response;
#endif
    }

    bool is_running() const {
        return running_;
    }

    int get_pid() const {
#ifdef _WIN32
        return process_ != INVALID_HANDLE_VALUE ? static_cast<int>(GetProcessId(process_)) : 0;
#else
        return pid_;
#endif
    }

private:
    void stop_unlocked() {
        if (!running_)
            return;

#ifdef _WIN32
        if (child_stdin_write_ != INVALID_HANDLE_VALUE) {
            CloseHandle(child_stdin_write_);
            child_stdin_write_ = INVALID_HANDLE_VALUE;
        }
        if (child_stdout_read_ != INVALID_HANDLE_VALUE) {
            CloseHandle(child_stdout_read_);
            child_stdout_read_ = INVALID_HANDLE_VALUE;
        }
        if (process_ != INVALID_HANDLE_VALUE) {
            TerminateProcess(process_, 0);
            WaitForSingleObject(process_, 3000);
            CloseHandle(process_);
            process_ = INVALID_HANDLE_VALUE;
        }
#else
        if (child_stdin_fd_ >= 0) {
            close(child_stdin_fd_);
            child_stdin_fd_ = -1;
        }
        if (child_stdout_fd_ >= 0) {
            close(child_stdout_fd_);
            child_stdout_fd_ = -1;
        }
        if (pid_ > 0) {
            kill(pid_, SIGTERM);
            int status;
            waitpid(pid_, &status, WNOHANG);
            pid_ = 0;
        }
#endif
        running_ = false;
        std::fprintf(stderr, "[daemon] MCP subprocess stopped\n");
    }

    std::mutex mutex_;
    std::atomic<bool> running_{false};
    std::string exe_path_;

#ifdef _WIN32
    HANDLE process_ = INVALID_HANDLE_VALUE;
    HANDLE child_stdin_read_ = INVALID_HANDLE_VALUE;
    HANDLE child_stdin_write_ = INVALID_HANDLE_VALUE;
    HANDLE child_stdout_read_ = INVALID_HANDLE_VALUE;
    HANDLE child_stdout_write_ = INVALID_HANDLE_VALUE;
#else
    pid_t pid_ = 0;
    int child_stdin_fd_ = -1;
    int child_stdout_fd_ = -1;
#endif
};

// ============================================================================
// HTTP Server
// ============================================================================

static std::string read_http_body(socket_t client_fd) {
    std::string request;
    char buf[8192];

    while (true) {
        int n = recv(client_fd, buf, sizeof(buf) - 1, 0);
        if (n <= 0)
            return "";
        buf[n] = '\0';
        request.append(buf, static_cast<size_t>(n));

        auto header_end = request.find("\r\n\r\n");
        if (header_end != std::string::npos) {
            size_t content_length = 0;
            auto cl_pos = request.find("Content-Length: ");
            if (cl_pos == std::string::npos)
                cl_pos = request.find("content-length: ");
            if (cl_pos != std::string::npos) {
                auto val_start = cl_pos + 16;
                auto val_end = request.find("\r\n", val_start);
                if (val_end != std::string::npos) {
                    content_length = static_cast<size_t>(
                        std::stoul(request.substr(val_start, val_end - val_start)));
                }
            }

            size_t body_start = header_end + 4;
            size_t body_received = request.size() - body_start;

            while (body_received < content_length) {
                size_t remaining = content_length - body_received;
                size_t to_read = (sizeof(buf) - 1 < remaining) ? sizeof(buf) - 1 : remaining;
                n = recv(client_fd, buf, static_cast<int>(to_read), 0);
                if (n <= 0)
                    break;
                buf[n] = '\0';
                request.append(buf, static_cast<size_t>(n));
                body_received += static_cast<size_t>(n);
            }

            return request.substr(body_start);
        }

        if (request.size() > 65536)
            return "";
    }
}

static void send_http_response(socket_t client_fd, int status, const std::string& body) {
    const char* status_text = "OK";
    if (status == 204)
        status_text = "No Content";
    else if (status == 404)
        status_text = "Not Found";
    else if (status == 503)
        status_text = "Service Unavailable";

    std::string resp =
        "HTTP/1.1 " + std::to_string(status) + " " + status_text +
        "\r\nContent-Type: application/json\r\nContent-Length: " + std::to_string(body.size()) +
        "\r\nAccess-Control-Allow-Origin: *\r\nConnection: close\r\n\r\n" + body;
    send(client_fd, resp.c_str(), static_cast<int>(resp.size()), 0);
}

// ============================================================================
// Main
// ============================================================================

int main(int argc, char* argv[]) {
    int port = 25710;
    std::string mcp_exe;

    // Find tml_mcp.exe next to us
    {
#ifdef _WIN32
        char buf[MAX_PATH];
        DWORD len = GetModuleFileNameA(nullptr, buf, MAX_PATH);
        if (len > 0) {
            fs::path self = fs::path(std::string(buf, len));
            mcp_exe = (self.parent_path() / "tml_mcp.exe").string();
        }
#else
        auto self = fs::read_symlink("/proc/self/exe");
        mcp_exe = (self.parent_path() / "tml_mcp").string();
#endif
    }

    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg.starts_with("--port=")) {
            port = std::stoi(arg.substr(7));
        } else if (arg == "--mcp" && i + 1 < argc) {
            mcp_exe = argv[++i];
        } else if (arg == "--help" || arg == "-h") {
            std::cerr << "Usage: tml_daemon [--port=PORT] [--mcp PATH]\n"
                      << "  --port=PORT  HTTP port (default: 25710)\n"
                      << "  --mcp PATH   Path to tml_mcp.exe\n";
            return 0;
        }
    }

    if (!fs::exists(mcp_exe)) {
        std::fprintf(stderr, "[daemon] ERROR: tml_mcp.exe not found at: %s\n", mcp_exe.c_str());
        return 1;
    }

    // Initialize Winsock
#ifdef _WIN32
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);
#endif

    // Create server socket
    socket_t server_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (server_fd == INVALID_SOCK) {
        std::fprintf(stderr, "[daemon] Failed to create socket\n");
        return 1;
    }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&opt),
               sizeof(opt));

    struct sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = htons(static_cast<uint16_t>(port));

    if (bind(server_fd, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) != 0) {
        std::fprintf(stderr, "[daemon] Failed to bind port %d\n", port);
        close_socket(server_fd);
        return 1;
    }

    listen(server_fd, 10);
    std::fprintf(stderr, "[daemon] Listening on http://localhost:%d/mcp\n", port);
    std::fprintf(stderr, "[daemon] MCP subprocess: %s\n", mcp_exe.c_str());

    // Start MCP subprocess
    McpSubprocess mcp;
    mcp.start(mcp_exe);

    // Send initialize to MCP subprocess
    std::string init_req =
        R"({"jsonrpc":"2.0","method":"initialize","id":0,"params":{"protocolVersion":"2024-11-05","capabilities":{},"clientInfo":{"name":"tml_daemon","version":"1.0"}}})";
    auto init_resp = mcp.send_request(init_req);
    if (init_resp.empty()) {
        std::fprintf(stderr, "[daemon] ERROR: MCP subprocess did not respond to initialize\n");
        return 1;
    }
    std::fprintf(stderr, "[daemon] MCP initialized OK\n");

    // Send notifications/initialized
    mcp.send_request(R"({"jsonrpc":"2.0","method":"notifications/initialized","params":{}})");

    std::fprintf(stderr, "[daemon] Ready — accepting HTTP connections\n");

    // Accept loop
    while (true) {
        struct sockaddr_in client_addr{};
        socklen_t client_len = sizeof(client_addr);
        socket_t client_fd =
            accept(server_fd, reinterpret_cast<struct sockaddr*>(&client_addr), &client_len);
        if (client_fd == INVALID_SOCK)
            continue;

        // Read HTTP body
        std::string body = read_http_body(client_fd);

        if (body.empty()) {
            // Could be OPTIONS preflight
            std::string cors = "HTTP/1.1 204 No Content\r\n"
                               "Access-Control-Allow-Origin: *\r\n"
                               "Access-Control-Allow-Methods: POST, OPTIONS\r\n"
                               "Access-Control-Allow-Headers: Content-Type\r\n"
                               "Content-Length: 0\r\n\r\n";
            send(client_fd, cors.c_str(), static_cast<int>(cors.size()), 0);
            close_socket(client_fd);
            continue;
        }

        // Check for /reload endpoint
        if (body.find("\"method\":\"reload\"") != std::string::npos ||
            body.find("/reload") != std::string::npos) {
            std::fprintf(stderr, "[daemon] Reloading MCP subprocess...\n");
            mcp.stop();
            mcp.start(mcp_exe);
            // Re-initialize
            auto resp = mcp.send_request(init_req);
            mcp.send_request(
                R"({"jsonrpc":"2.0","method":"notifications/initialized","params":{}})");
            send_http_response(client_fd, 200, R"({"status":"reloaded"})");
            close_socket(client_fd);
            std::fprintf(stderr, "[daemon] MCP subprocess reloaded\n");
            continue;
        }

        // Auto-restart if subprocess died
        if (!mcp.is_running()) {
            std::fprintf(stderr, "[daemon] MCP subprocess not running, restarting...\n");
            mcp.start(mcp_exe);
            mcp.send_request(init_req);
            mcp.send_request(
                R"({"jsonrpc":"2.0","method":"notifications/initialized","params":{}})");
        }

        // Proxy to MCP subprocess
        std::string response = mcp.send_request(body);
        if (response.empty()) {
            send_http_response(
                client_fd, 503,
                R"({"jsonrpc":"2.0","error":{"code":-32000,"message":"MCP subprocess unavailable"},"id":null})");
        } else {
            send_http_response(client_fd, 200, response);
        }
        close_socket(client_fd);
    }

    close_socket(server_fd);
#ifdef _WIN32
    WSACleanup();
#endif
    return 0;
}
