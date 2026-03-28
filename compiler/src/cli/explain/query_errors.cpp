TML_MODULE("compiler")

//! # Query System Error Explanations
//!
//! Error codes Q001-Q010 for query system errors.
//! These are internal compiler diagnostics emitted when the demand-driven
//! query pipeline encounters structural or caching failures.

#include "cli/explain/explain_internal.hpp"

namespace tml::cli::explain {

const std::unordered_map<std::string, std::string>& get_query_explanations() {
    static const std::unordered_map<std::string, std::string> db = {

        {"Q001", R"EX(
Query cycle detected [Q001]

A circular dependency was detected in the compilation query graph. This
means query A depends on query B which (transitively) depends on query A.

The error message includes the full cycle path, e.g.:
  typecheck_module -> hir_lower -> typecheck_module

This is always an internal compiler bug — the query system is designed
to be acyclic. Please report it with the source file that triggered this
error.

Note: this can occasionally happen if a module import graph contains a
genuine circular dependency that the import resolver failed to catch.
)EX"},

        {"Q002", R"EX(
Incremental cache read failure [Q002]

The incremental compilation cache file could not be read. This can
happen when:
1. The cache file is corrupted (truncated write, disk error)
2. The file was written by an incompatible compiler version
3. The entry count in the header exceeds the sanity limit (10,000)
4. An exception occurred while parsing the cache entries

Recovery: the compiler will automatically fall back to a full
recompilation. The stale cache file will be replaced on the next
successful build.

If this error appears repeatedly, use `mcp__tml__cache_invalidate`
to clear the cache manually.
)EX"},

        {"Q003", R"EX(
Incremental cache version mismatch [Q003]

The incremental cache file was written by a different version of the
compiler. The major version number in the cache header does not match
the current compiler's expected version.

This is expected after a compiler upgrade. The cache is automatically
invalidated and a full recompilation is performed.

You may also see this if you switch between debug and release builds
of the compiler, since they may use different cache format versions.
)EX"},

        {"Q004", R"EX(
Query result type error [Q004]

The query system encountered one of two type-related errors:

1. **No provider registered**: A query was forced but no provider
   function was registered for that query kind. This indicates the
   query provider registry was not initialized correctly.

2. **Result type mismatch**: The provider returned a value of an
   unexpected type (bad_any_cast). The provider's return type does not
   match the expected ResultType for that query kind.

Both cases are always internal compiler bugs. Please report them.
)EX"},

        {"Q005", R"EX(
Source file not found [Q005]

The ReadSource query could not open the requested source file. The
file path given to the compiler does not exist or is not readable.

Common causes:
1. The file path is wrong (typo, wrong working directory)
2. The file was deleted after the build started
3. File permissions deny read access

How to fix:
- Check that the file path is correct: `tml build path/to/file.tml`
- Ensure the file exists and is readable
- If using `use` imports, check that the imported module file exists
  relative to the current source file's directory

Related: Q002 (cache read failure)
)EX"},

        {"Q006", R"EX(
Query fingerprint computation failed [Q006]

The query system failed to compute a fingerprint for a compilation
result. Fingerprints are used for incremental compilation: they detect
when a query's inputs have changed so the cached result can be reused.

If fingerprint computation fails, the affected query will be
recomputed on every build (no incremental reuse), but compilation
will otherwise succeed.

This is an internal compiler issue. Please report it if it persists.
)EX"},

        {"Q007", R"EX(
Query dependency recording error [Q007]

The dependency tracker failed to record a dependency edge between
two queries. This can cause incorrect incremental compilation behavior:
a changed file might not invalidate all queries that depend on it.

This is an internal compiler bug. Please report it.
)EX"},

        {"Q008", R"EX(
Incremental cache write failure [Q008]

The compiler failed to write the incremental cache to disk. The next
build will perform a full recompilation.

Common causes:
1. Disk is full
2. The build directory is read-only
3. A concurrent build process is writing to the same cache file

The build output (the compiled binary) is not affected by this error.
Only incremental build performance degrades.
)EX"},

        {"Q009", R"EX(
Query provider execution failed [Q009]

A query provider threw an unexpected exception during execution. The
query system caught the exception to prevent a crash, but the
compilation result for this file is invalid.

The error message will identify which query kind failed. This is an
internal compiler bug — providers should handle all errors internally
and return a failed result rather than throwing.

Please report this with the source file that triggered the failure.
)EX"},

        {"Q010", R"EX(
Query cache consistency error [Q010]

The query cache detected an internal consistency violation. A cached
result was found but its dependencies reference queries that are no
longer in the cache, or the cache entry has an invalid state.

Recovery: the affected query will be recomputed. If this error appears
frequently, use `mcp__tml__cache_invalidate` to reset the cache.

This is an internal compiler bug. Please report it.
)EX"},

    };
    return db;
}

} // namespace tml::cli::explain
