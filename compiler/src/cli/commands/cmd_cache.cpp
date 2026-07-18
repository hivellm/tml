TML_MODULE("compiler")

//! # Cache Management Command
//!
//! This file implements the `tml cache` command for managing the build cache.
//!
//! ## Subcommands
//!
//! | Command          | Description                           |
//! |------------------|---------------------------------------|
//! | `cache info`     | Show cache statistics and location    |
//! | `cache clean`    | Remove old cache files (7+ days)      |
//! | `cache clean --all` | Remove all cache files             |
//! | `cache clean --days N` | Remove files older than N days   |
//!
//! ## Cache Statistics
//!
//! The `info` subcommand shows:
//! - Total cache size and file count
//! - Breakdown by file type (.obj, .exe, etc.)
//! - Detailed file listing with `--verbose`
//!
//! ## LRU Eviction
//!
//! `enforce_cache_limit()` implements LRU eviction when the cache
//! exceeds a size limit. Files are sorted by last access time and
//! the oldest are removed until under the limit.

#include "cmd_cache.hpp"

#include "cli/utils.hpp"
#include "log/log.hpp"
#include "query/query_incr.hpp"          // F-030: PrevSessionCache / IncrCacheWriter
#include "query/query_key.hpp"           // F-030: QueryKey file_path visitor
#include "testing/testing_test_cache.hpp" // F-030: targeted tests.json invalidation

#include <algorithm>
#include <chrono>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <set>
#include <sstream>
#include <variant>
#include <vector>

namespace fs = std::filesystem;

namespace tml::cli {

namespace {

/// Returns the run cache directory path (build/debug/cache/run/).
fs::path get_cache_dir() {
    fs::path cwd = fs::current_path();
    return cwd / "build" / "debug" / "cache" / "run";
}

/// Walk up from CWD to the project root (a dir with both lib/ and build/) and
/// return its `build/debug/cache` directory. Falls back to CWD-relative.
fs::path get_cache_root() {
    for (fs::path dir = fs::current_path();; dir = dir.parent_path()) {
        std::error_code e1, e2;
        if (fs::exists(dir / "lib", e1) && fs::exists(dir / "build", e2)) {
            return dir / "build" / "debug" / "cache";
        }
        if (dir.parent_path() == dir) {
            break;
        }
    }
    return fs::current_path() / "build" / "debug" / "cache";
}

/// Normalize a path to an absolute, forward-slash, (Windows: lowercased) form
/// so source arguments match the path forms stored in the various caches.
std::string normalize_path(const std::string& p) {
    std::error_code ec;
    fs::path fp = fs::weakly_canonical(fs::path(p), ec);
    std::string s = ec ? p : fp.string();
    std::replace(s.begin(), s.end(), '\\', '/');
#ifdef _WIN32
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
#endif
    return s;
}

/// Extract the `file_path` field common to every QueryKey variant.
std::string query_key_file_path(const tml::query::QueryKey& key) {
    return std::visit([](const auto& k) { return k.file_path; }, key);
}

/**
 * Format byte size in human-readable format
 */
std::string format_size(uintmax_t bytes) {
    const char* units[] = {"B", "KB", "MB", "GB"};
    int unit_index = 0;
    double size = static_cast<double>(bytes);

    while (size >= 1024.0 && unit_index < 3) {
        size /= 1024.0;
        unit_index++;
    }

    std::ostringstream oss;
    oss << std::fixed << std::setprecision(2) << size << " " << units[unit_index];
    return oss.str();
}

/**
 * Calculate total size of directory
 */
[[maybe_unused]] uintmax_t calculate_directory_size(const fs::path& dir) {
    uintmax_t total = 0;

    if (!fs::exists(dir)) {
        return 0;
    }

    try {
        for (const auto& entry : fs::recursive_directory_iterator(dir)) {
            if (fs::is_regular_file(entry.path())) {
                total += fs::file_size(entry.path());
            }
        }
    } catch (const std::exception&) {
        // Ignore errors during iteration
    }

    return total;
}

/**
 * Count files by extension in cache
 */
struct CacheStats {
    int object_files = 0;
    int executable_files = 0;
    int cache_files = 0;
    int other_files = 0;
    uintmax_t total_size = 0;
};

CacheStats gather_cache_stats(const fs::path& cache_dir) {
    CacheStats stats;

    if (!fs::exists(cache_dir)) {
        return stats;
    }

    try {
        for (const auto& entry : fs::directory_iterator(cache_dir)) {
            if (!fs::is_regular_file(entry.path())) {
                continue;
            }

            auto ext = entry.path().extension().string();
            uintmax_t size = fs::file_size(entry.path());
            stats.total_size += size;

            if (ext == ".obj" || ext == ".o") {
                stats.object_files++;
            } else if (ext == ".exe" || ext == "") {
                stats.executable_files++;
            } else if (entry.path().filename().string().find("-cache") != std::string::npos) {
                stats.cache_files++;
            } else {
                stats.other_files++;
            }
        }
    } catch (const std::exception&) {
        // Ignore errors
    }

    return stats;
}

/**
 * Get file age in days
 */
int get_file_age_days(const fs::path& file) {
    try {
        auto ftime = fs::last_write_time(file);
        auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
            ftime - fs::file_time_type::clock::now() + std::chrono::system_clock::now());
        auto now = std::chrono::system_clock::now();
        auto age = std::chrono::duration_cast<std::chrono::hours>(now - sctp);
        return static_cast<int>(age.count() / 24);
    } catch (...) {
        return 0;
    }
}

} // anonymous namespace

int run_cache_info(bool verbose) {
    fs::path cache_dir = get_cache_dir();

    if (!fs::exists(cache_dir)) {
        TML_LOG_INFO("cache", "Cache directory does not exist: " << cache_dir);
        TML_LOG_INFO("cache", "Cache is empty.");
        return 0;
    }

    TML_LOG_INFO("cache", "TML Build Cache Information");
    TML_LOG_INFO("cache", "===========================");
    TML_LOG_INFO("cache", "Cache location: " << cache_dir);

    // Gather statistics
    CacheStats stats = gather_cache_stats(cache_dir);

    int total_files =
        stats.object_files + stats.executable_files + stats.cache_files + stats.other_files;
    TML_LOG_INFO("cache", "Cache statistics:");
    TML_LOG_INFO("cache", "  Object files (.obj):     " << stats.object_files);
    TML_LOG_INFO("cache", "  Executable files (.exe): " << stats.executable_files);
    TML_LOG_INFO("cache", "  Cache metadata files:    " << stats.cache_files);
    TML_LOG_INFO("cache", "  Other files:             " << stats.other_files);
    TML_LOG_INFO("cache", "  Total files:             " << total_files);
    TML_LOG_INFO("cache", "  Total size:              " << format_size(stats.total_size));

    if (verbose) {
        TML_LOG_INFO("cache", "Cache contents:");

        std::vector<fs::directory_entry> entries;
        for (const auto& entry : fs::directory_iterator(cache_dir)) {
            if (fs::is_regular_file(entry.path())) {
                entries.push_back(entry);
            }
        }

        // Sort by modification time (newest first)
        std::sort(entries.begin(), entries.end(), [](const auto& a, const auto& b) {
            return fs::last_write_time(a) > fs::last_write_time(b);
        });

        int count = 0;
        for (const auto& entry : entries) {
            auto size = fs::file_size(entry.path());
            auto age_days = get_file_age_days(entry.path());

            TML_LOG_INFO("cache", "  " << entry.path().filename().string() << " ("
                                       << format_size(size) << ", " << age_days << " days old)");

            count++;
            if (count >= 20 && !verbose) {
                TML_LOG_INFO("cache", "  ... (" << (entries.size() - 20) << " more files)");
                break;
            }
        }
    }

    // Full cache-tree breakdown (F-030: info was previously blind to everything
    // except cache/run — report every layer so users see the real footprint).
    fs::path cache_root = get_cache_root();
    if (fs::exists(cache_root)) {
        std::cout << "Full cache tree: " << cache_root.string() << "\n";
        uintmax_t grand_total = 0;
        for (const char* sub : {"run", "incr", "tests", "meta", "build-scripts"}) {
            fs::path p = cache_root / sub;
            if (!fs::exists(p)) {
                continue;
            }
            uintmax_t sz = calculate_directory_size(p);
            grand_total += sz;
            std::cout << "  " << std::left << std::setw(14) << sub << format_size(sz) << "\n";
        }
        // Loose files directly under cache/ (e.g. *.obj, tests.json).
        uintmax_t loose = 0;
        std::error_code ec;
        for (const auto& entry : fs::directory_iterator(cache_root, ec)) {
            if (fs::is_regular_file(entry.path())) {
                loose += fs::file_size(entry.path());
            }
        }
        grand_total += loose;
        std::cout << "  " << std::left << std::setw(14) << "(loose files)" << format_size(loose)
                  << "\n";
        std::cout << "  " << std::left << std::setw(14) << "Grand total" << format_size(grand_total)
                  << "\n";
    }

    TML_LOG_INFO("cache", "Use 'tml cache clean' to remove old cached files (whole tree).");
    TML_LOG_INFO("cache", "Use 'tml cache clean --all' to remove all cached files.");

    return 0;
}

int run_cache_clean(bool clean_all, int max_age_days, bool /*verbose*/) {
    // F-030: clean the whole cache tree (run/, incr/, tests/, meta/, loose
    // files), not just cache/run/ as before.
    fs::path cache_dir = get_cache_root();

    if (!fs::exists(cache_dir)) {
        TML_LOG_INFO("cache", "Cache directory does not exist: " << cache_dir);
        TML_LOG_INFO("cache", "Nothing to clean.");
        return 0;
    }

    TML_LOG_INFO("cache", "Cleaning build cache...");

    if (clean_all) {
        TML_LOG_INFO("cache", "Removing all cached files from: " << cache_dir);
    } else {
        TML_LOG_INFO("cache",
                     "Removing files older than " << max_age_days << " days from: " << cache_dir);
    }

    int removed_count = 0;
    uintmax_t removed_size = 0;

    try {
        std::vector<fs::path> to_remove;

        std::error_code it_ec;
        for (auto it = fs::recursive_directory_iterator(
                 cache_dir, fs::directory_options::skip_permission_denied, it_ec);
             it != fs::recursive_directory_iterator(); it.increment(it_ec)) {
            if (it_ec) {
                break;
            }
            if (!fs::is_regular_file(it->path())) {
                continue;
            }

            bool should_remove = clean_all;

            if (!clean_all) {
                int age = get_file_age_days(it->path());
                should_remove = (age >= max_age_days);
            }

            if (should_remove) {
                to_remove.push_back(it->path());
            }
        }

        for (const auto& file : to_remove) {
            uintmax_t size = fs::file_size(file);

            TML_LOG_DEBUG("cache", "Removing: " << file.filename().string() << " ("
                                                << format_size(size) << ")");

            fs::remove(file);
            removed_count++;
            removed_size += size;
        }

        TML_LOG_INFO("cache",
                     "Cleaned " << removed_count << " files (" << format_size(removed_size) << ")");

    } catch (const std::exception& e) {
        TML_LOG_ERROR("cache", "Error cleaning cache: " << e.what());
        return 1;
    }

    return 0;
}

int enforce_cache_limit(uintmax_t max_size_mb, bool /*verbose*/) {
    fs::path cache_dir = get_cache_dir();

    if (!fs::exists(cache_dir)) {
        return 0; // No cache, nothing to do
    }

    // Convert MB to bytes
    uintmax_t max_size_bytes = max_size_mb * 1024 * 1024;

    // Collect all cache files with their sizes and timestamps
    struct FileInfo {
        fs::path path;
        uintmax_t size;
        fs::file_time_type last_access;
    };

    std::vector<FileInfo> files;
    uintmax_t total_size = 0;

    try {
        for (const auto& entry : fs::directory_iterator(cache_dir)) {
            if (!fs::is_regular_file(entry.path())) {
                continue;
            }

            uintmax_t size = fs::file_size(entry.path());
            auto last_write = fs::last_write_time(entry.path());

            files.push_back({entry.path(), size, last_write});
            total_size += size;
        }
    } catch (const std::exception&) {
        return 0; // Ignore errors during iteration
    }

    // Check if we need to evict
    if (total_size <= max_size_bytes) {
        return 0; // Under limit, nothing to do
    }

    TML_LOG_INFO("cache", "Cache size (" << format_size(total_size) << ") exceeds limit ("
                                         << format_size(max_size_bytes)
                                         << "), evicting old files...");

    // Sort files by last access time (oldest first) - LRU eviction
    std::sort(files.begin(), files.end(),
              [](const FileInfo& a, const FileInfo& b) { return a.last_access < b.last_access; });

    // Remove files until under limit
    int removed_count = 0;
    uintmax_t removed_size = 0;

    for (const auto& file_info : files) {
        if (total_size <= max_size_bytes) {
            break; // Under limit now
        }

        try {
            TML_LOG_DEBUG("cache", "Evicting: " << file_info.path.filename().string() << " ("
                                                << format_size(file_info.size) << ")");

            fs::remove(file_info.path);
            removed_count++;
            removed_size += file_info.size;
            total_size -= file_info.size;
        } catch (const std::exception&) {
            // Ignore removal errors
        }
    }

    if (removed_count > 0) {
        TML_LOG_INFO("cache", "Evicted " << removed_count << " files (" << format_size(removed_size)
                                         << "), cache size now: " << format_size(total_size));
    }

    return removed_count;
}

namespace {

/// Read a MB cap from an env var, falling back to `default_mb` when unset or
/// unparseable. Lets ops tune caps without a rebuild (F-031 "configurable caps").
uintmax_t cap_mb_from_env(const char* var, uintmax_t default_mb) {
    const char* v = std::getenv(var);
    if (!v || !*v) {
        return default_mb;
    }
    try {
        long long parsed = std::stoll(v);
        if (parsed > 0) {
            return static_cast<uintmax_t>(parsed);
        }
    } catch (...) {
        // fall through to default
    }
    return default_mb;
}

/// One evictable unit: a primary file plus any siblings deleted with it.
struct EvictUnit {
    fs::path path;                  // primary file (the one whose ext we filter on)
    uintmax_t size = 0;             // primary + sibling bytes
    fs::file_time_type last_access; // LRU key (newest sibling wins below)
    bool protected_ref = false;     // referenced by tests.json -> evict last
    std::vector<fs::path> siblings; // removed together with `path`
};

/// Sibling extensions removed alongside a compiled `.exe` so eviction leaves no
/// orphaned import-lib / debug-info / export files behind.
const char* const kExeSiblingExts[] = {".lib", ".pdb", ".exp", ".ilk"};

} // anonymous namespace

/**
 * LRU-evict files under `dir` until total tracked size <= `cap_mb` MB.
 *
 * @param dir          directory to sweep (must exist; no-op otherwise)
 * @param cap_mb       size cap in megabytes
 * @param recursive    walk subdirectories (obj_cache/run) vs top-level only (exes)
 * @param ext_filter   when non-empty, only files with this extension are units
 *                     (e.g. ".exe"); their siblings are folded into unit size
 * @param protect      normalized absolute paths evicted LAST (referenced exes)
 * @param label        human label for the one-line before->after log
 * @return bytes reclaimed
 */
uintmax_t evict_dir_to_cap(const fs::path& dir, uintmax_t cap_mb, bool recursive,
                           const std::string& ext_filter, const std::set<std::string>& protect,
                           const char* label) {
    std::error_code ec;
    if (!fs::exists(dir, ec)) {
        return 0;
    }

    const uintmax_t cap_bytes = cap_mb * 1024ull * 1024ull;
    const bool exe_mode = !ext_filter.empty();

    std::vector<EvictUnit> units;
    uintmax_t total = 0;

    auto consider_file = [&](const fs::path& p) {
        if (!fs::is_regular_file(p, ec)) {
            return;
        }
        if (!ext_filter.empty() && p.extension().string() != ext_filter) {
            return; // non-primary files are folded in via sibling scan below
        }
        EvictUnit unit;
        unit.path = p;
        unit.size = fs::file_size(p, ec);
        unit.last_access = fs::last_write_time(p, ec);

        if (exe_mode) {
            // Fold sibling artifacts (same stem) into this unit.
            for (const char* sx : kExeSiblingExts) {
                fs::path sib = p;
                sib.replace_extension(sx);
                if (fs::exists(sib, ec) && fs::is_regular_file(sib, ec)) {
                    unit.size += fs::file_size(sib, ec);
                    auto st = fs::last_write_time(sib, ec);
                    if (st > unit.last_access) {
                        unit.last_access = st;
                    }
                    unit.siblings.push_back(sib);
                }
            }
            std::string norm = p.string();
            std::replace(norm.begin(), norm.end(), '\\', '/');
#ifdef _WIN32
            std::transform(norm.begin(), norm.end(), norm.begin(),
                           [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
#endif
            unit.protected_ref = protect.count(norm) > 0;
        }

        total += unit.size;
        units.push_back(std::move(unit));
    };

    if (recursive) {
        for (auto it = fs::recursive_directory_iterator(
                 dir, fs::directory_options::skip_permission_denied, ec);
             it != fs::recursive_directory_iterator(); it.increment(ec)) {
            if (ec) {
                break;
            }
            consider_file(it->path());
        }
    } else {
        for (auto it = fs::directory_iterator(dir, fs::directory_options::skip_permission_denied,
                                              ec);
             it != fs::directory_iterator(); it.increment(ec)) {
            if (ec) {
                break;
            }
            consider_file(it->path());
        }
    }

    if (total <= cap_bytes) {
        return 0; // under cap, nothing to do
    }

    const uintmax_t before = total;

    // Eviction order: unprotected first, then protected; within each group,
    // least-recently-accessed first (LRU). Referenced/reusable exes thus survive
    // longest and unreferenced orphans die first.
    std::sort(units.begin(), units.end(), [](const EvictUnit& a, const EvictUnit& b) {
        if (a.protected_ref != b.protected_ref) {
            return !a.protected_ref; // unprotected sorts before protected
        }
        return a.last_access < b.last_access;
    });

    uintmax_t reclaimed = 0;
    int removed = 0;
    for (const auto& unit : units) {
        if (total <= cap_bytes) {
            break;
        }
        bool ok = fs::remove(unit.path, ec);
        if (!ok) {
            continue;
        }
        for (const auto& sib : unit.siblings) {
            fs::remove(sib, ec);
        }
        total -= unit.size;
        reclaimed += unit.size;
        ++removed;
    }

    if (reclaimed > 0) {
        TML_LOG_INFO("cache", "[evict] " << label << ": " << format_size(before) << " -> "
                                         << format_size(total) << " (cap " << cap_mb << " MB, "
                                         << removed << " evicted, " << format_size(reclaimed)
                                         << " reclaimed)");
    }
    return reclaimed;
}

void enforce_cache_caps() {
    const fs::path root = get_cache_root();
    std::error_code ec;
    if (!fs::exists(root, ec)) {
        return;
    }

    const uintmax_t obj_cap = cap_mb_from_env("TML_CACHE_OBJ_CAP_MB", 256);
    const uintmax_t tests_cap = cap_mb_from_env("TML_CACHE_TESTS_CAP_MB", 512);
    const uintmax_t run_cap = cap_mb_from_env("TML_CACHE_RUN_CAP_MB", 128);

    // Build the protected set: exe paths still referenced by tests.json. These
    // are reusable across runs (source unchanged), so they are evicted last.
    std::set<std::string> protected_exes;
    {
        fs::path tests_json = root / "tests.json";
        if (fs::exists(tests_json, ec)) {
            tml::testing::TestResultCache trc;
            if (trc.load(tests_json.string())) {
                for (const auto& exe : trc.referenced_exes()) {
                    std::string norm = exe;
                    std::replace(norm.begin(), norm.end(), '\\', '/');
#ifdef _WIN32
                    std::transform(norm.begin(), norm.end(), norm.begin(),
                                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
#endif
                    protected_exes.insert(std::move(norm));
                }
            }
        }
    }

    // 1. Backend object cache — content-addressed, recursive, no protected set.
    evict_dir_to_cap(root / "tests" / "obj_cache", obj_cap, /*recursive=*/true,
                     /*ext_filter=*/"", /*protect=*/{}, "tests/obj_cache");

    // 2. Compiled suite EXEs — top-level `.exe` under tests/ (never descend into
    //    obj_cache); unreferenced orphans first, referenced exes last.
    evict_dir_to_cap(root / "tests", tests_cap, /*recursive=*/false, /*ext_filter=*/".exe",
                     protected_exes, "tests/*.exe");

    // 3. `tml run` output cache — content-addressed, self-healing.
    evict_dir_to_cap(root / "run", run_cap, /*recursive=*/true, /*ext_filter=*/"", /*protect=*/{},
                     "run");
}

int run_cache_invalidate(const std::vector<std::string>& files, bool /*verbose*/) {
    if (files.empty()) {
        TML_LOG_ERROR(
            "cache",
            "No files specified for invalidation. Usage: tml cache invalidate <file1> [file2] ...");
        return 1;
    }

    const fs::path cache_root = get_cache_root();
    const fs::path tests_json = cache_root / "tests.json";
    const fs::path incr_dir = cache_root / "incr";
    const fs::path incr_bin = incr_dir / "incr.bin";

    int files_touched = 0;

    // User-facing report goes to stdout so it is visible regardless of log level.
    std::cout << "Invalidating cache for " << files.size() << " file(s)...\n";
    std::cout << "  Cache root: " << cache_root.string() << "\n";

    for (const auto& file : files) {
        const std::string abs_source = [&] {
            std::error_code ec;
            fs::path fp = fs::weakly_canonical(fs::path(file), ec);
            return ec ? fs::absolute(fs::path(file)).string() : fp.string();
        }();
        const std::string target = normalize_path(file);

        int removed_ast = 0, removed_exe = 0, removed_incr = 0, removed_ir = 0;

        // --- Layer 1: `<source>.ast.bin` sidecar (deterministic, beside source) ---
        {
            fs::path sidecar(abs_source + ".ast.bin");
            std::error_code ec;
            if (fs::exists(sidecar, ec) && fs::remove(sidecar, ec)) {
                ++removed_ast;
                std::cout << "  [ast.bin] removed " << sidecar.filename().string() << "\n";
            }
        }

        // --- Layer 2: test result cache — tests.json entries + suite EXEs ---
        if (fs::exists(tests_json)) {
            tml::testing::TestResultCache trc;
            if (trc.load(tests_json.string())) {
                // Do NOT touch compiler_hash — that is phase41c's global lever.
                auto cleared = trc.invalidate_source(abs_source);
                for (const auto& exe : cleared) {
                    std::error_code ec;
                    if (fs::exists(exe, ec) && fs::remove(exe, ec)) {
                        ++removed_exe;
                        std::cout << "  [tests] removed exe " << exe << "\n";
                    }
                    // Drop the sibling import library, if any (<stem>.lib).
                    fs::path lib = fs::path(exe);
                    lib.replace_extension(".lib");
                    if (fs::exists(lib, ec)) {
                        fs::remove(lib, ec);
                    }
                }
                if (!cleared.empty()) {
                    trc.save(tests_json.string());
                }
            }
        }

        // --- Layer 3: incremental cache — incr.bin entries + ir/*.ll sidecars ---
        if (fs::exists(incr_bin)) {
            tml::query::PrevSessionCache prev;
            if (prev.load(incr_bin)) {
                tml::query::IncrCacheWriter writer;
                for (const auto& [key, entry] : prev.entries()) {
                    if (normalize_path(query_key_file_path(key)) == target) {
                        // Drop this entry: delete its IR sidecars.
                        auto base = tml::query::get_ir_cache_filename(key);
                        for (const char* suffix : {".ll", ".libs", ".search_paths"}) {
                            fs::path irf = incr_dir / "ir" / (base + suffix);
                            std::error_code ec;
                            if (fs::exists(irf, ec) && fs::remove(irf, ec)) {
                                ++removed_ir;
                            }
                        }
                        ++removed_incr;
                    } else {
                        // Keep: re-record verbatim into the rewritten cache.
                        writer.record(key, entry.input_fingerprint, entry.output_fingerprint,
                                      entry.dependencies);
                    }
                }
                if (removed_incr > 0) {
                    writer.write(incr_bin, prev.options_hash());
                    std::cout << "  [incr] dropped " << removed_incr << " entr"
                              << (removed_incr == 1 ? "y" : "ies") << ", " << removed_ir
                              << " ir file(s)\n";
                }
            }
        }

        int total = removed_ast + removed_exe + removed_incr + removed_ir;
        if (total > 0) {
            ++files_touched;
            std::cout << "  Invalidated " << file << ": " << removed_ast << " ast.bin, "
                      << removed_exe << " exe, " << removed_incr << " incr entries, " << removed_ir
                      << " ir files\n";
        } else {
            std::cout << "  No cached artifacts found for: " << file << "\n";
        }
    }

    // The run/ and obj_cache/ layers are content-addressed (hashed on generated
    // IR / object content with no stored source path), so a single source cannot
    // be mapped to its entries without recompiling. They are self-healing — a
    // changed source produces a different hash, so stale entries are simply never
    // hit again. Use `cache clean --all` to reclaim their space.
    std::cout << "Invalidated artifacts for " << files_touched << " of " << files.size()
              << " file(s).\n";
    std::cout << "Note: run/ and obj_cache/ are content-addressed and self-healing; "
                 "use 'cache clean --all' to reclaim their space.\n";
    return 0;
}

int run_cache_clear_meta() {
    // Find project root (walk up looking for lib/)
    fs::path project_root;
    for (auto dir = fs::current_path(); !dir.empty() && dir.has_parent_path();
         dir = dir.parent_path()) {
        if (fs::exists(dir / "lib") && fs::exists(dir / "build")) {
            project_root = dir;
            break;
        }
    }

    if (project_root.empty()) {
        TML_LOG_ERROR("cache", "Could not find project root (no lib/ + build/ found)");
        return 1;
    }

    // Clear meta cache in all build configurations
    int total_removed = 0;
    for (const auto& config : {"debug", "release"}) {
        fs::path meta_dir = project_root / "build" / config / "cache" / "meta";
        if (!fs::exists(meta_dir)) {
            continue;
        }

        std::error_code ec;
        auto count = fs::remove_all(meta_dir, ec);
        if (!ec && count > 0) {
            total_removed += static_cast<int>(count);
            TML_LOG_INFO("cache", "Removed " << count << " files from " << meta_dir);
        }
    }

    if (total_removed == 0) {
        TML_LOG_INFO("cache", "No module metadata cache files found.");
    } else {
        TML_LOG_INFO(
            "cache",
            "Cleared " << total_removed
                       << " module metadata cache entries. Next build will re-parse all modules.");
    }

    return 0;
}

int run_cache(int argc, char* argv[]) {
    if (argc < 3) {
        std::cerr << "Usage: tml cache <subcommand> [options]\n";
        std::cerr << "\n";
        std::cerr << "Subcommands:\n";
        std::cerr << "  info                     Show cache statistics and information\n";
        std::cerr << "  clean                    Remove old cache files (7+ days)\n";
        std::cerr << "  clean --all              Remove all cache files\n";
        std::cerr << "  clean --days <N>         Remove files older than N days\n";
        std::cerr << "  invalidate <file> ...    Invalidate cache for specific files\n";
        std::cerr << "  clear-meta               Remove all module metadata (.tml.meta) caches\n";
        std::cerr << "\n";
        std::cerr << "Options:\n";
        std::cerr << "  --verbose, -v            Show detailed information\n";
        return 1;
    }

    std::string subcommand = argv[2];
    bool verbose = false;
    bool clean_all = false;
    int max_age_days = 7;
    std::vector<std::string> files;

    // Parse options
    for (int i = 3; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "--verbose" || arg == "-v") {
            verbose = true;
        } else if (arg == "--all" || arg == "-a") {
            clean_all = true;
        } else if (arg == "--days" || arg == "-d") {
            if (i + 1 < argc) {
                max_age_days = std::stoi(argv[i + 1]);
                i++; // Skip next arg
            } else {
                TML_LOG_ERROR("cache", "--days requires a number");
                return 1;
            }
        } else if (!arg.starts_with("-")) {
            // Collect file arguments for invalidate command
            files.push_back(arg);
        }
    }

    if (subcommand == "info") {
        return run_cache_info(verbose);
    } else if (subcommand == "clean") {
        return run_cache_clean(clean_all, max_age_days, verbose);
    } else if (subcommand == "invalidate") {
        return run_cache_invalidate(files, verbose);
    } else if (subcommand == "clear-meta") {
        return run_cache_clear_meta();
    } else {
        TML_LOG_ERROR("cache", "Unknown cache subcommand: "
                                   << subcommand
                                   << ". Use 'tml cache info', 'tml cache clean', 'tml cache "
                                      "invalidate', or 'tml cache clear-meta'");
        return 1;
    }
}

} // namespace tml::cli
