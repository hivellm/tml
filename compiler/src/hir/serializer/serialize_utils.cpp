TML_MODULE("compiler")

//! # HIR Serialization Utilities
//!
//! This file provides convenience functions for HIR serialization and
//! incremental compilation support.
//!
//! ## Content Hashing
//!
//! Uses FNV-1a hash algorithm to compute content hashes for:
//! - Source files (content + modification time)
//! - HIR modules (structure + types)
//!
//! Hashes enable change detection for incremental compilation.
//!
//! ## File I/O
//!
//! | Function | Description |
//! |----------|-------------|
//! | `serialize_hir_binary()` | Module → bytes |
//! | `deserialize_hir_binary()` | bytes → Module |
//! | `serialize_hir_text()` | Module → string |
//! | `write_hir_file()` | Module → file |
//! | `read_hir_file()` | file → Module |
//!
//! ## Dependency Tracking
//!
//! The `HirCacheInfo` struct stores:
//! - Module metadata (name, source path)
//! - Content hashes (source and HIR)
//! - Dependency list with their hashes
//! - Compilation timestamp
//!
//! This enables checking if cached HIR is still valid without fully loading it.
//!
//! ## Example: Incremental Compilation
//!
//! ```cpp
//! // Check if we can use cached HIR
//! auto cache_info = read_hir_cache_info("module.hir.info");
//! if (cache_info && are_dependencies_valid(*cache_info)) {
//!     // Cache is valid, load directly
//!     return read_hir_file("module.hir");
//! }
//!
//! // Cache invalid, recompile and update
//! HirModule module = compile_fresh(source);
//! write_hir_file(module, "module.hir");
//! ```
//!
//! ## See Also
//!
//! - `hir_serialize.hpp` - Public API definitions
//! - `binary_writer.cpp` / `binary_reader.cpp` - Binary format

#include "hir/hir_serialize.hpp"
#include "types/type.hpp"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <functional>
#include <sstream>

namespace tml::hir {

// ============================================================================
// Content Hashing
// ============================================================================

namespace {

// FNV-1a hash constants for 64-bit
constexpr uint64_t FNV_OFFSET = 14695981039346656037ULL;
constexpr uint64_t FNV_PRIME = 1099511628211ULL;

class Hasher {
public:
    void update(uint8_t byte) {
        hash_ = (hash_ ^ byte) * FNV_PRIME;
    }

    void update(const void* data, size_t size) {
        const auto* bytes = static_cast<const uint8_t*>(data);
        for (size_t i = 0; i < size; ++i) {
            update(bytes[i]);
        }
    }

    void update(const std::string& str) {
        update(str.size());
        update(str.data(), str.size());
    }

    void update(uint32_t value) {
        update(&value, sizeof(value));
    }

    void update(uint64_t value) {
        update(&value, sizeof(value));
    }

    void update(int64_t value) {
        update(&value, sizeof(value));
    }

    void update(double value) {
        update(&value, sizeof(value));
    }

    void update(bool value) {
        update(static_cast<uint8_t>(value ? 1 : 0));
    }

    [[nodiscard]] auto finish() const -> ContentHash {
        return hash_;
    }

private:
    uint64_t hash_ = FNV_OFFSET;
};

void hash_type(Hasher& h, const HirType& type) {
    if (type) {
        h.update(types::type_to_string(type));
    } else {
        h.update(std::string("null"));
    }
}

[[maybe_unused]] void hash_span(Hasher& h, const SourceSpan& span) {
    h.update(span.start.line);
    h.update(span.start.column);
    h.update(span.start.offset);
    h.update(span.end.line);
    h.update(span.end.column);
    h.update(span.end.offset);
}

[[maybe_unused]] void hash_expr(Hasher& h, const HirExpr& expr);

[[maybe_unused]] void hash_pattern(Hasher& h, const HirPattern& pattern) {
    h.update(static_cast<uint8_t>(pattern.kind.index()));
    h.update(pattern.id());
    hash_type(h, pattern.type());
}

[[maybe_unused]] void hash_stmt(Hasher& h, const HirStmt& stmt) {
    h.update(static_cast<uint8_t>(stmt.kind.index()));
    h.update(stmt.id());
}

[[maybe_unused]] void hash_expr(Hasher& h, const HirExpr& expr) {
    h.update(static_cast<uint8_t>(expr.kind.index()));
    h.update(expr.id());
    hash_type(h, expr.type());
}

void hash_function(Hasher& h, const HirFunction& func) {
    h.update(func.id);
    h.update(func.name);
    h.update(func.mangled_name);
    h.update(static_cast<uint64_t>(func.params.size()));
    for (const auto& p : func.params) {
        h.update(p.name);
        hash_type(h, p.type);
        h.update(p.is_mut);
    }
    hash_type(h, func.return_type);
    h.update(func.body.has_value());
    h.update(func.is_public);
    h.update(func.is_async);
    h.update(func.is_extern);
}

void hash_struct(Hasher& h, const HirStruct& s) {
    h.update(s.id);
    h.update(s.name);
    h.update(s.mangled_name);
    h.update(static_cast<uint64_t>(s.fields.size()));
    for (const auto& f : s.fields) {
        h.update(f.name);
        hash_type(h, f.type);
        h.update(f.is_public);
    }
    h.update(s.is_public);
}

void hash_enum(Hasher& h, const HirEnum& e) {
    h.update(e.id);
    h.update(e.name);
    h.update(e.mangled_name);
    h.update(static_cast<uint64_t>(e.variants.size()));
    for (const auto& v : e.variants) {
        h.update(v.name);
        h.update(static_cast<uint32_t>(v.index));
        h.update(static_cast<uint64_t>(v.payload_types.size()));
        for (const auto& pt : v.payload_types) {
            hash_type(h, pt);
        }
    }
    h.update(e.is_public);
}

} // namespace

auto compute_source_hash(const std::string& source_path) -> ContentHash {
    Hasher h;

    // Hash the path
    h.update(source_path);

    // Try to read file and hash contents
    std::ifstream file(source_path, std::ios::binary);
    if (file) {
        // Hash file contents
        char buffer[4096];
        while (file.read(buffer, sizeof(buffer))) {
            h.update(buffer, file.gcount());
        }
        if (file.gcount() > 0) {
            h.update(buffer, file.gcount());
        }

        // Hash file modification time
        try {
            auto mod_time = std::filesystem::last_write_time(source_path);
            auto duration = mod_time.time_since_epoch();
            h.update(static_cast<uint64_t>(duration.count()));
        } catch (...) {
            // Ignore filesystem errors
        }
    }

    return h.finish();
}

auto compute_hir_hash(const HirModule& module) -> ContentHash {
    Hasher h;

    // Hash module metadata
    h.update(module.name);
    h.update(module.source_path);

    // Hash structs
    h.update(static_cast<uint64_t>(module.structs.size()));
    for (const auto& s : module.structs) {
        hash_struct(h, s);
    }

    // Hash enums
    h.update(static_cast<uint64_t>(module.enums.size()));
    for (const auto& e : module.enums) {
        hash_enum(h, e);
    }

    // Hash functions
    h.update(static_cast<uint64_t>(module.functions.size()));
    for (const auto& f : module.functions) {
        hash_function(h, f);
    }

    // Hash behaviors
    h.update(static_cast<uint64_t>(module.behaviors.size()));
    for (const auto& b : module.behaviors) {
        h.update(b.id);
        h.update(b.name);
        h.update(static_cast<uint64_t>(b.methods.size()));
    }

    // Hash impls
    h.update(static_cast<uint64_t>(module.impls.size()));
    for (const auto& impl : module.impls) {
        h.update(impl.id);
        h.update(impl.type_name);
        h.update(static_cast<uint64_t>(impl.methods.size()));
    }

    // Hash constants
    h.update(static_cast<uint64_t>(module.constants.size()));
    for (const auto& c : module.constants) {
        h.update(c.id);
        h.update(c.name);
        hash_type(h, c.type);
    }

    // Hash imports
    h.update(static_cast<uint64_t>(module.imports.size()));
    for (const auto& imp : module.imports) {
        h.update(imp);
    }

    return h.finish();
}

// ============================================================================
// Convenience Functions
// ============================================================================

auto serialize_hir_binary(const HirModule& module, HirSerializeOptions options)
    -> std::vector<uint8_t> {
    std::ostringstream oss(std::ios::binary);
    HirBinaryWriter writer(oss, options);
    writer.write_module(module);

    std::string str = oss.str();
    return std::vector<uint8_t>(str.begin(), str.end());
}

auto deserialize_hir_binary(const std::vector<uint8_t>& data) -> HirModule {
    std::string str(data.begin(), data.end());
    std::istringstream iss(str, std::ios::binary);
    HirBinaryReader reader(iss);
    return reader.read_module();
}

auto serialize_hir_text(const HirModule& module, HirSerializeOptions options) -> std::string {
    std::ostringstream oss;
    HirTextWriter writer(oss, options);
    writer.write_module(module);
    return oss.str();
}

auto deserialize_hir_text(const std::string& text) -> HirModule {
    std::istringstream iss(text);
    HirTextReader reader(iss);
    return reader.read_module();
}

auto write_hir_file(const HirModule& module, const std::string& path, bool binary) -> bool {
    std::ofstream file(path, binary ? std::ios::binary : std::ios::out);
    if (!file) {
        return false;
    }

    if (binary) {
        HirBinaryWriter writer(file);
        writer.write_module(module);
    } else {
        HirTextWriter writer(file);
        writer.write_module(module);
    }

    return file.good();
}

auto read_hir_file(const std::string& path) -> HirModule {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        return HirModule{};
    }

    // Check magic to determine format
    uint32_t magic = 0;
    file.read(reinterpret_cast<char*>(&magic), 4);
    file.seekg(0);

    if (magic == HIR_MAGIC) {
        HirBinaryReader reader(file);
        return reader.read_module();
    } else {
        // Assume text format
        std::stringstream buffer;
        buffer << file.rdbuf();
        return deserialize_hir_text(buffer.str());
    }
}

auto is_hir_cache_valid(const std::string& cache_path, ContentHash source_hash) -> bool {
    // Try to read cache info file first (contains source hash)
    auto cache_info = read_hir_cache_info(cache_path + ".info");
    if (cache_info) {
        // Validate source hash matches
        if (cache_info->source_hash != source_hash) {
            return false;
        }
        // Validate dependencies
        return are_dependencies_valid(*cache_info);
    }

    // Fall back to checking just the binary file
    std::ifstream file(cache_path, std::ios::binary);
    if (!file) {
        return false;
    }

    // Read header and extract stored hash
    uint32_t magic = 0;
    file.read(reinterpret_cast<char*>(&magic), 4);
    if (magic != HIR_MAGIC) {
        return false;
    }

    uint16_t major = 0, minor = 0;
    file.read(reinterpret_cast<char*>(&major), 2);
    file.read(reinterpret_cast<char*>(&minor), 2);
    if (major != HIR_VERSION_MAJOR) {
        return false;
    }

    // Without cache info file, we can't validate source hash
    // Be conservative and invalidate cache
    (void)source_hash; // Mark as used to suppress warning in simple fallback
    return false;
}

auto get_hir_cache_hash(const std::string& cache_path) -> ContentHash {
    std::ifstream file(cache_path, std::ios::binary);
    if (!file) {
        return 0;
    }

    // Read header
    uint32_t magic = 0;
    file.read(reinterpret_cast<char*>(&magic), 4);
    if (magic != HIR_MAGIC) {
        return 0;
    }

    uint16_t major = 0, minor = 0;
    file.read(reinterpret_cast<char*>(&major), 2);
    file.read(reinterpret_cast<char*>(&minor), 2);

    ContentHash hash = 0;
    file.read(reinterpret_cast<char*>(&hash), 8);

    return hash;
}

// ============================================================================
// Dependency Tracking
// ============================================================================

auto write_hir_cache_info(const HirCacheInfo& info, const std::string& path) -> bool {
    std::ofstream file(path, std::ios::binary);
    if (!file) {
        return false;
    }

    // Write a simple format: magic, version, then cache info
    constexpr uint32_t CACHE_INFO_MAGIC = 0x49524948; // "HIRI"
    file.write(reinterpret_cast<const char*>(&CACHE_INFO_MAGIC), 4);

    // Module name
    uint32_t name_len = static_cast<uint32_t>(info.module_name.size());
    file.write(reinterpret_cast<const char*>(&name_len), 4);
    file.write(info.module_name.data(), name_len);

    // Source path
    uint32_t path_len = static_cast<uint32_t>(info.source_path.size());
    file.write(reinterpret_cast<const char*>(&path_len), 4);
    file.write(info.source_path.data(), path_len);

    // Hashes
    file.write(reinterpret_cast<const char*>(&info.source_hash), 8);
    file.write(reinterpret_cast<const char*>(&info.hir_hash), 8);

    // Timestamp
    file.write(reinterpret_cast<const char*>(&info.compile_timestamp), 8);

    // Dependencies
    uint32_t dep_count = static_cast<uint32_t>(info.deps.size());
    file.write(reinterpret_cast<const char*>(&dep_count), 4);
    for (const auto& dep : info.deps) {
        uint32_t dep_name_len = static_cast<uint32_t>(dep.module_name.size());
        file.write(reinterpret_cast<const char*>(&dep_name_len), 4);
        file.write(dep.module_name.data(), dep_name_len);

        uint32_t dep_path_len = static_cast<uint32_t>(dep.source_path.size());
        file.write(reinterpret_cast<const char*>(&dep_path_len), 4);
        file.write(dep.source_path.data(), dep_path_len);

        file.write(reinterpret_cast<const char*>(&dep.content_hash), 8);
    }

    return file.good();
}

auto read_hir_cache_info(const std::string& path) -> std::optional<HirCacheInfo> {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        return std::nullopt;
    }

    // Check magic
    constexpr uint32_t CACHE_INFO_MAGIC = 0x49524948;
    uint32_t magic = 0;
    file.read(reinterpret_cast<char*>(&magic), 4);
    if (magic != CACHE_INFO_MAGIC) {
        return std::nullopt;
    }

    HirCacheInfo info;

    // Module name
    uint32_t name_len = 0;
    file.read(reinterpret_cast<char*>(&name_len), 4);
    info.module_name.resize(name_len);
    file.read(info.module_name.data(), name_len);

    // Source path
    uint32_t path_len = 0;
    file.read(reinterpret_cast<char*>(&path_len), 4);
    info.source_path.resize(path_len);
    file.read(info.source_path.data(), path_len);

    // Hashes
    file.read(reinterpret_cast<char*>(&info.source_hash), 8);
    file.read(reinterpret_cast<char*>(&info.hir_hash), 8);

    // Timestamp
    file.read(reinterpret_cast<char*>(&info.compile_timestamp), 8);

    // Dependencies
    uint32_t dep_count = 0;
    file.read(reinterpret_cast<char*>(&dep_count), 4);
    for (uint32_t i = 0; i < dep_count; ++i) {
        HirDependency dep;

        uint32_t dep_name_len = 0;
        file.read(reinterpret_cast<char*>(&dep_name_len), 4);
        dep.module_name.resize(dep_name_len);
        file.read(dep.module_name.data(), dep_name_len);

        uint32_t dep_path_len = 0;
        file.read(reinterpret_cast<char*>(&dep_path_len), 4);
        dep.source_path.resize(dep_path_len);
        file.read(dep.source_path.data(), dep_path_len);

        file.read(reinterpret_cast<char*>(&dep.content_hash), 8);

        info.deps.push_back(dep);
    }

    if (!file) {
        return std::nullopt;
    }

    return info;
}

auto are_dependencies_valid(const HirCacheInfo& info) -> bool {
    // Check if source file changed
    ContentHash current_source_hash = compute_source_hash(info.source_path);
    if (current_source_hash != info.source_hash) {
        return false;
    }

    // Check all dependencies
    for (const auto& dep : info.deps) {
        ContentHash current_dep_hash = compute_source_hash(dep.source_path);
        if (current_dep_hash != dep.content_hash) {
            return false;
        }
    }

    return true;
}

} // namespace tml::hir
