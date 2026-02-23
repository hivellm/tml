TML_MODULE("compiler")

#include "query/query_incr.hpp"

#include "common/crc32c.hpp"
#include "log/log.hpp"

#include <algorithm>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace fs = std::filesystem;

namespace tml::query {

// ============================================================================
// Compiler Build Hash
// ============================================================================

uint32_t compiler_build_hash() {
    // Changes every time the compiler is recompiled (__DATE__ + __TIME__ are compile-time
    // constants)
    static const uint32_t hash = tml::crc32c(std::string(__DATE__) + " " + __TIME__);
    return hash;
}

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
                write_string(oss, k.file_path);
                write_string(oss, k.module_name);
                write_i32(oss, static_cast<int32_t>(k.optimization_level));
                write_u8(oss, k.debug_info ? 1 : 0);
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
        std::string file_path, module_name;
        int32_t opt_level = 0;
        uint8_t debug_info = 0;
        if (!read_str(file_path) || !read_str(module_name) || !read_i32(iss, opt_level) ||
            !read_u8(iss, debug_info))
            return std::nullopt;
        return QueryKey{CodegenUnitKey{std::move(file_path), std::move(module_name),
                                       static_cast<int>(opt_level), debug_info != 0}};
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

    try {
        // Read header
        uint32_t magic = 0;
        uint16_t ver_major = 0, ver_minor = 0;
        uint32_t entry_count = 0;

        if (!read_u32(in, magic) || magic != INCR_CACHE_MAGIC)
            return false;
        if (!read_u16(in, ver_major) || ver_major != INCR_CACHE_VERSION_MAJOR)
            return false;
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

        // Sanity check: a single compilation should not have more than 10K entries
        if (entry_count > 10000)
            return false;

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
    PrevSessionEntry entry;
    entry.key = key;
    entry.input_fingerprint = input_fp;
    entry.output_fingerprint = output_fp;
    entry.dependencies = std::move(deps);
    entries_.push_back(std::move(entry));
}

bool IncrCacheWriter::write(const fs::path& cache_file, uint32_t options_hash) {
    try {
        fs::create_directories(cache_file.parent_path());

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
            write_u32(out, static_cast<uint32_t>(entries_.size()));

            auto timestamp =
                static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
                                          std::chrono::system_clock::now().time_since_epoch())
                                          .count());
            write_u64(out, timestamp);
            write_u32(out, options_hash);
            write_u32(out, compiler_build_hash());

            // Entries
            for (const auto& entry : entries_) {
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

uint32_t compute_options_hash(int opt_level, bool debug_info, const std::string& target_triple,
                              const std::vector<std::string>& defines, bool coverage) {
    std::ostringstream oss;
    oss << "O" << opt_level << "|D" << (debug_info ? 1 : 0) << "|T" << target_triple << "|C"
        << (coverage ? 1 : 0);
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
    return std::string(query_kind_name(kind)) + "_" + fp.to_hex().substr(0, 16);
}

} // namespace tml::query
