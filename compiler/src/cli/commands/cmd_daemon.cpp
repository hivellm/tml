TML_MODULE("compiler")

//! # TML Compiler Daemon
//!
//! Implements `tml daemon start|stop|status` and the internal pipe server.
//!
//! ## Design
//!
//! - One daemon per project directory, keyed by CRC32(cwd).
//! - Windows named pipe in message mode for reliable framing.
//! - Sequential request serving: LLVM context is not thread-safe.
//! - PID file in `<cwd>/.tml-daemon.pid` for process tracking.
//! - Auto-shutdown after 30 minutes of inactivity.

#include "cli/driver.hpp"
#include "common/crc32c.hpp"
#include "log/log.hpp"
#include "plugin/loader.hpp"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <fcntl.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>
#endif

namespace fs = std::filesystem;
using namespace std::chrono_literals;

namespace tml::cli {

// ============================================================================
// Constants
// ============================================================================

static constexpr auto kIdleTimeout = 30min;         // Auto-shutdown after 30min idle
static constexpr DWORD kPipeBufferSize = 1 << 20;   // 1MB pipe buffer
static constexpr DWORD kConnectTimeoutMs = 2000;    // Client connect timeout

// ============================================================================
// Pipe name
// ============================================================================

/// Derive a unique pipe name for the current working directory.
/// Uses CRC32 of the canonical CWD path so different projects don't share pipes.
static auto make_pipe_name(const fs::path& cwd) -> std::wstring {
    std::string cwd_str = cwd.string();
    uint32_t crc = tml::crc32c(cwd_str);
    char hex[16];
    snprintf(hex, sizeof(hex), "%08x", crc);
#ifdef _WIN32
    std::wstring name = L"\\\\.\\pipe\\tml-daemon-";
    name += std::wstring(hex, hex + strlen(hex));
    return name;
#else
    // Unix domain socket in /tmp
    std::wstring name = L"/tmp/tml-daemon-";
    name += std::wstring(hex, hex + strlen(hex));
    return name;
#endif
}

static auto make_pipe_name_str(const fs::path& cwd) -> std::string {
    std::wstring w = make_pipe_name(cwd);
    return std::string(w.begin(), w.end());
}

// ============================================================================
// PID file
// ============================================================================

static auto pid_file_path(const fs::path& cwd) -> fs::path {
    return cwd / ".tml-daemon.pid";
}

static void write_pid_file(const fs::path& cwd) {
    auto path = pid_file_path(cwd);
    std::ofstream f(path);
    if (f) {
#ifdef _WIN32
        f << GetCurrentProcessId() << "\n";
#else
        f << getpid() << "\n";
#endif
    }
}

static auto read_pid_file(const fs::path& cwd) -> uint32_t {
    auto path = pid_file_path(cwd);
    std::ifstream f(path);
    if (!f) return 0;
    uint32_t pid = 0;
    f >> pid;
    return pid;
}

static void remove_pid_file(const fs::path& cwd) {
    std::error_code ec;
    fs::remove(pid_file_path(cwd), ec);
}

static auto is_process_alive(uint32_t pid) -> bool {
    if (pid == 0) return false;
#ifdef _WIN32
    HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, static_cast<DWORD>(pid));
    if (!h) return false;
    DWORD exit_code = STILL_ACTIVE;
    GetExitCodeProcess(h, &exit_code);
    CloseHandle(h);
    return exit_code == STILL_ACTIVE;
#else
    return kill(static_cast<pid_t>(pid), 0) == 0;
#endif
}

// ============================================================================
// JSON helpers (minimal — no dependency)
// ============================================================================

/// Escape a string for embedding in JSON.
static auto json_escape(const std::string& s) -> std::string {
    std::string out;
    out.reserve(s.size() + 8);
    for (unsigned char c : s) {
        if (c == '"')  { out += "\\\""; }
        else if (c == '\\') { out += "\\\\"; }
        else if (c == '\n')  { out += "\\n"; }
        else if (c == '\r')  { out += "\\r"; }
        else if (c == '\t')  { out += "\\t"; }
        else if (c < 0x20)  {
            char buf[8];
            snprintf(buf, sizeof(buf), "\\u%04x", c);
            out += buf;
        } else {
            out += static_cast<char>(c);
        }
    }
    return out;
}

/// Extract a string value from a flat JSON object by key.
/// Very minimal — assumes single-level JSON, properly encoded values.
static auto json_get_str(const std::string& json, const std::string& key) -> std::string {
    std::string search = "\"" + key + "\":\"";
    auto pos = json.find(search);
    if (pos == std::string::npos) return "";
    pos += search.size();
    std::string val;
    bool escape = false;
    for (; pos < json.size(); ++pos) {
        char c = json[pos];
        if (escape) {
            if (c == '"')  val += '"';
            else if (c == '\\') val += '\\';
            else if (c == 'n')  val += '\n';
            else if (c == 'r')  val += '\r';
            else if (c == 't')  val += '\t';
            else val += c;
            escape = false;
        } else if (c == '\\') {
            escape = true;
        } else if (c == '"') {
            break;
        } else {
            val += c;
        }
    }
    return val;
}

/// Extract an array of strings from a flat JSON object by key.
static auto json_get_str_array(const std::string& json, const std::string& key) -> std::vector<std::string> {
    std::string search = "\"" + key + "\":[";
    auto pos = json.find(search);
    if (pos == std::string::npos) return {};
    pos += search.size();

    std::vector<std::string> result;
    while (pos < json.size() && json[pos] != ']') {
        if (json[pos] != '"') { ++pos; continue; }
        ++pos;
        std::string val;
        bool escape = false;
        for (; pos < json.size(); ++pos) {
            char c = json[pos];
            if (escape) {
                if (c == '"')  val += '"';
                else if (c == '\\') val += '\\';
                else if (c == 'n')  val += '\n';
                else if (c == 'r')  val += '\r';
                else if (c == 't')  val += '\t';
                else val += c;
                escape = false;
            } else if (c == '\\') {
                escape = true;
            } else if (c == '"') {
                ++pos; break;
            } else {
                val += c;
            }
        }
        result.push_back(std::move(val));
    }
    return result;
}

/// Extract an integer from a flat JSON object by key.
static auto json_get_int(const std::string& json, const std::string& key) -> int {
    std::string search = "\"" + key + "\":";
    auto pos = json.find(search);
    if (pos == std::string::npos) return 0;
    pos += search.size();
    while (pos < json.size() && json[pos] == ' ') ++pos;
    int val = 0;
    bool neg = false;
    if (pos < json.size() && json[pos] == '-') { neg = true; ++pos; }
    while (pos < json.size() && std::isdigit(static_cast<unsigned char>(json[pos]))) {
        val = val * 10 + (json[pos] - '0');
        ++pos;
    }
    return neg ? -val : val;
}

// ============================================================================
// Stream capture (redirect cout/cerr to string during request)
// ============================================================================

class StreamCapture {
public:
    explicit StreamCapture(std::ostream& s) : stream_(s), buf_(), old_buf_(s.rdbuf(buf_.rdbuf())) {}
    ~StreamCapture() { stream_.rdbuf(old_buf_); }
    auto str() const -> std::string { return buf_.str(); }
private:
    std::ostream& stream_;
    std::ostringstream buf_;    // MUST be declared before old_buf_ for init order
    std::streambuf* old_buf_;
};

// ============================================================================
// In-process result cache — returns cached output instantly for no-change runs
// ============================================================================

struct CacheEntry {
    int exit_code = 0;
    std::string out;
    std::string err;
    /// (canonical_path, mtime) snapshot of every .tml file arg at compile time.
    std::vector<std::pair<std::string, fs::file_time_type>> mtimes;
};

/// Global cache — lives for the daemon's entire lifetime.
static std::unordered_map<std::string, CacheEntry> s_result_cache;

/// CRC32c of joined argv as a hex string (8 chars).
static auto argv_cache_key(const std::vector<std::string>& argv) -> std::string {
    std::string joined;
    for (const auto& a : argv) { joined += a; joined += '\0'; }
    uint32_t crc = tml::crc32c(joined);
    char buf[16];
    snprintf(buf, sizeof(buf), "%08x", crc);
    return std::string(buf, 8);
}

/// Collect (canonical_path, mtime) for each argument that ends in ".tml".
static auto collect_mtimes(const std::vector<std::string>& argv, const std::string& cwd_str)
    -> std::vector<std::pair<std::string, fs::file_time_type>>
{
    std::vector<std::pair<std::string, fs::file_time_type>> result;
    for (const auto& arg : argv) {
        if (arg.size() >= 4 && arg.rfind(".tml") == arg.size() - 4) {
            std::error_code ec;
            fs::path p(arg);
            if (p.is_relative() && !cwd_str.empty())
                p = fs::path(cwd_str) / p;
            p = fs::weakly_canonical(p, ec);
            if (ec) continue;
            auto mtime = fs::last_write_time(p, ec);
            if (!ec) result.emplace_back(p.string(), mtime);
        }
    }
    return result;
}

/// True if every (path, mtime) pair in the snapshot still matches disk.
static auto mtimes_still_valid(const std::vector<std::pair<std::string, fs::file_time_type>>& snap) -> bool {
    for (const auto& [path, cached_mtime] : snap) {
        std::error_code ec;
        auto cur = fs::last_write_time(fs::path(path), ec);
        if (ec || cur != cached_mtime) return false;
    }
    return true;
}

/// Commands whose output is a pure function of the source files (safe to cache).
static auto is_cacheable(const std::vector<std::string>& argv) -> bool {
    if (argv.empty()) return false;
    const auto& cmd = argv[0];
    return cmd == "check" || cmd == "build" || cmd == "emit-ir" || cmd == "emit-mir";
}

// ============================================================================
// DLL staleness detection — exit daemon when the compiler is rebuilt
// ============================================================================

/// Return the path to tml_compiler.dll (or .so) next to the running exe.
static auto compiler_dll_path() -> fs::path {
#ifdef _WIN32
    wchar_t exe_buf[MAX_PATH] = {};
    GetModuleFileNameW(nullptr, exe_buf, MAX_PATH);
    fs::path exe(exe_buf);
    return exe.parent_path() / "plugins" / "tml_compiler.dll";
#else
    // Linux/macOS: resolve /proc/self/exe or use dladdr
    char exe_buf[4096] = {};
    ssize_t n = readlink("/proc/self/exe", exe_buf, sizeof(exe_buf) - 1);
    if (n <= 0) return {};
    exe_buf[n] = '\0';
    fs::path exe(exe_buf);
    return exe.parent_path() / "plugins" / "libtml_compiler.so";
#endif
}

/// Record the mtime of tml_compiler.dll at daemon startup.
static auto record_dll_mtime() -> fs::file_time_type {
    std::error_code ec;
    auto t = fs::last_write_time(compiler_dll_path(), ec);
    return ec ? fs::file_time_type{} : t;
}

/// True if the compiler DLL mtime matches the recorded startup mtime.
static auto dll_still_fresh(const fs::file_time_type& recorded) -> bool {
    if (recorded == fs::file_time_type{}) return true;  // unknown — don't block
    std::error_code ec;
    auto cur = fs::last_write_time(compiler_dll_path(), ec);
    return ec || cur == recorded;
}

// ============================================================================
// Windows named pipe I/O helpers
// ============================================================================

#ifdef _WIN32

static auto pipe_write(HANDLE h, const std::string& msg) -> bool {
    DWORD written = 0;
    return WriteFile(h, msg.data(), static_cast<DWORD>(msg.size()), &written, nullptr) &&
           written == static_cast<DWORD>(msg.size());
}

static auto pipe_read(HANDLE h, std::string& out) -> bool {
    char buf[kPipeBufferSize];
    DWORD read = 0;
    if (!ReadFile(h, buf, sizeof(buf) - 1, &read, nullptr) && GetLastError() != ERROR_MORE_DATA) {
        return false;
    }
    out.assign(buf, read);
    return true;
}

#endif

// ============================================================================
// Request handler — runs inside the daemon server process
// ============================================================================

/// Execute a single compile request forwarded from a client.
/// The argv comes from the JSON request. We capture stdout/cerr and return them.
static auto handle_request(const std::string& request_json) -> std::string {
    std::string cmd = json_get_str(request_json, "cmd");
    int id = json_get_int(request_json, "id");

    if (cmd == "shutdown") {
        return "{\"id\":" + std::to_string(id) + ",\"exit\":0,\"shutdown\":true}\n";
    }

    if (cmd == "ping") {
        return "{\"id\":" + std::to_string(id) + ",\"exit\":0,\"pong\":true}\n";
    }

    // Build/run/check request
    auto argv_strs = json_get_str_array(request_json, "argv");
    std::string cwd_str = json_get_str(request_json, "cwd");

    if (argv_strs.empty()) {
        return "{\"id\":" + std::to_string(id) + ",\"exit\":1,\"err\":\"empty argv\"}\n";
    }

    // Change to the client's working directory
    std::error_code ec;
    fs::path old_cwd = fs::current_path(ec);
    if (!cwd_str.empty()) {
        fs::current_path(fs::path(cwd_str), ec);
        if (ec) {
            std::string err = "cannot chdir to " + cwd_str + ": " + ec.message();
            return "{\"id\":" + std::to_string(id) + ",\"exit\":1,\"err\":\"" +
                   json_escape(err) + "\"}\n";
        }
    }

    // --- Result cache: return instantly if nothing changed ---
    bool cacheable = is_cacheable(argv_strs);
    std::string cache_key;
    std::vector<std::pair<std::string, fs::file_time_type>> snap;

    if (cacheable) {
        cache_key = argv_cache_key(argv_strs);
        snap = collect_mtimes(argv_strs, cwd_str);
        auto it = s_result_cache.find(cache_key);
        if (it != s_result_cache.end() && it->second.mtimes == snap &&
            mtimes_still_valid(it->second.mtimes)) {
            // Cache hit — skip compilation entirely
            const auto& e = it->second;
            if (!cwd_str.empty() && !old_cwd.empty())
                fs::current_path(old_cwd, ec);
            return "{\"id\":" + std::to_string(id) +
                   ",\"exit\":" + std::to_string(e.exit_code) +
                   ",\"out\":\"" + json_escape(e.out) +
                   "\",\"err\":\"" + json_escape(e.err) + "\"}\n";
        }
    }

    // Build a C-style argv — prepend fake exe name so tml_main sees argv[0]=exe, argv[1]=cmd
    std::vector<const char*> cargv;
    cargv.reserve(argv_strs.size() + 2);
    cargv.push_back("tml");  // argv[0]: fake exe name
    for (const auto& s : argv_strs) cargv.push_back(s.c_str());
    cargv.push_back(nullptr);

    // Capture stdout + stderr during the compilation
    StreamCapture cap_out(std::cout);
    StreamCapture cap_err(std::cerr);

    int exit_code = 0;
    try {
        int real_argc = static_cast<int>(argv_strs.size()) + 1;
        exit_code = tml_main(real_argc, const_cast<char**>(cargv.data()));
    } catch (const std::exception& ex) {
        exit_code = 1;
        std::cerr << "daemon: request exception: " << ex.what() << "\n";
    } catch (...) {
        exit_code = 1;
        std::cerr << "daemon: request unknown exception\n";
    }

    // Restore working directory
    if (!cwd_str.empty() && !old_cwd.empty()) {
        fs::current_path(old_cwd, ec);
    }

    std::string out_str = cap_out.str();
    std::string err_str = cap_err.str();

    // --- Store result in cache for next identical request ---
    if (cacheable && !snap.empty()) {
        s_result_cache[cache_key] = CacheEntry{exit_code, out_str, err_str, snap};
    }

    // Build JSON response
    std::string resp = "{\"id\":" + std::to_string(id) +
                       ",\"exit\":" + std::to_string(exit_code) +
                       ",\"out\":\"" + json_escape(out_str) +
                       "\",\"err\":\"" + json_escape(err_str) + "\"}\n";
    return resp;
}

// ============================================================================
// Daemon server (runs as background process)
// ============================================================================

auto cmd_daemon_server(const std::vector<std::string>& /*args*/) -> int {
    // Unset TML_DAEMON so that tml_main() calls inside handle_request()
    // do NOT forward back to the daemon (infinite loop prevention).
#ifdef _WIN32
    _putenv_s("TML_DAEMON", "");
#else
    unsetenv("TML_DAEMON");
#endif

    // Keep compiler/codegen DLLs resident between requests: unload_all()
    // in daemon mode skips FreeLibrary so g_dll_cache retains handles.
    // Without this, each request's Loader destructor would call FreeLibrary,
    // corrupting LLVM static state and causing exit code 127 on the 2nd request.
    tml::plugin::Loader::set_daemon_mode(true);

    auto cwd = fs::current_path();
    write_pid_file(cwd);

    auto pipe_name = make_pipe_name(cwd);

    // Record the compiler DLL mtime at startup.  If the DLL is rebuilt while
    // the daemon is running, the daemon exits cleanly after the current request
    // so the next thin-launcher invocation starts a fresh daemon.
    const auto startup_dll_mtime = record_dll_mtime();

    TML_LOG_INFO("daemon", "Server starting, pipe: "
                               << std::string(pipe_name.begin(), pipe_name.end()));

    auto last_request = std::chrono::steady_clock::now();
    bool shutdown_requested = false;

#ifdef _WIN32
    while (!shutdown_requested) {
        // Check idle timeout
        auto now = std::chrono::steady_clock::now();
        if (now - last_request > kIdleTimeout) {
            TML_LOG_INFO("daemon", "Idle timeout reached, shutting down");
            break;
        }

        // Create a new pipe instance for the next client
        HANDLE hPipe = CreateNamedPipeW(
            pipe_name.c_str(),
            PIPE_ACCESS_DUPLEX,
            PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
            PIPE_UNLIMITED_INSTANCES,
            kPipeBufferSize, kPipeBufferSize,
            100,  // Default timeout 100ms for ConnectNamedPipe
            nullptr);

        if (hPipe == INVALID_HANDLE_VALUE) {
            TML_LOG_ERROR("daemon", "CreateNamedPipe failed: " << GetLastError());
            break;
        }

        // Wait for a client to connect (with timeout to check idle)
        // Use overlapped I/O so we can poll for idle timeout
        OVERLAPPED ov = {};
        ov.hEvent = CreateEvent(nullptr, TRUE, FALSE, nullptr);
        if (!ov.hEvent) { CloseHandle(hPipe); break; }

        BOOL connected = ConnectNamedPipe(hPipe, &ov);
        if (!connected) {
            DWORD err = GetLastError();
            if (err == ERROR_IO_PENDING) {
                // Wait up to 5 seconds, then check idle timeout
                DWORD wait_result = WaitForSingleObject(ov.hEvent, 5000);
                if (wait_result == WAIT_TIMEOUT) {
                    // No client connected — check idle, loop
                    CancelIo(hPipe);
                    CloseHandle(ov.hEvent);
                    CloseHandle(hPipe);
                    continue;
                }
                if (wait_result != WAIT_OBJECT_0) {
                    CloseHandle(ov.hEvent);
                    CloseHandle(hPipe);
                    break;
                }
                DWORD dummy = 0;
                GetOverlappedResult(hPipe, &ov, &dummy, FALSE);
            } else if (err == ERROR_PIPE_CONNECTED) {
                // Client already connected
            } else {
                CloseHandle(ov.hEvent);
                CloseHandle(hPipe);
                break;
            }
        }
        CloseHandle(ov.hEvent);

        // Read the request
        std::string request;
        if (pipe_read(hPipe, request)) {
            last_request = std::chrono::steady_clock::now();

            // Check if the compiler DLL was rebuilt since we started.
            // If so: clear the result cache, respond with a restart notice,
            // and exit so the next invocation spawns a fresh daemon with the
            // updated DLL.
            if (!dll_still_fresh(startup_dll_mtime)) {
                int id = json_get_int(request, "id");
                s_result_cache.clear();
                TML_LOG_INFO("daemon", "Compiler DLL updated — restarting daemon");
                std::string resp = "{\"id\":" + std::to_string(id) +
                    ",\"exit\":1,\"err\":\"daemon: compiler updated, restarting\"}\n";
                pipe_write(hPipe, resp);
                FlushFileBuffers(hPipe);
                DisconnectNamedPipe(hPipe);
                CloseHandle(hPipe);
                remove_pid_file(cwd);
                return 0;  // Exit cleanly; next invocation starts a fresh daemon
            }

            std::string response = handle_request(request);
            pipe_write(hPipe, response);

            // Check if the request was a shutdown command
            if (response.find("\"shutdown\":true") != std::string::npos) {
                shutdown_requested = true;
            }
        }

        FlushFileBuffers(hPipe);
        DisconnectNamedPipe(hPipe);
        CloseHandle(hPipe);
    }
#else
    // Unix domain socket implementation
    std::string sock_path = std::string(pipe_name.begin(), pipe_name.end());
    int server_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (server_fd < 0) {
        TML_LOG_ERROR("daemon", "socket() failed");
        remove_pid_file(cwd);
        return 1;
    }

    struct sockaddr_un addr = {};
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, sock_path.c_str(), sizeof(addr.sun_path) - 1);
    unlink(sock_path.c_str());

    if (bind(server_fd, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) < 0) {
        TML_LOG_ERROR("daemon", "bind() failed");
        close(server_fd);
        remove_pid_file(cwd);
        return 1;
    }
    listen(server_fd, 5);

    // Set socket to non-blocking for timeout checking
    fcntl(server_fd, F_SETFL, O_NONBLOCK);

    while (!shutdown_requested) {
        auto now = std::chrono::steady_clock::now();
        if (now - last_request > kIdleTimeout) {
            TML_LOG_INFO("daemon", "Idle timeout reached, shutting down");
            break;
        }

        // Poll for incoming connection (5 second intervals)
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(server_fd, &fds);
        struct timeval tv = {5, 0};
        int ready = select(server_fd + 1, &fds, nullptr, nullptr, &tv);
        if (ready <= 0) continue;

        int client_fd = accept(server_fd, nullptr, nullptr);
        if (client_fd < 0) continue;

        char buf[1 << 20] = {};
        ssize_t n = recv(client_fd, buf, sizeof(buf) - 1, 0);
        if (n > 0) {
            std::string request(buf, static_cast<size_t>(n));
            last_request = std::chrono::steady_clock::now();

            if (!dll_still_fresh(startup_dll_mtime)) {
                int id = json_get_int(request, "id");
                s_result_cache.clear();
                TML_LOG_INFO("daemon", "Compiler DLL updated — restarting daemon");
                std::string resp = "{\"id\":" + std::to_string(id) +
                    ",\"exit\":1,\"err\":\"daemon: compiler updated, restarting\"}\n";
                send(client_fd, resp.data(), resp.size(), 0);
                close(client_fd);
                close(server_fd);
                unlink(sock_path.c_str());
                remove_pid_file(cwd);
                return 0;
            }

            std::string response = handle_request(request);
            send(client_fd, response.data(), response.size(), 0);
            if (response.find("\"shutdown\":true") != std::string::npos) {
                shutdown_requested = true;
            }
        }
        close(client_fd);
    }

    close(server_fd);
    unlink(sock_path.c_str());
#endif

    remove_pid_file(cwd);
    TML_LOG_INFO("daemon", "Server exited cleanly");
    return 0;
}

// ============================================================================
// Daemon start
// ============================================================================

static auto daemon_start() -> int {
    auto cwd = fs::current_path();
    auto pid_path = pid_file_path(cwd);

    // Check if already running
    uint32_t existing_pid = read_pid_file(cwd);
    if (existing_pid && is_process_alive(existing_pid)) {
        std::cout << "TML daemon already running (PID " << existing_pid << ")\n";
        return 0;
    }

    // Remove stale PID file
    if (fs::exists(pid_path)) {
        std::error_code ec;
        fs::remove(pid_path, ec);
    }

    // Spawn ourselves with the hidden __daemon_server subcommand
#ifdef _WIN32
    wchar_t exe_path[MAX_PATH] = {};
    GetModuleFileNameW(nullptr, exe_path, MAX_PATH);

    std::wstring cmd_line = std::wstring(L"\"") + exe_path + L"\" __daemon_server";

    STARTUPINFOW si = {};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    // Detach from console: redirect handles to NUL
    HANDLE hNul = CreateFileW(L"NUL", GENERIC_READ | GENERIC_WRITE,
                              FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                              OPEN_EXISTING, 0, nullptr);
    si.hStdInput = hNul;
    si.hStdOutput = hNul;
    si.hStdError = hNul;

    PROCESS_INFORMATION pi = {};
    BOOL ok = CreateProcessW(
        nullptr,
        cmd_line.data(),
        nullptr, nullptr,
        FALSE,
        DETACHED_PROCESS | CREATE_NO_WINDOW,
        nullptr,
        nullptr,  // inherit CWD from parent
        &si, &pi);

    CloseHandle(hNul);
    if (!ok) {
        std::cerr << "error: failed to spawn daemon process: " << GetLastError() << "\n";
        return 1;
    }
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
#else
    // Fork + setsid on Unix
    pid_t pid = fork();
    if (pid < 0) {
        std::cerr << "error: fork() failed\n";
        return 1;
    }
    if (pid > 0) {
        // Parent: wait briefly for PID file
    } else {
        // Child: become daemon
        setsid();
        // Re-exec ourselves with __daemon_server
        // Get our own exe path
        char exe_path[4096] = {};
        ssize_t n = readlink("/proc/self/exe", exe_path, sizeof(exe_path) - 1);
        if (n > 0) {
            exe_path[n] = '\0';
            execl(exe_path, exe_path, "__daemon_server", nullptr);
        }
        _exit(1);
    }
#endif

    // Wait up to 3 seconds for the PID file to appear
    for (int i = 0; i < 30; ++i) {
        std::this_thread::sleep_for(100ms);
        uint32_t new_pid = read_pid_file(cwd);
        if (new_pid && is_process_alive(new_pid)) {
            std::cout << "TML daemon started (PID " << new_pid << ")\n";
            return 0;
        }
    }

    std::cerr << "warning: daemon may not have started (no PID file found)\n";
    return 0;
}

// ============================================================================
// Daemon stop
// ============================================================================

static auto daemon_stop() -> int {
    auto cwd = fs::current_path();
    uint32_t pid = read_pid_file(cwd);
    if (!pid || !is_process_alive(pid)) {
        std::cout << "TML daemon is not running\n";
        return 0;
    }

    // Send shutdown request via pipe
    auto pipe_name = make_pipe_name(cwd);

#ifdef _WIN32
    HANDLE hPipe = CreateFileW(
        pipe_name.c_str(),
        GENERIC_READ | GENERIC_WRITE,
        0, nullptr, OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL, nullptr);

    if (hPipe == INVALID_HANDLE_VALUE) {
        // Pipe not reachable — kill the process directly
        std::cerr << "warning: cannot connect to daemon pipe; process may already be gone\n";
        remove_pid_file(cwd);
        return 0;
    }

    // Switch to message read mode
    DWORD mode = PIPE_READMODE_MESSAGE;
    SetNamedPipeHandleState(hPipe, &mode, nullptr, nullptr);

    std::string req = "{\"id\":0,\"cmd\":\"shutdown\"}\n";
    pipe_write(hPipe, req);

    std::string resp;
    pipe_read(hPipe, resp);
    CloseHandle(hPipe);
#else
    std::string sock_path = std::string(pipe_name.begin(), pipe_name.end());
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    struct sockaddr_un addr = {};
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, sock_path.c_str(), sizeof(addr.sun_path) - 1);
    if (connect(fd, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) == 0) {
        std::string req = "{\"id\":0,\"cmd\":\"shutdown\"}\n";
        send(fd, req.data(), req.size(), 0);
        char buf[256] = {};
        recv(fd, buf, sizeof(buf), 0);
    }
    close(fd);
#endif

    std::cout << "TML daemon stopped\n";
    return 0;
}

// ============================================================================
// Daemon status
// ============================================================================

static auto daemon_status() -> int {
    auto cwd = fs::current_path();
    uint32_t pid = read_pid_file(cwd);
    if (!pid) {
        std::cout << "TML daemon: not running (no PID file)\n";
        return 1;
    }
    if (!is_process_alive(pid)) {
        std::cout << "TML daemon: not running (stale PID " << pid << ")\n";
        return 1;
    }

    // Try to ping it
    auto pipe_name = make_pipe_name(cwd);
    bool responsive = false;

#ifdef _WIN32
    HANDLE hPipe = CreateFileW(
        pipe_name.c_str(),
        GENERIC_READ | GENERIC_WRITE,
        0, nullptr, OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hPipe != INVALID_HANDLE_VALUE) {
        DWORD mode = PIPE_READMODE_MESSAGE;
        SetNamedPipeHandleState(hPipe, &mode, nullptr, nullptr);
        std::string req = "{\"id\":0,\"cmd\":\"ping\"}\n";
        if (pipe_write(hPipe, req)) {
            std::string resp;
            responsive = pipe_read(hPipe, resp) && resp.find("pong") != std::string::npos;
        }
        CloseHandle(hPipe);
    }
#else
    std::string sock_path = std::string(pipe_name.begin(), pipe_name.end());
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    struct sockaddr_un addr = {};
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, sock_path.c_str(), sizeof(addr.sun_path) - 1);
    if (connect(fd, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) == 0) {
        std::string req = "{\"id\":0,\"cmd\":\"ping\"}\n";
        send(fd, req.data(), req.size(), 0);
        char buf[256] = {};
        ssize_t n = recv(fd, buf, sizeof(buf), 0);
        responsive = n > 0 && std::string(buf, static_cast<size_t>(n)).find("pong") != std::string::npos;
    }
    close(fd);
#endif

    if (responsive) {
        std::cout << "TML daemon: running (PID " << pid << ", responsive)\n";
        return 0;
    } else {
        std::cout << "TML daemon: running (PID " << pid << ", pipe not reachable)\n";
        return 0;
    }
}

// ============================================================================
// Client-side: forward request to running daemon
// ============================================================================

auto try_daemon_forward(int argc, char* argv[]) -> int {
    auto cwd = fs::current_path();
    uint32_t pid = read_pid_file(cwd);
    if (!pid || !is_process_alive(pid)) return -1;

    auto pipe_name = make_pipe_name(cwd);

    // Build JSON request
    std::string argv_json = "[";
    for (int i = 1; i < argc; ++i) {  // skip argv[0] (exe name)
        if (i > 1) argv_json += ',';
        argv_json += '"';
        argv_json += json_escape(argv[i]);
        argv_json += '"';
    }
    argv_json += ']';

    std::string cwd_str = json_escape(cwd.string());
    std::string request = "{\"id\":1,\"cmd\":\"forward\",\"argv\":" + argv_json +
                          ",\"cwd\":\"" + cwd_str + "\"}\n";

#ifdef _WIN32
    // Try to connect; if the pipe is busy (server handling another request),
    // wait briefly before giving up.
    HANDLE hPipe = INVALID_HANDLE_VALUE;
    for (int attempt = 0; attempt < 5; ++attempt) {
        hPipe = CreateFileW(
            pipe_name.c_str(),
            GENERIC_READ | GENERIC_WRITE,
            0, nullptr, OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL, nullptr);
        if (hPipe != INVALID_HANDLE_VALUE) break;
        if (GetLastError() == ERROR_PIPE_BUSY) {
            if (!WaitNamedPipeW(pipe_name.c_str(), kConnectTimeoutMs)) break;
        } else {
            break;
        }
    }

    if (hPipe == INVALID_HANDLE_VALUE) return -1;  // Daemon not reachable

    DWORD mode = PIPE_READMODE_MESSAGE;
    SetNamedPipeHandleState(hPipe, &mode, nullptr, nullptr);

    if (!pipe_write(hPipe, request)) {
        CloseHandle(hPipe);
        return -1;
    }

    std::string response;
    if (!pipe_read(hPipe, response)) {
        CloseHandle(hPipe);
        return -1;
    }
    CloseHandle(hPipe);
#else
    std::string sock_path = std::string(pipe_name.begin(), pipe_name.end());
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    struct sockaddr_un addr = {};
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, sock_path.c_str(), sizeof(addr.sun_path) - 1);
    if (connect(fd, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) != 0) {
        close(fd);
        return -1;
    }
    send(fd, request.data(), request.size(), 0);
    char buf[1 << 20] = {};
    ssize_t n = recv(fd, buf, sizeof(buf) - 1, 0);
    close(fd);
    if (n <= 0) return -1;
    std::string response(buf, static_cast<size_t>(n));
#endif

    // Parse response: extract exit, out, err
    int exit_code = json_get_int(response, "exit");
    std::string out_str = json_get_str(response, "out");
    std::string err_str = json_get_str(response, "err");

    if (!out_str.empty()) std::cout << out_str;
    if (!err_str.empty()) std::cerr << err_str;

    return exit_code;
}

// ============================================================================
// Public entry point
// ============================================================================

auto cmd_daemon(const std::vector<std::string>& args) -> int {
    if (args.empty() || args[0] == "start") {
        return daemon_start();
    }
    if (args[0] == "stop") {
        return daemon_stop();
    }
    if (args[0] == "status") {
        return daemon_status();
    }
    if (args[0] == "--help" || args[0] == "-h") {
        std::cout << R"(Usage: tml daemon <subcommand>

Persistent compilation daemon for sub-10ms incremental builds.
The daemon keeps the TML compiler and LLVM context warm between
invocations, eliminating the ~2-7s cold-start overhead.

Subcommands:
  start     Start the daemon for the current project directory
  stop      Stop the daemon
  status    Check if the daemon is running

Usage:
  tml daemon start            # Start for this project
  TML_DAEMON=1 tml build ...  # Use daemon if running (auto mode)
  tml daemon stop             # Shut down

The daemon writes its PID to .tml-daemon.pid in the project directory.
It auto-shuts down after 30 minutes of inactivity.
)";
        return 0;
    }

    std::cerr << "error: unknown daemon subcommand '" << args[0] << "'\n";
    std::cerr << "  Valid subcommands: start, stop, status\n";
    return 1;
}

} // namespace tml::cli
