//! # Query Context
//!
//! Central coordinator for the query-based compilation pipeline.
//! Analogous to rustc's `TyCtxt`, it owns the cache, dependency tracker,
//! and provider registry. All compilation goes through `force()`.

#pragma once

#include "query/query_cache.hpp"
#include "query/query_deps.hpp"
#include "query/query_fingerprint.hpp"
#include "query/query_key.hpp"
#include "query/query_provider.hpp"

#include <string>

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
    ///
    /// 1. Check cache — if hit, return cached result.
    /// 2. Check for cycles — if cycle, report error.
    /// 3. Push query onto active stack.
    /// 4. Invoke provider function.
    /// 5. Record dependencies.
    /// 6. Cache result.
    /// 7. Pop query from active stack.
    /// 8. Return result.
    template <typename ResultType> ResultType force(const QueryKey& key);

    // ========================================================================
    // Convenience methods (construct key + call force)
    // ========================================================================

    /// Read and preprocess a source file.
    ReadSourceResult read_source(const std::string& file_path);

    /// Tokenize a source file.
    TokenizeResult tokenize(const std::string& file_path);

    /// Parse a module from a source file.
    ParseModuleResult parse_module(const std::string& file_path, const std::string& module_name);

    /// Type-check a module.
    TypecheckResult typecheck_module(const std::string& file_path, const std::string& module_name);

    /// Borrow-check a module.
    BorrowcheckResult borrowcheck_module(const std::string& file_path,
                                         const std::string& module_name);

    /// Lower AST to HIR.
    HirLowerResult hir_lower(const std::string& file_path, const std::string& module_name);

    /// Build MIR from HIR.
    MirBuildResult mir_build(const std::string& file_path, const std::string& module_name);

    /// Generate LLVM IR for a compilation unit.
    CodegenUnitResult codegen_unit(const std::string& file_path, const std::string& module_name);

    // ========================================================================
    // Cache management
    // ========================================================================

    /// Invalidate cache for a specific file (invalidates all queries for that file).
    void invalidate_file(const std::string& file_path);

    /// Clear the entire cache.
    void clear_cache() {
        cache_.clear();
    }

    /// Get cache statistics.
    [[nodiscard]] QueryCache::Stats cache_stats() const {
        return cache_.get_stats();
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
};

// Template implementation
template <typename ResultType> ResultType QueryContext::force(const QueryKey& key) {
    // 1. Check cache
    auto cached = cache_.lookup<ResultType>(key);
    if (cached) {
        deps_.record_dependency(key);
        return *cached;
    }

    // 2. Check for cycles
    auto cycle = deps_.detect_cycle(key);
    if (cycle) {
        // Return a failure result
        ResultType fail_result{};
        fail_result.success = false;
        return fail_result;
    }

    // 3. Get provider
    auto kind = query_kind(key);
    const auto* provider = providers_.get_provider(kind);
    if (!provider) {
        ResultType fail_result{};
        fail_result.success = false;
        return fail_result;
    }

    // 4. Push active, execute, pop
    deps_.push_active(key);
    std::any raw_result;
    try {
        raw_result = (*provider)(*this, key);
    } catch (...) {
        deps_.pop_active();
        throw;
    }

    // 5. Get dependencies recorded during execution
    auto recorded_deps = deps_.current_dependencies();
    deps_.pop_active();

    // 6. Record this query as a dependency of the caller
    deps_.record_dependency(key);

    // 7. Extract typed result
    ResultType result;
    try {
        result = std::any_cast<ResultType>(raw_result);
    } catch (const std::bad_any_cast&) {
        result.success = false;
        return result;
    }

    // 8. Compute fingerprints and cache
    // Input fingerprint is based on the key
    auto input_fp = fingerprint_string(std::to_string(static_cast<uint8_t>(kind)));
    // Output fingerprint is deferred to Phase 4 (just use zero for now)
    Fingerprint output_fp{};

    cache_.insert<ResultType>(key, result, input_fp, output_fp, std::move(recorded_deps));

    return result;
}

} // namespace tml::query
