//! # Test Runner Execution Infrastructure
//!
//! DynamicLibrary implementation, OutputCapture, crash handlers, logger bridge,
//! and test execution functions (run_test_in_process, run_suite_test).

#include "test_runner_internal.hpp"

namespace tml::cli {

// ============================================================================
// Thread Count Calculation
// ============================================================================

unsigned int calc_codegen_threads(unsigned int task_count) {
    unsigned int hw = std::thread::hardware_concurrency();
    if (hw == 0)
        hw = 8; // Fallback for unknown hardware
    // Budget: ~33% of cores per suite (3 suites compile in parallel),
    // clamped to [2, 4]. Total across 3 suites: 6-12 threads.
    unsigned int per_suite = hw / 3;
    unsigned int clamped = std::clamp(per_suite, 2u, 4u);
    return std::min(clamped, task_count);
}

// ============================================================================
// C Runtime Logger Bridge
// ============================================================================

void rt_log_bridge_callback(int level, const char* module, const char* message) {
    auto cpp_level = static_cast<tml::log::LogLevel>(level);
    tml::log::Logger::instance().log(cpp_level, module ? module : "runtime", message ? message : "",
                                     nullptr, 0);
}

// ============================================================================
// Windows Crash Handler
// ============================================================================

#ifdef _WIN32

thread_local char g_crash_msg[1024] = {0};
thread_local bool g_crash_occurred = false;

// SEH fallback exception name lookup.
// NOTE: The canonical version is tml_get_exception_name() in essential.c (exported from DLL).
// This is kept as a fallback for when the DLL's VEH handler doesn't catch first (rare).
const char* get_exception_name(DWORD code) {
    switch (code) {
    case EXCEPTION_ACCESS_VIOLATION:
        return "ACCESS_VIOLATION";
    case EXCEPTION_ILLEGAL_INSTRUCTION:
        return "ILLEGAL_INSTRUCTION";
    case EXCEPTION_INT_DIVIDE_BY_ZERO:
        return "INTEGER_DIVIDE_BY_ZERO";
    case EXCEPTION_INT_OVERFLOW:
        return "INTEGER_OVERFLOW";
    case EXCEPTION_STACK_OVERFLOW:
        return "STACK_OVERFLOW";
    case EXCEPTION_FLT_DIVIDE_BY_ZERO:
        return "FLOAT_DIVIDE_BY_ZERO";
    case EXCEPTION_FLT_INVALID_OPERATION:
        return "FLOAT_INVALID_OPERATION";
    case 0xC0000028:
        return "BAD_STACK";
    case 0xC0000374:
        return "HEAP_CORRUPTION";
    case 0xC0000409:
        return "STACK_BUFFER_OVERRUN";
    default:
        return "UNKNOWN_EXCEPTION";
    }
}

LONG WINAPI crash_filter(EXCEPTION_POINTERS* info) {
    DWORD code = info->ExceptionRecord->ExceptionCode;

    // Get crash context (test name, file, suite, phase) set before each test
    const char* phase = nullptr;
    const char* suite = nullptr;
    const char* test_name = nullptr;
    const char* test_file = nullptr;
    get_crash_context(&phase, &suite, &test_name, &test_file);

    // Format crash message with full test context + diagnostics
    int len = 0;
    len += snprintf(g_crash_msg + len, sizeof(g_crash_msg) - len, "CRASH: %s (0x%08lX)",
                    get_exception_name(code), (unsigned long)code);

    // ACCESS_VIOLATION: include fault address and read/write/execute
    if (code == EXCEPTION_ACCESS_VIOLATION && info->ExceptionRecord->NumberParameters >= 2) {
        ULONG_PTR op = info->ExceptionRecord->ExceptionInformation[0];
        ULONG_PTR fault_addr = info->ExceptionRecord->ExceptionInformation[1];
        const char* op_str = (op == 0) ? "READ" : (op == 1) ? "WRITE" : "EXECUTE";
        len += snprintf(g_crash_msg + len, sizeof(g_crash_msg) - len, " [%s at 0x%016llX]", op_str,
                        (unsigned long long)fault_addr);
    }

    // RIP (where the crash occurred)
#ifdef _M_X64
    len += snprintf(g_crash_msg + len, sizeof(g_crash_msg) - len, " RIP=0x%016llX",
                    (unsigned long long)info->ContextRecord->Rip);
#endif

    // Test context
    len += snprintf(g_crash_msg + len, sizeof(g_crash_msg) - len,
                    " in test \"%s\" [%s] (suite: %s, phase: %s)",
                    test_name ? test_name : "(unknown)", test_file ? test_file : "(unknown)",
                    suite ? suite : "(unknown)", phase ? phase : "(unknown)");
    g_crash_occurred = true;

    // Print to stderr immediately using low-level API for reliability
    HANDLE hErr = GetStdHandle(STD_ERROR_HANDLE);
    DWORD written;
    WriteFile(hErr, g_crash_msg, (DWORD)(len > 0 ? len : (int)strlen(g_crash_msg)), &written, NULL);
    WriteFile(hErr, "\n", 1, &written, NULL);
    FlushFileBuffers(hErr);

    return EXCEPTION_EXECUTE_HANDLER;
}

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wlanguage-extension-token"
#endif
int call_test_with_seh(TestMainFunc func) {
    g_crash_occurred = false;
    g_crash_msg[0] = '\0';

    int result = 0;
    __try {
        result = func();
    } __except (crash_filter(GetExceptionInformation())) {
        return -2;
    }
    return result;
}

int call_run_with_catch_seh(TmlRunTestWithCatchFn run_with_catch, TestMainFunc test_func) {
    g_crash_occurred = false;
    g_crash_msg[0] = '\0';

    int result = 0;
    __try {
        result = run_with_catch(test_func);
    } __except (crash_filter(GetExceptionInformation())) {
        return -2;
    }
    return result;
}
#ifdef __clang__
#pragma clang diagnostic pop
#endif

#endif // _WIN32

// ============================================================================
// OutputCapture Implementation
// ============================================================================

bool OutputCapture::start() {
    if (capturing_)
        return true;

    // Create a temporary file for capturing output
    temp_file_path_ = get_run_cache_dir() / ("capture_" + std::to_string(std::time(nullptr)) + "_" +
                                             std::to_string(rand()) + ".tmp");

    // Sync C++ streams with C stdio and flush all buffers
    std::ios_base::sync_with_stdio(true);
    std::cout << std::flush;
    std::cerr << std::flush;
    std::fflush(stdout);
    std::fflush(stderr);

#ifdef _WIN32
    // Save original stdout/stderr file descriptors
    saved_stdout_ = _dup(_fileno(stdout));
    saved_stderr_ = _dup(_fileno(stderr));

    if (saved_stdout_ < 0 || saved_stderr_ < 0) {
        return false;
    }

    // Open temp file for capturing output
    int temp_fd = -1;
    errno_t err = _sopen_s(&temp_fd, temp_file_path_.string().c_str(),
                           _O_WRONLY | _O_CREAT | _O_TRUNC, _SH_DENYNO, _S_IREAD | _S_IWRITE);
    if (err != 0 || temp_fd < 0) {
        _close(saved_stdout_);
        _close(saved_stderr_);
        saved_stdout_ = -1;
        saved_stderr_ = -1;
        return false;
    }

    // Redirect stdout/stderr to temp file
    _dup2(temp_fd, _fileno(stdout));
    _dup2(temp_fd, _fileno(stderr));
    _close(temp_fd);
#else
    // Save original stdout/stderr file descriptors
    saved_stdout_ = dup(STDOUT_FILENO);
    saved_stderr_ = dup(STDERR_FILENO);

    if (saved_stdout_ < 0 || saved_stderr_ < 0) {
        return false;
    }

    // Open temp file for capturing output
    int temp_fd = open(temp_file_path_.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (temp_fd < 0) {
        close(saved_stdout_);
        close(saved_stderr_);
        saved_stdout_ = -1;
        saved_stderr_ = -1;
        return false;
    }

    // Redirect stdout/stderr to temp file
    dup2(temp_fd, STDOUT_FILENO);
    dup2(temp_fd, STDERR_FILENO);
    close(temp_fd);
#endif

    capturing_ = true;
    return true;
}

std::string OutputCapture::stop() {
    if (!capturing_)
        return "";

    // Ensure C++ streams are synced and flushed
    std::ios_base::sync_with_stdio(true);
    std::cout << std::flush;
    std::cerr << std::flush;
    std::fflush(stdout);
    std::fflush(stderr);

#ifdef _WIN32
    // Restore original stdout/stderr
    _dup2(saved_stdout_, _fileno(stdout));
    _dup2(saved_stderr_, _fileno(stderr));
    _close(saved_stdout_);
    _close(saved_stderr_);
#else
    // Restore original stdout/stderr
    dup2(saved_stdout_, STDOUT_FILENO);
    dup2(saved_stderr_, STDERR_FILENO);
    close(saved_stdout_);
    close(saved_stderr_);
#endif

    saved_stdout_ = -1;
    saved_stderr_ = -1;
    capturing_ = false;

    // Read the captured output from the temp file
    std::ifstream file(temp_file_path_);
    if (file) {
        std::stringstream buffer;
        buffer << file.rdbuf();
        captured_output_ = buffer.str();
    }

    return captured_output_;
}

void OutputCapture::cleanup() {
    if (!temp_file_path_.empty() && fs::exists(temp_file_path_)) {
        try {
            fs::remove(temp_file_path_);
        } catch (...) {
            // Ignore cleanup errors
        }
    }
}

// ============================================================================
// DynamicLibrary Implementation
// ============================================================================

DynamicLibrary::~DynamicLibrary() {
    unload();
}

DynamicLibrary::DynamicLibrary(DynamicLibrary&& other) noexcept
    : handle_(other.handle_), error_(std::move(other.error_)) {
    other.handle_ = nullptr;
}

DynamicLibrary& DynamicLibrary::operator=(DynamicLibrary&& other) noexcept {
    if (this != &other) {
        unload();
        handle_ = other.handle_;
        error_ = std::move(other.error_);
        other.handle_ = nullptr;
    }
    return *this;
}

bool DynamicLibrary::load(const std::string& path) {
    unload();
    error_.clear();

#ifdef _WIN32
    // Convert to absolute path for faster loading
    fs::path abs_path = fs::absolute(path);
    std::wstring wpath = abs_path.wstring();

    // Add vcpkg bin directories to DLL search path for dependencies (zstd, brotli, zlib)
    // This is needed because the test DLL depends on these libraries
    static bool vcpkg_paths_added = false;
    if (!vcpkg_paths_added) {
        vcpkg_paths_added = true;
        // Add vcpkg bin directory for DLL dependencies (OpenSSL, zlib, etc.)
        fs::path project_root = build::find_project_root();
        fs::path vcpkg_bin = project_root / "vcpkg_installed" / "x64-windows" / "bin";
        if (fs::exists(vcpkg_bin)) {
            fs::path abs_vcpkg_path = fs::absolute(vcpkg_bin);
            AddDllDirectory(abs_vcpkg_path.wstring().c_str());
        }
    }

    // Use LoadLibraryExW with optimized flags:
    // - LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR: Search only the DLL's directory for dependencies
    // - LOAD_LIBRARY_SEARCH_DEFAULT_DIRS: Also search system directories
    // - LOAD_LIBRARY_SEARCH_USER_DIRS: Search directories added with AddDllDirectory
    // This avoids searching the entire PATH which can be slow
    handle_ = LoadLibraryExW(wpath.c_str(), nullptr,
                             LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS |
                                 LOAD_LIBRARY_SEARCH_USER_DIRS);
    if (!handle_) {
        // Fallback to regular LoadLibrary if the optimized version fails
        // (e.g., on older Windows versions)
        handle_ = LoadLibraryW(wpath.c_str());
        if (!handle_) {
            DWORD err = GetLastError();
            error_ = "LoadLibrary failed with error code " + std::to_string(err);
            return false;
        }
    }
#else
    handle_ = dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL);
    if (!handle_) {
        const char* err = dlerror();
        error_ = err ? err : "Unknown dlopen error";
        return false;
    }
#endif

    return true;
}

void DynamicLibrary::unload() {
    if (handle_) {
        // If coverage is enabled, write profile data before unloading
        // __llvm_profile_write_file() is provided by the LLVM profile runtime
        if (CompilerOptions::coverage_source) {
#ifdef _WIN32
            auto write_profile =
                reinterpret_cast<int (*)()>(GetProcAddress(handle_, "__llvm_profile_write_file"));
#else
            auto write_profile =
                reinterpret_cast<int (*)()>(dlsym(handle_, "__llvm_profile_write_file"));
#endif
            if (write_profile) {
                write_profile();
            }
        }

#ifdef _WIN32
        FreeLibrary(handle_);
#else
        dlclose(handle_);
#endif
        handle_ = nullptr;
    }
}

bool DynamicLibrary::is_loaded() const {
    return handle_ != nullptr;
}

void* DynamicLibrary::get_symbol(const std::string& name) const {
    if (!handle_) {
        return nullptr;
    }

#ifdef _WIN32
    return reinterpret_cast<void*>(GetProcAddress(handle_, name.c_str()));
#else
    return dlsym(handle_, name.c_str());
#endif
}

// ============================================================================
// Run Test In-Process
// ============================================================================

InProcessTestResult run_test_in_process(const std::string& lib_path) {
    using Clock = std::chrono::high_resolution_clock;
    InProcessTestResult result;

    // Load the shared library
    DynamicLibrary lib;
    if (!lib.load(lib_path)) {
        result.error = "Failed to load shared library: " + lib.get_error();
        return result;
    }

    // Get the test entry function
    auto test_entry = lib.get_function<TestMainFunc>("tml_test_entry");
    if (!test_entry) {
        result.error = "Failed to find tml_test_entry in shared library";
        return result;
    }

    // Route C runtime log messages through the C++ Logger
    using RtLogSetCallback = void (*)(void (*)(int, const char*, const char*));
    auto set_log_callback = lib.get_function<RtLogSetCallback>("rt_log_set_callback");
    if (set_log_callback) {
        set_log_callback(rt_log_bridge_callback);
    }
    using RtLogSetLevel = void (*)(int);
    auto set_log_level = lib.get_function<RtLogSetLevel>("rt_log_set_level");
    if (set_log_level) {
        set_log_level(static_cast<int>(tml::log::Logger::instance().level()));
    }

    // Set up output capture
    OutputCapture capture;
    bool capture_started = capture.start();

    // Execute the test
    auto start = Clock::now();

    try {
        result.exit_code = test_entry();
        result.success = (result.exit_code == 0);
    } catch (...) {
        result.error = "Exception during test execution";
        result.exit_code = 1;
    }

    auto end = Clock::now();
    result.duration_us = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();

    // Stop capturing and retrieve output
    if (capture_started) {
        result.output = capture.stop();
    }

    return result;
}

// ============================================================================
// Run Test In-Process with Sub-Phase Profiling
// ============================================================================

InProcessTestResult run_test_in_process_profiled(const std::string& lib_path,
                                                 PhaseTimings* timings) {
    using Clock = std::chrono::high_resolution_clock;
    auto record_phase = [&](const std::string& phase, Clock::time_point start) {
        if (timings) {
            auto end = Clock::now();
            timings->timings_us[phase] =
                std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
        }
    };

    InProcessTestResult result;

    // Phase: Load the shared library
    auto phase_start = Clock::now();
    DynamicLibrary lib;
    if (!lib.load(lib_path)) {
        result.error = "Failed to load shared library: " + lib.get_error();
        record_phase("exec.load_lib", phase_start);
        return result;
    }
    record_phase("exec.load_lib", phase_start);

    // Phase: Get the test entry function
    phase_start = Clock::now();
    auto test_entry = lib.get_function<TestMainFunc>("tml_test_entry");
    if (!test_entry) {
        result.error = "Failed to find tml_test_entry in shared library";
        record_phase("exec.get_symbol", phase_start);
        return result;
    }
    record_phase("exec.get_symbol", phase_start);

    // Route C runtime log messages through the C++ Logger
    {
        using RtLogSetCallback = void (*)(void (*)(int, const char*, const char*));
        auto set_log_callback = lib.get_function<RtLogSetCallback>("rt_log_set_callback");
        if (set_log_callback) {
            set_log_callback(rt_log_bridge_callback);
        }
        using RtLogSetLevel = void (*)(int);
        auto set_log_level = lib.get_function<RtLogSetLevel>("rt_log_set_level");
        if (set_log_level) {
            set_log_level(static_cast<int>(tml::log::Logger::instance().level()));
        }
    }

    // Phase: Set up output capture
    phase_start = Clock::now();
    OutputCapture capture;
    bool capture_started = capture.start();
    record_phase("exec.capture_start", phase_start);

    // Phase: Execute the test
    phase_start = Clock::now();
    try {
        result.exit_code = test_entry();
        result.success = (result.exit_code == 0);
    } catch (...) {
        result.error = "Exception during test execution";
        result.exit_code = 1;
    }
    auto end = Clock::now();
    result.duration_us =
        std::chrono::duration_cast<std::chrono::microseconds>(end - phase_start).count();
    record_phase("exec.run", phase_start);

    // Phase: Stop capturing and retrieve output
    phase_start = Clock::now();
    if (capture_started) {
        result.output = capture.stop();
    }
    record_phase("exec.capture_stop", phase_start);

    // Phase: Cleanup (library unload happens in destructor, but we measure what we can)
    phase_start = Clock::now();
    record_phase("exec.cleanup", phase_start);

    return result;
}

// ============================================================================
// Suite Test Execution
// ============================================================================

// Function pointer type for tml_run_test_with_catch from runtime
using TmlRunTestWithCatch = int32_t (*)(TestMainFunc);
using TmlGetPanicMessage = const char* (*)();
using TmlGetPanicBacktrace = const char* (*)();
using TmlGetPanicBacktraceJson = const char* (*)();
using TmlEnableBacktrace = void (*)();

SuiteTestResult run_suite_test(DynamicLibrary& lib, int test_index, bool verbose,
                               int timeout_seconds, const std::string& test_name, bool backtrace) {
    SuiteTestResult result;

    // Flush output to help debug crashes
    std::cout << std::flush;
    std::cerr << std::flush;
    std::fflush(stdout);
    std::fflush(stderr);

    // Get the indexed test function
    std::string func_name = "tml_test_" + std::to_string(test_index);
    TML_LOG_INFO("test", "  Looking up symbol: " << func_name);
    auto test_func = lib.get_function<TestMainFunc>(func_name.c_str());
    if (!test_func) {
        result.error = "Failed to find " + func_name + " in suite DLL";
        TML_LOG_ERROR("test", result.error);
        return result;
    }

    // Try to get the panic-catching wrapper from the runtime
    auto run_with_catch = lib.get_function<TmlRunTestWithCatch>("tml_run_test_with_catch");
    TML_LOG_INFO("test", "  tml_run_test_with_catch: " << (run_with_catch ? "found" : "NOT FOUND"));

    // Get panic message and backtrace functions
    auto get_panic_msg = lib.get_function<TmlGetPanicMessage>("tml_get_panic_message");
    auto get_panic_bt =
        backtrace ? lib.get_function<TmlGetPanicBacktrace>("tml_get_panic_backtrace") : nullptr;
    auto get_panic_bt_json =
        backtrace ? lib.get_function<TmlGetPanicBacktraceJson>("tml_get_panic_backtrace_json")
                  : nullptr;
    auto enable_bt =
        backtrace ? lib.get_function<TmlEnableBacktrace>("tml_enable_backtrace_on_panic") : nullptr;

    // Enable backtrace for test failures (if available and enabled)
    if (backtrace && enable_bt) {
        enable_bt();
    }

    // Get output suppression function from runtime (to suppress test output when not verbose)
    using TmlSetOutputSuppressed = void (*)(int32_t);
    auto set_output_suppressed =
        lib.get_function<TmlSetOutputSuppressed>("tml_set_output_suppressed");
    TML_LOG_INFO(
        "test", "  tml_set_output_suppressed: " << (set_output_suppressed ? "found" : "NOT FOUND"));

    // Suppress output when not in verbose mode
    if (!verbose) {
        if (set_output_suppressed) {
            set_output_suppressed(1);
        }
        // Also flush to ensure no buffered output appears
        std::fflush(stdout);
        std::fflush(stderr);
    }

    // Route C runtime log messages through the C++ Logger
    using RtLogSetCallback = void (*)(void (*)(int, const char*, const char*));
    auto set_log_callback = lib.get_function<RtLogSetCallback>("rt_log_set_callback");
    if (set_log_callback) {
        set_log_callback(rt_log_bridge_callback);
    }

    // Sync C runtime log level with C++ Logger level
    using RtLogSetLevel = void (*)(int);
    auto set_log_level = lib.get_function<RtLogSetLevel>("rt_log_set_level");
    if (set_log_level) {
        set_log_level(static_cast<int>(tml::log::Logger::instance().level()));
    }

    // Save reference to original stderr BEFORE capture for timeout messages
#ifdef _WIN32
    int original_stderr_fd = _dup(_fileno(stderr));
#else
    int original_stderr_fd = dup(STDERR_FILENO);
#endif

    // Skip output capture in suite mode (parallel execution) - stdout/stderr redirection
    // is not thread-safe and causes deadlocks. Instead rely on tml_set_output_suppressed
    // at the TML runtime level to suppress output when not verbose.
    OutputCapture capture;
    bool capture_started = false; // Disabled - causes deadlocks in parallel mode

    // Execute the test
    using Clock = std::chrono::high_resolution_clock;
    auto start = Clock::now();

    TML_LOG_INFO("test", "  Executing test function...");

    // Ensure output is flushed before test execution in case of crash
    std::cout << std::flush;
    std::cerr << std::flush;
    std::fflush(stdout);
    std::fflush(stderr);

    // Timeout watchdog thread - monitors test execution and reports hangs
    std::atomic<bool> test_completed{false};
    std::atomic<bool> timeout_triggered{false};
    std::mutex watchdog_mutex;
    std::condition_variable watchdog_cv;
    std::thread watchdog_thread;

    if (timeout_seconds > 0) {
        watchdog_thread = std::thread([&, original_stderr_fd]() {
            std::unique_lock<std::mutex> lock(watchdog_mutex);
            auto deadline =
                std::chrono::steady_clock::now() + std::chrono::seconds(timeout_seconds);

            while (std::chrono::steady_clock::now() < deadline) {
                if (watchdog_cv.wait_for(lock, std::chrono::seconds(1),
                                         [&]() { return test_completed.load(); })) {
                    return; // Test completed normally
                }

                auto elapsed =
                    std::chrono::duration_cast<std::chrono::seconds>(Clock::now() - start).count();

                if (!verbose && elapsed >= 5 && elapsed % 5 == 0) {
                    std::ostringstream progress_msg;
                    progress_msg << "\033[33m[WARNING] Test '"
                                 << (test_name.empty() ? func_name : test_name)
                                 << "' still running... (" << elapsed << "s)\033[0m\n";
                    std::string msg = progress_msg.str();
#ifdef _WIN32
                    _write(original_stderr_fd, msg.c_str(), (unsigned int)msg.size());
#else
                    write(original_stderr_fd, msg.c_str(), msg.size());
#endif
                }
            }

            // Timeout reached - test is hanging
            timeout_triggered.store(true);

            if (set_output_suppressed) {
                set_output_suppressed(0);
            }

            std::string test_display = test_name.empty() ? func_name : test_name;
            std::ostringstream msg;
            msg << "\n\n\033[1;31m"
                << "============================================================\n"
                << "               TEST TIMEOUT DETECTED\n"
                << "============================================================\n"
                << " Test:    " << test_display << "\n"
                << " Timeout: " << timeout_seconds << " seconds\n"
                << "\n"
                << " The test appears to be stuck in an infinite loop\n"
                << " or deadlock. Terminating test process...\n"
                << "============================================================\n"
                << "\033[0m\n";

            std::string msgStr = msg.str();
#ifdef _WIN32
            _write(original_stderr_fd, msgStr.c_str(), (unsigned int)msgStr.size());
            _commit(original_stderr_fd);
            Sleep(200);
            TerminateProcess(GetCurrentProcess(), 124);
#else
            write(original_stderr_fd, msgStr.c_str(), msgStr.size());
            fsync(original_stderr_fd);
            usleep(200000);
            _exit(124);
#endif
        });
    }

    // Execute test with crash protection
    if (run_with_catch) {
        TML_LOG_INFO("test", "  Calling tml_run_test_with_catch wrapper...");
#ifdef _WIN32
        // VEH handler in essential.c catches hardware exceptions (ACCESS_VIOLATION, etc.)
        // via longjmp BEFORE SEH unwinding occurs (stack is still intact).
        // Crash context is set by suite_execution.cpp before calling run_suite_test().
        // SEH is kept as belt-and-suspenders fallback.
        result.exit_code = call_run_with_catch_seh(run_with_catch, test_func);

        if (g_crash_occurred) {
            result.success = false;
            result.error = std::string("Test crashed: ") + g_crash_msg;
            TML_LOG_INFO("test", "[DEBUG]   tml_run_test_with_catch crashed (SEH caught)");
        } else
#else
        result.exit_code = run_with_catch(test_func);
#endif
            if (result.exit_code == -1) {
            result.success = false;
            std::string error_msg = "Test panicked";
            if (get_panic_msg) {
                const char* panic_msg = get_panic_msg();
                if (panic_msg && panic_msg[0] != '\0') {
                    error_msg += ": ";
                    error_msg += panic_msg;
                }
            }
            if (get_panic_bt) {
                const char* bt_str = get_panic_bt();
                if (bt_str && bt_str[0] != '\0') {
                    error_msg += "\n\nBacktrace:\n";
                    error_msg += bt_str;
                }
            }
            if (get_panic_bt_json) {
                const char* bt_json = get_panic_bt_json();
                if (bt_json && bt_json[0] != '\0' && bt_json[0] != ']') {
                    TML_LOG_ERROR("test", "PANIC backtrace (JSON): " << bt_json);
                }
            }
            result.error = error_msg;
        } else if (result.exit_code == -2) {
            result.success = false;
            std::string crash_msg = "Test crashed (SIGSEGV/SIGFPE/etc)";
            if (get_panic_msg) {
                const char* msg = get_panic_msg();
                if (msg && msg[0] != '\0') {
                    crash_msg = msg;
                }
            }
            result.error = crash_msg;
        } else {
            result.success = (result.exit_code == 0);
        }
        TML_LOG_INFO("test", "[DEBUG]   tml_run_test_with_catch returned: " << result.exit_code);
    } else {
#ifdef _WIN32
        TML_LOG_INFO("test", "  Calling test function with SEH protection...");
        result.exit_code = call_test_with_seh(test_func);
        if (g_crash_occurred) {
            result.success = false;
            result.error = std::string("Test crashed: ") + g_crash_msg;
        } else {
            result.success = (result.exit_code == 0);
        }
#else
        result.exit_code = test_func();
        result.success = (result.exit_code == 0);
#endif
        TML_LOG_INFO("test", "  Test returned: " << result.exit_code);
    }

    // Signal watchdog that test completed
    test_completed.store(true);
    watchdog_cv.notify_all();

    if (watchdog_thread.joinable()) {
        watchdog_thread.join();
    }

    TML_LOG_INFO("test", "  Test execution complete, exit_code=" << result.exit_code);

    auto end = Clock::now();
    result.duration_us = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();

    if (capture_started) {
        result.output = capture.stop();
    }

    // Restore output after test (important for error messages)
    if (!verbose && set_output_suppressed) {
        set_output_suppressed(0);
    }

    // Close the duplicated stderr fd
#ifdef _WIN32
    _close(original_stderr_fd);
#else
    close(original_stderr_fd);
#endif

    return result;
}

SuiteTestResult run_suite_test_profiled(DynamicLibrary& lib, int test_index, PhaseTimings* timings,
                                        bool /*verbose*/, bool backtrace) {
    using Clock = std::chrono::high_resolution_clock;
    auto record_phase = [&](const std::string& phase, Clock::time_point start) {
        if (timings) {
            auto end = Clock::now();
            timings->timings_us[phase] =
                std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
        }
    };

    SuiteTestResult result;

    // Phase: Get the indexed test function
    auto phase_start = Clock::now();
    std::string func_name = "tml_test_" + std::to_string(test_index);
    auto test_func = lib.get_function<TestMainFunc>(func_name.c_str());
    if (!test_func) {
        result.error = "Failed to find " + func_name + " in suite DLL";
        record_phase("exec.get_symbol", phase_start);
        return result;
    }

    auto run_with_catch = lib.get_function<TmlRunTestWithCatch>("tml_run_test_with_catch");

    auto get_panic_msg = lib.get_function<TmlGetPanicMessage>("tml_get_panic_message");
    auto get_panic_bt =
        backtrace ? lib.get_function<TmlGetPanicBacktrace>("tml_get_panic_backtrace") : nullptr;
    auto enable_bt =
        backtrace ? lib.get_function<TmlEnableBacktrace>("tml_enable_backtrace_on_panic") : nullptr;

    if (backtrace && enable_bt) {
        enable_bt();
    }

    using TmlSetOutputSuppressed = void (*)(int32_t);
    auto set_output_suppressed =
        lib.get_function<TmlSetOutputSuppressed>("tml_set_output_suppressed");
    record_phase("exec.get_symbol", phase_start);

    // Suppress output for profiled tests
    if (set_output_suppressed) {
        set_output_suppressed(1);
    }

    // Phase: Set up output capture (DISABLED)
    phase_start = Clock::now();
    OutputCapture capture;
    bool capture_started = false; // Disabled - causes deadlocks
    record_phase("exec.capture_start", phase_start);

    // Phase: Execute the test
    phase_start = Clock::now();
    if (run_with_catch) {
#ifdef _WIN32
        // Set crash context so VEH handler can report which test crashed
        using TmlSetCrashCtx = void (*)(const char*, const char*, const char*);
        auto set_crash_ctx = lib.get_function<TmlSetCrashCtx>("tml_set_test_crash_context");
        if (set_crash_ctx) {
            set_crash_ctx(func_name.c_str(), nullptr, nullptr);
        }

        result.exit_code = call_run_with_catch_seh(run_with_catch, test_func);

        using TmlClearCrashCtx = void (*)();
        auto clear_crash_ctx = lib.get_function<TmlClearCrashCtx>("tml_clear_test_crash_context");
        if (clear_crash_ctx) {
            clear_crash_ctx();
        }

        if (g_crash_occurred) {
            result.success = false;
            result.error = std::string("Test crashed: ") + g_crash_msg;
        } else
#else
        result.exit_code = run_with_catch(test_func);
#endif
            if (result.exit_code == -1) {
            result.success = false;
            std::string error_msg = "Test panicked";
            if (get_panic_msg) {
                const char* panic_msg = get_panic_msg();
                if (panic_msg && panic_msg[0] != '\0') {
                    error_msg += ": ";
                    error_msg += panic_msg;
                }
            }
            if (get_panic_bt) {
                const char* bt_str = get_panic_bt();
                if (bt_str && bt_str[0] != '\0') {
                    error_msg += "\n\nBacktrace:\n";
                    error_msg += bt_str;
                }
            }
            result.error = error_msg;
        } else if (result.exit_code == -2) {
            result.success = false;
            std::string crash_msg = "Test crashed";
            if (get_panic_msg) {
                const char* msg = get_panic_msg();
                if (msg && msg[0] != '\0') {
                    crash_msg = msg;
                }
            }
            result.error = crash_msg;
        } else {
            result.success = (result.exit_code == 0);
        }
    } else {
#ifdef _WIN32
        result.exit_code = call_test_with_seh(test_func);
        if (g_crash_occurred) {
            result.success = false;
            result.error = std::string("Test crashed: ") + g_crash_msg;
        } else {
            result.success = (result.exit_code == 0);
        }
#else
        try {
            result.exit_code = test_func();
            result.success = (result.exit_code == 0);
        } catch (...) {
            result.error = "Exception during test execution";
            result.exit_code = 1;
        }
#endif
    }
    result.duration_us =
        std::chrono::duration_cast<std::chrono::microseconds>(Clock::now() - phase_start).count();
    record_phase("exec.run", phase_start);

    // Phase: Stop capture
    phase_start = Clock::now();
    if (capture_started) {
        result.output = capture.stop();
    }
    record_phase("exec.capture_stop", phase_start);

    // Restore output after test
    if (set_output_suppressed) {
        set_output_suppressed(0);
    }

    return result;
}

} // namespace tml::cli
