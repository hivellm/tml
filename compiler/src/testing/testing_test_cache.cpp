TML_MODULE("test")

//! # Test Result Cache — Implementation
//!
//! Suite-level test result caching. Stores results in a JSON file and uses
//! CRC32C hashing for content verification.

#include "testing/testing_test_cache.hpp"

#include "common/crc32c.hpp"
#include "log/log.hpp"
#include "testing/testing_coordinator.hpp"
#include "version_generated.hpp" // tml::VERSION (content-aware compiler hash)

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <set>
#include <sstream>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

namespace fs = std::filesystem;

namespace tml::testing {

namespace {

// ============================================================================
// Minimal JSON writing helpers
// ============================================================================

void write_json_string(std::ostream& os, const std::string& s) {
    os << '"';
    for (char c : s) {
        if (c == '"')
            os << "\\\"";
        else if (c == '\\')
            os << "\\\\";
        else if (c == '\n')
            os << "\\n";
        else if (c == '\r')
            os << "\\r";
        else if (c == '\t')
            os << "\\t";
        else
            os << c;
    }
    os << '"';
}

void write_json_string_array(std::ostream& os, const std::vector<std::string>& arr) {
    os << '[';
    for (size_t i = 0; i < arr.size(); ++i) {
        if (i > 0)
            os << ',';
        write_json_string(os, arr[i]);
    }
    os << ']';
}

// ============================================================================
// Minimal JSON reading helpers
// ============================================================================

size_t skip_ws(const std::string& s, size_t pos) {
    while (pos < s.size() && (s[pos] == ' ' || s[pos] == '\t' || s[pos] == '\n' || s[pos] == '\r'))
        ++pos;
    return pos;
}

// Parse a JSON string starting at pos (which should point to the opening quote).
// Returns {value, next_pos} or {"", npos} on error.
std::pair<std::string, size_t> read_json_string(const std::string& s, size_t pos) {
    pos = skip_ws(s, pos);
    if (pos >= s.size() || s[pos] != '"')
        return {"", std::string::npos};
    ++pos; // skip opening quote

    std::string result;
    while (pos < s.size() && s[pos] != '"') {
        if (s[pos] == '\\' && pos + 1 < s.size()) {
            ++pos;
            if (s[pos] == '"')
                result += '"';
            else if (s[pos] == '\\')
                result += '\\';
            else if (s[pos] == 'n')
                result += '\n';
            else if (s[pos] == 'r')
                result += '\r';
            else if (s[pos] == 't')
                result += '\t';
            else
                result += s[pos];
        } else {
            result += s[pos];
        }
        ++pos;
    }
    if (pos >= s.size())
        return {"", std::string::npos};
    ++pos; // skip closing quote
    return {result, pos};
}

std::pair<int64_t, size_t> read_json_int(const std::string& s, size_t pos) {
    pos = skip_ws(s, pos);
    if (pos >= s.size())
        return {0, std::string::npos};

    bool negative = false;
    if (s[pos] == '-') {
        negative = true;
        ++pos;
    }

    int64_t val = 0;
    size_t start = pos;
    while (pos < s.size() && s[pos] >= '0' && s[pos] <= '9') {
        val = val * 10 + (s[pos] - '0');
        ++pos;
    }
    if (pos == start)
        return {0, std::string::npos};
    return {negative ? -val : val, pos};
}

std::pair<bool, size_t> read_json_bool(const std::string& s, size_t pos) {
    pos = skip_ws(s, pos);
    if (pos + 4 <= s.size() && s.substr(pos, 4) == "true")
        return {true, pos + 4};
    if (pos + 5 <= s.size() && s.substr(pos, 5) == "false")
        return {false, pos + 5};
    return {false, std::string::npos};
}

std::pair<std::vector<std::string>, size_t> read_json_string_array(const std::string& s,
                                                                   size_t pos) {
    pos = skip_ws(s, pos);
    if (pos >= s.size() || s[pos] != '[')
        return {{}, std::string::npos};
    ++pos;

    std::vector<std::string> result;
    pos = skip_ws(s, pos);
    if (pos < s.size() && s[pos] == ']')
        return {result, pos + 1};

    while (true) {
        auto [val, next] = read_json_string(s, pos);
        if (next == std::string::npos)
            return {{}, std::string::npos};
        result.push_back(std::move(val));
        pos = skip_ws(s, next);
        if (pos >= s.size())
            return {{}, std::string::npos};
        if (s[pos] == ']')
            return {result, pos + 1};
        if (s[pos] != ',')
            return {{}, std::string::npos};
        ++pos; // skip comma
    }
}

// Skip a JSON value (string, number, bool, array, object) starting at pos.
size_t skip_json_value(const std::string& s, size_t pos) {
    pos = skip_ws(s, pos);
    if (pos >= s.size())
        return std::string::npos;

    if (s[pos] == '"') {
        auto [_, next] = read_json_string(s, pos);
        return next;
    }
    if (s[pos] == '[') {
        ++pos;
        int depth = 1;
        while (pos < s.size() && depth > 0) {
            if (s[pos] == '[')
                ++depth;
            else if (s[pos] == ']')
                --depth;
            else if (s[pos] == '"') {
                auto [_, next] = read_json_string(s, pos);
                if (next == std::string::npos)
                    return std::string::npos;
                pos = next;
                continue;
            }
            ++pos;
        }
        return pos;
    }
    if (s[pos] == '{') {
        ++pos;
        int depth = 1;
        while (pos < s.size() && depth > 0) {
            if (s[pos] == '{')
                ++depth;
            else if (s[pos] == '}')
                --depth;
            else if (s[pos] == '"') {
                auto [_, next] = read_json_string(s, pos);
                if (next == std::string::npos)
                    return std::string::npos;
                pos = next;
                continue;
            }
            ++pos;
        }
        return pos;
    }
    // number, true, false, null
    while (pos < s.size() && s[pos] != ',' && s[pos] != '}' && s[pos] != ']' && s[pos] != ' ' &&
           s[pos] != '\n' && s[pos] != '\r' && s[pos] != '\t')
        ++pos;
    return pos;
}

// Parse a suite cache entry from a JSON object.
// pos should point just past the opening '{'.
std::pair<SuiteCacheEntry, size_t> parse_suite_entry(const std::string& s, size_t pos) {
    SuiteCacheEntry entry;
    pos = skip_ws(s, pos);

    while (pos < s.size() && s[pos] != '}') {
        auto [key, kpos] = read_json_string(s, pos);
        if (kpos == std::string::npos)
            return {entry, std::string::npos};
        pos = skip_ws(s, kpos);
        if (pos >= s.size() || s[pos] != ':')
            return {entry, std::string::npos};
        ++pos;

        if (key == "source_hashes") {
            auto [arr, next] = read_json_string_array(s, pos);
            if (next == std::string::npos)
                return {entry, std::string::npos};
            entry.source_hashes = std::move(arr);
            pos = next;
        } else if (key == "flags_hash") {
            auto [val, next] = read_json_string(s, pos);
            if (next == std::string::npos)
                return {entry, std::string::npos};
            entry.flags_hash = std::move(val);
            pos = next;
        } else if (key == "all_passed") {
            auto [val, next] = read_json_bool(s, pos);
            if (next == std::string::npos)
                return {entry, std::string::npos};
            entry.all_passed = val;
            pos = next;
        } else if (key == "test_count") {
            auto [val, next] = read_json_int(s, pos);
            if (next == std::string::npos)
                return {entry, std::string::npos};
            entry.test_count = static_cast<int>(val);
            pos = next;
        } else if (key == "passed_count") {
            auto [val, next] = read_json_int(s, pos);
            if (next == std::string::npos)
                return {entry, std::string::npos};
            entry.passed_count = static_cast<int>(val);
            pos = next;
        } else if (key == "failed_count") {
            auto [val, next] = read_json_int(s, pos);
            if (next == std::string::npos)
                return {entry, std::string::npos};
            entry.failed_count = static_cast<int>(val);
            pos = next;
        } else if (key == "duration_us") {
            auto [val, next] = read_json_int(s, pos);
            if (next == std::string::npos)
                return {entry, std::string::npos};
            entry.duration_us = val;
            pos = next;
        } else if (key == "compile_time_us") {
            auto [val, next] = read_json_int(s, pos);
            if (next == std::string::npos)
                return {entry, std::string::npos};
            entry.compile_time_us = val;
            pos = next;
        } else if (key == "exe_path") {
            auto [val, next] = read_json_string(s, pos);
            if (next == std::string::npos)
                return {entry, std::string::npos};
            entry.exe_path = std::move(val);
            pos = next;
        } else if (key == "source_paths") {
            // Phase 8.5 W5: transitive source file paths for re-hashing
            auto [arr, next] = read_json_string_array(s, pos);
            if (next == std::string::npos)
                return {entry, std::string::npos};
            entry.source_paths = std::move(arr);
            pos = next;
        } else {
            pos = skip_json_value(s, pos);
            if (pos == std::string::npos)
                return {entry, std::string::npos};
        }

        pos = skip_ws(s, pos);
        if (pos < s.size() && s[pos] == ',')
            ++pos;
        pos = skip_ws(s, pos);
    }

    if (pos < s.size() && s[pos] == '}')
        ++pos;
    return {entry, pos};
}

} // anonymous namespace

// ============================================================================
// TestResultCache implementation
// ============================================================================

bool TestResultCache::load(const std::string& cache_file) {
    std::ifstream ifs(cache_file);
    if (!ifs)
        return false;

    std::ostringstream oss;
    oss << ifs.rdbuf();
    std::string content = oss.str();

    if (content.empty())
        return false;

    // Parse top-level JSON object
    size_t pos = skip_ws(content, 0);
    if (pos >= content.size() || content[pos] != '{')
        return false;
    ++pos;

    entries_.clear();
    compiler_hash_.clear();

    while (pos < content.size() && content[pos] != '}') {
        pos = skip_ws(content, pos);
        auto [key, kpos] = read_json_string(content, pos);
        if (kpos == std::string::npos)
            return false;
        pos = skip_ws(content, kpos);
        if (pos >= content.size() || content[pos] != ':')
            return false;
        ++pos;

        if (key == "version") {
            auto [val, next] = read_json_int(content, pos);
            if (next == std::string::npos || val != 1)
                return false;
            pos = next;
        } else if (key == "compiler_hash") {
            auto [val, next] = read_json_string(content, pos);
            if (next == std::string::npos)
                return false;
            compiler_hash_ = std::move(val);
            pos = next;
        } else if (key == "suites") {
            pos = skip_ws(content, pos);
            if (pos >= content.size() || content[pos] != '{')
                return false;
            ++pos;

            while (pos < content.size() && content[pos] != '}') {
                pos = skip_ws(content, pos);
                auto [suite_name, spos] = read_json_string(content, pos);
                if (spos == std::string::npos)
                    return false;
                pos = skip_ws(content, spos);
                if (pos >= content.size() || content[pos] != ':')
                    return false;
                ++pos;
                pos = skip_ws(content, pos);
                if (pos >= content.size() || content[pos] != '{')
                    return false;
                ++pos;

                auto [entry, epos] = parse_suite_entry(content, pos);
                if (epos == std::string::npos)
                    return false;
                entries_[suite_name] = std::move(entry);
                pos = skip_ws(content, epos);
                if (pos < content.size() && content[pos] == ',')
                    ++pos;
                pos = skip_ws(content, pos);
            }
            if (pos < content.size())
                ++pos; // skip '}'
        } else {
            pos = skip_json_value(content, pos);
            if (pos == std::string::npos)
                return false;
        }

        pos = skip_ws(content, pos);
        if (pos < content.size() && content[pos] == ',')
            ++pos;
        pos = skip_ws(content, pos);
    }

    TML_LOG_DEBUG("test", "[cache] Loaded " << entries_.size() << " cached suites");
    return true;
}

bool TestResultCache::save(const std::string& cache_file) const {
    // Ensure parent directory exists
    fs::path path(cache_file);
    if (path.has_parent_path()) {
        std::error_code ec;
        fs::create_directories(path.parent_path(), ec);
    }

    // Build the entire JSON in memory first, then write atomically
    std::ostringstream ss;
    ss << "{\n";
    ss << "  \"version\": 1,\n";
    ss << "  \"compiler_hash\": ";
    write_json_string(ss, compiler_hash_);
    ss << ",\n";
    ss << "  \"suites\": {\n";

    size_t idx = 0;
    for (const auto& [name, entry] : entries_) {
        ss << "    ";
        write_json_string(ss, name);
        ss << ": {\n";
        ss << "      \"source_hashes\": ";
        write_json_string_array(ss, entry.source_hashes);
        ss << ",\n";
        ss << "      \"flags_hash\": ";
        write_json_string(ss, entry.flags_hash);
        ss << ",\n";
        ss << "      \"all_passed\": " << (entry.all_passed ? "true" : "false") << ",\n";
        ss << "      \"test_count\": " << entry.test_count << ",\n";
        ss << "      \"passed_count\": " << entry.passed_count << ",\n";
        ss << "      \"failed_count\": " << entry.failed_count << ",\n";
        ss << "      \"duration_us\": " << entry.duration_us << ",\n";
        ss << "      \"compile_time_us\": " << entry.compile_time_us << ",\n";
        ss << "      \"exe_path\": ";
        write_json_string(ss, entry.exe_path);
        // Phase 8.5 W5: persist transitive source paths when available
        if (!entry.source_paths.empty()) {
            ss << ",\n";
            ss << "      \"source_paths\": ";
            write_json_string_array(ss, entry.source_paths);
        }
        ss << "\n";
        ss << "    }";
        if (++idx < entries_.size()) {
            ss << ",";
        }
        ss << "\n";
    }

    ss << "  }\n";
    ss << "}\n";

    // Write to temp file, then rename for atomicity
    std::string tmp_file = cache_file + ".tmp";
    {
        std::ofstream ofs(tmp_file, std::ios::binary | std::ios::trunc);
        if (!ofs) {
            return false;
        }
        std::string content = ss.str();
        ofs.write(content.data(), static_cast<std::streamsize>(content.size()));
        ofs.flush();
        if (!ofs.good()) {
            return false;
        }
    } // close file before rename

    std::error_code ec;
    fs::rename(tmp_file, cache_file, ec);
    if (ec) {
        // rename failed (cross-device?), try copy
        fs::copy_file(tmp_file, cache_file, fs::copy_options::overwrite_existing, ec);
        fs::remove(tmp_file, ec);
    }

    TML_LOG_DEBUG("test", "[cache] Saved " << entries_.size() << " suite entries");
    return true;
}

bool TestResultCache::is_cached(const std::string& suite_name,
                                const std::vector<std::string>& source_hashes,
                                const std::string& flags_hash) const {
    auto it = entries_.find(suite_name);
    if (it == entries_.end())
        return false;

    const auto& entry = it->second;

    // Rule 1: Must have passed last time (Go's cache-passing-only rule)
    if (!entry.all_passed)
        return false;

    // Rule 2: Flags must match (coverage, optimization, etc.)
    if (entry.flags_hash != flags_hash)
        return false;

    // Rule 3: Source hashes must match exactly (same files, same content)
    if (entry.source_hashes != source_hashes)
        return false;

    // Rule 4: Compiler hash must match (checked at load time via compiler_hash_)
    // This is validated externally by the coordinator comparing compiler_hash()
    // with the current compiler hash. If they differ, the entire cache is invalidated.

    return true;
}

const SuiteCacheEntry* TestResultCache::get(const std::string& suite_name) const {
    auto it = entries_.find(suite_name);
    if (it == entries_.end())
        return nullptr;
    return &it->second;
}

std::string TestResultCache::get_reusable_exe(const std::string& suite_name,
                                              const std::vector<std::string>& source_hashes,
                                              const std::string& flags_hash) const {
    auto it = entries_.find(suite_name);
    if (it == entries_.end())
        return {};

    const auto& entry = it->second;

    // Source must be unchanged
    if (entry.source_hashes != source_hashes)
        return {};

    // Flags must match (coverage mode produces different instrumentation)
    if (entry.flags_hash != flags_hash)
        return {};

    // Exe must actually exist on disk
    if (entry.exe_path.empty())
        return {};

    std::error_code ec;
    if (!std::filesystem::exists(entry.exe_path, ec))
        return {};

    return entry.exe_path;
}

void TestResultCache::update(const std::string& suite_name, const SuiteCacheEntry& entry) {
    entries_[suite_name] = entry;
}

int TestResultCache::invalidate_all_exes() {
    // Sibling artifacts written next to each compiled suite EXE.
    static const char* const sibling_exts[] = {".lib", ".pdb", ".exp", ".ilk"};
    int removed = 0;
    for (auto& [name, entry] : entries_) {
        entry.all_passed = false;
        if (!entry.exe_path.empty()) {
            std::error_code ec;
            fs::path exe(entry.exe_path);
            if (fs::exists(exe, ec) && fs::remove(exe, ec)) {
                ++removed;
            }
            for (const char* sx : sibling_exts) {
                fs::path sib = exe;
                sib.replace_extension(sx);
                fs::remove(sib, ec); // best-effort; siblings may not exist
            }
        }
        entry.exe_path.clear();
    }
    if (removed > 0) {
        TML_LOG_INFO("test", "[cache] invalidate_all_exes: deleted " << removed
                                                                     << " stale EXE file(s)");
    }
    return removed;
}

std::vector<std::string> TestResultCache::referenced_exes() const {
    std::vector<std::string> out;
    for (const auto& [name, entry] : entries_) {
        if (!entry.exe_path.empty()) {
            out.push_back(entry.exe_path);
        }
    }
    return out;
}

std::vector<std::string> TestResultCache::invalidate_source(const std::string& source_path) {
    // Normalize to an absolute, forward-slash, (Windows: lowercased) form so a
    // relative/backslash argument matches the absolute paths stored in
    // `source_paths` regardless of slash direction or case.
    auto norm = [](const std::string& p) {
        std::error_code ec;
        fs::path fp = fs::weakly_canonical(fs::path(p), ec);
        std::string s = ec ? p : fp.string();
        std::replace(s.begin(), s.end(), '\\', '/');
#ifdef _WIN32
        std::transform(s.begin(), s.end(), s.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
#endif
        return s;
    };

    const std::string target = norm(source_path);
    std::vector<std::string> cleared_exes;
    std::vector<std::string> to_erase;

    for (const auto& [name, entry] : entries_) {
        bool match = false;
        for (const auto& sp : entry.source_paths) {
            if (norm(sp) == target) {
                match = true;
                break;
            }
        }
        if (match) {
            if (!entry.exe_path.empty()) {
                cleared_exes.push_back(entry.exe_path);
            }
            to_erase.push_back(name);
        }
    }

    for (const auto& name : to_erase) {
        entries_.erase(name);
    }
    return cleared_exes;
}

// ============================================================================
// Static utility functions
// ============================================================================

std::string TestResultCache::compute_compiler_hash() {
    // phase41c / F-014: content-aware, computed ONCE per process.
    //
    // The old key was `mtime:size` of the compiler DLL. mtime changes on EVERY
    // relink (and even a `touch`) regardless of whether the emitted bytes — and
    // therefore codegen behaviour — actually changed, so any build wiped the
    // whole EXE cache via the coordinator's invalidate_all_exes(). The dominant
    // workflow here is compiler development (frequent rebuilds), so in practice
    // the cache was cold on the first run after every build.
    //
    // The new key is the compiler VERSION string plus a CRC32C of the DLL's
    // *content*. It changes iff the emitted bytes change, so a no-op relink or a
    // `touch` that produces byte-identical output keeps the cache warm, while any
    // real codegen change still invalidates (correctness bias: content differs →
    // invalidate). Hashing the ~71 MB DLL costs ~tens of ms; we memoise so it
    // happens exactly once per process (a long-lived daemon restarts on a DLL
    // mtime change, so a memoised value never goes stale within one process).
    static std::once_flag once;
    static std::string cached_hash;

    std::call_once(once, []() {
        std::error_code ec;

        // Build candidate paths for the compiler binary/DLL
        std::vector<fs::path> compiler_candidates;

#ifdef _WIN32
        char path[MAX_PATH];
        DWORD len = GetModuleFileNameA(nullptr, path, MAX_PATH);
        if (len > 0 && len < MAX_PATH) {
            fs::path exe(std::string(path, len));
            compiler_candidates.push_back(exe.parent_path() / "plugins" / "tml_compiler.dll");
            compiler_candidates.push_back(exe);
        }
#else
        auto exe_path = fs::read_symlink("/proc/self/exe", ec);
        if (!ec) {
            compiler_candidates.push_back(exe_path);
        }
#endif

        // Also try CWD-relative paths for both debug and release
        auto cwd = fs::current_path(ec);
        if (!ec) {
            compiler_candidates.push_back(cwd / "build" / "debug" / "bin" / "plugins" /
                                          "tml_compiler.dll");
            compiler_candidates.push_back(cwd / "build" / "debug" / "bin" / "tml.exe");
        }

        // Helper: CRC32C the *content* of the first existing candidate.
        auto content_hash_first_existing = [&](const std::vector<fs::path>& paths) -> std::string {
            for (const auto& p : paths) {
                if (!fs::exists(p, ec) || ec) {
                    continue;
                }
                auto fsize = fs::file_size(p, ec);
                if (ec || fsize == 0) {
                    continue;
                }
                // crc32c_file streams the whole file — content-addressed, so an
                // identical relink yields an identical hash (unlike mtime).
                auto h = crc32c_file(p.string());
                if (!h.empty()) {
                    // Fold the size in too so a truncated/padded file can't collide.
                    return h + ":" + std::to_string(fsize);
                }
            }
            return "";
        };

        // Content-hash the compiler binary
        std::string compiler_fp = content_hash_first_existing(compiler_candidates);
        if (compiler_fp.empty()) {
            TML_LOG_WARN("test", "[cache] Could not compute compiler hash from any candidate");
            cached_hash = "unknown";
            return;
        }

        // Build candidate paths for the test runtime lib (debug and release).
        // Content-hash it when present so a runtime change invalidates too.
        std::vector<fs::path> runtime_candidates;
        if (!ec) { // cwd is still valid
            runtime_candidates.push_back(cwd / "build" / "debug" / "cache" /
                                         "tml_test_runtime.lib");
            runtime_candidates.push_back(cwd / "build" / "release" / "cache" /
                                         "tml_test_runtime.lib");
        }
        std::string runtime_fp = content_hash_first_existing(runtime_candidates);

        // Combine VERSION + content hashes into the final key.
        std::string combined = "version:";
        combined += tml::VERSION;
        combined += "+compiler:" + compiler_fp;
        if (!runtime_fp.empty()) {
            combined += "+runtime:" + runtime_fp;
        }
        cached_hash = crc32c_hex(combined.data(), combined.size());
    });

    return cached_hash;
}

std::string TestResultCache::compute_flags_hash(const TestConfig& config) {
    // Combine flags that affect test grouping/execution into a single string.
    std::string flags;
    flags += "coverage=" + std::to_string(config.coverage ? 1 : 0);
    flags += ",max_per_suite=" + std::to_string(config.max_per_suite);
    // Note: optimization level is currently not configurable per-test, but if it
    // becomes configurable, add it here.

    return crc32c_hex(flags.data(), flags.size());
}

std::vector<std::string>
TestResultCache::compute_source_hashes(const std::vector<std::string>& file_paths) {
    // Phase 8.5 W5: dedupe by canonical path before hashing. The transitive
    // source set passed from `compile_suite` may legitimately overlap with
    // the test files vector (the test file itself is in both), and on Windows
    // the same file can also appear with mixed slash forms. Without dedup we
    // would either (a) waste I/O hashing the same file twice, or (b) end up
    // with a hash vector whose length depends on input order rather than the
    // unique file set, breaking equality checks in `is_cached`.
    std::set<std::string> unique_paths;
    for (const auto& path : file_paths) {
        if (path.empty())
            continue;
        // Normalise slashes so `lib/foo.tml` and `lib\foo.tml` collapse to one.
        std::string normalised = path;
        std::replace(normalised.begin(), normalised.end(), '\\', '/');
        unique_paths.insert(std::move(normalised));
    }

    std::vector<std::string> hashes;
    hashes.reserve(unique_paths.size());
    for (const auto& path : unique_paths) {
        auto hash = crc32c_file(path);
        if (hash.empty())
            hash = "error";
        hashes.push_back(std::move(hash));
    }

    // Sort for deterministic comparison (file discovery order may vary)
    std::sort(hashes.begin(), hashes.end());
    return hashes;
}

} // namespace tml::testing
