//! # Query Context
//!
//! Central coordinator for the query-based compilation pipeline.
//! Analogous to rustc's `TyCtxt`, it owns the cache, dependency tracker,
//! and provider registry. All compilation goes through `force()`.
//!
//! ## Incremental Compilation (Phase 4)
//!
//! When incremental mode is enabled, fingerprints and dependency edges
//! are persisted to disk between sessions. On rebuild, if all inputs
//! are unchanged (GREEN), the CodegenUnit result is loaded from disk,
//! skipping the entire compilation pipeline.

#pragma once

#include "log/log.hpp"
#include "query/query_cache.hpp"
#include "query/query_deps.hpp"
#include "query/query_fingerprint.hpp"
#include "query/query_incr.hpp"
#include "query/query_key.hpp"
#include "query/query_provider.hpp"

#include <filesystem>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <type_traits>
#include <unordered_map>

namespace tml::codegen {
struct CodegenLibraryState;
} // namespace tml::codegen

namespace tml::query {

/// Options passed to QueryContext from the build system.
struct QueryOptions {
    bool verbose = false;
    bool debug_info = false;
    bool coverage = false;
    int optimization_level = 0;
    std::string target_triple;
    std::string sysroot;
    std::string source_directory;
    std::vector<std::string> defines;

    // PGO options
    bool profile_generate = false;
    std::string profile_use;

    // Incremental compilation
    bool incremental = true;

    // Backend selection ("llvm" or "cranelift")
    std::string backend = "llvm";

    // Pipeline dump: emit all intermediate representations to a directory.
    // Enabled by --emit-pipeline on the CLI.
    bool emit_pipeline = false;
    std::string pipeline_output_dir; // Default: .sandbox/pipeline/ relative to source

    // Test entry point generation (for v3 test system).
    // When true, generate @main wrapper. When false, generate tml_test_N() entry.
    // Default: true (standalone executable). Set to false for v3 test suites.
    bool generate_exe_main = true;
    int test_entry_index = -1; // -1 = tml_test_entry, >=0 = tml_test_N

    /// Pre-compiled stdlib state. When set, the AST codegen path uses this
    /// to skip emit_module_pure_tml_functions() and emits only declarations.
    /// The stdlib definitions come from a pre-compiled .obj linked at link time.
    std::shared_ptr<const codegen::CodegenLibraryState> cached_library_state;

    /// When true AND cached_library_state is set, emit only `declare` stubs
    /// for library functions (definitions come from pre-compiled stdlib .obj).
    bool library_decls_only = false;

    /// Hybrid pipeline (phase12f/phase13d): map from canonical stage name to
    /// implementation tag. "tml" uses the TML frontend subprocess; "cpp" forces
    /// the C++ pipeline. Valid stage names: lexer, parser, typechecker, hir, mir, codegen.
    /// Empty by default here — callers (BuildOptions, RunOptions) default parser to "tml"
    /// since the phase13d switchover. Use --stage=parser:cpp to force C++ frontend.
    std::map<std::string, std::string> stage_overrides;
};

/// Phase12f: canonical stage names matching QueryContext query kinds.
/// Returns true if `name` is a valid hybrid-pipeline stage identifier.
bool is_valid_stage_name(const std::string& name);

/// Returns the comma-separated list of valid stage names (for diagnostics).
const char* valid_stage_names_csv();

/// Central query context for the compilation session.
///
/// Owns the query cache, dependency tracker, and provider registry.
/// All compilation goes through this context via `force()`.
class QueryContext {
public:
    /// Construct with options. Registers all core providers.
    explicit QueryContext(const QueryOptions& options = {});

    /// Force-execute a query, returning the cached result or computing it.
    template <typename ResultType> ResultType force(const QueryKey& key);

    // ========================================================================
    // Convenience methods (construct key + call force)
    // ========================================================================

    ReadSourceResult read_source(const std::string& file_path);
    TokenizeResult tokenize(const std::string& file_path);
    ParseModuleResult parse_module(const std::string& file_path, const std::string& module_name);
    TypecheckResult typecheck_module(const std::string& file_path, const std::string& module_name);
    BorrowcheckResult borrowcheck_module(const std::string& file_path,
                                         const std::string& module_name);
    HirLowerResult hir_lower(const std::string& file_path, const std::string& module_name);
    ThirLowerResult thir_lower(const std::string& file_path, const std::string& module_name);
    MirBuildResult mir_build(const std::string& file_path, const std::string& module_name);
    CodegenUnitResult codegen_unit(const std::string& file_path, const std::string& module_name);

    // ========================================================================
    // Cache management
    // ========================================================================

    void invalidate_file(const std::string& file_path);

    void clear_cache() {
        cache_.clear();
    }

    /// Phase 8.5 W5: collect every file path from ReadSourceKey entries in the
    /// transitive dependency graph of a codegen unit. This works on both the
    /// GREEN path (deps from prev_session_) and the fresh path (deps from
    /// in-memory cache). The returned set includes every `.tml` file that
    /// was loaded during compilation, suitable for test-binary cache hashing.
    [[nodiscard]] std::set<std::string>
    collect_transitive_source_files(const std::string& file_path,
                                    const std::string& module_name) const;

    [[nodiscard]] QueryCache::Stats cache_stats() const {
        return cache_.get_stats();
    }

    // ========================================================================
    // Incremental compilation
    // ========================================================================

    /// Load incremental cache from previous session.
    bool load_incremental_cache(const std::filesystem::path& build_dir);

    /// Save incremental cache for this session.
    bool save_incremental_cache(const std::filesystem::path& build_dir);

    /// phase41c / F-010: fold this context's accumulated incremental-cache
    /// entries into `dest` without touching disk. Lets the test path batch many
    /// per-file writers into a single per-suite `incr.bin` flush, removing the
    /// per-file global-mutex serialization (`g_incr_cache_mutex`). No-op when
    /// incremental mode is inactive for this context.
    void merge_incremental_into(IncrCacheWriter& dest) const;

    /// The options hash computed for this context's incremental session. Valid
    /// after load_incremental_cache(); needed to write a batched accumulator
    /// with the correct session key.
    [[nodiscard]] uint32_t incremental_options_hash() const {
        return options_hash_;
    }

    /// Check if incremental mode is active.
    [[nodiscard]] bool incremental_active() const {
        return incr_enabled_;
    }

    // ========================================================================
    // Accessors
    // ========================================================================

    [[nodiscard]] const QueryOptions& options() const {
        return options_;
    }
    [[nodiscard]] QueryProviderRegistry& providers() {
        return providers_;
    }
    [[nodiscard]] DependencyTracker& deps() {
        return deps_;
    }
    [[nodiscard]] QueryCache& cache() {
        return cache_;
    }

private:
    QueryOptions options_;
    QueryCache cache_;
    DependencyTracker deps_;
    QueryProviderRegistry providers_;

    // Incremental compilation state
    std::unique_ptr<PrevSessionCache> prev_session_;
    std::unique_ptr<IncrCacheWriter> incr_writer_;
    std::unordered_map<QueryKey, QueryColor, QueryKeyHash, QueryKeyEqual> color_map_;
    Fingerprint lib_env_fp_;
    Fingerprint codegen_opts_fp_; ///< Precomputed fingerprint for codegen-affecting options.
    bool incr_enabled_ = false;
    std::filesystem::path incr_cache_dir_;
    uint32_t options_hash_ = 0;

    /// Compute input fingerprint for a query based on its dependencies.
    Fingerprint compute_input_fingerprint(const QueryKey& key, const std::vector<QueryKey>& deps);

    /// Compute output fingerprint for a query result.
    Fingerprint compute_output_fingerprint(const QueryKey& key, const std::any& raw_result,
                                           QueryKind kind);

    /// Try to mark a CodegenUnit as green (reuse previous session result).
    std::optional<CodegenUnitResult> try_mark_green_codegen(const QueryKey& key);

    /// Recursively verify that all inputs for a query are unchanged.
    bool verify_all_inputs_green(const QueryKey& key);
};

// ============================================================================
// Template implementation of force<R>()
// ============================================================================

template <typename ResultType> ResultType QueryContext::force(const QueryKey& key) {
    // 1. Check in-memory cache
    auto cached = cache_.lookup<ResultType>(key);
    if (cached) {
        deps_.record_dependency(key);
        return *cached;
    }

    // 2. For CodegenUnit: try incremental reuse from previous session
    if constexpr (std::is_same_v<ResultType, CodegenUnitResult>) {
        if (incr_enabled_ && prev_session_) {
            auto green = try_mark_green_codegen(key);
            if (green) {
                deps_.record_dependency(key);
                return *green;
            }
        }
    }

    // 3. Check for cycles
    auto cycle = deps_.detect_cycle(key);
    if (cycle) {
        // Build cycle path string for the error message
        std::string cycle_path;
        for (size_t i = 0; i < cycle->size(); ++i) {
            if (i > 0)
                cycle_path += " -> ";
            cycle_path += query_kind_name(query_kind((*cycle)[i]));
        }
        TML_LOG_ERROR("query", "[Q001] Query cycle detected: " << cycle_path);
        ResultType fail_result{};
        fail_result.success = false;
        return fail_result;
    }

    // 4. Get provider
    auto kind = query_kind(key);
    const auto* provider = providers_.get_provider(kind);
    if (!provider) {
        TML_LOG_ERROR("query",
                      "[Q004] No provider registered for query kind: " << query_kind_name(kind));
        ResultType fail_result{};
        fail_result.success = false;
        return fail_result;
    }

    // 5. Push active, execute, pop
    deps_.push_active(key);
    std::any raw_result;
    try {
        raw_result = (*provider)(*this, key);
    } catch (...) {
        deps_.pop_active();
        throw;
    }

    // 6. Get dependencies recorded during execution
    auto recorded_deps = deps_.current_dependencies();
    deps_.pop_active();

    // 7. Record this query as a dependency of the caller
    deps_.record_dependency(key);

    // 8. Extract typed result
    ResultType result;
    try {
        result = std::any_cast<ResultType>(raw_result);
    } catch (const std::bad_any_cast&) {
        TML_LOG_ERROR(
            "query", "[Q004] Query result type mismatch for query kind: " << query_kind_name(kind));
        result.success = false;
        return result;
    }

    // 9. Compute proper fingerprints
    auto input_fp = compute_input_fingerprint(key, recorded_deps);
    auto output_fp = compute_output_fingerprint(key, raw_result, kind);

    cache_.insert<ResultType>(key, result, input_fp, output_fp, std::move(recorded_deps));

    // 10. Record in incremental writer for persistence
    if (incr_writer_) {
        auto entry = cache_.get_entry(key);
        if (entry) {
            incr_writer_->record(key, input_fp, output_fp, entry->dependencies);

            // For CodegenUnit, also save the IR and link_libs to disk
            if constexpr (std::is_same_v<ResultType, CodegenUnitResult>) {
                if (result.success) {
                    incr_writer_->save_ir(key, result.llvm_ir, incr_cache_dir_);
                    incr_writer_->save_link_libs(key, result.link_libs, incr_cache_dir_);
                }
            }
        }
    }

    return result;
}

} // namespace tml::query
