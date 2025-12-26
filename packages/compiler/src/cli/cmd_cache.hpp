#ifndef TML_CLI_CMD_CACHE_HPP
#define TML_CLI_CMD_CACHE_HPP

#include <string>

namespace tml::cli {

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
 * Enforce cache size limit using LRU eviction
 * If cache size exceeds max_size_mb, removes oldest files until under limit
 *
 * @param max_size_mb Maximum cache size in megabytes (default: 1024 = 1GB)
 * @param verbose Print information about evicted files
 * @return Number of files removed
 */
int enforce_cache_limit(uintmax_t max_size_mb = 1024, bool verbose = false);

/**
 * Main cache command dispatcher
 * Handles: tml cache info, tml cache clean
 *
 * @param argc Argument count
 * @param argv Argument values (starting from "cache")
 * @return 0 on success, non-zero on error
 */
int run_cache(int argc, char* argv[]);

} // namespace tml::cli

#endif // TML_CLI_CMD_CACHE_HPP
