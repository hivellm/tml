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

#include "query/query_cache.hpp"
#include "query/query_deps.hpp"
#include "query/query_fingerprint.hpp"
#include "query/query_incr.hpp"
#include "query/query_key.hpp"
#include "query/query_provider.hpp"

#include <filesystem>
#include <memory>
#include <string>
#include <type_traits>
#include <unordered_map>

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
};

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
        ResultType fail_result{};
        fail_result.success = false;
        return fail_result;
    }

    // 4. Get provider
    auto kind = query_kind(key);
    const auto* provider = providers_.get_provider(kind);
    if (!provider) {
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
