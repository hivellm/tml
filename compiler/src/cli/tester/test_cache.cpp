//! # Test Cache Manager Implementation
//!
//! This file implements the test caching system that tracks file hashes
//! and test results to enable smart test skipping.

#include "cli/tester/test_cache.hpp"
#include "common.hpp"
#include "common/crc32c.hpp"

#include <algorithm>
#include <chrono>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
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
        case CachedTestStatus::Pass: return "pass";
        case CachedTestStatus::Fail: return "fail";
        case CachedTestStatus::Error: return "error";
        case CachedTestStatus::Timeout: return "timeout";
        case CachedTestStatus::Unknown: return "unknown";
    }
    return "unknown";
}

CachedTestStatus TestCacheManager::string_to_status(const std::string& str) {
    if (str == "pass") return CachedTestStatus::Pass;
    if (str == "fail") return CachedTestStatus::Fail;
    if (str == "error") return CachedTestStatus::Error;
    if (str == "timeout") return CachedTestStatus::Timeout;
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
    if (pos >= json.size() || json[pos] != '"') return "";
    ++pos;  // Skip opening quote

    std::string result;
    while (pos < json.size() && json[pos] != '"') {
        if (json[pos] == '\\' && pos + 1 < json.size()) {
            ++pos;
            switch (json[pos]) {
                case 'n': result += '\n'; break;
                case 't': result += '\t'; break;
                case 'r': result += '\r'; break;
                case '\\': result += '\\'; break;
                case '"': result += '"'; break;
                default: result += json[pos]; break;
            }
        } else {
            result += json[pos];
        }
        ++pos;
    }

    if (pos < json.size()) ++pos;  // Skip closing quote
    return result;
}

// Parse a JSON number
int64_t parse_number(const std::string& json, size_t& pos) {
    size_t start = pos;
    if (pos < json.size() && json[pos] == '-') ++pos;
    while (pos < json.size() && std::isdigit(json[pos])) ++pos;
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
            case '"': result += "\\\""; break;
            case '\\': result += "\\\\"; break;
            case '\n': result += "\\n"; break;
            case '\r': result += "\\r"; break;
            case '\t': result += "\\t"; break;
            default: result += c; break;
        }
    }
    return result;
}

}  // namespace

// ============================================================================
// Load Cache
// ============================================================================

bool TestCacheManager::load(const std::string& cache_file) {
    cache_file_ = cache_file;
    tests_.clear();

    std::ifstream file(cache_file);
    if (!file) {
        return false;  // File doesn't exist, will be created on save
    }

    std::ostringstream oss;
    oss << file.rdbuf();
    std::string json = oss.str();

    size_t pos = 0;
    skip_ws(json, pos);

    if (pos >= json.size() || json[pos] != '{') {
        return false;  // Invalid JSON
    }
    ++pos;  // Skip opening brace

    // Parse top-level object
    while (pos < json.size()) {
        skip_ws(json, pos);
        if (json[pos] == '}') break;
        if (json[pos] == ',') { ++pos; skip_ws(json, pos); }

        // Parse key
        std::string key = parse_string(json, pos);
        skip_ws(json, pos);
        if (json[pos] != ':') break;
        ++pos;
        skip_ws(json, pos);

        if (key == "version") {
            int64_t version = parse_number(json, pos);
            if (version != CACHE_VERSION) {
                tests_.clear();
                return false;  // Version mismatch, invalidate cache
            }
        } else if (key == "tests") {
            // Parse tests object
            if (json[pos] != '{') continue;
            ++pos;

            while (pos < json.size()) {
                skip_ws(json, pos);
                if (json[pos] == '}') { ++pos; break; }
                if (json[pos] == ',') { ++pos; skip_ws(json, pos); }

                // Parse test file path
                std::string test_path = parse_string(json, pos);
                skip_ws(json, pos);
                if (json[pos] != ':') break;
                ++pos;
                skip_ws(json, pos);

                // Parse test info object
                if (json[pos] != '{') continue;
                ++pos;

                CachedTestInfo info;
                info.file_path = test_path;

                while (pos < json.size()) {
                    skip_ws(json, pos);
                    if (json[pos] == '}') { ++pos; break; }
                    if (json[pos] == ',') { ++pos; skip_ws(json, pos); }

                    std::string field = parse_string(json, pos);
                    skip_ws(json, pos);
                    if (json[pos] != ':') break;
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
                                if (json[pos] == ']') { ++pos; break; }
                                if (json[pos] == ',') { ++pos; skip_ws(json, pos); }
                                info.test_functions.push_back(parse_string(json, pos));
                            }
                        }
                    } else if (field == "dependency_hashes") {
                        // Parse object
                        if (json[pos] == '{') {
                            ++pos;
                            while (pos < json.size()) {
                                skip_ws(json, pos);
                                if (json[pos] == '}') { ++pos; break; }
                                if (json[pos] == ',') { ++pos; skip_ws(json, pos); }
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
                                if (json[pos] == '{') ++depth;
                                else if (json[pos] == '}') --depth;
                                ++pos;
                            }
                        } else if (json[pos] == '[') {
                            int depth = 1;
                            ++pos;
                            while (pos < json.size() && depth > 0) {
                                if (json[pos] == '[') ++depth;
                                else if (json[pos] == ']') --depth;
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
        if (!first) file << ",\n";
        first = false;

        file << "    \"" << escape_json(path) << "\": {\n";
        file << "      \"sha512\": \"" << escape_json(info.sha512) << "\",\n";
        file << "      \"suite\": \"" << escape_json(info.suite) << "\",\n";
        file << "      \"last_updated\": \"" << escape_json(info.last_updated) << "\",\n";
        file << "      \"last_result\": \"" << status_to_string(info.last_result) << "\",\n";
        file << "      \"duration_ms\": " << info.duration_ms << ",\n";
        file << "      \"coverage_enabled\": " << (info.coverage_enabled ? "true" : "false") << ",\n";
        file << "      \"profile_enabled\": " << (info.profile_enabled ? "true" : "false") << ",\n";

        // Test functions array
        file << "      \"test_functions\": [";
        for (size_t i = 0; i < info.test_functions.size(); ++i) {
            if (i > 0) file << ", ";
            file << "\"" << escape_json(info.test_functions[i]) << "\"";
        }
        file << "],\n";

        // Dependency hashes object
        file << "      \"dependency_hashes\": {";
        bool first_dep = true;
        for (const auto& [dep_path, dep_hash] : info.dependency_hashes) {
            if (!first_dep) file << ", ";
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

std::optional<CachedTestInfo> TestCacheManager::get_cached_info(const std::string& test_file) const {
    std::string normalized = normalize_path(test_file);
    auto it = tests_.find(normalized);
    if (it == tests_.end()) {
        return std::nullopt;
    }
    return it->second;
}

void TestCacheManager::update(const std::string& test_file,
                               const std::string& sha512,
                               const std::string& suite,
                               const std::vector<std::string>& test_functions,
                               CachedTestStatus result,
                               int64_t duration_ms,
                               const std::map<std::string, std::string>& dependency_hashes,
                               bool coverage_enabled,
                               bool profile_enabled) {
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

}  // namespace tml::cli
