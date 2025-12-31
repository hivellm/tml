#include "build_cache.hpp"

#include <fstream>
#include <set>
#include <sstream>

namespace tml::cli {

// Thread-local global phase timer
thread_local PhaseTimer* g_phase_timer = nullptr;

// ============================================================================
// Utility Functions
// ============================================================================

std::string hash_file_content(const std::string& content) {
    // Simple hash using std::hash - for production, consider xxHash or SHA256
    std::hash<std::string> hasher;
    size_t hash = hasher(content);

    std::ostringstream oss;
    oss << std::hex << std::setw(16) << std::setfill('0') << hash;
    return oss.str();
}

int64_t get_mtime(const fs::path& path) {
    try {
        auto ftime = fs::last_write_time(path);
        return ftime.time_since_epoch().count();
    } catch (...) {
        return 0;
    }
}

// ============================================================================
// MirCache Implementation
// ============================================================================

MirCache::MirCache(const fs::path& cache_dir) : cache_dir_(cache_dir) {
    fs::create_directories(cache_dir_);
    index_file_ = cache_dir_ / "mir_cache.idx";
    func_index_file_ = cache_dir_ / "func_cache.idx";
}

void MirCache::load_index() const {
    if (loaded_)
        return;
    loaded_ = true;

    if (!fs::exists(index_file_))
        return;

    std::ifstream file(index_file_);
    if (!file)
        return;

    std::string line;
    while (std::getline(file, line)) {
        std::istringstream iss(line);
        std::string source_path, source_hash, mir_file, object_file;
        int64_t mtime;
        int opt_level;
        bool debug_info;

        // Format: source_path|source_hash|mir_file|object_file|mtime|opt_level|debug_info
        if (std::getline(iss, source_path, '|') && std::getline(iss, source_hash, '|') &&
            std::getline(iss, mir_file, '|') && std::getline(iss, object_file, '|')) {

            std::string mtime_str, opt_str, debug_str;
            if (std::getline(iss, mtime_str, '|') && std::getline(iss, opt_str, '|') &&
                std::getline(iss, debug_str)) {
                try {
                    mtime = std::stoll(mtime_str);
                    opt_level = std::stoi(opt_str);
                    debug_info = (debug_str == "1");

                    CacheEntry entry;
                    entry.source_hash = source_hash;
                    entry.mir_file = mir_file;
                    entry.object_file = object_file;
                    entry.source_mtime = mtime;
                    entry.optimization_level = opt_level;
                    entry.debug_info = debug_info;

                    entries_[source_path] = entry;
                } catch (...) {
                    // Skip malformed entries
                }
            }
        }
    }
}

void MirCache::save_index() const {
    std::ofstream file(index_file_);
    if (!file)
        return;

    for (const auto& [source_path, entry] : entries_) {
        file << source_path << "|" << entry.source_hash << "|" << entry.mir_file << "|"
             << entry.object_file << "|" << entry.source_mtime << "|" << entry.optimization_level
             << "|" << (entry.debug_info ? "1" : "0") << "\n";
    }
}

std::string MirCache::compute_cache_key(const std::string& source_path) const {
    std::hash<std::string> hasher;
    size_t hash = hasher(source_path);

    std::ostringstream oss;
    oss << std::hex << std::setw(16) << std::setfill('0') << hash;
    return oss.str();
}

fs::path MirCache::get_mir_path(const std::string& cache_key) const {
    return cache_dir_ / (cache_key + ".mir");
}

fs::path MirCache::get_obj_path(const std::string& cache_key) const {
#ifdef _WIN32
    return cache_dir_ / (cache_key + ".obj");
#else
    return cache_dir_ / (cache_key + ".o");
#endif
}

bool MirCache::has_valid_cache(const std::string& source_path, const std::string& content_hash,
                               int opt_level, bool debug_info) const {
    load_index();

    auto it = entries_.find(source_path);
    if (it == entries_.end())
        return false;

    const auto& entry = it->second;

    // Check if source hash matches
    if (entry.source_hash != content_hash)
        return false;

    // Check if optimization level matches
    if (entry.optimization_level != opt_level)
        return false;

    // Check if debug info setting matches
    if (entry.debug_info != debug_info)
        return false;

    // Check if cached files exist
    std::string cache_key = compute_cache_key(source_path);
    if (!fs::exists(get_mir_path(cache_key)))
        return false;

    return true;
}

std::optional<mir::Module> MirCache::load_mir(const std::string& source_path) const {
    load_index();

    auto it = entries_.find(source_path);
    if (it == entries_.end())
        return std::nullopt;

    std::string cache_key = compute_cache_key(source_path);
    fs::path mir_path = get_mir_path(cache_key);

    if (!fs::exists(mir_path))
        return std::nullopt;

    try {
        auto module = mir::read_mir_file(mir_path.string());
        return module;
    } catch (...) {
        return std::nullopt;
    }
}

bool MirCache::save_mir(const std::string& source_path, const std::string& content_hash,
                        const mir::Module& module, int opt_level, bool debug_info) {
    load_index();

    std::string cache_key = compute_cache_key(source_path);
    fs::path mir_path = get_mir_path(cache_key);

    try {
        if (!mir::write_mir_file(module, mir_path.string(), true /* binary */)) {
            return false;
        }

        CacheEntry entry;
        entry.source_hash = content_hash;
        entry.mir_file = mir_path.string();
        entry.object_file = get_obj_path(cache_key).string();
        entry.source_mtime = get_mtime(fs::path(source_path));
        entry.optimization_level = opt_level;
        entry.debug_info = debug_info;

        entries_[source_path] = entry;
        save_index();

        return true;
    } catch (...) {
        return false;
    }
}

fs::path MirCache::get_cached_object(const std::string& source_path) const {
    load_index();

    auto it = entries_.find(source_path);
    if (it == entries_.end())
        return {};

    std::string cache_key = compute_cache_key(source_path);
    fs::path obj_path = get_obj_path(cache_key);

    if (fs::exists(obj_path))
        return obj_path;

    return {};
}

bool MirCache::save_object(const std::string& source_path, const fs::path& object_file) {
    load_index();

    auto it = entries_.find(source_path);
    if (it == entries_.end())
        return false;

    std::string cache_key = compute_cache_key(source_path);
    fs::path cached_obj = get_obj_path(cache_key);

    try {
        fs::copy_file(object_file, cached_obj, fs::copy_options::overwrite_existing);
        it->second.object_file = cached_obj.string();
        save_index();
        return true;
    } catch (...) {
        return false;
    }
}

void MirCache::clear() {
    loaded_ = false;
    entries_.clear();

    try {
        if (fs::exists(index_file_)) {
            fs::remove(index_file_);
        }
        // Remove all .mir and object files
        for (const auto& entry : fs::directory_iterator(cache_dir_)) {
            auto ext = entry.path().extension().string();
            if (ext == ".mir" || ext == ".o" || ext == ".obj") {
                fs::remove(entry.path());
            }
        }
    } catch (...) {}
}

void MirCache::invalidate(const std::string& source_path) {
    load_index();

    auto it = entries_.find(source_path);
    if (it == entries_.end())
        return;

    std::string cache_key = compute_cache_key(source_path);

    // Remove cached files
    try {
        fs::remove(get_mir_path(cache_key));
        fs::remove(get_obj_path(cache_key));
    } catch (...) {}

    entries_.erase(it);
    save_index();
}

MirCache::CacheStats MirCache::get_stats() const {
    load_index();
    load_func_index();

    CacheStats stats = {0, 0, 0, 0, 0};
    stats.total_entries = entries_.size();
    stats.function_entries = func_entries_.size();
    stats.function_cache_hits = func_stats_.cache_hits;

    for (const auto& [source_path, entry] : entries_) {
        std::string cache_key = compute_cache_key(source_path);
        fs::path mir_path = get_mir_path(cache_key);
        fs::path obj_path = get_obj_path(cache_key);

        bool valid = fs::exists(mir_path);
        if (valid) {
            stats.valid_entries++;
            try {
                stats.total_size_bytes += fs::file_size(mir_path);
            } catch (...) {}
        }

        if (fs::exists(obj_path)) {
            try {
                stats.total_size_bytes += fs::file_size(obj_path);
            } catch (...) {}
        }
    }

    // Add function cache sizes
    for (const auto& [key, entry] : func_entries_) {
        fs::path func_mir = entry.mir_file;
        if (fs::exists(func_mir)) {
            try {
                stats.total_size_bytes += fs::file_size(func_mir);
            } catch (...) {}
        }
    }

    return stats;
}

// ============================================================================
// Per-Function Caching Implementation
// ============================================================================

void MirCache::load_func_index() const {
    if (func_loaded_)
        return;
    func_loaded_ = true;

    if (!fs::exists(func_index_file_))
        return;

    std::ifstream file(func_index_file_);
    if (!file)
        return;

    std::string line;
    while (std::getline(file, line)) {
        // Format: source_path|func_name|sig_hash|body_hash|deps_hash|mir_file|opt_level
        std::istringstream iss(line);
        std::string source_path, func_name, sig_hash, body_hash, deps_hash, mir_file, opt_str;

        if (std::getline(iss, source_path, '|') && std::getline(iss, func_name, '|') &&
            std::getline(iss, sig_hash, '|') && std::getline(iss, body_hash, '|') &&
            std::getline(iss, deps_hash, '|') && std::getline(iss, mir_file, '|') &&
            std::getline(iss, opt_str)) {
            try {
                FunctionCacheEntry entry;
                entry.function_name = func_name;
                entry.signature_hash = sig_hash;
                entry.body_hash = body_hash;
                entry.deps_hash = deps_hash;
                entry.mir_file = mir_file;
                entry.optimization_level = std::stoi(opt_str);

                std::string key = source_path + "::" + func_name;
                func_entries_[key] = entry;
            } catch (...) {
                // Skip malformed entries
            }
        }
    }
}

void MirCache::save_func_index() const {
    std::ofstream file(func_index_file_);
    if (!file)
        return;

    for (const auto& [key, entry] : func_entries_) {
        // Extract source_path from key (format: source_path::func_name)
        size_t sep = key.find("::");
        std::string source_path = (sep != std::string::npos) ? key.substr(0, sep) : "";

        file << source_path << "|" << entry.function_name << "|" << entry.signature_hash << "|"
             << entry.body_hash << "|" << entry.deps_hash << "|" << entry.mir_file << "|"
             << entry.optimization_level << "\n";
    }
}

std::string MirCache::compute_func_cache_key(const std::string& source_path,
                                             const std::string& function_name) const {
    std::hash<std::string> hasher;
    size_t hash = hasher(source_path + "::" + function_name);

    std::ostringstream oss;
    oss << "func_" << std::hex << std::setw(16) << std::setfill('0') << hash;
    return oss.str();
}

fs::path MirCache::get_func_mir_path(const std::string& cache_key) const {
    return cache_dir_ / (cache_key + ".fmir");
}

// Helper to convert MIR type to string for hashing
static std::string mir_type_to_string(const mir::MirTypePtr& type) {
    if (!type)
        return "void";
    mir::MirPrinter printer(false);
    return printer.print_type(type);
}

std::string MirCache::hash_function_signature(const mir::Function& func) {
    std::ostringstream oss;

    // Hash parameters
    for (const auto& param : func.params) {
        oss << param.name << ":" << mir_type_to_string(param.type) << ";";
    }

    // Hash return type
    oss << "->" << mir_type_to_string(func.return_type);

    std::hash<std::string> hasher;
    size_t hash = hasher(oss.str());

    std::ostringstream result;
    result << std::hex << std::setw(16) << std::setfill('0') << hash;
    return result.str();
}

std::string MirCache::hash_function_body(const mir::Function& func) {
    std::ostringstream oss;

    // Hash all instructions in all blocks
    for (const auto& block : func.blocks) {
        oss << "BB" << block.id << "{";
        for (const auto& inst : block.instructions) {
            // Use instruction variant index and result for hashing
            oss << inst.inst.index() << ",";
            oss << "r" << inst.result << ",";
            if (inst.type) {
                oss << mir_type_to_string(inst.type) << ",";
            }
        }
        // Hash terminator
        if (block.terminator.has_value()) {
            oss << "T" << block.terminator->index();
        }
        oss << "}";
    }

    std::hash<std::string> hasher;
    size_t hash = hasher(oss.str());

    std::ostringstream result;
    result << std::hex << std::setw(16) << std::setfill('0') << hash;
    return result.str();
}

std::string MirCache::hash_function_deps(const mir::Function& func, const mir::Module& module) {
    std::ostringstream oss;

    // Collect used struct and enum names from the function
    std::set<std::string> used_types;

    // Scan instructions for type references
    for (const auto& block : func.blocks) {
        for (const auto& inst : block.instructions) {
            if (inst.type) {
                std::string type_str = mir_type_to_string(inst.type);
                // Extract struct/enum names from type string
                if (type_str.find("struct.") != std::string::npos ||
                    type_str.find("enum.") != std::string::npos) {
                    used_types.insert(type_str);
                }
            }
        }
    }

    // Hash the definitions of used types
    for (const auto& type_name : used_types) {
        oss << type_name << ";";
    }

    // Also hash the module's struct/enum count as a simple dependency check
    oss << "S" << module.structs.size() << "E" << module.enums.size();

    std::hash<std::string> hasher;
    size_t hash = hasher(oss.str());

    std::ostringstream result;
    result << std::hex << std::setw(16) << std::setfill('0') << hash;
    return result.str();
}

bool MirCache::has_valid_function_cache(const std::string& source_path,
                                        const std::string& function_name,
                                        const std::string& signature_hash,
                                        const std::string& body_hash, const std::string& deps_hash,
                                        int opt_level) const {
    load_func_index();

    std::string key = source_path + "::" + function_name;
    auto it = func_entries_.find(key);
    if (it == func_entries_.end()) {
        func_stats_.cache_misses++;
        return false;
    }

    const auto& entry = it->second;

    // Check all hashes match
    if (entry.signature_hash != signature_hash || entry.body_hash != body_hash ||
        entry.deps_hash != deps_hash || entry.optimization_level != opt_level) {
        func_stats_.cache_misses++;
        return false;
    }

    // Check cached file exists
    if (!fs::exists(entry.mir_file)) {
        func_stats_.cache_misses++;
        return false;
    }

    func_stats_.cache_hits++;
    return true;
}

std::optional<mir::Function> MirCache::load_function(const std::string& source_path,
                                                     const std::string& function_name) const {
    load_func_index();

    std::string key = source_path + "::" + function_name;
    auto it = func_entries_.find(key);
    if (it == func_entries_.end())
        return std::nullopt;

    fs::path func_mir_path = it->second.mir_file;
    if (!fs::exists(func_mir_path))
        return std::nullopt;

    try {
        // Read the function MIR file - it contains a mini-module with one function
        auto module = mir::read_mir_file(func_mir_path.string());
        if (!module.functions.empty()) {
            return std::move(module.functions[0]);
        }
    } catch (...) {}

    return std::nullopt;
}

bool MirCache::save_function(const std::string& source_path, const std::string& function_name,
                             const std::string& signature_hash, const std::string& body_hash,
                             const std::string& deps_hash, const mir::Function& func,
                             int opt_level) {
    load_func_index();

    std::string cache_key = compute_func_cache_key(source_path, function_name);
    fs::path func_mir_path = get_func_mir_path(cache_key);

    try {
        // Create a mini-module containing just this function
        mir::Module mini_module;
        mini_module.name = function_name;
        mini_module.functions.push_back(func);

        if (!mir::write_mir_file(mini_module, func_mir_path.string(), true /* binary */)) {
            return false;
        }

        FunctionCacheEntry entry;
        entry.function_name = function_name;
        entry.signature_hash = signature_hash;
        entry.body_hash = body_hash;
        entry.deps_hash = deps_hash;
        entry.mir_file = func_mir_path.string();
        entry.optimization_level = opt_level;

        std::string key = source_path + "::" + function_name;
        func_entries_[key] = entry;
        func_stats_.cached_functions++;
        save_func_index();

        return true;
    } catch (...) {
        return false;
    }
}

MirCache::FunctionCacheStats MirCache::get_function_stats() const {
    return func_stats_;
}

} // namespace tml::cli
