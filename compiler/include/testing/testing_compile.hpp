//! # Independent Compilation Pipeline
//!
//! Compiles test suites to executables using QueryContext (demand-driven pipeline)
//! instead of the old sequential lex→parse→typecheck→codegen flow.
//! Zero dependency on cli/tester/. Part of the v3 independent test system.
//!
//! Pipeline per suite:
//!   QueryContext::codegen_unit() → NDJSON dispatcher IR → compile to .obj → link → .exe

#pragma once

#include "testing/testing_discovery.hpp"

#include <atomic>
#include <cstdint>
#include <string>
#include <vector>

namespace tml::testing {

/// Result of compiling a single suite to an executable.
struct CompileResult {
    bool success = false;
    std::string exe_path;
    std::string error_message;
    int64_t compile_time_us = 0;
};

/// Configuration for the compilation pipeline.
struct CompileConfig {
    bool verbose = false;
    bool coverage = false;
    bool no_cache = false;
    int optimization_level = 0;
    int num_threads = 0; ///< Parallel compile workers (0 = auto)
};

/// Initialize the compilation environment.
/// Preloads library .meta caches and discovers the clang path.
/// Must be called once before compile_suite().
void init_compile_env();

/// Compile a single suite to an executable.
/// Uses QueryContext::codegen_unit() for each test file,
/// generates NDJSON dispatcher IR, compiles to .obj, and links.
CompileResult compile_suite(const Suite& suite, const CompileConfig& config);

/// Compile multiple suites in parallel using a thread pool.
/// @param should_stop  Atomic flag for fail-fast abort
std::vector<CompileResult> compile_suites_parallel(const std::vector<Suite>& suites,
                                                   const CompileConfig& config,
                                                   std::atomic<bool>& should_stop);

} // namespace tml::testing
