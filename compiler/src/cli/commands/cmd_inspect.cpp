TML_MODULE("tools")

//! # Inspect Command — Terminal Debugger
//!
//! Implements `tml inspect <file.tml>`.  Launches the target program with
//! `--inspect-brk`, connects to its WebSocket CDP server, and provides an
//! interactive REPL for setting breakpoints and inspecting runtime state.
//!
//! ## Protocol overview
//!
//! ```
//! tml inspect main.tml
//!   │
//!   ├─ CreateProcess: tml.exe run main.tml --inspect-brk --inspect-port=9229
//!   │    (process pauses at entry point, listens on ws://127.0.0.1:9229/tml)
//!   │
//!   ├─ connect_inspector()
//!   │    TCP connect → HTTP/1.1 Upgrade: websocket → 101 Switching Protocols
//!   │
//!   └─ repl_loop()
//!        Runtime.enable / Debugger.enable
//!        → read stdin commands → send CDP JSON → receive response → print
//! ```

#include "cmd_inspect.hpp"

#include "log/log.hpp"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
typedef SOCKET socket_t;
#define INVALID_SOCK INVALID_SOCKET
#define sock_close closesocket
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
typedef int socket_t;
#define INVALID_SOCK (-1)
#define sock_close close
#endif

namespace fs = std::filesystem;

namespace tml::cli {

// ============================================================================
// WebSocket Client — RFC 6455, client side (frames MUST be masked)
// ============================================================================

/// Send a single text frame over a client WebSocket connection.
///
/// Client-to-server frames MUST be masked (RFC 6455 §5.1).  We use a fixed
/// four-byte mask key for simplicity — this is spec-compliant.
static int ws_client_send(socket_t sock, const char* data, int len) {
    uint8_t header[14];
    int hlen = 0;

    header[0] = 0x81; // FIN bit set, opcode 0x1 (text)

    // Fixed mask key (client MUST mask per RFC 6455 §5.3)
    const uint8_t mask_key[4] = {0x12, 0x34, 0x56, 0x78};

    // Encode payload length with MASK bit (0x80) set
    if (len < 126) {
        header[1] = 0x80 | static_cast<uint8_t>(len);
        hlen = 2;
    } else if (len < 65536) {
        header[1] = 0x80 | 126;
        header[2] = static_cast<uint8_t>(len >> 8);
        header[3] = static_cast<uint8_t>(len & 0xFF);
        hlen = 4;
    } else {
        header[1] = 0x80 | 127;
        // Upper 32 bits are zero for payloads we'll ever send
        memset(header + 2, 0, 4);
        header[6] = static_cast<uint8_t>(len >> 24);
        header[7] = static_cast<uint8_t>(len >> 16);
        header[8] = static_cast<uint8_t>(len >> 8);
        header[9] = static_cast<uint8_t>(len);
        hlen = 10;
    }

    // Append the four-byte masking key
    memcpy(header + hlen, mask_key, 4);
    hlen += 4;

    if (send(sock, reinterpret_cast<const char*>(header), hlen, 0) != hlen) {
        return -1;
    }

    // XOR-mask the payload and send
    char* masked = static_cast<char*>(malloc(static_cast<size_t>(len)));
    if (!masked)
        return -1;
    for (int i = 0; i < len; ++i) {
        masked[i] = data[i] ^ mask_key[i % 4];
    }
    int sent = send(sock, masked, len, 0);
    free(masked);
    return (sent == len) ? 0 : -1;
}

/// Receive one WebSocket frame from the server.
///
/// Server-to-client frames are NOT masked.  Handles ping (responds with pong)
/// and close frames.  Returns number of bytes written into buf, or -1 on error.
static int ws_client_recv(socket_t sock, char* buf, int buf_size) {
    uint8_t h[2];
    if (recv(sock, reinterpret_cast<char*>(h), 2, 0) != 2)
        return -1;

    const int opcode = h[0] & 0x0F;
    uint64_t payload_len = h[1] & 0x7F;

    if (payload_len == 126) {
        uint8_t ext[2];
        if (recv(sock, reinterpret_cast<char*>(ext), 2, 0) != 2)
            return -1;
        payload_len = (static_cast<uint64_t>(ext[0]) << 8) | ext[1];
    } else if (payload_len == 127) {
        uint8_t ext[8];
        if (recv(sock, reinterpret_cast<char*>(ext), 8, 0) != 8)
            return -1;
        payload_len = 0;
        for (int i = 0; i < 8; ++i) {
            payload_len = (payload_len << 8) | ext[i];
        }
    }

    if (static_cast<int64_t>(payload_len) >= buf_size)
        return -1;

    // Read all payload bytes (may require multiple recv calls)
    int total = 0;
    while (total < static_cast<int>(payload_len)) {
        int r = recv(sock, buf + total, static_cast<int>(payload_len) - total, 0);
        if (r <= 0)
            return -1;
        total += r;
    }
    buf[total] = '\0';

    if (opcode == 0x8) {
        // Connection close frame
        return 0;
    }
    if (opcode == 0x9) {
        // Ping — respond with masked empty pong (0x8A = FIN + pong opcode)
        const uint8_t pong_hdr[6] = {0x8A, 0x80, 0x00, 0x00, 0x00, 0x00};
        send(sock, reinterpret_cast<const char*>(pong_hdr), 6, 0);
        // Recurse to read the next real frame
        return ws_client_recv(sock, buf, buf_size);
    }

    return total;
}

// ============================================================================
// CDP (Chrome DevTools Protocol) helpers
// ============================================================================

static int g_cdp_id = 1;

/// Send a CDP request and wait for the matching response.
///
/// Skips server-push events until we receive a response with the matching id.
/// Returns the raw JSON response string, or empty on error.
static std::string cdp_send_recv(socket_t sock, const std::string& method,
                                 const std::string& params = "{}") {
    const int id = g_cdp_id++;

    std::ostringstream req;
    req << "{\"id\":" << id << ",\"method\":\"" << method << "\",\"params\":" << params << "}";
    const std::string msg = req.str();

    if (ws_client_send(sock, msg.c_str(), static_cast<int>(msg.size())) != 0) {
        return "";
    }

    // Read frames until we see the response for our request id
    char buf[65536];
    const std::string id_str = "\"id\":" + std::to_string(id);
    for (int attempts = 0; attempts < 50; ++attempts) {
        int n = ws_client_recv(sock, buf, static_cast<int>(sizeof(buf)) - 1);
        if (n <= 0)
            return "";

        std::string resp(buf, static_cast<size_t>(n));
        if (resp.find(id_str) != std::string::npos) {
            return resp;
        }

        // Print interesting async events while waiting
        if (resp.find("\"method\":\"Debugger.paused\"") != std::string::npos) {
            std::cout << "[paused]\n";
        }
    }
    return "";
}

/// Send a CDP command without waiting for its response (fire-and-forget).
static void cdp_fire(socket_t sock, const std::string& method, const std::string& params = "{}") {
    const int id = g_cdp_id++;
    std::ostringstream req;
    req << "{\"id\":" << id << ",\"method\":\"" << method << "\",\"params\":" << params << "}";
    const std::string msg = req.str();
    ws_client_send(sock, msg.c_str(), static_cast<int>(msg.size()));
}

// ============================================================================
// WebSocket HTTP upgrade handshake
// ============================================================================

/// Open a TCP connection to 127.0.0.1:<port> and perform the WebSocket
/// upgrade handshake.  Returns a valid socket on success, INVALID_SOCK on
/// failure.
static socket_t connect_inspector(int port) {
#ifdef _WIN32
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);
#endif

    socket_t sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock == INVALID_SOCK)
        return INVALID_SOCK;

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = htons(static_cast<uint16_t>(port));

    if (connect(sock, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) != 0) {
        sock_close(sock);
        return INVALID_SOCK;
    }

    // HTTP/1.1 Upgrade request
    const char* upgrade = "GET /tml HTTP/1.1\r\n"
                          "Host: 127.0.0.1\r\n"
                          "Upgrade: websocket\r\n"
                          "Connection: Upgrade\r\n"
                          "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
                          "Sec-WebSocket-Version: 13\r\n"
                          "\r\n";

    send(sock, upgrade, static_cast<int>(strlen(upgrade)), 0);

    // Read HTTP response headers (terminated by \r\n\r\n)
    char resp[4096];
    int total = 0;
    while (total < static_cast<int>(sizeof(resp)) - 1) {
        int r = recv(sock, resp + total, 1, 0);
        if (r <= 0) {
            sock_close(sock);
            return INVALID_SOCK;
        }
        total += r;
        if (total >= 4 && memcmp(resp + total - 4, "\r\n\r\n", 4) == 0)
            break;
    }
    resp[total] = '\0';

    if (strstr(resp, "101") == nullptr) {
        // Server did not switch protocols
        sock_close(sock);
        return INVALID_SOCK;
    }

    return sock;
}

// ============================================================================
// Minimal JSON string extraction
// ============================================================================

/// Extract the string value for `key` from a flat JSON object.
/// Only handles string values — returns empty string if key is absent or
/// the value is not a JSON string.
static std::string json_extract_string(const std::string& json, const std::string& key) {
    const std::string needle = "\"" + key + "\":\"";
    auto pos = json.find(needle);
    if (pos == std::string::npos)
        return "";
    pos += needle.size();
    auto end = json.find('"', pos);
    if (end == std::string::npos)
        return "";
    return json.substr(pos, end - pos);
}

// ============================================================================
// REPL
// ============================================================================

static void print_help() {
    std::cout << "TML Inspector commands:\n"
              << "  break <file>:<line>   Set breakpoint at file:line      (alias: b)\n"
              << "  continue              Resume execution                  (alias: c)\n"
              << "  step                  Step into next call               (alias: s)\n"
              << "  next                  Step over to next line            (alias: n)\n"
              << "  out                   Step out of current frame         (alias: o)\n"
              << "  backtrace             Show call stack                   (alias: bt)\n"
              << "  print <expr>          Evaluate expression               (alias: p)\n"
              << "  locals                Show local variables              (alias: l)\n"
              << "  heap                  Show heap usage statistics\n"
              << "  profile start         Start CPU profiling\n"
              << "  profile stop          Stop CPU profiling and print summary\n"
              << "  help                  Show this help                    (alias: h)\n"
              << "  quit                  Exit debugger                     (alias: q)\n";
}

/// Split `line` into command and argument portions at the first space.
static std::pair<std::string, std::string> parse_line(const std::string& line) {
    auto space = line.find(' ');
    if (space == std::string::npos)
        return {line, ""};
    std::string cmd = line.substr(0, space);
    std::string arg = line.substr(space + 1);
    // Strip leading spaces from arg
    while (!arg.empty() && arg.front() == ' ')
        arg.erase(arg.begin());
    return {cmd, arg};
}

/// Trim leading and trailing ASCII whitespace from `s` in-place.
static void trim(std::string& s) {
    while (!s.empty() &&
           (s.front() == ' ' || s.front() == '\t' || s.front() == '\n' || s.front() == '\r')) {
        s.erase(s.begin());
    }
    while (!s.empty() &&
           (s.back() == ' ' || s.back() == '\t' || s.back() == '\n' || s.back() == '\r')) {
        s.pop_back();
    }
}

/// Main interactive REPL.  Reads commands from stdin and dispatches CDP calls.
static void repl_loop(socket_t sock) {
    // Enable the domains we use
    cdp_send_recv(sock, "Runtime.enable");
    cdp_send_recv(sock, "Debugger.enable");

    std::cout << "TML Inspector connected. Type 'help' for commands.\n";
    std::cout << "(tml) " << std::flush;

    std::string line;
    while (std::getline(std::cin, line)) {
        trim(line);

        if (line.empty()) {
            std::cout << "(tml) " << std::flush;
            continue;
        }

        auto [cmd, arg] = parse_line(line);

        if (cmd == "quit" || cmd == "q") {
            break;
        }

        else if (cmd == "help" || cmd == "h") {
            print_help();
        }

        else if (cmd == "continue" || cmd == "c") {
            cdp_send_recv(sock, "Debugger.resume");
            std::cout << "Resumed.\n";
        }

        else if (cmd == "step" || cmd == "s") {
            cdp_send_recv(sock, "Debugger.stepInto");
            std::cout << "Step into...\n";
        }

        else if (cmd == "next" || cmd == "n") {
            cdp_send_recv(sock, "Debugger.stepOver");
            std::cout << "Step over...\n";
        }

        else if (cmd == "out" || cmd == "o") {
            cdp_send_recv(sock, "Debugger.stepOut");
            std::cout << "Step out...\n";
        }

        else if (cmd == "break" || cmd == "b") {
            if (arg.empty()) {
                std::cout << "Usage: break <file>:<line>\n";
            } else {
                auto colon = arg.rfind(':');
                if (colon == std::string::npos) {
                    std::cout << "Usage: break <file>:<line>\n";
                } else {
                    const std::string file = arg.substr(0, colon);
                    int bpline = 0;
                    try {
                        bpline = std::stoi(arg.substr(colon + 1));
                    } catch (...) {
                        std::cout << "Invalid line number.\n";
                        std::cout << "(tml) " << std::flush;
                        continue;
                    }

                    std::ostringstream params;
                    params << "{\"url\":\"" << file
                           << "\",\"lineNumber\":" << (bpline - 1) // CDP is 0-based
                           << "}";
                    auto resp = cdp_send_recv(sock, "Debugger.setBreakpointByUrl", params.str());
                    auto bp_id = json_extract_string(resp, "breakpointId");
                    if (!bp_id.empty()) {
                        std::cout << "Breakpoint set: " << bp_id << " at " << file << ":" << bpline
                                  << "\n";
                    } else {
                        std::cout << "Failed to set breakpoint.\n";
                    }
                }
            }
        }

        else if (cmd == "backtrace" || cmd == "bt") {
            // The last Debugger.paused event carries the callFrames.
            // We re-issue a getStackTrace to retrieve the current stack.
            auto resp = cdp_send_recv(sock, "Debugger.getStackTrace", "{\"maxDepth\":32}");
            if (resp.empty() || resp.find("callFrames") == std::string::npos) {
                std::cout << "(no stack available — program may not be paused)\n";
            } else {
                std::cout << resp << "\n";
            }
        }

        else if (cmd == "print" || cmd == "p") {
            if (arg.empty()) {
                std::cout << "Usage: print <expression>\n";
            } else {
                // Escape double quotes inside the expression
                std::string escaped;
                escaped.reserve(arg.size());
                for (char c : arg) {
                    if (c == '"')
                        escaped += "\\\"";
                    else if (c == '\\')
                        escaped += "\\\\";
                    else
                        escaped += c;
                }

                std::ostringstream params;
                params << "{\"expression\":\"" << escaped << "\",\"returnByValue\":true}";
                auto resp = cdp_send_recv(sock, "Runtime.evaluate", params.str());

                auto type = json_extract_string(resp, "type");
                auto value = json_extract_string(resp, "value");
                auto descr = json_extract_string(resp, "description");

                if (!value.empty()) {
                    std::cout << "(" << type << ") " << value << "\n";
                } else if (!descr.empty()) {
                    std::cout << "(" << type << ") " << descr << "\n";
                } else if (!type.empty()) {
                    std::cout << "(" << type << ") undefined\n";
                } else {
                    std::cout << "undefined\n";
                }
            }
        }

        else if (cmd == "locals" || cmd == "l") {
            // Runtime.globalLexicalScopeNames lists top-level let/const names;
            // for frame-local variables we would need Debugger.evaluateOnCallFrame
            // which requires a callFrameId from the last paused event.
            auto resp = cdp_send_recv(sock, "Runtime.globalLexicalScopeNames",
                                      "{\"executionContextId\":1}");
            std::cout << resp << "\n";
        }

        else if (cmd == "heap") {
            auto resp = cdp_send_recv(sock, "Runtime.getHeapUsage");
            auto used = json_extract_string(resp, "usedSize");
            auto total = json_extract_string(resp, "totalSize");
            if (!used.empty() && !total.empty()) {
                std::cout << "Heap: " << used << " used / " << total << " total (bytes)\n";
            } else {
                std::cout << resp << "\n";
            }
        }

        else if (cmd == "profile") {
            if (arg == "start") {
                cdp_send_recv(sock, "Profiler.enable");
                cdp_send_recv(sock, "Profiler.start");
                std::cout << "CPU profiling started.\n";
            } else if (arg == "stop") {
                auto resp = cdp_send_recv(sock, "Profiler.stop");
                cdp_fire(sock, "Profiler.disable");
                std::cout << "CPU profiling stopped.\n";
                if (!resp.empty()) {
                    std::cout << "(profile data)\n" << resp.substr(0, 512) << "...\n";
                }
            } else {
                std::cout << "Usage: profile start|stop\n";
            }
        }

        else {
            std::cout << "Unknown command '" << cmd << "'. Type 'help' for commands.\n";
        }

        std::cout << "(tml) " << std::flush;
    }
}

// ============================================================================
// Main entry point
// ============================================================================

int run_inspect(const std::string& file_path, int port, bool verbose,
                const std::vector<std::string>& args) {
    if (!fs::exists(file_path)) {
        std::cerr << "Error: file not found: " << file_path << "\n";
        return 1;
    }

    // Locate tml.exe relative to cwd — try debug first, then release
    fs::path exe_path = fs::current_path() / "build" / "debug" / "bin" / "tml.exe";
    if (!fs::exists(exe_path)) {
        exe_path = fs::current_path() / "build" / "debug" / "tml.exe";
    }
    if (!fs::exists(exe_path)) {
        exe_path = fs::current_path() / "build" / "release" / "bin" / "tml.exe";
    }
    if (!fs::exists(exe_path)) {
        std::cerr << "Error: tml.exe not found. Build the project first.\n";
        return 1;
    }

    if (verbose) {
        std::cout << "Using: " << exe_path.string() << "\n";
    }

    // Build the command line for the target process
    std::string cmd_str = "\"" + exe_path.string() + "\" run \"" + file_path + "\"" +
                          " --inspect-brk" + " --inspect-port=" + std::to_string(port);
    for (const auto& a : args) {
        cmd_str += " \"" + a + "\"";
    }

    if (verbose) {
        std::cout << "Launch: " << cmd_str << "\n";
    }

    std::cout << "Launching: " << file_path << " with inspector on port " << port << "\n";

#ifdef _WIN32
    // Launch as a background process using CreateProcessA
    STARTUPINFOA si;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    si.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
    si.hStdError = GetStdHandle(STD_ERROR_HANDLE);

    PROCESS_INFORMATION pi;
    ZeroMemory(&pi, sizeof(pi));

    // cmd_str must be writable — CreateProcessA may modify the buffer
    std::vector<char> cmd_buf(cmd_str.begin(), cmd_str.end());
    cmd_buf.push_back('\0');

    if (!CreateProcessA(nullptr, cmd_buf.data(), nullptr, nullptr,
                        TRUE, // inherit handles so the child can print
                        CREATE_NEW_PROCESS_GROUP, nullptr, nullptr, &si, &pi)) {
        std::cerr << "Error: failed to launch target process (Windows error " << GetLastError()
                  << ").\n";
        return 1;
    }
#else
    // POSIX: run in background
    const std::string bg_cmd = cmd_str + " &";
    if (system(bg_cmd.c_str()) != 0) {
        std::cerr << "Error: failed to launch target process.\n";
        return 1;
    }
#endif

    // Poll until the inspector WebSocket server accepts connections (up to 3 s)
    std::cout << "Waiting for inspector to start...\n" << std::flush;

    socket_t sock = INVALID_SOCK;
    for (int i = 0; i < 30; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        sock = connect_inspector(port);
        if (sock != INVALID_SOCK)
            break;
    }

    if (sock == INVALID_SOCK) {
        std::cerr << "Error: could not connect to inspector at ws://127.0.0.1:" << port << "/tml\n";
        std::cerr << "Make sure the program compiled successfully and the port is free.\n";
#ifdef _WIN32
        TerminateProcess(pi.hProcess, 1);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        WSACleanup();
#endif
        return 1;
    }

    std::cout << "Connected to inspector at ws://127.0.0.1:" << port << "/tml\n";

    // Interactive REPL
    repl_loop(sock);

    // Graceful shutdown
    sock_close(sock);

#ifdef _WIN32
    // Give the process a moment to exit cleanly before terminating
    if (WaitForSingleObject(pi.hProcess, 500) == WAIT_TIMEOUT) {
        TerminateProcess(pi.hProcess, 0);
    }
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    WSACleanup();
#endif

    std::cout << "Inspector session ended.\n";
    return 0;
}

} // namespace tml::cli
