//! # Independent Compilation Pipeline
//!
//! Compiles test suites to executables using QueryContext (demand-driven pipeline)
//! instead of the old sequential lex→parse→typecheck→codegen flow.
//! Zero dependency on cli/tester/. Part of the v3 independent test system.
//!
//! Two compilation modes:
//!   1. Per-suite: compile_suite() → one .exe per suite (for filtered runs)
//!   2. Unified:   compile_unified_binary() → one .exe for ALL tests (Zig model)

#pragma once

#include "testing/testing_discovery.hpp"

#include <atomic>
#include <cstdint>
#include <functional>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace tml::testing {

/// Result of compiling a single suite to an executable.
struct CompileResult {
    bool success = false;
    std::string exe_path;
    std::string error_message;
    int64_t compile_time_us = 0;

    /// Per-file errors for resilient suite compilation.
    /// When a single file fails, remaining files still compile and link.
    struct FileError {
        std::string file_path;
        std::string error;
    };
    std::vector<FileError> per_file_errors;

    /// True when per-file codegen succeeded (for at least the surviving files)
    /// but the suite failed at the dispatcher-compile or link stage. The
    /// coordinator uses this to fall back to per-file compilation for the
    /// suite's files (offender isolation) instead of dropping the whole suite.
    bool link_stage_failure = false;

    /// Total number of tests successfully compiled (for unified binary).
    int compiled_test_count = 0;

    /// Phase 8.5 W5: union of all `.tml` source paths transitively loaded
    /// during compilation of this suite — i.e. the test files themselves
    /// PLUS every imported library/package module the type-checker pulled in
    /// (via `load_native_module → load_module_from_file`). The test cache
    /// folds these into its per-suite content fingerprint so editing any
    /// transitively-imported module invalidates the cached `.exe`.
    ///
    /// Stored as `set` for deterministic iteration in `compute_source_hashes`,
    /// and de-duplicated across the suite's individual test files.
    std::set<std::string> loaded_source_files;
};

/// Configuration for the compilation pipeline.
struct CompileConfig {
    bool verbose = false;
    bool coverage = false;
    bool no_cache = false;
    bool fail_fast = false; ///< Stop immediately on first compile error
    int optimization_level = 0;
    int num_threads = 0; ///< Parallel compile workers (0 = auto)

    /// Parallel per-file codegen threads WITHIN one suite (set by
    /// compile_suites_parallel from the idle worker budget). With aggregated
    /// suites (phase41a default: 25 files/EXE) a targeted run may have fewer
    /// suites than workers; intra-suite threads keep codegen parallel while
    /// staying inside the same global 8-thread envelope that suite-level
    /// parallelism already exercises (each file has its own QueryContext).
    int intra_suite_threads = 1;

    // Hybrid pipeline (phase13d): stage overrides propagated from --stage flag.
    // Empty = use default for each stage.
    std::map<std::string, std::string> stage_overrides;
};

/// Initialize the compilation environment.
/// Preloads library .meta caches and discovers the clang path.
/// Must be called once before compile_suite().
void init_compile_env();

/// phase42a: mark the start of a test run's incremental-cache session — resets
/// the GREEN/RED/load/save telemetry counters (F-019).
void incr_test_run_begin();

/// phase42a: run-teardown for the incremental cache (F-021/F-022/F-025). Prunes
/// old config partitions, GCs the ir/ store to the surviving keys, drops the
/// shared prev-session memo, and logs the telemetry summary. Call ONCE after all
/// suites in a run have compiled.
void incr_test_run_end();

/// Compile a single suite to an executable.
/// Uses QueryContext::codegen_unit() for each test file,
/// generates NDJSON dispatcher IR, compiles to .obj, and links.
CompileResult compile_suite(const Suite& suite, const CompileConfig& config);

/// Callback invoked after each suite finishes compilation (thread-safe).
/// @param idx  Index into suites vector
/// @param cr   Compilation result for that suite
using CompileCallback = std::function<void(int idx, const CompileResult& cr)>;

/// Compile multiple suites in parallel using a thread pool.
/// @param should_stop  Atomic flag for fail-fast abort
/// @param on_complete  Optional per-suite callback for incremental cache saves
std::vector<CompileResult> compile_suites_parallel(const std::vector<Suite>& suites,
                                                   const CompileConfig& config,
                                                   std::atomic<bool>& should_stop,
                                                   CompileCallback on_complete = nullptr);

// ============================================================================
// Unified binary compilation (Zig model)
// ============================================================================

/// Mapping from global test index to suite/test position.
struct UnifiedTestMapping {
    int global_index;        ///< Index in the unified binary (0..N-1)
    int suite_index;         ///< Index into the suites vector
    int test_index_in_suite; ///< Index within that suite's tests vector
    std::string test_name;
    std::string file_path;
};

/// Compile ALL test suites into a single unified executable.
/// This is the Zig-model compilation: stdlib compiled once, all test files
/// compiled with library_decls_only, everything linked into one .exe.
///
/// @param suites       All discovered suites
/// @param config       Compilation configuration
/// @param should_stop  Atomic flag for fail-fast abort
/// @param out_mapping  Output: global index → suite/test mapping
/// @return CompileResult with the unified .exe path
CompileResult compile_unified_binary(const std::vector<Suite>& suites, const CompileConfig& config,
                                     std::atomic<bool>& should_stop,
                                     std::vector<UnifiedTestMapping>& out_mapping);

} // namespace tml::testing
