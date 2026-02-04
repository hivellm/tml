//! # Test Cache Manager Implementation
//!
//! This file implements the test caching system that tracks file hashes
//! and test results to enable smart test skipping.

#include "cli/tester/test_cache.hpp"

#include "common.hpp"
#include "common/crc32c.hpp"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <set>
#include <sstream>

// Simple JSON parsing/writing (avoid dependency on external JSON library)
// We use a minimal custom implementation

namespace fs = std::filesystem;

namespace tml::cli {

// ============================================================================
// Path Normalization
// ============================================================================

std::string TestCacheManager::normalize_path(const std::string& path) {
    // Convert to forward slashes and make relative
    std::string normalized = path;
    std::replace(normalized.begin(), normalized.end(), '\\', '/');

    // Remove leading ./ if present
    if (normalized.size() >= 2 && normalized[0] == '.' && normalized[1] == '/') {
        normalized = normalized.substr(2);
    }

    return normalized;
}

// ============================================================================
// Status Conversion
// ============================================================================

std::string TestCacheManager::status_to_string(CachedTestStatus status) {
    switch (status) {
    case CachedTestStatus::Pass:
        return "pass";
    case CachedTestStatus::Fail:
        return "fail";
    case CachedTestStatus::Error:
        return "error";
    case CachedTestStatus::Timeout:
        return "timeout";
    case CachedTestStatus::Unknown:
        return "unknown";
    }
    return "unknown";
}

CachedTestStatus TestCacheManager::string_to_status(const std::string& str) {
    if (str == "pass")
        return CachedTestStatus::Pass;
    if (str == "fail")
        return CachedTestStatus::Fail;
    if (str == "error")
        return CachedTestStatus::Error;
    if (str == "timeout")
        return CachedTestStatus::Timeout;
    return CachedTestStatus::Unknown;
}

// ============================================================================
// Timestamp
// ============================================================================

std::string TestCacheManager::current_timestamp() {
    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);

    std::tm tm_buf;
#ifdef _WIN32
    gmtime_s(&tm_buf, &time_t_now);
#else
    gmtime_r(&time_t_now, &tm_buf);
#endif

    std::ostringstream oss;
    oss << std::put_time(&tm_buf, "%Y-%m-%dT%H:%M:%SZ");
    return oss.str();
}

// ============================================================================
// File Hash Computation
// ============================================================================

std::string TestCacheManager::compute_file_hash(const std::string& file_path) {
    return tml::crc32c_file(file_path);
}

// ============================================================================
// JSON Helpers (minimal implementation to avoid external dependency)
// ============================================================================

namespace {

// Skip whitespace in JSON string
void skip_ws(const std::string& json, size_t& pos) {
    while (pos < json.size() && std::isspace(json[pos])) {
        ++pos;
    }
}

// Parse a JSON string value (assumes pos is at opening quote)
std::string parse_string(const std::string& json, size_t& pos) {
    if (pos >= json.size() || json[pos] != '"')
        return "";
    ++pos; // Skip opening quote

    std::string result;
    while (pos < json.size() && json[pos] != '"') {
        if (json[pos] == '\\' && pos + 1 < json.size()) {
            ++pos;
            switch (json[pos]) {
            case 'n':
                result += '\n';
                break;
            case 't':
                result += '\t';
                break;
            case 'r':
                result += '\r';
                break;
            case '\\':
                result += '\\';
                break;
            case '"':
                result += '"';
                break;
            default:
                result += json[pos];
                break;
            }
        } else {
            result += json[pos];
        }
        ++pos;
    }

    if (pos < json.size())
        ++pos; // Skip closing quote
    return result;
}

// Parse a JSON number
int64_t parse_number(const std::string& json, size_t& pos) {
    size_t start = pos;
    if (pos < json.size() && json[pos] == '-')
        ++pos;
    while (pos < json.size() && std::isdigit(json[pos]))
        ++pos;
    return std::stoll(json.substr(start, pos - start));
}

// Parse a JSON boolean
bool parse_bool(const std::string& json, size_t& pos) {
    if (json.compare(pos, 4, "true") == 0) {
        pos += 4;
        return true;
    } else if (json.compare(pos, 5, "false") == 0) {
        pos += 5;
        return false;
    }
    return false;
}

// Escape a string for JSON output
std::string escape_json(const std::string& str) {
    std::string result;
    for (char c : str) {
        switch (c) {
        case '"':
            result += "\\\"";
            break;
        case '\\':
            result += "\\\\";
            break;
        case '\n':
            result += "\\n";
            break;
        case '\r':
            result += "\\r";
            break;
        case '\t':
            result += "\\t";
            break;
        default:
            result += c;
            break;
        }
    }
    return result;
}

} // namespace

// ============================================================================
// Load Cache
// ============================================================================

bool TestCacheManager::load(const std::string& cache_file) {
    cache_file_ = cache_file;
    tests_.clear();

    std::ifstream file(cache_file);
    if (!file) {
        return false; // File doesn't exist, will be created on save
    }

    std::ostringstream oss;
    oss << file.rdbuf();
    std::string json = oss.str();

    size_t pos = 0;
    skip_ws(json, pos);

    if (pos >= json.size() || json[pos] != '{') {
        return false; // Invalid JSON
    }
    ++pos; // Skip opening brace

    // Parse top-level object
    while (pos < json.size()) {
        skip_ws(json, pos);
        if (json[pos] == '}')
            break;
        if (json[pos] == ',') {
            ++pos;
            skip_ws(json, pos);
        }

        // Parse key
        std::string key = parse_string(json, pos);
        skip_ws(json, pos);
        if (json[pos] != ':')
            break;
        ++pos;
        skip_ws(json, pos);

        if (key == "version") {
            int64_t version = parse_number(json, pos);
            if (version != CACHE_VERSION) {
                tests_.clear();
                return false; // Version mismatch, invalidate cache
            }
        } else if (key == "tests") {
            // Parse tests object
            if (json[pos] != '{')
                continue;
            ++pos;

            while (pos < json.size()) {
                skip_ws(json, pos);
                if (json[pos] == '}') {
                    ++pos;
                    break;
                }
                if (json[pos] == ',') {
                    ++pos;
                    skip_ws(json, pos);
                }

                // Parse test file path
                std::string test_path = parse_string(json, pos);
                skip_ws(json, pos);
                if (json[pos] != ':')
                    break;
                ++pos;
                skip_ws(json, pos);

                // Parse test info object
                if (json[pos] != '{')
                    continue;
                ++pos;

                CachedTestInfo info;
                info.file_path = test_path;

                while (pos < json.size()) {
                    skip_ws(json, pos);
                    if (json[pos] == '}') {
                        ++pos;
                        break;
                    }
                    if (json[pos] == ',') {
                        ++pos;
                        skip_ws(json, pos);
                    }

                    std::string field = parse_string(json, pos);
                    skip_ws(json, pos);
                    if (json[pos] != ':')
                        break;
                    ++pos;
                    skip_ws(json, pos);

                    if (field == "sha512") {
                        info.sha512 = parse_string(json, pos);
                    } else if (field == "suite") {
                        info.suite = parse_string(json, pos);
                    } else if (field == "last_updated") {
                        info.last_updated = parse_string(json, pos);
                    } else if (field == "last_result") {
                        info.last_result = string_to_status(parse_string(json, pos));
                    } else if (field == "duration_ms") {
                        info.duration_ms = parse_number(json, pos);
                    } else if (field == "coverage_enabled") {
                        info.coverage_enabled = parse_bool(json, pos);
                    } else if (field == "profile_enabled") {
                        info.profile_enabled = parse_bool(json, pos);
                    } else if (field == "test_functions") {
                        // Parse array
                        if (json[pos] == '[') {
                            ++pos;
                            while (pos < json.size()) {
                                skip_ws(json, pos);
                                if (json[pos] == ']') {
                                    ++pos;
                                    break;
                                }
                                if (json[pos] == ',') {
                                    ++pos;
                                    skip_ws(json, pos);
                                }
                                info.test_functions.push_back(parse_string(json, pos));
                            }
                        }
                    } else if (field == "dependency_hashes") {
                        // Parse object
                        if (json[pos] == '{') {
                            ++pos;
                            while (pos < json.size()) {
                                skip_ws(json, pos);
                                if (json[pos] == '}') {
                                    ++pos;
                                    break;
                                }
                                if (json[pos] == ',') {
                                    ++pos;
                                    skip_ws(json, pos);
                                }
                                std::string dep_path = parse_string(json, pos);
                                skip_ws(json, pos);
                                if (json[pos] == ':') {
                                    ++pos;
                                    skip_ws(json, pos);
                                    info.dependency_hashes[dep_path] = parse_string(json, pos);
                                }
                            }
                        }
                    } else {
                        // Skip unknown field value
                        if (json[pos] == '"') {
                            parse_string(json, pos);
                        } else if (json[pos] == '{') {
                            int depth = 1;
                            ++pos;
                            while (pos < json.size() && depth > 0) {
                                if (json[pos] == '{')
                                    ++depth;
                                else if (json[pos] == '}')
                                    --depth;
                                ++pos;
                            }
                        } else if (json[pos] == '[') {
                            int depth = 1;
                            ++pos;
                            while (pos < json.size() && depth > 0) {
                                if (json[pos] == '[')
                                    ++depth;
                                else if (json[pos] == ']')
                                    --depth;
                                ++pos;
                            }
                        } else {
                            while (pos < json.size() && json[pos] != ',' && json[pos] != '}') {
                                ++pos;
                            }
                        }
                    }
                }

                tests_[normalize_path(test_path)] = std::move(info);
            }
        }
    }

    return true;
}

// ============================================================================
// Save Cache
// ============================================================================

bool TestCacheManager::save(const std::string& cache_file) const {
    std::ofstream file(cache_file);
    if (!file) {
        return false;
    }

    file << "{\n";
    file << "  \"version\": " << CACHE_VERSION << ",\n";
    file << "  \"tests\": {\n";

    bool first = true;
    for (const auto& [path, info] : tests_) {
        if (!first)
            file << ",\n";
        first = false;

        file << "    \"" << escape_json(path) << "\": {\n";
        file << "      \"sha512\": \"" << escape_json(info.sha512) << "\",\n";
        file << "      \"suite\": \"" << escape_json(info.suite) << "\",\n";
        file << "      \"last_updated\": \"" << escape_json(info.last_updated) << "\",\n";
        file << "      \"last_result\": \"" << status_to_string(info.last_result) << "\",\n";
        file << "      \"duration_ms\": " << info.duration_ms << ",\n";
        file << "      \"coverage_enabled\": " << (info.coverage_enabled ? "true" : "false")
             << ",\n";
        file << "      \"profile_enabled\": " << (info.profile_enabled ? "true" : "false") << ",\n";

        // Test functions array
        file << "      \"test_functions\": [";
        for (size_t i = 0; i < info.test_functions.size(); ++i) {
            if (i > 0)
                file << ", ";
            file << "\"" << escape_json(info.test_functions[i]) << "\"";
        }
        file << "],\n";

        // Dependency hashes object
        file << "      \"dependency_hashes\": {";
        bool first_dep = true;
        for (const auto& [dep_path, dep_hash] : info.dependency_hashes) {
            if (!first_dep)
                file << ", ";
            first_dep = false;
            file << "\"" << escape_json(dep_path) << "\": \"" << escape_json(dep_hash) << "\"";
        }
        file << "}\n";

        file << "    }";
    }

    file << "\n  }\n";
    file << "}\n";

    return true;
}

// ============================================================================
// Validation
// ============================================================================

CacheValidationResult TestCacheManager::validate(const std::string& test_file) const {
    CacheValidationResult result;
    std::string normalized = normalize_path(test_file);

    auto it = tests_.find(normalized);
    if (it == tests_.end()) {
        result.valid = false;
        result.reason = "Not in cache";
        return result;
    }

    const auto& info = it->second;

    // Check if the file still exists
    if (!fs::exists(test_file)) {
        result.valid = false;
        result.reason = "File no longer exists";
        return result;
    }

    // Compute current hash
    std::string current_hash = compute_file_hash(test_file);
    if (current_hash.empty()) {
        result.valid = false;
        result.reason = "Failed to compute file hash";
        return result;
    }

    // Compare hashes
    if (current_hash != info.sha512) {
        result.valid = false;
        result.reason = "File content changed";
        return result;
    }

    // Check dependency hashes
    for (const auto& [dep_path, dep_hash] : info.dependency_hashes) {
        if (!fs::exists(dep_path)) {
            result.valid = false;
            result.reason = "Dependency '" + dep_path + "' no longer exists";
            return result;
        }

        std::string current_dep_hash = compute_file_hash(dep_path);
        if (current_dep_hash != dep_hash) {
            result.valid = false;
            result.reason = "Dependency '" + dep_path + "' changed";
            return result;
        }
    }

    result.valid = true;
    return result;
}

bool TestCacheManager::can_skip(const std::string& test_file) const {
    auto validation = validate(test_file);
    if (!validation.valid) {
        return false;
    }

    auto info = get_cached_info(test_file);
    if (!info) {
        return false;
    }

    // Don't skip if coverage mode changed - need to recompile with coverage instrumentation
    if (info->coverage_enabled != CompilerOptions::coverage) {
        return false;
    }

    // Only skip if the last result was a pass
    return info->last_result == CachedTestStatus::Pass;
}

// ============================================================================
// Cache Access
// ============================================================================

std::optional<CachedTestInfo>
TestCacheManager::get_cached_info(const std::string& test_file) const {
    std::string normalized = normalize_path(test_file);
    auto it = tests_.find(normalized);
    if (it == tests_.end()) {
        return std::nullopt;
    }
    return it->second;
}

void TestCacheManager::update(const std::string& test_file, const std::string& sha512,
                              const std::string& suite,
                              const std::vector<std::string>& test_functions,
                              CachedTestStatus result, int64_t duration_ms,
                              const std::map<std::string, std::string>& dependency_hashes,
                              bool coverage_enabled, bool profile_enabled) {
    std::string normalized = normalize_path(test_file);

    CachedTestInfo info;
    info.file_path = normalized;
    info.sha512 = sha512;
    info.suite = suite;
    info.last_updated = current_timestamp();
    info.test_functions = test_functions;
    info.last_result = result;
    info.duration_ms = duration_ms;
    info.dependency_hashes = dependency_hashes;
    info.coverage_enabled = coverage_enabled;
    info.profile_enabled = profile_enabled;

    tests_[normalized] = std::move(info);
}

void TestCacheManager::remove(const std::string& test_file) {
    std::string normalized = normalize_path(test_file);
    tests_.erase(normalized);
}

void TestCacheManager::clear() {
    tests_.clear();
}

TestCacheManager::CacheStats TestCacheManager::get_stats() const {
    CacheStats stats;
    stats.total_entries = tests_.size();

    for (const auto& [path, info] : tests_) {
        auto validation = validate(path);
        if (validation.valid) {
            ++stats.valid_entries;
        }

        if (info.last_result == CachedTestStatus::Pass) {
            ++stats.passed_entries;
        } else if (info.last_result == CachedTestStatus::Fail ||
                   info.last_result == CachedTestStatus::Error ||
                   info.last_result == CachedTestStatus::Timeout) {
            ++stats.failed_entries;
        }
    }

    return stats;
}

// ============================================================================
// Backup and Recovery
// ============================================================================

std::string TestCacheManager::get_temp_backup_dir() {
#ifdef _WIN32
    // Use %TEMP% on Windows with _dupenv_s for security
    char* temp_buf = nullptr;
    size_t temp_len = 0;
    std::string temp_path;

    if (_dupenv_s(&temp_buf, &temp_len, "TEMP") == 0 && temp_buf) {
        temp_path = temp_buf;
        free(temp_buf);
    } else if (_dupenv_s(&temp_buf, &temp_len, "TMP") == 0 && temp_buf) {
        temp_path = temp_buf;
        free(temp_buf);
    } else {
        temp_path = "C:\\Temp";
    }
    return temp_path + "\\tml-cache-backup";
#else
    // Use /tmp on Unix
    const char* temp = std::getenv("TMPDIR");
    if (!temp) {
        temp = "/tmp";
    }
    return std::string(temp) + "/tml-cache-backup";
#endif
}

bool TestCacheManager::has_temp_backup() {
    fs::path backup_dir = get_temp_backup_dir();
    fs::path cache_backup = backup_dir / ".test-cache.json";
    return fs::exists(cache_backup);
}

bool TestCacheManager::backup_to_temp(const std::string& cache_file,
                                       const std::string& run_cache_dir) {
    try {
        fs::path backup_dir = get_temp_backup_dir();

        // Create backup directory
        fs::create_directories(backup_dir);

        // Backup .test-cache.json
        if (fs::exists(cache_file)) {
            fs::path cache_backup = backup_dir / ".test-cache.json";
            fs::copy_file(cache_file, cache_backup, fs::copy_options::overwrite_existing);
        }

        // Backup .run-cache directory (only DLL files to save space)
        if (fs::exists(run_cache_dir)) {
            fs::path run_backup = backup_dir / ".run-cache";
            fs::create_directories(run_backup);

            for (const auto& entry : fs::directory_iterator(run_cache_dir)) {
                if (!entry.is_regular_file())
                    continue;

                auto ext = entry.path().extension().string();
                // Only backup compiled test DLLs
                if (ext == ".dll" || ext == ".so" || ext == ".dylib") {
                    fs::path dest = run_backup / entry.path().filename();
                    fs::copy_file(entry.path(), dest, fs::copy_options::overwrite_existing);
                }
            }
        }

        return true;
    } catch (const std::exception& e) {
        std::cerr << "[WARN] Failed to backup cache to temp: " << e.what() << "\n";
        return false;
    }
}

bool TestCacheManager::restore_from_temp(const std::string& cache_file,
                                          const std::string& run_cache_dir) {
    try {
        fs::path backup_dir = get_temp_backup_dir();
        fs::path cache_backup = backup_dir / ".test-cache.json";

        // Check if backup exists
        if (!fs::exists(cache_backup)) {
            return false;
        }

        // Restore .test-cache.json
        fs::path cache_parent = fs::path(cache_file).parent_path();
        if (!cache_parent.empty()) {
            fs::create_directories(cache_parent);
        }
        fs::copy_file(cache_backup, cache_file, fs::copy_options::overwrite_existing);

        // Restore .run-cache directory
        fs::path run_backup = backup_dir / ".run-cache";
        if (fs::exists(run_backup)) {
            fs::create_directories(run_cache_dir);

            int restored_count = 0;
            for (const auto& entry : fs::directory_iterator(run_backup)) {
                if (!entry.is_regular_file())
                    continue;

                fs::path dest = fs::path(run_cache_dir) / entry.path().filename();
                try {
                    fs::copy_file(entry.path(), dest, fs::copy_options::overwrite_existing);
                    ++restored_count;
                } catch (...) {
                    // Skip files that can't be copied (e.g., in use)
                }
            }

            std::cerr << "[INFO] Restored " << restored_count << " cached test DLLs from backup\n";
        }

        std::cerr << "[INFO] Restored test cache from backup at: " << backup_dir.string() << "\n";
        return true;
    } catch (const std::exception& e) {
        std::cerr << "[WARN] Failed to restore cache from temp: " << e.what() << "\n";
        return false;
    }
}

// ============================================================================
// Cache Cleanup
// ============================================================================

std::vector<std::string> TestCacheManager::get_known_suite_hashes() const {
    std::vector<std::string> hashes;
    // Collect unique suite names from cached tests
    std::set<std::string> suite_names;
    for (const auto& [path, info] : tests_) {
        if (!info.suite.empty()) {
            suite_names.insert(info.suite);
        }
    }
    // Note: The actual hash is computed from test file content + suite grouping
    // For now, we can't easily predict the hash without recomputing
    // This function would need to be called with hashes from suite_execution
    return hashes;
}

size_t TestCacheManager::cleanup_orphaned_files(const std::string& run_cache_dir,
                                                 const std::vector<std::string>& known_suite_hashes,
                                                 bool verbose) {
    if (!fs::exists(run_cache_dir)) {
        return 0;
    }

    // Build a set of valid prefixes for quick lookup
    std::set<std::string> valid_prefixes(known_suite_hashes.begin(), known_suite_hashes.end());

    size_t removed_count = 0;
    size_t removed_bytes = 0;

    // Extensions to clean up
    std::vector<std::string> cleanup_extensions = {".ll", ".obj", ".dll", ".pdb", ".exp", ".lib"};

    try {
        for (const auto& entry : fs::directory_iterator(run_cache_dir)) {
            if (!entry.is_regular_file())
                continue;

            std::string filename = entry.path().filename().string();
            std::string ext = entry.path().extension().string();

            // Check if this is a cleanable extension
            bool is_cleanup_target = false;
            for (const auto& target_ext : cleanup_extensions) {
                if (ext == target_ext) {
                    is_cleanup_target = true;
                    break;
                }
            }

            if (!is_cleanup_target)
                continue;

            // Extract hash prefix from filename (format: HASH_suite_N.ext)
            // e.g., "abc123def456_suite_0.ll" -> "abc123def456"
            size_t underscore_pos = filename.find('_');
            if (underscore_pos == std::string::npos)
                continue;

            std::string hash_prefix = filename.substr(0, underscore_pos);

            // Check if this hash is in our known list
            if (valid_prefixes.find(hash_prefix) == valid_prefixes.end()) {
                // This file is orphaned - remove it
                try {
                    auto file_size = fs::file_size(entry.path());
                    fs::remove(entry.path());
                    ++removed_count;
                    removed_bytes += file_size;

                    if (verbose) {
                        std::cerr << "[CLEANUP] Removed orphaned file: " << filename << "\n";
                    }
                } catch (const std::exception& e) {
                    if (verbose) {
                        std::cerr << "[CLEANUP] Failed to remove " << filename << ": " << e.what()
                                  << "\n";
                    }
                }
            }
        }

        if (removed_count > 0) {
            double mb = static_cast<double>(removed_bytes) / (1024 * 1024);
            std::cerr << "[CLEANUP] Removed " << removed_count << " orphaned files ("
                      << std::fixed << std::setprecision(1) << mb << " MB)\n";
        }
    } catch (const std::exception& e) {
        std::cerr << "[CLEANUP] Error during cleanup: " << e.what() << "\n";
    }

    return removed_count;
}

} // namespace tml::cli
