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

// Defined in mcp_tools_docs_index.cpp
extern DocSearchCache g_doc_cache;
auto files_changed(const DocSearchCache& cache) -> bool;
void build_doc_index(DocSearchCache& cache);

/// Ensures the doc index is built and up-to-date.
void ensure_doc_index() {
    std::lock_guard<std::mutex> lock(g_doc_cache.mutex);

    if (!g_doc_cache.initialized || files_changed(g_doc_cache)) {
        try {
            TML_LOG_INFO("mcp", "Building doc index...");
            build_doc_index(g_doc_cache);
            TML_LOG_INFO("mcp", "Doc index built: " << g_doc_cache.all_items.size() << " items");
        } catch (const std::exception& e) {
            TML_LOG_ERROR("mcp", "Doc index build failed: " << e.what());
            g_doc_cache.initialized = true;
        } catch (...) {
            TML_LOG_ERROR("mcp", "Doc index build failed: unknown error");
            g_doc_cache.initialized = true;
        }
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