TML_MODULE("compiler")

// __DATE__/__TIME__ are used as a last-resort fallback hash when the binary
// path is unavailable (non-Windows without /proc/self/exe). The warning
// about non-reproducibility is intentional — we want the hash to change on
// rebuild in that fallback path.
#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdate-time"
#endif

#include "query/query_incr.hpp"

#include "common.hpp"
#include "common/crc32c.hpp"
#include "log/log.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <sstream>
#include <vector>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace fs = std::filesystem;

namespace tml::query {

// ============================================================================
// Compiler Build Hash
// ============================================================================

// F-024: streaming CRC32C over a file's content, memoized in a `<file>.bhash`
// sidecar keyed by (mtime,size). Returns the content CRC, or nullopt on I/O
// error. The whole 100+MB module is read at most once per actual rebuild: after
// a rebuild the sidecar's (mtime,size) no longer match, so we re-hash and
// rewrite it; unchanged binaries hit the sidecar and skip the read entirely.
static std::optional<uint32_t> content_hash_with_sidecar(const fs::path& binary_path) {
    std::error_code ec;
    auto size = fs::file_size(binary_path, ec);
    if (ec)
        return std::nullopt;
    auto ftime = fs::last_write_time(binary_path, ec);
    if (ec)
        return std::nullopt;
    auto mtime = static_cast<uint64_t>(ftime.time_since_epoch().count());

    auto sidecar = binary_path;
    sidecar += ".bhash";

    // Fast path: sidecar records a CRC for this exact (mtime,size).
    {
        std::ifstream in(sidecar);
        uint64_t s_mtime = 0, s_size = 0;
        uint32_t s_crc = 0;
        if (in >> s_mtime >> s_size >> s_crc && s_mtime == mtime &&
            s_size == static_cast<uint64_t>(size)) {
            return s_crc;
        }
    }

    // Slow path: stream the content through CRC32C (Castagnoli).
    std::ifstream f(binary_path, std::ios::binary);
    if (!f)
        return std::nullopt;
    uint32_t crc = 0xFFFFFFFFu;
    std::vector<char> buf(1u << 20); // 1 MiB chunks
    while (f) {
        f.read(buf.data(), static_cast<std::streamsize>(buf.size()));
        auto got = static_cast<size_t>(f.gcount());
        for (size_t i = 0; i < got; ++i) {
            crc = tml::CRC32C_TABLE[(crc ^ static_cast<uint8_t>(buf[i])) & 0xFF] ^ (crc >> 8);
        }
    }
    crc ^= 0xFFFFFFFFu;

    // Update the sidecar (atomic-ish: temp + rename). Racing processes write the
    // same content, so a lost race is harmless.
    {
        auto tmp = sidecar;
        tmp += ".tmp";
        std::ofstream out(tmp, std::ios::trunc);
        if (out) {
            out << mtime << " " << static_cast<uint64_t>(size) << " " << crc << "\n";
            out.close();
            std::error_code rec;
            fs::rename(tmp, sidecar, rec);
            if (rec)
                fs::remove(tmp, rec);
        }
    }
    return crc;
}

uint32_t compiler_build_hash() {
    // F-024: hash the compiler binary's CONTENT (memoized by a sidecar), not its
    // mtime. A relink that changes behavior changes the content → cache
    // invalidated; a no-op relink that reproduces byte-identical output keeps
    // the cache GREEN, where the old raw-mtime policy wiped it on every build.
    static const uint32_t hash = [] {
        std::error_code ec;
#ifdef _WIN32
        // On Windows, get the path to our own DLL/EXE module
        char module_path[MAX_PATH] = {};
        HMODULE hModule = nullptr;
        // Get handle to the DLL containing this function
        GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                               GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                           reinterpret_cast<LPCSTR>(&compiler_build_hash), &hModule);
        if (hModule) {
            GetModuleFileNameA(hModule, module_path, MAX_PATH);
        }
        fs::path binary_path(module_path);
#else
        // On Linux/macOS, /proc/self/exe or argv[0] — fall back to __DATE__/__TIME__
        fs::path binary_path("/proc/self/exe");
        if (!fs::exists(binary_path, ec)) {
            return tml::crc32c(std::string(__DATE__) + " " + __TIME__);
        }
#endif
        if (!binary_path.empty() && fs::exists(binary_path, ec)) {
            if (auto ch = content_hash_with_sidecar(binary_path)) {
                return *ch;
            }
            // Content unreadable: fall back to mtime (still a valid invalidation
            // signal, just coarser).
            auto ftime = fs::last_write_time(binary_path, ec);
            if (!ec) {
                return tml::crc32c(std::to_string(ftime.time_since_epoch().count()));
            }
        }
        // Fallback to compile-time constants if binary path unavailable
        return tml::crc32c(std::string(__DATE__) + " " + __TIME__);
    }();
    return hash;
}

// ============================================================================
// F-023: stable per-file symbol id
// ============================================================================

uint32_t stable_test_symbol_id(const std::string& file_path) {
    // 31-bit, non-zero. Path-derived so it is identical across suite positions
    // and (with overwhelming probability) distinct across files. Both codegen
    // and the dispatcher generator call this on the same file_path.
    uint32_t h = tml::crc32c(file_path) & 0x7FFFFFFFu;
    return h == 0 ? 1u : h;
}

// ============================================================================
// F-019: telemetry
// ============================================================================

IncrTelemetry& incr_telemetry() {
    static IncrTelemetry t;
    return t;
}

void reset_incr_telemetry() {
    auto& t = incr_telemetry();
    t.green_hits.store(0);
    t.red_misses.store(0);
    t.cache_loads.store(0);
    t.cache_saves.store(0);
    t.load_us.store(0);
    t.save_us.store(0);
    t.ir_bytes_gc.store(0);
    t.ir_files_gc.store(0);
}

std::string incr_telemetry_report() {
    auto& t = incr_telemetry();
    uint64_t green = t.green_hits.load();
    uint64_t red = t.red_misses.load();
    uint64_t total = green + red;
    double pct = total > 0 ? (100.0 * static_cast<double>(green) / static_cast<double>(total)) : 0.0;
    std::ostringstream oss;
    oss << "incr: GREEN=" << green << " RED=" << red << " (" << static_cast<int>(pct + 0.5)
        << "% green) loads=" << t.cache_loads.load() << " (" << (t.load_us.load() / 1000)
        << "ms) saves=" << t.cache_saves.load() << " (" << (t.save_us.load() / 1000)
        << "ms) ir_gc=" << t.ir_files_gc.load() << " files/"
        << (t.ir_bytes_gc.load() / (1024 * 1024)) << "MB";
    return oss.str();
}

#ifdef __clang__
#pragma clang diagnostic pop
#endif

// ============================================================================
// Binary Helpers
// ============================================================================

static void write_u8(std::ostream& out, uint8_t val) {
    out.write(reinterpret_cast<const char*>(&val), 1);
}

static void write_u16(std::ostream& out, uint16_t val) {
    out.write(reinterpret_cast<const char*>(&val), 2);
}

static void write_u32(std::ostream& out, uint32_t val) {
    out.write(reinterpret_cast<const char*>(&val), 4);
}

static void write_u64(std::ostream& out, uint64_t val) {
    out.write(reinterpret_cast<const char*>(&val), 8);
}

static void write_i32(std::ostream& out, int32_t val) {
    out.write(reinterpret_cast<const char*>(&val), 4);
}

static void write_string(std::ostream& out, const std::string& str) {
    auto len = static_cast<uint16_t>(std::min(str.size(), size_t(UINT16_MAX)));
    write_u16(out, len);
    out.write(str.data(), len);
}

static bool read_u8(std::istream& in, uint8_t& val) {
    return !!in.read(reinterpret_cast<char*>(&val), 1);
}

static bool read_u16(std::istream& in, uint16_t& val) {
    return !!in.read(reinterpret_cast<char*>(&val), 2);
}

static bool read_u32(std::istream& in, uint32_t& val) {
    return !!in.read(reinterpret_cast<char*>(&val), 4);
}

static bool read_u64(std::istream& in, uint64_t& val) {
    return !!in.read(reinterpret_cast<char*>(&val), 8);
}

static bool read_i32(std::istream& in, int32_t& val) {
    return !!in.read(reinterpret_cast<char*>(&val), 4);
}

static bool read_string(std::istream& in, std::string& str) {
    uint16_t len = 0;
    if (!read_u16(in, len))
        return false;
    if (len > 32768) // Sanity check: no single string should exceed 32KB
        return false;
    str.resize(len);
    if (len == 0)
        return true;
    if (!in.read(str.data(), len))
        return false;
    // Validate we actually read the expected number of bytes
    if (in.gcount() != static_cast<std::streamsize>(len))
        return false;
    return true;
}

// ============================================================================
// QueryKey Serialization
// ============================================================================

std::vector<uint8_t> serialize_query_key(const QueryKey& key) {
    std::ostringstream oss(std::ios::binary);

    std::visit(
        [&](const auto& k) {
            using T = std::decay_t<decltype(k)>;
            if constexpr (std::is_same_v<T, ReadSourceKey> || std::is_same_v<T, TokenizeKey>) {
                write_string(oss, k.file_path);
            } else if constexpr (std::is_same_v<T, CodegenUnitKey>) {
                // F-023 (format v3): test_entry_index + has_cached_library_state
                // are no longer part of the key identity, so they are not
                // serialized. library_decls_only stays (it selects decls-only vs
                // full-definition IR).
                write_string(oss, k.file_path);
                write_string(oss, k.module_name);
                write_i32(oss, static_cast<int32_t>(k.optimization_level));
                write_u8(oss, k.debug_info ? 1 : 0);
                write_u8(oss, k.library_decls_only ? 1 : 0);
            } else {
                // ParseModuleKey, TypecheckModuleKey, BorrowcheckModuleKey,
                // HirLowerKey, MirBuildKey all have file_path + module_name
                write_string(oss, k.file_path);
                write_string(oss, k.module_name);
            }
        },
        key);

    auto str = oss.str();
    return {str.begin(), str.end()};
}

std::optional<QueryKey> deserialize_query_key(const uint8_t* data, size_t len, QueryKind kind) {
    std::string buf(reinterpret_cast<const char*>(data), len);
    std::istringstream iss(buf, std::ios::binary);

    auto read_str = [&](std::string& s) -> bool { return read_string(iss, s); };

    switch (kind) {
    case QueryKind::ReadSource: {
        std::string file_path;
        if (!read_str(file_path))
            return std::nullopt;
        return QueryKey{ReadSourceKey{std::move(file_path)}};
    }
    case QueryKind::Tokenize: {
        std::string file_path;
        if (!read_str(file_path))
            return std::nullopt;
        return QueryKey{TokenizeKey{std::move(file_path)}};
    }
    case QueryKind::ParseModule: {
        std::string file_path, module_name;
        if (!read_str(file_path) || !read_str(module_name))
            return std::nullopt;
        return QueryKey{ParseModuleKey{std::move(file_path), std::move(module_name)}};
    }
    case QueryKind::TypecheckModule: {
        std::string file_path, module_name;
        if (!read_str(file_path) || !read_str(module_name))
            return std::nullopt;
        return QueryKey{TypecheckModuleKey{std::move(file_path), std::move(module_name)}};
    }
    case QueryKind::BorrowcheckModule: {
        std::string file_path, module_name;
        if (!read_str(file_path) || !read_str(module_name))
            return std::nullopt;
        return QueryKey{BorrowcheckModuleKey{std::move(file_path), std::move(module_name)}};
    }
    case QueryKind::HirLower: {
        std::string file_path, module_name;
        if (!read_str(file_path) || !read_str(module_name))
            return std::nullopt;
        return QueryKey{HirLowerKey{std::move(file_path), std::move(module_name)}};
    }
    case QueryKind::ThirLower: {
        std::string file_path, module_name;
        if (!read_str(file_path) || !read_str(module_name))
            return std::nullopt;
        return QueryKey{ThirLowerKey{std::move(file_path), std::move(module_name)}};
    }
    case QueryKind::MirBuild: {
        std::string file_path, module_name;
        if (!read_str(file_path) || !read_str(module_name))
            return std::nullopt;
        return QueryKey{MirBuildKey{std::move(file_path), std::move(module_name)}};
    }
    case QueryKind::CodegenUnit: {
        // F-023 (format v3): file_path, module_name, opt_level, debug_info,
        // library_decls_only. No test_entry_index / has_cached_library_state.
        std::string file_path, module_name;
        int32_t opt_level = 0;
        uint8_t debug_info = 0;
        uint8_t library_decls_only = 0;
        if (!read_str(file_path) || !read_str(module_name) || !read_i32(iss, opt_level) ||
            !read_u8(iss, debug_info) || !read_u8(iss, library_decls_only))
            return std::nullopt;
        return QueryKey{CodegenUnitKey{std::move(file_path), std::move(module_name),
                                       static_cast<int>(opt_level), debug_info != 0,
                                       library_decls_only != 0}};
    }
    default:
        return std::nullopt;
    }
}

// ============================================================================
// PrevSessionCache
// ============================================================================

static bool read_query_key_from_stream(std::istream& in, QueryKind kind, QueryKey& out_key) {
    uint16_t key_len = 0;
    if (!read_u16(in, key_len))
        return false;

    std::vector<uint8_t> key_data(key_len);
    if (!in.read(reinterpret_cast<char*>(key_data.data()), key_len))
        return false;

    auto key_opt = deserialize_query_key(key_data.data(), key_data.size(), kind);
    if (!key_opt)
        return false;

    out_key = std::move(*key_opt);
    return true;
}

bool PrevSessionCache::load(const fs::path& cache_file) {
    if (!fs::exists(cache_file))
        return false;

    std::ifstream in(cache_file, std::ios::binary);
    if (!in)
        return false;

    auto _load_t0 = std::chrono::steady_clock::now();
    incr_telemetry().cache_loads.fetch_add(1, std::memory_order_relaxed);
    struct LoadTimer {
        std::chrono::steady_clock::time_point t0;
        ~LoadTimer() {
            auto us = std::chrono::duration_cast<std::chrono::microseconds>(
                          std::chrono::steady_clock::now() - t0)
                          .count();
            incr_telemetry().load_us.fetch_add(static_cast<uint64_t>(us), std::memory_order_relaxed);
        }
    } _lt{_load_t0};

    try {
        // Read header
        uint32_t magic = 0;
        uint16_t ver_major = 0, ver_minor = 0;
        uint32_t entry_count = 0;

        if (!read_u32(in, magic) || magic != INCR_CACHE_MAGIC) {
            TML_LOG_ERROR("query", "[Q002] Incremental cache read failed: invalid magic number");
            return false;
        }
        if (!read_u16(in, ver_major) || ver_major != INCR_CACHE_VERSION_MAJOR) {
            TML_LOG_ERROR("query", "[Q003] Incremental cache version mismatch: expected major "
                                       << INCR_CACHE_VERSION_MAJOR << " got " << ver_major);
            return false;
        }
        if (!read_u16(in, ver_minor))
            return false;
        if (!read_u32(in, entry_count))
            return false;
        if (!read_u64(in, session_timestamp_))
            return false;
        if (!read_u32(in, options_hash_))
            return false;
        // V2+: read compiler build hash
        if (!read_u32(in, build_hash_))
            return false;
        // Reject cache if compiled by a different compiler build
        if (build_hash_ != compiler_build_hash()) {
            TML_LOG_DEBUG("incr", "Compiler build hash mismatch (cache="
                                      << build_hash_ << " current=" << compiler_build_hash()
                                      << "), invalidating cache");
            return false;
        }

        // F-021: the old hard `entry_count > 10000` rejection was a silent
        // total-reset cliff — at 9,282 entries one wave from wiping the whole
        // cache. Aging now happens gracefully at SAVE (write's max_entries cap),
        // so here we only guard against obviously-corrupt headers with a much
        // larger limit.
        constexpr uint32_t CORRUPT_ENTRY_LIMIT = 5'000'000;
        if (entry_count > CORRUPT_ENTRY_LIMIT) {
            TML_LOG_ERROR("query", "[Q002] Incremental cache read failed: entry count "
                                       << entry_count << " exceeds corruption limit");
            return false;
        }

        // Read entries
        entries_.reserve(entry_count);
        for (uint32_t i = 0; i < entry_count; ++i) {
            PrevSessionEntry entry;

            // Query kind
            uint8_t kind_val = 0;
            if (!read_u8(in, kind_val))
                return false;
            auto kind = static_cast<QueryKind>(kind_val);

            // Key
            if (!read_query_key_from_stream(in, kind, entry.key))
                return false;

            // Fingerprints
            if (!read_u64(in, entry.input_fingerprint.high) ||
                !read_u64(in, entry.input_fingerprint.low))
                return false;
            if (!read_u64(in, entry.output_fingerprint.high) ||
                !read_u64(in, entry.output_fingerprint.low))
                return false;

            // Dependencies
            uint16_t dep_count = 0;
            if (!read_u16(in, dep_count))
                return false;

            entry.dependencies.reserve(dep_count);
            for (uint16_t d = 0; d < dep_count; ++d) {
                uint8_t dep_kind_val = 0;
                if (!read_u8(in, dep_kind_val))
                    return false;
                auto dep_kind = static_cast<QueryKind>(dep_kind_val);

                QueryKey dep_key;
                if (!read_query_key_from_stream(in, dep_kind, dep_key))
                    return false;

                entry.dependencies.push_back(std::move(dep_key));
            }

            entries_[entry.key] = std::move(entry);
        }

        TML_LOG_DEBUG("incr", "Loaded incremental cache: " << entries_.size() << " entries");
        return true;
    } catch (...) {
        TML_LOG_ERROR("query",
                      "[Q002] Incremental cache read failed: exception while parsing cache file");
        entries_.clear();
        return false;
    }
}

const PrevSessionEntry* PrevSessionCache::lookup(const QueryKey& key) const {
    auto it = entries_.find(key);
    if (it == entries_.end())
        return nullptr;
    return &it->second;
}

// ============================================================================
// IncrCacheWriter
// ============================================================================

void IncrCacheWriter::record(const QueryKey& key, Fingerprint input_fp, Fingerprint output_fp,
                             std::vector<QueryKey> deps) {
    recorded_keys_.insert(key);
    PrevSessionEntry entry;
    entry.key = key;
    entry.input_fingerprint = input_fp;
    entry.output_fingerprint = output_fp;
    entry.dependencies = std::move(deps);
    entries_.push_back(std::move(entry));
}

void IncrCacheWriter::merge_from(const PrevSessionCache& prev) {
    for (const auto& [key, entry] : prev.entries()) {
        if (recorded_keys_.count(key) == 0) {
            recorded_keys_.insert(key);
            entries_.push_back(entry);
        }
    }
}

void IncrCacheWriter::merge_from_writer(const IncrCacheWriter& other) {
    // phase41c / F-010: fold another writer's recorded entries in, skipping any
    // key we already hold. First-writer-wins on duplicate keys, matching the
    // "entries not already recorded" policy of merge_from(PrevSessionCache).
    for (const auto& entry : other.entries_) {
        if (recorded_keys_.count(entry.key) == 0) {
            recorded_keys_.insert(entry.key);
            entries_.push_back(entry);
        }
    }
}

bool IncrCacheWriter::write(const fs::path& cache_file, uint32_t options_hash, size_t max_entries) {
    auto _save_t0 = std::chrono::steady_clock::now();
    incr_telemetry().cache_saves.fetch_add(1, std::memory_order_relaxed);
    struct SaveTimer {
        std::chrono::steady_clock::time_point t0;
        ~SaveTimer() {
            auto us = std::chrono::duration_cast<std::chrono::microseconds>(
                          std::chrono::steady_clock::now() - t0)
                          .count();
            incr_telemetry().save_us.fetch_add(static_cast<uint64_t>(us), std::memory_order_relaxed);
        }
    } _st{_save_t0};

    try {
        fs::create_directories(cache_file.parent_path());

        // F-021 aging: cap the written set to the first `max_entries` (0 =
        // unlimited). Recorded order is current-session-first, then merged
        // previous-session entries, so this keeps the most-recently-used entries.
        size_t write_count = entries_.size();
        if (max_entries != 0 && write_count > max_entries) {
            write_count = max_entries;
        }

        // Write to temp file first, then rename (atomic)
        auto temp_file = cache_file;
        temp_file += ".tmp";

        {
            std::ofstream out(temp_file, std::ios::binary);
            if (!out)
                return false;

            // Header
            write_u32(out, INCR_CACHE_MAGIC);
            write_u16(out, INCR_CACHE_VERSION_MAJOR);
            write_u16(out, INCR_CACHE_VERSION_MINOR);
            write_u32(out, static_cast<uint32_t>(write_count));

            auto timestamp =
                static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
                                          std::chrono::system_clock::now().time_since_epoch())
                                          .count());
            write_u64(out, timestamp);
            write_u32(out, options_hash);
            write_u32(out, compiler_build_hash());

            // Entries
            size_t written = 0;
            for (const auto& entry : entries_) {
                if (written++ >= write_count)
                    break;
                auto kind = query_kind(entry.key);
                write_u8(out, static_cast<uint8_t>(kind));

                auto key_data = serialize_query_key(entry.key);
                write_u16(out, static_cast<uint16_t>(key_data.size()));
                out.write(reinterpret_cast<const char*>(key_data.data()),
                          static_cast<std::streamsize>(key_data.size()));

                write_u64(out, entry.input_fingerprint.high);
                write_u64(out, entry.input_fingerprint.low);
                write_u64(out, entry.output_fingerprint.high);
                write_u64(out, entry.output_fingerprint.low);

                write_u16(out, static_cast<uint16_t>(entry.dependencies.size()));
                for (const auto& dep : entry.dependencies) {
                    auto dep_kind = query_kind(dep);
                    write_u8(out, static_cast<uint8_t>(dep_kind));

                    auto dep_data = serialize_query_key(dep);
                    write_u16(out, static_cast<uint16_t>(dep_data.size()));
                    out.write(reinterpret_cast<const char*>(dep_data.data()),
                              static_cast<std::streamsize>(dep_data.size()));
                }
            }

            if (!out.good())
                return false;
        }

        // Atomic rename
        std::error_code ec;
        fs::rename(temp_file, cache_file, ec);
        if (ec) {
            // Fallback: copy + remove
            fs::copy_file(temp_file, cache_file, fs::copy_options::overwrite_existing, ec);
            fs::remove(temp_file, ec);
        }

        TML_LOG_DEBUG("incr", "Saved incremental cache: " << entries_.size() << " entries");
        return true;
    } catch (...) {
        return false;
    }
}

bool IncrCacheWriter::save_ir(const QueryKey& key, const std::string& llvm_ir,
                              const fs::path& cache_dir) {
    try {
        auto ir_dir = cache_dir / "ir";
        fs::create_directories(ir_dir);

        auto filename = get_ir_cache_filename(key);
        auto ir_path = ir_dir / (filename + ".ll");

        std::ofstream out(ir_path, std::ios::binary);
        if (!out)
            return false;
        out.write(llvm_ir.data(), static_cast<std::streamsize>(llvm_ir.size()));
        return out.good();
    } catch (...) {
        return false;
    }
}

bool IncrCacheWriter::save_link_libs(const QueryKey& key, const std::set<std::string>& link_libs,
                                     const fs::path& cache_dir) {
    try {
        auto ir_dir = cache_dir / "ir";
        fs::create_directories(ir_dir);

        auto filename = get_ir_cache_filename(key);
        auto libs_path = ir_dir / (filename + ".libs");

        std::ofstream out(libs_path);
        if (!out)
            return false;
        for (const auto& lib : link_libs) {
            out << lib << "\n";
        }
        return out.good();
    } catch (...) {
        return false;
    }
}

bool IncrCacheWriter::save_link_search_paths(const QueryKey& key,
                                             const std::vector<fs::path>& search_paths,
                                             const fs::path& cache_dir) {
    try {
        auto ir_dir = cache_dir / "ir";
        fs::create_directories(ir_dir);

        auto filename = get_ir_cache_filename(key);
        auto paths_file = ir_dir / (filename + ".search_paths");

        std::ofstream out(paths_file);
        if (!out)
            return false;
        for (const auto& p : search_paths) {
            out << p.string() << "\n";
        }
        return out.good();
    } catch (...) {
        return false;
    }
}

// ============================================================================
// Free Functions
// ============================================================================

std::optional<std::string> load_cached_ir(const QueryKey& key, const fs::path& cache_dir) {
    try {
        auto filename = get_ir_cache_filename(key);
        auto ir_path = cache_dir / "ir" / (filename + ".ll");

        if (!fs::exists(ir_path))
            return std::nullopt;

        std::ifstream in(ir_path, std::ios::binary | std::ios::ate);
        if (!in)
            return std::nullopt;

        auto size = in.tellg();
        if (size <= 0)
            return std::nullopt;

        std::string content(static_cast<size_t>(size), '\0');
        in.seekg(0);
        in.read(content.data(), size);
        return content;
    } catch (...) {
        return std::nullopt;
    }
}

std::set<std::string> load_cached_link_libs(const QueryKey& key, const fs::path& cache_dir) {
    std::set<std::string> libs;
    try {
        auto filename = get_ir_cache_filename(key);
        auto libs_path = cache_dir / "ir" / (filename + ".libs");

        if (!fs::exists(libs_path))
            return libs;

        std::ifstream in(libs_path);
        std::string line;
        while (std::getline(in, line)) {
            if (!line.empty()) {
                libs.insert(line);
            }
        }
    } catch (...) {}
    return libs;
}

std::vector<fs::path> load_cached_link_search_paths(const QueryKey& key,
                                                    const fs::path& cache_dir) {
    std::vector<fs::path> paths;
    try {
        auto filename = get_ir_cache_filename(key);
        auto paths_file = cache_dir / "ir" / (filename + ".search_paths");

        if (!fs::exists(paths_file))
            return paths;

        std::ifstream in(paths_file);
        std::string line;
        while (std::getline(in, line)) {
            // Strip trailing \r if present (Windows line endings)
            if (!line.empty() && line.back() == '\r') {
                line.pop_back();
            }
            if (!line.empty()) {
                paths.push_back(fs::path(line));
            }
        }
    } catch (...) {}
    return paths;
}

uint32_t compute_options_hash(int opt_level, bool debug_info, const std::string& target_triple,
                              const std::vector<std::string>& defines, bool coverage) {
    std::ostringstream oss;
    oss << "O" << opt_level << "|D" << (debug_info ? 1 : 0) << "|T" << target_triple << "|C"
        << (coverage ? 1 : 0) << "|M" << (CompilerOptions::checked_math ? 1 : 0);
    for (const auto& def : defines) {
        oss << "|" << def;
    }
    return tml::crc32c(oss.str());
}

Fingerprint compute_library_env_fingerprint(const fs::path& build_dir) {
    Fingerprint fp{};
    try {
        auto meta_dir = build_dir / "cache" / "meta";
        if (!fs::exists(meta_dir))
            return fp;

        // Collect all .tml.meta files, sorted for determinism
        std::vector<std::string> meta_files;
        for (const auto& entry : fs::recursive_directory_iterator(meta_dir)) {
            if (entry.is_regular_file() && entry.path().extension() == ".meta") {
                meta_files.push_back(entry.path().string());
            }
        }
        std::sort(meta_files.begin(), meta_files.end());

        // Combine fingerprints of all meta files
        for (const auto& file : meta_files) {
            auto file_fp = fingerprint_source(file);
            fp = fingerprint_combine(fp, file_fp);
        }
    } catch (...) {}
    return fp;
}

std::string get_ir_cache_filename(const QueryKey& key) {
    // Hash the query key to produce a filename
    auto key_data = serialize_query_key(key);
    auto fp = fingerprint_bytes(key_data.data(), key_data.size());
    // Include query kind in the name for debugging
    auto kind = query_kind(key);
    return std::string(query_kind_name(kind)) + "_" + fp.to_hex();
}

// ============================================================================
// F-025: config-partitioned cache files
// ============================================================================

fs::path incr_cache_file_for(const fs::path& incr_dir, uint32_t options_hash) {
    char buf[16];
    std::snprintf(buf, sizeof(buf), "%08x", options_hash);
    return incr_dir / ("incr." + std::string(buf) + ".bin");
}

size_t prune_incr_partitions(const fs::path& incr_dir, size_t keep_newest) {
    size_t removed = 0;
    try {
        // Remove any legacy unpartitioned incr.bin (superseded by incr.<hash>.bin).
        std::error_code ec;
        auto legacy = incr_dir / "incr.bin";
        if (fs::exists(legacy, ec)) {
            if (fs::remove(legacy, ec))
                ++removed;
        }

        // Collect incr.<hash>.bin partitions with their mtimes.
        std::vector<std::pair<fs::file_time_type, fs::path>> parts;
        for (const auto& e : fs::directory_iterator(incr_dir, ec)) {
            if (ec)
                break;
            if (!e.is_regular_file())
                continue;
            auto name = e.path().filename().string();
            if (name.rfind("incr.", 0) == 0 && e.path().extension() == ".bin" &&
                name != "incr.bin") {
                std::error_code tec;
                auto t = fs::last_write_time(e.path(), tec);
                parts.emplace_back(tec ? fs::file_time_type{} : t, e.path());
            }
        }
        if (parts.size() <= keep_newest)
            return removed;

        // Newest first; delete everything past keep_newest.
        std::sort(parts.begin(), parts.end(),
                  [](const auto& a, const auto& b) { return a.first > b.first; });
        for (size_t i = keep_newest; i < parts.size(); ++i) {
            std::error_code rec;
            if (fs::remove(parts[i].second, rec))
                ++removed;
        }
    } catch (...) {}
    return removed;
}

// ============================================================================
// F-020: session-scoped shared previous-session cache
// ============================================================================

namespace {
struct SharedPrevSlot {
    std::shared_ptr<const PrevSessionCache> cache;
    uint64_t mtime = 0;
    uint32_t options_hash = 0;
    bool loaded = false; ///< True once we've attempted a load for this mtime.
};
std::mutex g_shared_prev_mtx;
std::unordered_map<std::string, SharedPrevSlot> g_shared_prev; ///< keyed by cache_file path
} // namespace

std::shared_ptr<const PrevSessionCache>
get_shared_prev_session(const fs::path& cache_file, uint32_t options_hash) {
    std::error_code ec;
    uint64_t mtime = 0;
    if (fs::exists(cache_file, ec)) {
        auto ft = fs::last_write_time(cache_file, ec);
        if (!ec)
            mtime = static_cast<uint64_t>(ft.time_since_epoch().count());
    }

    std::lock_guard<std::mutex> lk(g_shared_prev_mtx);
    auto key = cache_file.string();
    auto& slot = g_shared_prev[key];
    // Reuse the parsed copy if the file is unchanged and the options match. The
    // options_hash guards against a partition rename; the mtime guards against a
    // rewrite between runs.
    if (slot.loaded && slot.mtime == mtime && slot.options_hash == options_hash) {
        return slot.cache;
    }

    slot.loaded = true;
    slot.mtime = mtime;
    slot.options_hash = options_hash;
    slot.cache.reset();

    auto parsed = std::make_shared<PrevSessionCache>();
    if (parsed->load(cache_file) && parsed->options_hash() == options_hash) {
        slot.cache = parsed;
    }
    return slot.cache;
}

void reset_shared_prev_session() {
    std::lock_guard<std::mutex> lk(g_shared_prev_mtx);
    g_shared_prev.clear();
}

// ============================================================================
// F-022: IR store garbage collection
// ============================================================================

uint64_t gc_ir_store(const fs::path& cache_dir,
                     const std::unordered_set<std::string>& surviving_stems) {
    uint64_t reclaimed = 0;
    uint64_t files = 0;
    try {
        auto ir_dir = cache_dir / "ir";
        std::error_code ec;
        if (!fs::exists(ir_dir, ec))
            return 0;

        for (const auto& e : fs::directory_iterator(ir_dir, ec)) {
            if (ec)
                break;
            if (!e.is_regular_file())
                continue;
            // Filename is `<stem>.ll` / `<stem>.libs` / `<stem>.search_paths`.
            // The stem is everything before the FIRST extension we recognize.
            auto path = e.path();
            auto ext = path.extension().string();
            if (ext != ".ll" && ext != ".libs" && ext != ".search_paths")
                continue;
            auto stem = path.stem().string(); // strips the final extension only
            if (surviving_stems.count(stem) != 0)
                continue;
            std::error_code sec;
            auto sz = fs::file_size(path, sec);
            std::error_code rec;
            if (fs::remove(path, rec)) {
                if (!sec)
                    reclaimed += sz;
                ++files;
            }
        }
    } catch (...) {}
    incr_telemetry().ir_bytes_gc.fetch_add(reclaimed, std::memory_order_relaxed);
    incr_telemetry().ir_files_gc.fetch_add(files, std::memory_order_relaxed);
    return reclaimed;
}

uint64_t incr_run_teardown(const fs::path& incr_dir) {
    std::error_code ec;
    if (!fs::exists(incr_dir, ec))
        return 0;

    // F-025: keep only the newest partitions.
    prune_incr_partitions(incr_dir, MAX_INCR_PARTITIONS);

    // Collect the ir/ stems referenced by every surviving partition. The final
    // on-disk cache is the source of truth — independent of any single writer,
    // so this is safe after parallel per-suite writes.
    std::unordered_set<std::string> surviving;
    for (const auto& e : fs::directory_iterator(incr_dir, ec)) {
        if (ec)
            break;
        if (!e.is_regular_file())
            continue;
        auto name = e.path().filename().string();
        if (!(name.rfind("incr.", 0) == 0 && e.path().extension() == ".bin"))
            continue;
        PrevSessionCache part;
        if (!part.load(e.path()))
            continue;
        for (const auto& [key, entry] : part.entries()) {
            if (query_kind(key) == QueryKind::CodegenUnit) {
                surviving.insert(get_ir_cache_filename(key));
            }
        }
    }

    // F-022: reclaim orphaned IR dumps.
    uint64_t reclaimed = gc_ir_store(incr_dir, surviving);

    // Next run in this process must re-read a freshly written cache.
    reset_shared_prev_session();
    return reclaimed;
}

} // namespace tml::query
