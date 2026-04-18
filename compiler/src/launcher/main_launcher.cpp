TML_MODULE("launcher")

//! # Thin Launcher Entry Point (Modular Build)
//!
//! This is the entry point for the modular build of `tml.exe`.
//! It is intentionally tiny (~500KB without LLVM) and performs:
//!
//! 1. Minimal command parsing (just argv[1])
//! 2. `--help` / `--version` handled locally (no plugins needed)
//! 3. Daemon forwarding if TML_DAEMON=1 (no DLL load — pure IPC)
//! 4. Everything else: load the compiler plugin and delegate
//!
//! The compiler plugin itself loads codegen/tools/test plugins on demand.

#include "common/crc32c.hpp"
#include "plugin/abi.h"
#include "plugin/loader.hpp"
#include "version_generated.hpp"  // tml::VERSION (CMake-generated from /VERSION)

#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#endif

// ============================================================================
// Thin daemon client — runs BEFORE loading any compiler DLL
// ============================================================================

namespace {

namespace fs = std::filesystem;

// CRC32c provided by common/crc32c.hpp — use tml::crc32c() directly.
#if 0  // removed: was using wrong CRC table; replaced with tml::crc32c() from crc32c.hpp
static uint32_t crc32c_compute_old(const char* data, size_t len) {
    static const uint32_t kTable[256] = {
        0x00000000,0xF26B8303,0xE13B70F7,0x1350F3F4,0xC79A971F,0x35F1141C,
        0x26A1E7E8,0xD4CA64EB,0x8AD958CF,0x78B2DBCC,0x6BE22838,0x9989AB3B,
        0x4D43CFD0,0xBF284CD3,0xAC78BF27,0x5E133C24,0x105EC76F,0xE235446C,
        0xF165B798,0x030E349B,0xD7C45070,0x25AFD373,0x36FF2087,0xC494A384,
        0x9A879FA0,0x68EC1CA3,0x7BBCEF57,0x89D76C54,0x5D1D08BF,0xAF768BBC,
        0xBC267848,0x4E4DFB4B,0x20BD8EDE,0xD2D60DDD,0xC186FE29,0x33ED7D2A,
        0xE72719C1,0x154C9AC2,0x061C6936,0xF477EA35,0xAA64D611,0x580F5512,
        0x4B5FA6E6,0xB93425E5,0x6DFE410E,0x9F95C20D,0x8CC531F9,0x7EAEB2FA,
        0x30E349B1,0xC288CAB2,0xD1D83946,0x23B3BA45,0xF779DEAE,0x05125DAD,
        0x1642AE59,0xE4292D5A,0xBA3A117E,0x4851927D,0x5B016189,0xA96AE28A,
        0x7DA08661,0x8FCB0562,0x9C9BF696,0x6EF07595,0x417B1DBC,0xB3109EBF,
        0xA0406D4B,0x522BEE48,0x86E18AA3,0x748A09A0,0x67DAFA54,0x95B17957,
        0xCBA24573,0x39C9C670,0x2A993584,0xD8F2B687,0x0C38D26C,0xFE53516F,
        0xED03A29B,0x1F682198,0x5125DAD3,0xA34E59D0,0xB01EAA24,0x42752927,
        0x96BF4DCC,0x64D4CECF,0x77843D3B,0x85EFBE38,0xDBFC821C,0x2997011F,
        0x3AC7F2EB,0xC8AC71E8,0x1C661503,0xEE0D9600,0xFD5D65F4,0x0F36E6F7,
        0x61C69362,0x93AD1061,0x80FDE395,0x72966096,0xA65C047D,0x5437877E,
        0x4767748A,0xB50CF789,0xEB1FCBAD,0x197448AE,0x0A24BB5A,0xF84F3859,
        0x2C855CB2,0xDEEEDFB1,0xCDBE2C45,0x3FD5AF46,0x7198540D,0x83F3D70E,
        0x90A324FA,0x62C8A7F9,0xB602C312,0x44694011,0x5739B3E5,0xA55230E6,
        0xFB410CC2,0x092A8FC1,0x1A7A7C35,0xE811FF36,0x3CDB9BDD,0xCEB018DE,
        0xDDE0EB2A,0x2F8B6829,0x82F63B78,0x709DB87B,0x63CD4B8F,0x91A6C88C,
        0x456CAC67,0xB7072F64,0xA457DC90,0x563C5F93,0x082F63B7,0xFA44E0B4,
        0xE9141340,0x1B7F9043,0xCFB5F4A8,0x3DDE77AB,0x2E8E845F,0xDCE5075C,
        0x92A8FC17,0x60C37F14,0x7393BCE0,0x81F83FE3,0x55325B08,0xA759D80B,
        0xB4092BFF,0x4662A8FC,0x187194D8,0xEA1A17DB,0xF94AE42F,0x0B21672C,
        0xDFEB03C7,0x2D8080C4,0x3ED07330,0xCCBBF033,0xA24B85A6,0x502006A5,
        0x4370F551,0xB11B7652,0x65D112B9,0x97BA91BA,0x84EA624E,0x7681E14D,
        0x2892DD69,0xDAF95E6A,0xC9A9AD9E,0x3BC22E9D,0xEF084A76,0x1D63C975,
        0x0E333A81,0xFC58B982,0xB21542C9,0x407EC1CA,0x532E323E,0xA145B13D,
        0x758FD5D6,0x87E456D5,0x94B4A521,0x66DF2622,0x38CC1A06,0xCAA79905,
        0xD9F76AF1,0x2B9CE9F2,0xFF568D19,0x0D3D0E1A,0x1E6DFDEE,0xEC067EED,
        0xC38D26C4,0x31E6A5C7,0x22B65633,0xD0DDD530,0x0417B1DB,0xF67C32D8,
        0xE52CC12C,0x1747422F,0x49547E0B,0xBB3FFD08,0xA86F0EFC,0x5A048DFF,
        0x8ECEE914,0x7CA56A17,0x6FF599E3,0x9D9E1AE0,0xD3D3E1AB,0x21B862A8,
        0x32E8915C,0xC083125F,0x144976B4,0xE622F5B7,0xF5720643,0x07198540,
        0x590AB964,0xAB613A67,0xB831C993,0x4A5A4A90,0x9E902E7B,0x6CFBAD78,
        0x7FAB5E8C,0x8DC0DD8F,0xE330A81A,0x115B2B19,0x020BD8ED,0xF0605BEE,
        0x24AA3F05,0xD6C1BC06,0xC5914FF2,0x37FACCF1,0x69E9F0D5,0x9B8273D6,
        0x88D28022,0x7AB90321,0xAE7367CA,0x5C18E4C9,0x4F48173D,0xBD23943E,
        0xF36E6F75,0x0105EC76,0x12551F82,0xE03E9C81,0x34F4F86A,0xC69F7B69,
        0xD5CF889D,0x27A40B9E,0x79B737BA,0x8BDCB4B9,0x988C474D,0x6AE7C44E,
        0xBE2DA0A5,0x4C4623A6,0x5F16D052,0xAD7D5351
    };
    uint32_t crc = 0xFFFFFFFF;
    for (size_t i = 0; i < len; ++i)
        crc = (crc >> 8) ^ kTable[(crc ^ static_cast<uint8_t>(data[i])) & 0xFF];
    return ~crc;
}
#endif // deleted crc32c_compute_old

static std::string json_escape_simple(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 4);
    for (char c : s) {
        switch (c) {
        case '"':
            out += "\\\"";
            break;
        case '\\':
            out += "\\\\";
            break;
        case '\n':
            out += "\\n";
            break;
        case '\r':
            out += "\\t";
            break;
        default:
            out += c;
            break;
        }
    }
    return out;
}

static int json_get_exit(const std::string& json) {
    auto pos = json.find("\"exit\":");
    if (pos == std::string::npos)
        return -1;
    pos += 7;
    while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\t'))
        ++pos;
    return std::stoi(json.substr(pos));
}

static std::string json_get_str_field(const std::string& json, const std::string& key) {
    auto k = '"' + key + "\":\"";
    auto pos = json.find(k);
    if (pos == std::string::npos)
        return {};
    pos += k.size();
    std::string out;
    while (pos < json.size() && json[pos] != '"') {
        if (json[pos] == '\\' && pos + 1 < json.size()) {
            ++pos;
            switch (json[pos]) {
            case '"':
                out += '"';
                break;
            case '\\':
                out += '\\';
                break;
            case 'n':
                out += '\n';
                break;
            case 't':
                out += '\t';
                break;
            default:
                out += json[pos];
                break;
            }
        } else {
            out += json[pos];
        }
        ++pos;
    }
    return out;
}

/// Attempt to forward this invocation to a running daemon.
/// Returns the exit code (0+) on success, or -1 if the daemon is not running
/// or the pipe connection failed (caller should fall through to normal DLL path).
static int try_daemon_forward_launcher(int argc, char* argv[]) {
#ifndef _WIN32
    return -1; // Only implemented for Windows named pipes currently
#else
    // Check TML_DAEMON env var
    const char* daemon_env = std::getenv("TML_DAEMON");
    if (!daemon_env || std::string(daemon_env) != "1")
        return -1;

    // Check command is forwardable
    if (argc < 2)
        return -1;
    std::string cmd = argv[1];
    if (cmd != "build" && cmd != "run" && cmd != "check")
        return -1;

    // Check PID file in system temp dir (keyed by CRC32 of CWD)
    std::error_code ec;
    auto cwd = fs::current_path(ec);
    if (ec)
        return -1;
    std::string cwd_str = cwd.string();
    uint32_t cwd_crc = tml::crc32c(cwd_str);
    char crc_hex[16];
    snprintf(crc_hex, sizeof(crc_hex), "%08x", cwd_crc);
    auto pid_path = fs::temp_directory_path(ec) / (std::string("tml-daemon-") + crc_hex + ".pid");
    if (ec || !fs::exists(pid_path, ec))
        return -1;

    // Read PID via Win32 ReadFile.
    // NOTE: std::ifstream >> on Windows can block indefinitely when another
    // process holds the file in certain states. Win32 ReadFile is always
    // non-blocking for local files opened with FILE_ATTRIBUTE_NORMAL.
    char pid_buf[32] = {};
    {
        HANDLE hPid = CreateFileW(pid_path.wstring().c_str(), GENERIC_READ,
                                  FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
                                  OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (hPid == INVALID_HANDLE_VALUE)
            return -1;
        DWORD nread = 0;
        ReadFile(hPid, pid_buf, sizeof(pid_buf) - 1, &nread, nullptr);
        CloseHandle(hPid);
    }
    uint32_t pid = static_cast<uint32_t>(std::strtoul(pid_buf, nullptr, 10));
    if (!pid)
        return -1;

    // Check process alive
    HANDLE hProc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!hProc)
        return -1;
    DWORD code = 0;
    bool alive = GetExitCodeProcess(hProc, &code) && code == STILL_ACTIVE;
    CloseHandle(hProc);
    if (!alive)
        return -1;

    // Compute pipe name — must match cmd_daemon.cpp::make_pipe_name()
    // cwd_str and cwd_crc already computed above for PID path
    std::wstring pipe_name = L"\\\\.\\pipe\\tml-daemon-";
    pipe_name += std::wstring(crc_hex, crc_hex + 8);

    // Build JSON argv (skip argv[0] = exe name, daemon prepends fake exe)
    std::string argv_json = "[";
    for (int i = 1; i < argc; ++i) {
        if (i > 1)
            argv_json += ',';
        argv_json += '"';
        argv_json += json_escape_simple(argv[i]);
        argv_json += '"';
    }
    argv_json += ']';

    std::string cwd_esc = json_escape_simple(cwd_str);
    std::string request =
        "{\"id\":1,\"cmd\":\"forward\",\"argv\":" + argv_json + ",\"cwd\":\"" + cwd_esc + "\"}\n";

    // Connect to named pipe (retry a few times if busy)
    HANDLE hPipe = INVALID_HANDLE_VALUE;
    for (int attempt = 0; attempt < 5; ++attempt) {
        hPipe = CreateFileW(pipe_name.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr,
                            OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (hPipe != INVALID_HANDLE_VALUE)
            break;
        if (GetLastError() == ERROR_PIPE_BUSY) {
            WaitNamedPipeW(pipe_name.c_str(), 2000);
        } else {
            break;
        }
    }
    if (hPipe == INVALID_HANDLE_VALUE)
        return -1;

    DWORD mode = PIPE_READMODE_MESSAGE;
    SetNamedPipeHandleState(hPipe, &mode, nullptr, nullptr);

    // Send request
    DWORD written = 0;
    if (!WriteFile(hPipe, request.data(), static_cast<DWORD>(request.size()), &written, nullptr)) {
        CloseHandle(hPipe);
        return -1;
    }

    // Read response (may be large)
    std::string response;
    char buf[1 << 16];
    while (true) {
        DWORD nread = 0;
        BOOL ok = ReadFile(hPipe, buf, sizeof(buf), &nread, nullptr);
        if (nread > 0)
            response.append(buf, nread);
        if (!ok) {
            DWORD err = GetLastError();
            if (err == ERROR_MORE_DATA)
                continue;
            break;
        }
        break;
    }
    CloseHandle(hPipe);

    if (response.empty())
        return -1;

    // Print captured output
    std::string out_str = json_get_str_field(response, "out");
    std::string err_str = json_get_str_field(response, "err");
    if (!out_str.empty())
        std::cout << out_str;
    if (!err_str.empty())
        std::cerr << err_str;

    int exit_code = json_get_exit(response);
    return (exit_code >= 0) ? exit_code : 0;
#endif
}

} // anonymous namespace

// Version comes from compiler/include/version_generated.hpp
// which CMake materializes from the /VERSION file on every build.
#define TML_VERSION tml::VERSION

// Function pointer type for the compiler's main entry point
typedef int (*CompilerMainFn)(int argc, char* argv[]);

static void print_usage() {
    std::cout << "TML Compiler " << TML_VERSION << " (modular)\n"
              << "\n"
              << "Usage: tml <command> [options]\n"
              << "\n"
              << "Commands:\n"
              << "  build   <file>    Compile a TML source file\n"
              << "  run     <file>    Build and run immediately\n"
              << "  check   <file>    Type check without codegen\n"
              << "  test              Run tests\n"
              << "  fmt     <file>    Format source code\n"
              << "  lint    <file>    Lint source code\n"
              << "  lex     <file>    Show lexer tokens\n"
              << "  parse   <file>    Show parse tree\n"
              << "  init              Initialize a new project\n"
              << "  mcp               Start MCP server\n"
              << "  explain <code>    Explain an error code\n"
              << "\n"
              << "Global Flags:\n"
              << "  --help, -h        Show this help\n"
              << "  --version, -V     Show version\n"
              << "  --verbose, -v     Enable verbose output\n"
              << "\n"
              << "Test Command (tml test [PATTERN...]):\n"
              << "  --backend=llvm|cranelift        Select compilation backend\n"
              << "  --out-dir=/path                 Output test DLLs to custom directory\n"
              << "  --emit-pipeline[=/path]         Emit compilation pipeline stages (MIR/IR)\n"
              << "  --coverage                      Generate code coverage report\n"
              << "  --release                       Run tests in release mode\n"
              << "  --no-suite                      1 DLL per test (vs. 8 per suite)\n"
              << "  --no-cache                      Force full recompilation\n"
              << "  --verbose                       Show detailed compiler output\n"
              << "  --nocapture                     Show stdout/stderr during tests\n"
              << "  --filter=PATTERN                Run only tests matching pattern\n";
}

static void print_version() {
    std::cout << "tml " << TML_VERSION << " (modular)\n";
}

int main(int argc, char* argv[]) {
    // No arguments → show help (no DLL needed)
    if (argc < 2) {
        print_usage();
        return 0;
    }

    std::string command = argv[1];

    // Handle --help and --version without loading any plugin.
    if (command == "--help" || command == "-h") {
        print_usage();
        return 0;
    }
    if (command == "--version" || command == "-V") {
        print_version();
        return 0;
    }

    // Daemon forwarding: if TML_DAEMON=1 and a daemon is running for this
    // directory, forward the request over the named pipe WITHOUT loading any
    // DLL. This is the sub-10ms incremental-compile path.
    {
        int daemon_result = try_daemon_forward_launcher(argc, argv);
        if (daemon_result >= 0)
            return daemon_result;
        // -1 means daemon not running or not forwardable → fall through
    }

    // Real command: start loading the compiler DLL in the background NOW.
    // The Loader constructor calls wait_preload_done(), so any remaining
    // argument parsing in the compiler runs in parallel with the DLL load.
    // On warm OS page cache, LoadLibrary completes before the Loader is
    // even constructed, giving a near-zero DLL overhead.
    tml::plugin::Loader::preload_async({"tml_compiler"});

    // Everything else requires the compiler plugin.
    // The Loader constructor calls wait_preload_done(), so by the time
    // loader.load("tml_compiler") runs, the DLL is already in g_dll_cache.
    tml::plugin::Loader loader;

    if (!loader.load("tml_compiler")) {
        std::cerr << "error: failed to load compiler plugin (tml_compiler)\n";
        std::cerr << "  Searched: " << loader.plugins_dir().string() << "\n";
        std::cerr << "  Set TML_PLUGIN_DIR to the directory containing plugin DLLs.\n";
        return 1;
    }

    // Get the compiler's main entry point
    auto* plugin = loader.get("tml_compiler");
    if (!plugin || !plugin->handle) {
        std::cerr << "error: compiler plugin loaded but handle is null\n";
        return 1;
    }

    // Look up the exported compiler_main function
    auto compiler_main = reinterpret_cast<CompilerMainFn>(
        tml::plugin::Loader::get_symbol(plugin->handle, "compiler_main"));

    if (!compiler_main) {
        std::cerr << "error: compiler plugin does not export 'compiler_main'\n";
        return 1;
    }

    // Delegate to the compiler
    int result = compiler_main(argc, argv);

    // Cleanup
    loader.unload_all();

    return result;
}
