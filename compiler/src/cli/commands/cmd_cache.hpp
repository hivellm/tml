//! # Cache Command Interface
//!
//! This header defines the cache management command API.
//!
//! ## Subcommands
//!
//! | Function              | Description                          |
//! |-----------------------|--------------------------------------|
//! | `run_cache_info()`    | Display cache statistics             |
//! | `run_cache_clean()`   | Remove old or all cache files        |
//! | `run_cache_invalidate()` | Invalidate cache for specific file |
//! | `enforce_cache_limit()` | LRU eviction when over size limit  |

#ifndef TML_CLI_CMD_CACHE_HPP
#define TML_CLI_CMD_CACHE_HPP

#include <cstdint>
#include <filesystem>
#include <set>
#include <string>
#include <vector>

namespace tml::cli {

/**
 * F-031: LRU-evict files under `dir` until total tracked size <= `cap_mb` MB.
 * Exposed for unit testing; used internally by enforce_cache_caps().
 *
 * @param dir        directory to sweep (no-op if it does not exist)
 * @param cap_mb     size cap in megabytes
 * @param recursive  walk subdirectories vs top-level only
 * @param ext_filter when non-empty (e.g. ".exe"), only files with this extension
 *                   are eviction units; their siblings (.lib/.pdb/.exp/.ilk) are
 *                   folded into unit size and removed together
 * @param protect    normalized absolute paths evicted LAST (referenced artifacts
 *                   survive longest; unreferenced orphans go first)
 * @param label      human label for the one-line before->after log
 * @return bytes reclaimed
 */
std::uintmax_t evict_dir_to_cap(const std::filesystem::path& dir, std::uintmax_t cap_mb,
                                bool recursive, const std::string& ext_filter,
                                const std::set<std::string>& protect, const char* label);

/**
 * Display cache statistics and information
 * Shows: cache size, number of entries, hit rate (if tracked)
 *
 * @param verbose Print detailed information
 * @return 0 on success, non-zero on error
 */
int run_cache_info(bool verbose = false);

/**
 * Clean the build cache
 * Options:
 * - all: Remove all cached files
 * - old: Remove files older than N days (default: 7)
 *
 * @param clean_all If true, remove all cache entries. If false, only old ones
 * @param max_age_days Maximum age in days for "old" cleanup (default: 7)
 * @param verbose Print detailed information
 * @return 0 on success, non-zero on error
 */
int run_cache_clean(bool clean_all = false, int max_age_days = 7, bool verbose = false);

/**
 * Invalidate cache for specific source files
 * Clears HIR, MIR, object, and test caches for the specified files.
 * This forces a full recompilation on the next build.
 *
 * @param files List of source file paths to invalidate
 * @param verbose Print detailed information
 * @return 0 on success, non-zero on error
 */
int run_cache_invalidate(const std::vector<std::string>& files, bool verbose = false);

/**
 * Enforce cache size limit using LRU eviction
 * If cache size exceeds max_size_mb, removes oldest files until under limit
 *
 * @param max_size_mb Maximum cache size in megabytes (default: 1024 = 1GB)
 * @param verbose Print information about evicted files
 * @return Number of files removed
 */
int enforce_cache_limit(uintmax_t max_size_mb = 1024, bool verbose = false);

/**
 * F-031: LRU-evict the build-time cache dirs to their configured size caps.
 *
 * Called at the end of test/build/run to keep the cache bounded. Evicts, by
 * least-recently-accessed order, three independent layers under
 * `build/debug/cache`:
 *   - `tests/obj_cache`   (default cap 256 MB) — content-addressed backend objs
 *   - suite EXEs          (default cap 512 MB) — the `.exe` files directly under
 *                         `tests`; entries still referenced by `tests.json` are
 *                         evicted LAST so reusable EXEs survive longest
 *                         (unreferenced orphans go first). Sibling
 *                         `.lib/.pdb/.exp` are removed with the `.exe`.
 *   - `run`               (default cap 128 MB) — `tml run` output cache
 *
 * All three layers are content-addressed / self-healing: a deleted artifact is
 * simply regenerated on next use. Never touches source, `build/debug/bin`, or
 * the incr/ir store (phase42a owns that). Caps are overridable via env vars
 * `TML_CACHE_OBJ_CAP_MB`, `TML_CACHE_TESTS_CAP_MB`, `TML_CACHE_RUN_CAP_MB`.
 *
 * Logs a one-line before→after per layer that crossed its cap.
 */
void enforce_cache_caps();

/**
 * Main cache command dispatcher
 * Handles: tml cache info, tml cache clean, tml cache invalidate
 *
 * @param argc Argument count
 * @param argv Argument values (starting from "cache")
 * @return 0 on success, non-zero on error
 */
int run_cache(int argc, char* argv[]);

} // namespace tml::cli

#endif // TML_CLI_CMD_CACHE_HPP
