TML_MODULE("mcp")

//! # MCP Documentation Search Tools
//!
//! Documentation search infrastructure: DocSearchCache, BM25/HNSW indexing,
//! query expansion, MMR diversification, and the docs/search handler.

#include "codegen/llvm/llvm_ir_gen.hpp"
#include "doc/doc_model.hpp"
#include "doc/extractor.hpp"
#include "hir/hir_builder.hpp"
#include "mcp_tools_internal.hpp"
#include "mir/hir_mir_builder.hpp"
#include "mir/mir_pass.hpp"
#include "search/bm25_index.hpp"
#include "search/hnsw_index.hpp"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <iomanip>
#include <map>
#include <mutex>
#include <set>
#include <sstream>
#include <unordered_map>
#include <unordered_set>

namespace tml::mcp {

// ============================================================================
// Documentation Search Infrastructure
// ============================================================================

/// Cached documentation index for the docs/search tool.
/// Built lazily on first query, rebuilt when source files change.
/// Includes BM25 text index and HNSW vector index for hybrid search.
struct DocSearchCache {
    doc::DocIndex index;
    search::BM25Index bm25;
    std::unique_ptr<search::TfIdfVectorizer> vectorizer;
    std::unique_ptr<search::HnswIndex> hnsw;
    /// Flat list of all doc items for doc_id -> DocItem* mapping.
    std::vector<std::pair<const doc::DocItem*, std::string>> all_items;
    std::vector<std::pair<fs::path, fs::file_time_type>> tracked_files;
    bool initialized = false;
    int64_t build_time_ms = 0; // Index build time in milliseconds
    std::mutex mutex;
};

DocSearchCache g_doc_cache;

/// Discovers the TML project root by walking up from cwd or executable location.
/// Returns empty path if not found.
auto find_tml_root() -> fs::path {
    // Strategy 1: Walk up from current working directory
    auto cwd = fs::current_path();
    for (auto dir = cwd; dir.has_parent_path() && dir != dir.root_path(); dir = dir.parent_path()) {
        if (fs::exists(dir / "lib" / "core" / "src") && fs::exists(dir / "lib" / "std" / "src")) {
            return dir;
        }
    }

    // Strategy 2: Check relative to executable common locations
    std::vector<fs::path> candidates = {
        cwd / ".." / "..",        // build/debug/ -> root
        cwd / "..",               // build/ -> root
        cwd / ".." / ".." / "..", // build/debug/subdir -> root
    };

    for (const auto& candidate : candidates) {
        auto normalized = fs::weakly_canonical(candidate);
        if (fs::exists(normalized / "lib" / "core" / "src") &&
            fs::exists(normalized / "lib" / "std" / "src")) {
            return normalized;
        }
    }

    return {};
}

/// Collects all .tml source files from a directory recursively.
static auto collect_tml_files(const fs::path& dir) -> std::vector<fs::path> {
    std::vector<fs::path> files;
    if (!fs::exists(dir) || !fs::is_directory(dir)) {
        return files;
    }

    std::error_code ec;
    for (const auto& entry : fs::recursive_directory_iterator(dir, ec)) {
        if (entry.is_regular_file() && entry.path().extension() == ".tml") {
            // Skip test files — they don't contain public API docs
            auto path_str = entry.path().string();
            if (path_str.find("tests") == std::string::npos &&
                path_str.find(".test.") == std::string::npos) {
                files.push_back(entry.path());
            }
        }
    }

    return files;
}

/// Derives a module path from a file path relative to the lib root.
/// e.g. lib/core/src/str/mod.tml -> core::str
///      lib/std/src/json/types.tml -> std::json::types
static auto derive_module_path(const fs::path& file, const fs::path& root) -> std::string {
    auto rel = fs::relative(file, root);
    auto parts = rel.string();

    // Normalize separators
    std::replace(parts.begin(), parts.end(), '\\', '/');

    // Remove lib/ prefix
    if (parts.find("lib/") == 0) {
        parts = parts.substr(4);
    }

    // Remove src/ component
    auto src_pos = parts.find("/src/");
    if (src_pos != std::string::npos) {
        parts = parts.substr(0, src_pos) + "/" + parts.substr(src_pos + 5);
    }

    // Remove .tml extension
    auto ext_pos = parts.rfind(".tml");
    if (ext_pos != std::string::npos) {
        parts = parts.substr(0, ext_pos);
    }

    // Remove /mod suffix (mod.tml represents the parent module)
    if (parts.size() >= 4 && parts.substr(parts.size() - 4) == "/mod") {
        parts = parts.substr(0, parts.size() - 4);
    }

    // Convert / to ::
    std::string module_path;
    for (char c : parts) {
        module_path += (c == '/') ? ':' : c;
    }

    // Fix single : to ::
    std::string result;
    for (size_t i = 0; i < module_path.size(); ++i) {
        result += module_path[i];
        if (module_path[i] == ':' && (i + 1 >= module_path.size() || module_path[i + 1] != ':')) {
            result += ':';
        }
    }

    return result;
}

/// Extracts documentation items from a C++ header file (.hpp).
/// Parses `///` and `//!` doc comments and associates them with
/// function/class/struct/enum declarations following the comments.
///
/// Returns a DocModule with the extracted items, or nullopt if the file
/// has no documentable items.
static auto extract_hpp_docs(const fs::path& file_path, const fs::path& root)
    -> std::optional<doc::DocModule> {
    // Read entire file using C FILE* to avoid iostream ABI issues across DLL boundaries
    FILE* fp = fopen(file_path.string().c_str(), "rb");
    if (!fp) {
        return std::nullopt;
    }
    fseek(fp, 0, SEEK_END);
    long file_size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    std::string file_content(static_cast<size_t>(file_size), '\0');
    size_t nread = fread(&file_content[0], 1, static_cast<size_t>(file_size), fp);
    fclose(fp);
    file_content.resize(nread);

    // Derive module path from compiler/include/X/Y.hpp -> compiler::X::Y
    auto rel = fs::relative(file_path, root / "compiler" / "include");
    std::string mod_name = rel.string();
    std::replace(mod_name.begin(), mod_name.end(), '\\', '/');
    // Remove .hpp extension
    auto ext_pos = mod_name.rfind(".hpp");
    if (ext_pos != std::string::npos) {
        mod_name = mod_name.substr(0, ext_pos);
    }
    // Convert / to :: and prepend compiler::
    std::string mod_path = "compiler::";
    for (char c : mod_name) {
        if (c == '/') {
            mod_path += "::";
        } else {
            mod_path += c;
        }
    }

    doc::DocModule result;
    result.name = file_path.stem().string();
    result.path = mod_path;
    result.source_file = file_path.string();

    // Parse lines from the in-memory content (avoid std::getline with ifstream)
    std::istringstream file_stream(file_content);
    std::string line;
    std::string doc_comment;
    std::string module_doc;
    uint32_t line_num = 0;

    while (std::getline(file_stream, line)) {
        ++line_num;

        // Trim leading whitespace
        size_t start = line.find_first_not_of(" \t");
        if (start == std::string::npos) {
            if (!doc_comment.empty()) {
                doc_comment.clear(); // Blank line breaks doc comment
            }
            continue;
        }
        auto trimmed = line.substr(start);

        // Module-level doc comments (//!)
        if (trimmed.find("//!") == 0) {
            auto content = trimmed.substr(3);
            if (!content.empty() && content[0] == ' ') {
                content = content.substr(1);
            }
            module_doc += content + "\n";
            continue;
        }

        // Item-level doc comments (///)
        if (trimmed.find("///") == 0 && (trimmed.size() <= 3 || trimmed[3] != '/')) {
            auto content = trimmed.substr(3);
            if (!content.empty() && content[0] == ' ') {
                content = content.substr(1);
            }
            doc_comment += content + "\n";
            continue;
        }

        // If we have accumulated doc comments, check what follows
        if (!doc_comment.empty()) {
            doc::DocItem item;
            item.doc = doc_comment;
            item.source_file = file_path.string();
            item.source_line = line_num;
            item.path = mod_path;
            item.visibility = doc::DocVisibility::Public;

            // Extract first paragraph as summary
            auto nl_pos = doc_comment.find("\n\n");
            if (nl_pos != std::string::npos) {
                item.summary = doc_comment.substr(0, nl_pos);
            } else {
                item.summary = doc_comment;
                // Trim trailing newline
                while (!item.summary.empty() && item.summary.back() == '\n') {
                    item.summary.pop_back();
                }
            }

            bool found = false;

            // Match: auto function_name(... -> ...
            if (trimmed.find("auto ") != std::string::npos &&
                trimmed.find("->") != std::string::npos) {
                auto auto_pos = trimmed.find("auto ");
                auto name_start = auto_pos + 5;
                auto paren_pos = trimmed.find('(', name_start);
                if (paren_pos != std::string::npos) {
                    item.name = trimmed.substr(name_start, paren_pos - name_start);
                    item.kind = doc::DocItemKind::Function;
                    item.signature = trimmed;
                    // Trim trailing ; or {
                    while (!item.signature.empty() &&
                           (item.signature.back() == ';' || item.signature.back() == '{')) {
                        item.signature.pop_back();
                    }
                    item.id = mod_path + "::" + item.name;
                    found = true;
                }
            }

            // Match: class ClassName or struct ClassName
            if (!found) {
                for (const char* keyword : {"class ", "struct "}) {
                    if (trimmed.find(keyword) == 0) {
                        auto name_start = std::string(keyword).size();
                        std::string name;
                        for (size_t i = name_start; i < trimmed.size(); ++i) {
                            if (trimmed[i] == ' ' || trimmed[i] == '{' || trimmed[i] == ':' ||
                                trimmed[i] == ';') {
                                break;
                            }
                            name += trimmed[i];
                        }
                        if (!name.empty() && name != "}" && name != "=") {
                            item.name = name;
                            item.kind = doc::DocItemKind::Struct;
                            item.signature = std::string(keyword) + name;
                            item.id = mod_path + "::" + item.name;
                            found = true;
                        }
                        break;
                    }
                }
            }

            // Match: enum class EnumName
            if (!found && trimmed.find("enum ") == 0) {
                auto rest = trimmed.substr(5);
                if (rest.find("class ") == 0) {
                    rest = rest.substr(6);
                }
                std::string name;
                for (char c : rest) {
                    if (c == ' ' || c == '{' || c == ':')
                        break;
                    name += c;
                }
                if (!name.empty()) {
                    item.name = name;
                    item.kind = doc::DocItemKind::Enum;
                    item.signature = trimmed;
                    item.id = mod_path + "::" + item.name;
                    found = true;
                }
            }

            // Match: void/int/bool/... function_name(
            if (!found) {
                for (const char* ret : {"void ", "int ", "bool ", "size_t ", "std::string ",
                                        "static auto ", "static void ", "static int "}) {
                    if (trimmed.find(ret) == 0) {
                        auto name_start = std::string(ret).size();
                        auto paren_pos = trimmed.find('(', name_start);
                        if (paren_pos != std::string::npos) {
                            item.name = trimmed.substr(name_start, paren_pos - name_start);
                            item.kind = doc::DocItemKind::Function;
                            item.signature = trimmed;
                            while (!item.signature.empty() &&
                                   (item.signature.back() == ';' || item.signature.back() == '{')) {
                                item.signature.pop_back();
                            }
                            item.id = mod_path + "::" + item.name;
                            found = true;
                        }
                        break;
                    }
                }
            }

            if (found && !item.name.empty()) {
                result.items.push_back(std::move(item));
            }

            doc_comment.clear();
        }
    }

    // Set module-level doc
    if (!module_doc.empty()) {
        result.doc = module_doc;
        auto nl_pos = module_doc.find("\n\n");
        if (nl_pos != std::string::npos) {
            result.summary = module_doc.substr(0, nl_pos);
        } else {
            result.summary = module_doc;
            while (!result.summary.empty() && result.summary.back() == '\n') {
                result.summary.pop_back();
            }
        }
    }

    if (result.items.empty() && result.doc.empty()) {
        return std::nullopt;
    }

    return result;
}

/// Parses a single TML file and extracts documentation (parse-only, no type check).
static auto parse_file_for_docs(const fs::path& file_path) -> std::optional<parser::Module> {
    // Read source — use C FILE* to avoid C++ iostream ABI issues across DLL boundaries.
    // std::ifstream crashes when called from MCP DLL context on Windows (Zig CC toolchain).
    std::string source;
    {
        FILE* fp = fopen(file_path.string().c_str(), "rb");
        if (!fp) {
            return std::nullopt;
        }
        fseek(fp, 0, SEEK_END);
        long sz = ftell(fp);
        fseek(fp, 0, SEEK_SET);
        source.resize(static_cast<size_t>(sz));
        size_t nread = fread(&source[0], 1, static_cast<size_t>(sz), fp);
        fclose(fp);
        source.resize(nread);
    }

    // Preprocess
    preprocessor::Preprocessor pp;
    auto preprocessed = pp.process(source, file_path.string());
    if (!preprocessed.success()) {
        return std::nullopt;
    }

    // Lex
    auto src = lexer::Source::from_string(preprocessed.output, file_path.string());
    lexer::Lexer lex(src);
    auto tokens = lex.tokenize();
    if (lex.has_errors()) {
        return std::nullopt;
    }

    // Parse — use partial mode so files with some parse errors (e.g. unit-variant
    // enums that the parser currently misidentifies as struct fields) still get
    // their successfully-parsed declarations indexed.
    parser::Parser parser(std::move(tokens));
    auto module_name = file_path.stem().string();
    auto module = parser.parse_module_partial(module_name);

    // If the partial parse yielded no declarations and no module docs, there is
    // nothing useful to index — skip the file.
    if (module.decls.empty() && module.module_docs.empty()) {
        return std::nullopt;
    }

    return std::move(module);
}

/// Checks if any tracked files have changed since the index was built.
auto files_changed(const DocSearchCache& cache) -> bool {
    for (const auto& [path, mtime] : cache.tracked_files) {
        std::error_code ec;
        auto current_mtime = fs::last_write_time(path, ec);
        if (ec || current_mtime != mtime) {
            return true;
        }
    }
    return false;
}

/// Computes a content fingerprint for all source files.
/// Uses combined file sizes + mtimes as a fast fingerprint.
static auto compute_source_fingerprint(const std::vector<fs::path>& files) -> uint64_t {
    uint64_t hash = 0x517CC1B727220A95ULL; // FNV offset basis
    for (const auto& f : files) {
        std::error_code ec;
        auto sz = fs::file_size(f, ec);
        if (!ec) {
            hash ^= sz;
            hash *= 0x00000100000001B3ULL; // FNV prime
        }
        auto mtime = fs::last_write_time(f, ec);
        if (!ec) {
            auto mtime_val = mtime.time_since_epoch().count();
            hash ^= static_cast<uint64_t>(mtime_val);
            hash *= 0x00000100000001B3ULL;
        }
        // Include file path in hash to detect renames
        for (char c : f.string()) {
            hash ^= static_cast<uint64_t>(c);
            hash *= 0x00000100000001B3ULL;
        }
    }
    return hash;
}

/// Returns the cache directory for persisted indices.
static auto get_cache_dir(const fs::path& root) -> fs::path {
    return root / "build" / "debug" / ".doc-index";
}

/// Saves the BM25, TfIdf, and HNSW indices to disk.
/// Writes binary data to a file using C FILE* (avoids iostream ABI issues across DLL boundaries).
static bool write_binary_file(const fs::path& path, const void* data, size_t size) {
    FILE* fp = fopen(path.string().c_str(), "wb");
    if (!fp)
        return false;
    size_t written = fwrite(data, 1, size, fp);
    fclose(fp);
    return written == size;
}

static void save_cached_indices(const DocSearchCache& cache, const fs::path& root,
                                uint64_t fingerprint) {
    auto cache_dir = get_cache_dir(root);
    std::error_code ec;
    fs::create_directories(cache_dir, ec);
    if (ec)
        return;

    // Save fingerprint
    if (!write_binary_file(cache_dir / "fingerprint.bin", &fingerprint, sizeof(fingerprint)))
        return;

    // Save BM25 index
    auto bm25_data = cache.bm25.serialize();
    write_binary_file(cache_dir / "bm25.bin", bm25_data.data(), bm25_data.size());

    // Save TfIdf vectorizer
    if (cache.vectorizer) {
        auto tfidf_data = cache.vectorizer->serialize();
        write_binary_file(cache_dir / "tfidf.bin", tfidf_data.data(), tfidf_data.size());
    }

    // Save HNSW index
    if (cache.hnsw) {
        auto hnsw_data = cache.hnsw->serialize();
        write_binary_file(cache_dir / "hnsw.bin", hnsw_data.data(), hnsw_data.size());
    }
}

/// Reads a binary file into a byte vector using C FILE* (avoids iostream ABI issues).
static auto read_binary_file(const fs::path& path) -> std::vector<uint8_t> {
    FILE* fp = fopen(path.string().c_str(), "rb");
    if (!fp)
        return {};
    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    if (size <= 0) {
        fclose(fp);
        return {};
    }
    std::vector<uint8_t> data(static_cast<size_t>(size));
    size_t nread = fread(data.data(), 1, static_cast<size_t>(size), fp);
    fclose(fp);
    data.resize(nread);
    return data;
}

/// Tries to load persisted indices from disk.
/// Returns true if successfully loaded, false if rebuild needed.
static auto load_cached_indices(DocSearchCache& cache, const fs::path& root, uint64_t fingerprint)
    -> bool {
    auto cache_dir = get_cache_dir(root);

    // Check fingerprint
    auto fp_path = cache_dir / "fingerprint.bin";
    if (!fs::exists(fp_path))
        return false;

    uint64_t cached_fp = 0;
    {
        auto fp_data = read_binary_file(fp_path);
        if (fp_data.size() != sizeof(cached_fp))
            return false;
        memcpy(&cached_fp, fp_data.data(), sizeof(cached_fp));
    }
    if (cached_fp != fingerprint)
        return false;

    // Load BM25
    auto bm25_data = read_binary_file(cache_dir / "bm25.bin");
    if (bm25_data.empty())
        return false;
    search::BM25Index bm25;
    if (!bm25.deserialize(bm25_data.data(), bm25_data.size()))
        return false;

    // Load TfIdf
    auto tfidf_data = read_binary_file(cache_dir / "tfidf.bin");
    if (tfidf_data.empty())
        return false;
    auto vectorizer = std::make_unique<search::TfIdfVectorizer>(512);
    if (!vectorizer->deserialize(tfidf_data.data(), tfidf_data.size()))
        return false;

    // Load HNSW
    auto hnsw_data = read_binary_file(cache_dir / "hnsw.bin");
    if (hnsw_data.empty())
        return false;
    auto hnsw = std::make_unique<search::HnswIndex>(vectorizer->dims());
    if (!hnsw->deserialize(hnsw_data.data(), hnsw_data.size()))
        return false;

    // All loaded successfully — install into cache
    cache.bm25 = std::move(bm25);
    cache.vectorizer = std::move(vectorizer);
    cache.hnsw = std::move(hnsw);

    return true;
}

/// Builds or rebuilds the documentation index from TML library sources.
/// Uses persisted search indices when source files haven't changed.
void build_doc_index(DocSearchCache& cache) {
    auto build_start = std::chrono::steady_clock::now();

    auto root = find_tml_root();
    if (root.empty()) {
        return;
    }

    // Collect source files from all library directories dynamically
    std::vector<fs::path> all_files;
    auto lib_root = root / "lib";
    if (fs::exists(lib_root) && fs::is_directory(lib_root)) {
        for (const auto& lib_entry : fs::directory_iterator(lib_root)) {
            if (lib_entry.is_directory()) {
                auto src_dir = lib_entry.path() / "src";
                if (fs::exists(src_dir)) {
                    auto files = collect_tml_files(src_dir);
                    all_files.insert(all_files.end(), files.begin(), files.end());
                }
            }
        }
    }

    // Collect compiler header files for separate doc extraction
    std::vector<fs::path> hpp_files;
    auto compiler_include = root / "compiler" / "include";
    if (fs::exists(compiler_include)) {
        for (const auto& entry : fs::recursive_directory_iterator(compiler_include)) {
            if (entry.is_regular_file() && entry.path().extension() == ".hpp") {
                hpp_files.push_back(entry.path());
            }
        }
    }

    // Compute source fingerprint for cache validation (include both .tml and .hpp files)
    std::vector<fs::path> all_tracked_files = all_files;
    all_tracked_files.insert(all_tracked_files.end(), hpp_files.begin(), hpp_files.end());
    auto fingerprint = compute_source_fingerprint(all_tracked_files);

    // Parse each file and extract documentation
    // (always needed for the DocItem pointers in all_items)
    doc::ExtractorConfig config;
    config.include_private = false;
    config.extract_examples = true;
    doc::Extractor extractor(config);

    std::vector<std::pair<const parser::Module*, std::string>> module_pairs;
    std::vector<parser::Module> parsed_modules; // Keep alive for pointers
    parsed_modules.reserve(all_files.size());

    cache.tracked_files.clear();

    for (const auto& file : all_files) {
        try {
            auto module_opt = parse_file_for_docs(file);
            if (!module_opt) {
                continue;
            }

            parsed_modules.push_back(std::move(*module_opt));
            auto module_path = derive_module_path(file, root);
            module_pairs.push_back({&parsed_modules.back(), module_path});

            // Track file modification time
            std::error_code ec;
            auto mtime = fs::last_write_time(file, ec);
            if (!ec) {
                cache.tracked_files.push_back({file, mtime});
            }
        } catch (const std::exception& e) {
            // Skip files that fail to parse — don't crash the MCP server
            TML_LOG_WARN("mcp",
                         "Skipping doc extraction for " << file.string() << ": " << e.what());
        } catch (...) {
            TML_LOG_WARN("mcp",
                         "Skipping doc extraction for " << file.string() << ": unknown error");
        }
    }

    if (module_pairs.empty() && hpp_files.empty()) {
        return;
    }

    // Build the doc index from TML library sources
    if (!module_pairs.empty()) {
        try {
            cache.index = extractor.extract_all(module_pairs);
        } catch (const std::exception& e) {
            TML_LOG_ERROR("mcp", "Doc index build failed: " << e.what());
        } catch (...) {
            TML_LOG_ERROR("mcp", "Doc index build failed: unknown error");
        }
    }

    // Extract documentation from compiler C++ headers
    for (const auto& hpp_file : hpp_files) {
        try {
            auto hpp_mod = extract_hpp_docs(hpp_file, root);
            if (hpp_mod && (!hpp_mod->items.empty() || !hpp_mod->doc.empty())) {
                cache.index.modules.push_back(std::move(*hpp_mod));
            }

            // Track hpp file modification time
            std::error_code ec;
            auto mtime = fs::last_write_time(hpp_file, ec);
            if (!ec) {
                cache.tracked_files.push_back({hpp_file, mtime});
            }
        } catch (const std::exception& e) {
            TML_LOG_WARN("mcp", "Skipping hpp doc extraction for " << hpp_file.string() << ": "
                                                                   << e.what());
        } catch (...) {
            TML_LOG_WARN("mcp", "Skipping hpp doc extraction for " << hpp_file.string()
                                                                   << ": unknown error");
        }
    }

    // Flatten all items for doc_id mapping
    cache.all_items.clear();

    std::function<void(const std::vector<doc::DocItem>&, const std::string&)> collect_items;
    collect_items = [&](const std::vector<doc::DocItem>& items, const std::string& mod_path) {
        for (const auto& item : items) {
            cache.all_items.push_back({&item, mod_path});
            collect_items(item.methods, mod_path);
            collect_items(item.fields, mod_path);
            collect_items(item.variants, mod_path);
        }
    };

    std::function<void(const std::vector<doc::DocModule>&)> collect_modules;
    collect_modules = [&](const std::vector<doc::DocModule>& modules) {
        for (const auto& mod : modules) {
            collect_items(mod.items, mod.path);
            collect_modules(mod.submodules);
        }
    };

    collect_modules(cache.index.modules);

    // Try loading persisted search indices (BM25 + TfIdf + HNSW)
    bool loaded_from_cache = load_cached_indices(cache, root, fingerprint);

    if (loaded_from_cache) {
        // Verify the cached indices match the current item count
        if (cache.bm25.size() == cache.all_items.size() && cache.bm25.is_built()) {
            cache.initialized = true;
            auto build_end = std::chrono::steady_clock::now();
            cache.build_time_ms =
                std::chrono::duration_cast<std::chrono::milliseconds>(build_end - build_start)
                    .count();
            return; // Cache hit — skip expensive BM25/HNSW build
        }
        // Mismatch — fall through to rebuild
    }

    // Full rebuild: feed items to BM25 + HNSW
    cache.bm25 = search::BM25Index{};
    cache.vectorizer = std::make_unique<search::TfIdfVectorizer>(512);

    uint32_t doc_id = 0;
    for (const auto& [item, mod_path] : cache.all_items) {
        std::string combined = item->name + " " + item->signature + " " + item->doc + " " +
                               item->path + " " + mod_path;
        cache.bm25.add_document(doc_id, item->name, item->signature, item->doc, item->path);
        cache.vectorizer->add_document(doc_id, combined);
        doc_id++;
    }

    // Build BM25 index
    cache.bm25.build();

    // Build HNSW: vectorize all documents and insert
    // NOTE: HNSW disabled temporarily — crashes with 4000+ items on Windows.
    // BM25 text search still works.
    // TODO: Re-enable once the crash root cause is identified
    // cache.vectorizer->build();
    // size_t dims = cache.vectorizer->dims();
    size_t dims = 0; // Disabled

    if (dims > 0) {
        cache.hnsw = std::make_unique<search::HnswIndex>(dims);
        cache.hnsw->set_params(16, 200, 50);

        for (uint32_t i = 0; i < static_cast<uint32_t>(cache.all_items.size()); ++i) {
            const auto& [item, mod_path] = cache.all_items[i];
            std::string combined = item->name + " " + item->signature + " " + item->doc + " " +
                                   item->path + " " + mod_path;
            auto vec = cache.vectorizer->vectorize(combined);
            cache.hnsw->insert(i, vec);
        }
    }

    cache.initialized = true;

    auto build_end = std::chrono::steady_clock::now();
    cache.build_time_ms =
        std::chrono::duration_cast<std::chrono::milliseconds>(build_end - build_start).count();

    // Persist indices to disk for next startup
    save_cached_indices(cache, root, fingerprint);
}

} // namespace tml::mcp
