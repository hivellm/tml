#pragma once

// Internal shared state for testing_compile.cpp and testing_compile_build.cpp.
// NOT part of the public API.

#include "cli/builder/builder_internal.hpp"
#include "cli/builder/compiler_setup.hpp"
#include "cli/builder/object_compiler.hpp"
#include "codegen/llvm/llvm_ir_gen.hpp"
#include "common.hpp"
#include "log/log.hpp"
#include "query/query_context.hpp"
#include "query/query_fingerprint.hpp"
#include "query/query_key.hpp"
#include "testing/testing_compile.hpp"
#include "testing/testing_dispatcher_gen.hpp"
#include "types/module_binary.hpp"

#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <filesystem>
#include <fstream>
#include <llvm/Support/ErrorHandling.h>
#include <mutex>
#include <set>
#include <thread>

namespace fs = std::filesystem;

namespace tml::testing {

using Clock = std::chrono::high_resolution_clock;

// Shared global state — defined in testing_compile.cpp
extern std::string g_clang_path;
extern bool g_env_initialized;
extern std::mutex g_env_mutex;
extern std::mutex g_incr_cache_mutex;
extern std::string g_runtime_archive_path;
extern std::set<std::string> g_archive_obj_stems;
extern std::string g_stdlib_obj_path;
extern std::shared_ptr<const codegen::CodegenLibraryState> g_stdlib_codegen_state;

struct LlvmFatalException : std::runtime_error {
    LlvmFatalException(const std::string& msg) : std::runtime_error(msg) {}
};

// Shared helpers — defined in testing_compile.cpp
void init_compile_env();
std::string to_fwd_slashes(const std::string& s);
fs::path get_test_exe_cache_dir();
void ensure_runtime_dlls(const fs::path& target_dir);
std::string build_runtime_archive(const CompileConfig& config);
std::string build_stdlib_object(const CompileConfig& config);
CompileResult compile_suite_safe(const Suite& suite, const CompileConfig& config);

/// Thread budget singleton
class ThreadBudget {
public:
    static ThreadBudget& instance() {
        static ThreadBudget budget;
        return budget;
    }
    void acquire() {
        std::unique_lock<std::mutex> lock(mutex_);
        cv_.wait(lock, [this] { return available_ > 0; });
        --available_;
    }
    void release() {
        std::lock_guard<std::mutex> lock(mutex_);
        ++available_;
        cv_.notify_one();
    }
    int max_threads() const {
        return max_;
    }

private:
    ThreadBudget() {
        int hw = static_cast<int>(std::thread::hardware_concurrency());
        max_ = std::max(2, hw);
        available_ = max_;
    }
    std::mutex mutex_;
    std::condition_variable cv_;
    int available_;
    int max_;
};

} // namespace tml::testing
