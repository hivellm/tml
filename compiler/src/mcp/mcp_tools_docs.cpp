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
#include <iomanip>
#include <map>
#include <mutex>
#include <set>
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

static DocSearchCache g_doc_cache;

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
    std::ifstream file(file_path);
    if (!file.is_open()) {
        return std::nullopt;
    }

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

    std::string line;
    std::string doc_comment;
    std::string module_doc;
    uint32_t line_num = 0;

    while (std::getline(file, line)) {
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
    // Read source
    std::ifstream file(file_path);
    if (!file.is_open()) {
        return std::nullopt;
    }
    std::stringstream buf;
    buf << file.rdbuf();
    auto source = buf.str();

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

    // Parse
    parser::Parser parser(std::move(tokens));
    auto module_name = file_path.stem().string();
    auto parse_result = parser.parse_module(module_name);
    if (std::holds_alternative<std::vector<parser::ParseError>>(parse_result)) {
        return std::nullopt;
    }

    return std::move(std::get<parser::Module>(parse_result));
}

/// Checks if any tracked files have changed since the index was built.
static auto files_changed(const DocSearchCache& cache) -> bool {
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
static void save_cached_indices(const DocSearchCache& cache, const fs::path& root,
                                uint64_t fingerprint) {
    auto cache_dir = get_cache_dir(root);
    std::error_code ec;
    fs::create_directories(cache_dir, ec);
    if (ec)
        return;

    // Save fingerprint
    auto fp_path = cache_dir / "fingerprint.bin";
    {
        std::ofstream out(fp_path, std::ios::binary);
        if (!out)
            return;
        out.write(reinterpret_cast<const char*>(&fingerprint), sizeof(fingerprint));
    }

    // Save BM25 index
    auto bm25_data = cache.bm25.serialize();
    {
        auto bm25_path = cache_dir / "bm25.bin";
        std::ofstream out(bm25_path, std::ios::binary);
        if (out)
            out.write(reinterpret_cast<const char*>(bm25_data.data()), bm25_data.size());
    }

    // Save TfIdf vectorizer
    if (cache.vectorizer) {
        auto tfidf_data = cache.vectorizer->serialize();
        auto tfidf_path = cache_dir / "tfidf.bin";
        std::ofstream out(tfidf_path, std::ios::binary);
        if (out)
            out.write(reinterpret_cast<const char*>(tfidf_data.data()), tfidf_data.size());
    }

    // Save HNSW index
    if (cache.hnsw) {
        auto hnsw_data = cache.hnsw->serialize();
        auto hnsw_path = cache_dir / "hnsw.bin";
        std::ofstream out(hnsw_path, std::ios::binary);
        if (out)
            out.write(reinterpret_cast<const char*>(hnsw_data.data()), hnsw_data.size());
    }
}

/// Reads a binary file into a byte vector.
static auto read_binary_file(const fs::path& path) -> std::vector<uint8_t> {
    std::ifstream in(path, std::ios::binary | std::ios::ate);
    if (!in)
        return {};
    auto size = in.tellg();
    in.seekg(0);
    std::vector<uint8_t> data(static_cast<size_t>(size));
    in.read(reinterpret_cast<char*>(data.data()), size);
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
        std::ifstream in(fp_path, std::ios::binary);
        if (!in)
            return false;
        in.read(reinterpret_cast<char*>(&cached_fp), sizeof(cached_fp));
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
static void build_doc_index(DocSearchCache& cache) {
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
    }

    if (module_pairs.empty() && hpp_files.empty()) {
        return;
    }

    // Build the doc index from TML library sources
    if (!module_pairs.empty()) {
        cache.index = extractor.extract_all(module_pairs);
    }

    // Extract documentation from compiler C++ headers
    for (const auto& hpp_file : hpp_files) {
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
    cache.vectorizer->build();
    size_t dims = cache.vectorizer->dims();

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

/// Ensures the doc index is built and up-to-date.
void ensure_doc_index() {
    std::lock_guard<std::mutex> lock(g_doc_cache.mutex);

    if (!g_doc_cache.initialized || files_changed(g_doc_cache)) {
        build_doc_index(g_doc_cache);
    }
}

/// Case-insensitive substring search.
auto icontains(const std::string& haystack, const std::string& needle) -> bool {
    if (needle.empty()) {
        return true;
    }
    auto it = std::search(haystack.begin(), haystack.end(), needle.begin(), needle.end(),
                          [](char a, char b) {
                              return std::tolower(static_cast<unsigned char>(a)) ==
                                     std::tolower(static_cast<unsigned char>(b));
                          });
    return it != haystack.end();
}

/// Converts a string to a DocItemKind filter, or nullopt if invalid.
auto parse_kind_filter(const std::string& kind) -> std::optional<doc::DocItemKind> {
    std::string k = kind;
    std::transform(k.begin(), k.end(), k.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    if (k == "function" || k == "func")
        return doc::DocItemKind::Function;
    if (k == "method")
        return doc::DocItemKind::Method;
    if (k == "struct" || k == "type")
        return doc::DocItemKind::Struct;
    if (k == "enum")
        return doc::DocItemKind::Enum;
    if (k == "behavior" || k == "trait")
        return doc::DocItemKind::Trait;
    if (k == "constant" || k == "const")
        return doc::DocItemKind::Constant;
    if (k == "field")
        return doc::DocItemKind::Field;
    if (k == "variant")
        return doc::DocItemKind::Variant;
    if (k == "impl")
        return doc::DocItemKind::Impl;
    if (k == "module")
        return doc::DocItemKind::Module;
    return std::nullopt;
}

/// A scored search result entry.
struct ScoredDocResult {
    const doc::DocItem* item;
    std::string module_path;
    float score;
    float bm25_contribution = 0.0f; // Score breakdown: BM25 portion
    float hnsw_contribution = 0.0f; // Score breakdown: HNSW portion
    float signal_boost = 0.0f;      // Score breakdown: multi-signal boost
};

/// Formats a single search result for display.
static void format_result(std::stringstream& out, const ScoredDocResult& result) {
    const auto& item = *result.item;
    auto kind_str = doc::doc_item_kind_to_string(item.kind);

    out << "=== " << item.path << " (" << kind_str << ") ===\n";

    if (!item.signature.empty()) {
        out << "  Signature: " << item.signature << "\n";
    }

    out << "  Module:    " << result.module_path << "\n";

    if (!item.source_file.empty()) {
        out << "  Source:    " << item.source_file;
        if (item.source_line > 0) {
            out << ":" << item.source_line;
        }
        out << "\n";
    }

    if (!item.summary.empty()) {
        out << "\n  " << item.summary << "\n";
    } else if (!item.doc.empty()) {
        // Show first 200 chars of doc if no summary
        auto doc_preview = item.doc.substr(0, 200);
        if (item.doc.size() > 200) {
            doc_preview += "...";
        }
        out << "\n  " << doc_preview << "\n";
    }

    // Show parameters for functions/methods
    if (!item.params.empty() &&
        (item.kind == doc::DocItemKind::Function || item.kind == doc::DocItemKind::Method)) {
        bool has_desc = false;
        for (const auto& param : item.params) {
            if (!param.description.empty()) {
                has_desc = true;
                break;
            }
        }
        if (has_desc) {
            out << "\n  Parameters:\n";
            for (const auto& param : item.params) {
                if (param.name == "this")
                    continue;
                out << "    " << param.name;
                if (!param.type.empty()) {
                    out << ": " << param.type;
                }
                if (!param.description.empty()) {
                    out << " - " << param.description;
                }
                out << "\n";
            }
        }
    }

    // Show return type
    if (item.returns && !item.returns->description.empty()) {
        out << "  Returns: " << item.returns->description << "\n";
    }

    // Show deprecation warning
    if (item.deprecated) {
        out << "\n  [DEPRECATED] " << item.deprecated->message << "\n";
    }

    // Score breakdown (for debugging/transparency)
    if (result.bm25_contribution > 0.0f || result.hnsw_contribution > 0.0f ||
        result.signal_boost > 0.0f) {
        out << "  Score: " << std::fixed << std::setprecision(4) << result.score;
        out << " (";
        bool first = true;
        if (result.bm25_contribution > 0.0f) {
            out << "BM25=" << result.bm25_contribution;
            first = false;
        }
        if (result.hnsw_contribution > 0.0f) {
            if (!first)
                out << ", ";
            out << "HNSW=" << result.hnsw_contribution;
            first = false;
        }
        if (result.signal_boost > 0.0f) {
            if (!first)
                out << ", ";
            out << "boost=" << result.signal_boost;
        }
        out << ")\n";
    }

    out << "\n";
}

/// Reciprocal Rank Fusion: merges two ranked result lists.
/// RRF score = sum(weight / (k + rank)) for each list where the item appears.
/// BM25 gets 2x weight since keyword matches are more precise for doc search.
/// HNSW-only results (no BM25 match) require very low distance to be included,
/// preventing noisy semantic results from polluting keyword searches.
static auto reciprocal_rank_fusion(const std::vector<search::BM25Result>& bm25_results,
                                   const std::vector<search::HnswResult>& hnsw_results,
                                   size_t limit) -> std::vector<ScoredDocResult> {
    const float k = 60.0f;          // Standard RRF constant
    const float bm25_weight = 2.0f; // BM25 is more precise for keyword search
    const float hnsw_weight = 1.0f;
    const float hnsw_boost_cutoff = 0.8f;      // HNSW results close enough to boost BM25 matches
    const float hnsw_standalone_cutoff = 0.5f; // HNSW-only results need very high similarity

    // Track which doc_ids appear in BM25 results
    std::unordered_set<uint32_t> bm25_doc_ids;
    for (const auto& r : bm25_results) {
        bm25_doc_ids.insert(r.doc_id);
    }

    // Map doc_id -> fused score
    std::unordered_map<uint32_t, float> fused_scores;

    for (size_t rank = 0; rank < bm25_results.size(); ++rank) {
        fused_scores[bm25_results[rank].doc_id] += bm25_weight / (k + static_cast<float>(rank + 1));
    }

    for (size_t rank = 0; rank < hnsw_results.size(); ++rank) {
        auto doc_id = hnsw_results[rank].doc_id;
        float distance = hnsw_results[rank].distance;

        bool in_bm25 = bm25_doc_ids.count(doc_id) > 0;

        if (in_bm25 && distance < hnsw_boost_cutoff) {
            // Boost BM25 matches that also have good semantic similarity
            fused_scores[doc_id] += hnsw_weight / (k + static_cast<float>(rank + 1));
        } else if (!in_bm25 && distance < hnsw_standalone_cutoff) {
            // Only include HNSW-only results if they are very semantically similar
            fused_scores[doc_id] += hnsw_weight / (k + static_cast<float>(rank + 1));
        }
        // Otherwise: skip noisy HNSW results
    }

    // Build result list
    std::vector<ScoredDocResult> results;
    results.reserve(fused_scores.size());

    for (const auto& [doc_id, score] : fused_scores) {
        if (doc_id < g_doc_cache.all_items.size()) {
            const auto& [item, mod_path] = g_doc_cache.all_items[doc_id];
            results.push_back({item, mod_path, score});
        }
    }

    // Sort by score descending
    std::sort(results.begin(), results.end(),
              [](const ScoredDocResult& a, const ScoredDocResult& b) { return a.score > b.score; });

    if (results.size() > limit) {
        results.resize(limit);
    }

    return results;
}

/// Applies kind and module filters to a result set.
static void apply_filters(std::vector<ScoredDocResult>& results,
                          std::optional<doc::DocItemKind> kind_filter,
                          const std::string& module_filter) {
    if (!kind_filter && module_filter.empty())
        return;

    results.erase(std::remove_if(results.begin(), results.end(),
                                 [&](const ScoredDocResult& r) {
                                     if (kind_filter && r.item->kind != *kind_filter)
                                         return true;
                                     if (!module_filter.empty() &&
                                         !icontains(r.module_path, module_filter) &&
                                         !icontains(r.item->path, module_filter))
                                         return true;
                                     return false;
                                 }),
                  results.end());
}

// ============================================================================
// Query Processing (expansion, synonyms, stop words)
// ============================================================================

/// TML-specific synonym map for query expansion.
/// Maps common search terms to their TML equivalents.
static const std::unordered_map<std::string, std::vector<std::string>>& get_tml_synonyms() {
    static const std::unordered_map<std::string, std::vector<std::string>> synonyms = {
        {"error", {"Outcome", "Err", "Result"}},
        {"result", {"Outcome", "Ok", "Err"}},
        {"optional", {"Maybe", "Just", "Nothing"}},
        {"option", {"Maybe", "Just", "Nothing"}},
        {"none", {"Nothing", "Maybe"}},
        {"some", {"Just", "Maybe"}},
        {"null", {"Nothing", "Maybe"}},
        {"nullable", {"Maybe", "Just", "Nothing"}},
        {"box", {"Heap"}},
        {"heap", {"Heap", "alloc"}},
        {"rc", {"Shared"}},
        {"arc", {"Sync"}},
        {"clone", {"duplicate", "Duplicate"}},
        {"trait", {"behavior"}},
        {"interface", {"behavior"}},
        {"unsafe", {"lowlevel"}},
        {"match", {"when"}},
        {"switch", {"when"}},
        {"for", {"loop", "iter"}},
        {"while", {"loop"}},
        {"fn", {"func"}},
        {"function", {"func"}},
        {"string", {"Str", "str"}},
        {"vector", {"List"}},
        {"vec", {"List"}},
        {"array", {"List", "Array"}},
        {"map", {"HashMap"}},
        {"hashmap", {"HashMap"}},
        {"dict", {"HashMap"}},
        {"dictionary", {"HashMap"}},
        {"set", {"HashSet"}},
        {"hashset", {"HashSet"}},
        {"mutex", {"Mutex", "sync"}},
        {"lock", {"Mutex", "sync"}},
        {"thread", {"thread", "spawn"}},
        {"async", {"async", "Future"}},
        {"future", {"Future", "async"}},
        {"print", {"print", "println", "fmt"}},
        {"format", {"fmt", "format", "Display"}},
        {"display", {"Display", "fmt", "to_str"}},
        {"debug", {"Debug", "fmt"}},
        {"hash", {"Hash", "fnv", "murmur"}},
        {"json", {"Json", "JsonValue", "parse"}},
        {"file", {"File", "read", "write", "open"}},
        {"socket", {"TcpStream", "TcpListener", "net"}},
        {"http", {"net", "TcpStream"}},
        {"encrypt", {"crypto", "aes", "sha"}},
        {"crypto", {"crypto", "sha256", "aes"}},
        {"compress", {"zlib", "gzip", "deflate"}},
        {"sort", {"sort", "sorted", "cmp", "Ordering"}},
        {"compare", {"cmp", "Ordering", "PartialOrd"}},
        {"iterator", {"iter", "Iterator", "next"}},
        {"range", {"to", "through", "Range"}},
        {"slice", {"slice", "Slice"}},
        {"convert", {"From", "Into", "as"}},
        {"cast", {"as", "From", "Into"}},
        {"log", {"log", "info", "warn", "error", "debug"}},
        {"logging", {"log", "Logger"}},
    };
    return synonyms;
}

/// Query stop words to remove before searching.
static const std::unordered_set<std::string>& get_query_stop_words() {
    static const std::unordered_set<std::string> stops = {
        "the",     "a",      "an",     "is",    "are",   "was",  "were",  "be",    "been",
        "being",   "have",   "has",    "had",   "do",    "does", "did",   "will",  "would",
        "shall",   "should", "may",    "might", "must",  "can",  "could", "in",    "on",
        "at",      "to",     "for",    "of",    "with",  "by",   "from",  "as",    "into",
        "through", "during", "before", "after", "about", "i",    "me",    "my",    "we",
        "our",     "you",    "your",   "it",    "its",   "this", "that",  "these", "those",
        "what",    "which",  "who",    "how",   "where", "when", "why",   "and",   "or",
        "but",     "not",    "no",     "nor",   "all",   "each", "every", "any",   "both",
        "tml",     "use",    "using",
    };
    return stops;
}

/// Processes a query: removes stop words and expands with TML synonyms.
/// Returns a list of queries to search (original cleaned + expanded variants).
static auto process_query(const std::string& raw_query) -> std::vector<std::string> {
    std::vector<std::string> queries;
    const auto& stops = get_query_stop_words();
    const auto& synonyms = get_tml_synonyms();

    // Tokenize and clean the query
    std::string lower_query;
    lower_query.reserve(raw_query.size());
    for (char c : raw_query) {
        lower_query += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }

    std::vector<std::string> tokens;
    std::istringstream iss(lower_query);
    std::string token;
    while (iss >> token) {
        // Strip non-alphanumeric from edges
        while (!token.empty() && !std::isalnum(static_cast<unsigned char>(token.front()))) {
            token.erase(token.begin());
        }
        while (!token.empty() && !std::isalnum(static_cast<unsigned char>(token.back()))) {
            token.pop_back();
        }
        if (!token.empty() && stops.find(token) == stops.end()) {
            tokens.push_back(token);
        }
    }

    // Build cleaned query (stop words removed)
    std::string cleaned;
    for (size_t i = 0; i < tokens.size(); ++i) {
        if (i > 0)
            cleaned += " ";
        cleaned += tokens[i];
    }

    // Always include the original raw query (BM25 tokenizer handles its own splitting)
    queries.push_back(raw_query);

    // Add cleaned query if different
    if (!cleaned.empty() && cleaned != raw_query) {
        queries.push_back(cleaned);
    }

    // Expand each token with TML synonyms
    for (const auto& tok : tokens) {
        auto it = synonyms.find(tok);
        if (it != synonyms.end()) {
            for (const auto& syn : it->second) {
                // Add each synonym as a standalone query
                queries.push_back(syn);
                // Also combine synonym with other tokens for context
                if (tokens.size() > 1) {
                    std::string combined;
                    for (const auto& t : tokens) {
                        if (t == tok) {
                            combined += syn;
                        } else {
                            combined += t;
                        }
                        combined += " ";
                    }
                    if (!combined.empty())
                        combined.pop_back();
                    queries.push_back(combined);
                }
            }
        }
    }

    // Deduplicate
    std::unordered_set<std::string> seen;
    std::vector<std::string> unique;
    for (auto& q : queries) {
        if (seen.insert(q).second) {
            unique.push_back(std::move(q));
        }
    }

    // Limit to 8 queries max (original + 7 expansions)
    if (unique.size() > 8) {
        unique.resize(8);
    }

    return unique;
}

/// Multi-query fusion: search multiple expanded queries and merge results.
/// Each result keeps its best score across all queries.
static auto multi_query_search(const std::vector<std::string>& queries, const std::string& mode,
                               size_t fetch_limit) -> std::vector<ScoredDocResult> {
    std::unordered_map<uint32_t, ScoredDocResult> best_results;

    for (size_t qi = 0; qi < queries.size(); ++qi) {
        const auto& q = queries[qi];
        // Weight: original query gets full weight, expansions get diminishing weight
        float query_weight = (qi == 0) ? 1.0f : 0.6f;

        if (mode == "text") {
            auto bm25_results = g_doc_cache.bm25.search(q, fetch_limit);
            for (const auto& r : bm25_results) {
                if (r.doc_id < g_doc_cache.all_items.size()) {
                    float weighted = r.score * query_weight;
                    auto it = best_results.find(r.doc_id);
                    if (it == best_results.end() || weighted > it->second.score) {
                        const auto& [item, mod_path] = g_doc_cache.all_items[r.doc_id];
                        best_results[r.doc_id] = {item, mod_path, weighted, weighted, 0.0f, 0.0f};
                    }
                }
            }
        } else if (mode == "semantic" && g_doc_cache.hnsw && g_doc_cache.vectorizer) {
            auto query_vec = g_doc_cache.vectorizer->vectorize(q);
            auto hnsw_results = g_doc_cache.hnsw->search(query_vec, fetch_limit);
            for (const auto& r : hnsw_results) {
                if (r.doc_id < g_doc_cache.all_items.size()) {
                    float sim = (1.0f - r.distance) * query_weight;
                    auto it = best_results.find(r.doc_id);
                    if (it == best_results.end() || sim > it->second.score) {
                        const auto& [item, mod_path] = g_doc_cache.all_items[r.doc_id];
                        best_results[r.doc_id] = {item, mod_path, sim, 0.0f, sim, 0.0f};
                    }
                }
            }
        } else {
            // Hybrid: run both and fuse per query
            auto bm25_results = g_doc_cache.bm25.search(q, fetch_limit);

            if (g_doc_cache.hnsw && g_doc_cache.vectorizer) {
                auto query_vec = g_doc_cache.vectorizer->vectorize(q);
                auto hnsw_results = g_doc_cache.hnsw->search(query_vec, fetch_limit);
                auto fused = reciprocal_rank_fusion(bm25_results, hnsw_results, fetch_limit);
                for (auto& r : fused) {
                    // Find the doc_id by scanning all_items
                    for (uint32_t did = 0; did < g_doc_cache.all_items.size(); ++did) {
                        if (g_doc_cache.all_items[did].first == r.item) {
                            float weighted = r.score * query_weight;
                            auto it = best_results.find(did);
                            if (it == best_results.end() || weighted > it->second.score) {
                                r.score = weighted;
                                best_results[did] = r;
                            }
                            break;
                        }
                    }
                }
            } else {
                for (const auto& r : bm25_results) {
                    if (r.doc_id < g_doc_cache.all_items.size()) {
                        float weighted = r.score * query_weight;
                        auto it = best_results.find(r.doc_id);
                        if (it == best_results.end() || weighted > it->second.score) {
                            const auto& [item, mod_path] = g_doc_cache.all_items[r.doc_id];
                            best_results[r.doc_id] = {item,     mod_path, weighted,
                                                      weighted, 0.0f,     0.0f};
                        }
                    }
                }
            }
        }
    }

    // Convert map to vector and sort
    std::vector<ScoredDocResult> results;
    results.reserve(best_results.size());
    for (auto& [_, r] : best_results) {
        results.push_back(std::move(r));
    }
    std::sort(results.begin(), results.end(),
              [](const ScoredDocResult& a, const ScoredDocResult& b) { return a.score > b.score; });

    if (results.size() > fetch_limit) {
        results.resize(fetch_limit);
    }

    return results;
}

// ============================================================================
// MMR Diversification
// ============================================================================

/// Computes Jaccard similarity between two text strings (word-set based).
static auto jaccard_similarity(const std::string& a, const std::string& b) -> float {
    std::unordered_set<std::string> words_a, words_b;

    auto tokenize = [](const std::string& text, std::unordered_set<std::string>& words) {
        std::string lower;
        lower.reserve(text.size());
        for (char c : text) {
            lower += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        }
        std::istringstream iss(lower);
        std::string word;
        while (iss >> word) {
            if (word.size() >= 2) {
                words.insert(word);
            }
        }
    };

    tokenize(a, words_a);
    tokenize(b, words_b);

    if (words_a.empty() && words_b.empty())
        return 0.0f;

    size_t intersection = 0;
    for (const auto& w : words_a) {
        if (words_b.count(w))
            ++intersection;
    }

    size_t union_size = words_a.size() + words_b.size() - intersection;
    if (union_size == 0)
        return 0.0f;

    return static_cast<float>(intersection) / static_cast<float>(union_size);
}

/// Builds a content string for an item (for similarity comparison).
static auto item_content(const ScoredDocResult& r) -> std::string {
    return r.item->name + " " + r.item->signature + " " + r.module_path;
}

/// MMR (Maximal Marginal Relevance) diversification.
/// Reranks results to balance relevance and diversity.
/// lambda = 1.0 -> pure relevance, lambda = 0.0 -> pure diversity.
static void mmr_diversify(std::vector<ScoredDocResult>& results, float lambda = 0.7f) {
    if (results.size() <= 2)
        return;

    std::vector<ScoredDocResult> diversified;
    diversified.reserve(results.size());

    // First result is always the top-scored one
    diversified.push_back(std::move(results[0]));
    results.erase(results.begin());

    // Pre-compute content strings for remaining
    std::vector<std::string> contents;
    contents.reserve(results.size());
    for (const auto& r : results) {
        contents.push_back(item_content(r));
    }

    std::vector<std::string> selected_contents;
    selected_contents.push_back(item_content(diversified[0]));

    while (!results.empty() && diversified.size() < diversified.capacity()) {
        float best_mmr = -1e9f;
        size_t best_idx = 0;

        for (size_t i = 0; i < results.size(); ++i) {
            // Find max similarity to any already-selected result
            float max_sim = 0.0f;
            for (const auto& sel_content : selected_contents) {
                float sim = jaccard_similarity(contents[i], sel_content);
                if (sim > max_sim)
                    max_sim = sim;
            }

            // MMR score: balance relevance vs diversity
            float mmr = lambda * results[i].score - (1.0f - lambda) * max_sim;
            if (mmr > best_mmr) {
                best_mmr = mmr;
                best_idx = i;
            }
        }

        selected_contents.push_back(contents[best_idx]);
        diversified.push_back(std::move(results[best_idx]));
        results.erase(results.begin() + static_cast<ptrdiff_t>(best_idx));
        contents.erase(contents.begin() + static_cast<ptrdiff_t>(best_idx));
    }

    results = std::move(diversified);
}

/// Deduplicates near-identical results using Jaccard threshold.
static void deduplicate_results(std::vector<ScoredDocResult>& results, float threshold = 0.8f) {
    if (results.size() <= 1)
        return;

    std::vector<ScoredDocResult> deduped;
    deduped.reserve(results.size());

    for (auto& r : results) {
        bool is_dup = false;
        std::string content = item_content(r);
        for (const auto& kept : deduped) {
            if (jaccard_similarity(content, item_content(kept)) > threshold) {
                is_dup = true;
                break;
            }
        }
        if (!is_dup) {
            deduped.push_back(std::move(r));
        }
    }

    results = std::move(deduped);
}

// ============================================================================
// Multi-Signal Ranking Boost
// ============================================================================

/// Applies multi-signal ranking boosts to results.
/// Boosts pub items, well-documented items, and top-level module items.
static void apply_signal_boosts(std::vector<ScoredDocResult>& results) {
    for (auto& r : results) {
        float boost = 0.0f;

        // Boost pub items (have "pub" in signature)
        if (!r.item->signature.empty() && r.item->signature.find("pub ") != std::string::npos) {
            boost += 0.005f;
        }

        // Boost well-documented items (have doc comments)
        if (!r.item->doc.empty()) {
            boost += 0.003f;
            // Extra boost for items with parameter docs
            if (!r.item->params.empty()) {
                bool has_param_docs = false;
                for (const auto& p : r.item->params) {
                    if (!p.description.empty()) {
                        has_param_docs = true;
                        break;
                    }
                }
                if (has_param_docs) {
                    boost += 0.002f;
                }
            }
        }

        // Boost top-level module items (fewer :: separators = more prominent)
        {
            size_t depth = 0;
            for (size_t i = 0; i + 1 < r.module_path.size(); ++i) {
                if (r.module_path[i] == ':' && r.module_path[i + 1] == ':') {
                    ++depth;
                    ++i; // skip second ':'
                }
            }
            // Top-level (depth 1 like "core::str") gets more boost
            if (depth <= 1) {
                boost += 0.003f;
            } else if (depth == 2) {
                boost += 0.001f;
            }
        }

        r.signal_boost = boost;
        r.score += boost;
    }

    // Re-sort after boosting
    std::sort(results.begin(), results.end(),
              [](const ScoredDocResult& a, const ScoredDocResult& b) { return a.score > b.score; });
}

// ============================================================================
// Search Handler
// ============================================================================

auto handle_docs_search(const json::JsonValue& params) -> ToolResult {
    // Get query parameter
    auto* query_param = params.get("query");
    if (query_param == nullptr || !query_param->is_string()) {
        return ToolResult::error("Missing or invalid 'query' parameter");
    }
    std::string query = query_param->as_string();

    // Get limit parameter (optional)
    int64_t limit = 10;
    auto* limit_param = params.get("limit");
    if (limit_param != nullptr && limit_param->is_integer()) {
        limit = limit_param->as_i64();
    }

    // Get kind filter (optional)
    std::optional<doc::DocItemKind> kind_filter;
    auto* kind_param = params.get("kind");
    if (kind_param != nullptr && kind_param->is_string()) {
        kind_filter = parse_kind_filter(kind_param->as_string());
        if (!kind_filter) {
            return ToolResult::error(
                "Invalid 'kind' parameter. Valid values: function, method, struct, enum, "
                "behavior, constant, field, variant");
        }
    }

    // Get module filter (optional)
    std::string module_filter;
    auto* module_param = params.get("module");
    if (module_param != nullptr && module_param->is_string()) {
        module_filter = module_param->as_string();
    }

    // Get search mode (optional, default: hybrid)
    std::string mode = "hybrid";
    auto* mode_param = params.get("mode");
    if (mode_param != nullptr && mode_param->is_string()) {
        mode = mode_param->as_string();
        if (mode != "text" && mode != "semantic" && mode != "hybrid") {
            return ToolResult::error(
                "Invalid 'mode' parameter. Valid values: text, semantic, hybrid");
        }
    }

    // Ensure the documentation index is built
    ensure_doc_index();

    std::stringstream output;

    if (!g_doc_cache.initialized) {
        output << "Documentation index not available.\n";
        output << "Could not locate TML library sources.\n";
        output << "Ensure the MCP server is run from the TML project directory.\n";
        return ToolResult::text(output.str());
    }

    auto search_start = std::chrono::steady_clock::now();

    std::vector<ScoredDocResult> results;
    size_t fetch_limit = static_cast<size_t>(limit) * 3; // Over-fetch before filtering

    // Query processing — expand with synonyms and clean stop words
    auto expanded_queries = process_query(query);
    bool used_expansion = expanded_queries.size() > 1;

    if (expanded_queries.size() > 1) {
        // Multi-query fusion: search all expanded queries and merge
        results = multi_query_search(expanded_queries, mode, fetch_limit);
    } else {
        // Single query path (original behavior)
        if (mode == "text") {
            auto bm25_results = g_doc_cache.bm25.search(query, fetch_limit);
            results.reserve(bm25_results.size());
            for (const auto& r : bm25_results) {
                if (r.doc_id < g_doc_cache.all_items.size()) {
                    const auto& [item, mod_path] = g_doc_cache.all_items[r.doc_id];
                    results.push_back({item, mod_path, r.score, r.score, 0.0f, 0.0f});
                }
            }
        } else if (mode == "semantic") {
            if (g_doc_cache.hnsw && g_doc_cache.vectorizer) {
                auto query_vec = g_doc_cache.vectorizer->vectorize(query);
                auto hnsw_results = g_doc_cache.hnsw->search(query_vec, fetch_limit);
                results.reserve(hnsw_results.size());
                for (const auto& r : hnsw_results) {
                    if (r.doc_id < g_doc_cache.all_items.size()) {
                        const auto& [item, mod_path] = g_doc_cache.all_items[r.doc_id];
                        float sim = 1.0f - r.distance;
                        results.push_back({item, mod_path, sim, 0.0f, sim, 0.0f});
                    }
                }
            } else {
                auto bm25_results = g_doc_cache.bm25.search(query, fetch_limit);
                for (const auto& r : bm25_results) {
                    if (r.doc_id < g_doc_cache.all_items.size()) {
                        const auto& [item, mod_path] = g_doc_cache.all_items[r.doc_id];
                        results.push_back({item, mod_path, r.score, r.score, 0.0f, 0.0f});
                    }
                }
            }
        } else {
            auto bm25_results = g_doc_cache.bm25.search(query, fetch_limit);
            if (g_doc_cache.hnsw && g_doc_cache.vectorizer) {
                auto query_vec = g_doc_cache.vectorizer->vectorize(query);
                auto hnsw_results = g_doc_cache.hnsw->search(query_vec, fetch_limit);
                results = reciprocal_rank_fusion(bm25_results, hnsw_results, fetch_limit);
            } else {
                for (const auto& r : bm25_results) {
                    if (r.doc_id < g_doc_cache.all_items.size()) {
                        const auto& [item, mod_path] = g_doc_cache.all_items[r.doc_id];
                        results.push_back({item, mod_path, r.score, r.score, 0.0f, 0.0f});
                    }
                }
            }
        }
    }

    // Apply kind and module filters
    apply_filters(results, kind_filter, module_filter);

    // Multi-signal ranking boosts (pub, documented, top-level)
    apply_signal_boosts(results);

    // Deduplicate near-identical results, then MMR diversify
    deduplicate_results(results);
    mmr_diversify(results);

    // Apply final limit
    if (results.size() > static_cast<size_t>(limit)) {
        results.resize(static_cast<size_t>(limit));
    }

    auto search_end = std::chrono::steady_clock::now();
    auto search_ms =
        std::chrono::duration_cast<std::chrono::microseconds>(search_end - search_start).count();

    // Format header
    output << "Documentation search for: \"" << query << "\"";
    output << " [mode: " << mode << "]";
    if (kind_filter) {
        output << " (kind: " << doc::doc_item_kind_to_string(*kind_filter) << ")";
    }
    if (!module_filter.empty()) {
        output << " (module: " << module_filter << ")";
    }
    if (used_expansion) {
        output << " (expanded to " << expanded_queries.size() << " queries)";
    }
    output << "\n";
    output << "Index: " << g_doc_cache.all_items.size() << " items, BM25 + HNSW";
    if (g_doc_cache.hnsw) {
        output << " (" << g_doc_cache.hnsw->dims() << "-dim vectors)";
    }
    if (g_doc_cache.build_time_ms > 0) {
        output << " [built in " << g_doc_cache.build_time_ms << "ms]";
    }
    output << " [query: " << std::fixed << std::setprecision(1) << (search_ms / 1000.0) << "ms]";
    output << "\n\n";

    if (results.empty()) {
        output << "No results found.\n\n";
        output << "Tips:\n";
        output << "- Search by name: \"split\", \"Maybe\", \"fnv1a64\"\n";
        output << "- Filter by kind: kind=\"function\", kind=\"struct\"\n";
        output << "- Filter by module: module=\"core::str\", module=\"std::json\"\n";
        output << "- Use mode=\"semantic\" for intent-based search\n";
        output << "- Use mode=\"text\" for exact keyword search\n";
    } else {
        for (const auto& result : results) {
            format_result(output, result);
        }
        output << "(" << results.size() << " result(s) found)\n";
    }

    return ToolResult::text(output.str());
}

// ============================================================================
// Doc Cache Accessors (used by mcp_tools_docs_handlers.cpp)
// ============================================================================

auto get_doc_all_items() -> const std::vector<std::pair<const doc::DocItem*, std::string>>& {
    return g_doc_cache.all_items;
}

auto is_doc_cache_initialized() -> bool {
    return g_doc_cache.initialized;
}

} // namespace tml::mcp