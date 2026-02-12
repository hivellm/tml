//! # BM25 Text Index — Implementation
//!
//! Implements tokenization, indexing, and BM25 scoring.

#include "search/bm25_index.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstring>

namespace tml::search {

namespace {

/// Checks if a character is a word boundary for camelCase splitting.
auto is_camel_boundary(char prev, char curr) -> bool {
    // lowercase -> uppercase transition: "camelCase" -> ["camel", "Case"]
    if (std::islower(static_cast<unsigned char>(prev)) &&
        std::isupper(static_cast<unsigned char>(curr))) {
        return true;
    }
    return false;
}

/// Splits a single word on camelCase and snake_case boundaries.
auto split_compound_word(const std::string& word) -> std::vector<std::string> {
    std::vector<std::string> parts;
    if (word.empty())
        return parts;

    std::string current;
    for (size_t i = 0; i < word.size(); ++i) {
        char c = word[i];

        // snake_case: split on underscore
        if (c == '_') {
            if (!current.empty()) {
                parts.push_back(current);
                current.clear();
            }
            continue;
        }

        // camelCase: split before uppercase
        if (i > 0 && is_camel_boundary(word[i - 1], c)) {
            if (!current.empty()) {
                parts.push_back(current);
                current.clear();
            }
        }

        current += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }

    if (!current.empty()) {
        parts.push_back(current);
    }
    return parts;
}

/// Counts term frequencies in a token list.
auto count_tf(const std::vector<std::string>& tokens) -> std::unordered_map<std::string, uint32_t> {
    std::unordered_map<std::string, uint32_t> tf;
    for (const auto& token : tokens) {
        tf[token]++;
    }
    return tf;
}

} // namespace

auto BM25Index::stop_words() -> const std::unordered_set<std::string>& {
    static const std::unordered_set<std::string> words = {
        // English common words
        "the",
        "a",
        "an",
        "and",
        "or",
        "not",
        "is",
        "are",
        "was",
        "were",
        "be",
        "been",
        "being",
        "have",
        "has",
        "had",
        "do",
        "does",
        "did",
        "will",
        "would",
        "shall",
        "should",
        "may",
        "might",
        "can",
        "could",
        "this",
        "that",
        "these",
        "those",
        "it",
        "its",
        "of",
        "in",
        "on",
        "at",
        "to",
        "for",
        "with",
        "by",
        "from",
        "as",
        "into",
        "through",
        "if",
        "then",
        "else",
        "when",
        "but",
        "so",
        "no",
        "all",
        "each",
        "every",
        "both",
        "few",
        "more",
        "most",
        "other",
        "some",
        "such",

        // TML keywords that appear everywhere
        "func",
        "let",
        "var",
        "pub",
        "mut",
        "ref",
        "type",
        "use",
        "mod",
        "return",
        "self",
        "true",
        "false",
    };
    return words;
}

auto BM25Index::tokenize(const std::string& text) -> std::vector<std::string> {
    std::vector<std::string> tokens;
    const auto& stops = stop_words();

    // First split on whitespace and punctuation
    std::string current;
    for (char c : text) {
        if (std::isalnum(static_cast<unsigned char>(c)) || c == '_') {
            current += c;
        } else {
            if (!current.empty()) {
                // Split compound word (camelCase, snake_case)
                auto parts = split_compound_word(current);
                for (auto& part : parts) {
                    if (part.size() >= 2 && stops.find(part) == stops.end()) {
                        tokens.push_back(std::move(part));
                    }
                }
                current.clear();
            }
        }
    }
    // Handle last word
    if (!current.empty()) {
        auto parts = split_compound_word(current);
        for (auto& part : parts) {
            if (part.size() >= 2 && stops.find(part) == stops.end()) {
                tokens.push_back(std::move(part));
            }
        }
    }

    return tokens;
}

void BM25Index::add_document(uint32_t doc_id, const std::string& name, const std::string& signature,
                             const std::string& doc_text, const std::string& path) {
    auto name_tokens = tokenize(name);
    auto sig_tokens = tokenize(signature);
    auto doc_tokens = tokenize(doc_text);
    auto path_tokens = tokenize(path);

    BM25Document doc;
    doc.id = doc_id;
    doc.name_tf = count_tf(name_tokens);
    doc.signature_tf = count_tf(sig_tokens);
    doc.doc_tf = count_tf(doc_tokens);
    doc.path_tf = count_tf(path_tokens);
    doc.name_len = static_cast<uint32_t>(name_tokens.size());
    doc.signature_len = static_cast<uint32_t>(sig_tokens.size());
    doc.doc_len = static_cast<uint32_t>(doc_tokens.size());
    doc.path_len = static_cast<uint32_t>(path_tokens.size());

    // Track document frequency (unique terms across all fields)
    std::unordered_set<std::string> seen;
    for (const auto& t : name_tokens)
        seen.insert(t);
    for (const auto& t : sig_tokens)
        seen.insert(t);
    for (const auto& t : doc_tokens)
        seen.insert(t);
    for (const auto& t : path_tokens)
        seen.insert(t);

    for (const auto& term : seen) {
        doc_freq_[term]++;
    }

    documents_.push_back(std::move(doc));
    built_ = false;
}

void BM25Index::build() {
    size_t n = documents_.size();
    if (n == 0) {
        built_ = true;
        return;
    }

    // Compute average field lengths
    float total_name = 0, total_sig = 0, total_doc = 0, total_path = 0;
    for (const auto& doc : documents_) {
        total_name += static_cast<float>(doc.name_len);
        total_sig += static_cast<float>(doc.signature_len);
        total_doc += static_cast<float>(doc.doc_len);
        total_path += static_cast<float>(doc.path_len);
    }
    float fn = static_cast<float>(n);
    avg_name_len_ = total_name / fn;
    avg_signature_len_ = total_sig / fn;
    avg_doc_len_ = total_doc / fn;
    avg_path_len_ = total_path / fn;

    // Compute IDF for each term: log((N - df + 0.5) / (df + 0.5) + 1)
    idf_.clear();
    for (const auto& [term, df] : doc_freq_) {
        float fdf = static_cast<float>(df);
        idf_[term] = std::log((fn - fdf + 0.5f) / (fdf + 0.5f) + 1.0f);
    }

    built_ = true;
}

auto BM25Index::idf(const std::string& term) const -> float {
    auto it = idf_.find(term);
    return it != idf_.end() ? it->second : 0.0f;
}

auto BM25Index::score_field(const std::unordered_map<std::string, uint32_t>& tf, uint32_t field_len,
                            float avg_field_len, const std::vector<std::string>& query_terms) const
    -> float {
    float score = 0.0f;
    float fl = static_cast<float>(field_len);
    float avg_fl = avg_field_len > 0.0f ? avg_field_len : 1.0f;

    for (const auto& term : query_terms) {
        auto tf_it = tf.find(term);
        if (tf_it == tf.end())
            continue;

        float term_tf = static_cast<float>(tf_it->second);
        float term_idf = idf(term);

        // BM25 formula: IDF * (tf * (k1 + 1)) / (tf + k1 * (1 - b + b * dl / avgdl))
        float numerator = term_tf * (k1 + 1.0f);
        float denominator = term_tf + k1 * (1.0f - b + b * fl / avg_fl);
        score += term_idf * numerator / denominator;
    }

    return score;
}

auto BM25Index::search(const std::string& query, size_t limit) const -> std::vector<BM25Result> {
    if (!built_ || documents_.empty()) {
        return {};
    }

    auto query_terms = tokenize(query);
    if (query_terms.empty()) {
        return {};
    }

    // Score each document across all fields with boosting
    std::vector<BM25Result> results;
    results.reserve(documents_.size());

    for (const auto& doc : documents_) {
        float score = 0.0f;
        score += name_boost * score_field(doc.name_tf, doc.name_len, avg_name_len_, query_terms);
        score += signature_boost *
                 score_field(doc.signature_tf, doc.signature_len, avg_signature_len_, query_terms);
        score += doc_boost * score_field(doc.doc_tf, doc.doc_len, avg_doc_len_, query_terms);
        score += path_boost * score_field(doc.path_tf, doc.path_len, avg_path_len_, query_terms);

        if (score > 0.0f) {
            results.push_back({doc.id, score});
        }
    }

    // Sort by score descending
    std::sort(results.begin(), results.end(),
              [](const BM25Result& a, const BM25Result& b) { return a.score > b.score; });

    // Truncate to limit
    if (results.size() > limit) {
        results.resize(limit);
    }

    return results;
}

// ============================================================================
// Binary Serialization
// ============================================================================

namespace {

// Helper: write a primitive to a byte vector
template <typename T> void write_val(std::vector<uint8_t>& buf, T val) {
    const auto* p = reinterpret_cast<const uint8_t*>(&val);
    buf.insert(buf.end(), p, p + sizeof(T));
}

// Helper: write a string (length-prefixed)
void write_str(std::vector<uint8_t>& buf, const std::string& s) {
    write_val(buf, static_cast<uint32_t>(s.size()));
    buf.insert(buf.end(), s.begin(), s.end());
}

// Helper: write a term-frequency map
void write_tf_map(std::vector<uint8_t>& buf, const std::unordered_map<std::string, uint32_t>& tf) {
    write_val(buf, static_cast<uint32_t>(tf.size()));
    for (const auto& [term, count] : tf) {
        write_str(buf, term);
        write_val(buf, count);
    }
}

// Helper: read a primitive from a buffer
template <typename T> bool read_val(const uint8_t*& ptr, size_t& remaining, T& out) {
    if (remaining < sizeof(T))
        return false;
    std::memcpy(&out, ptr, sizeof(T));
    ptr += sizeof(T);
    remaining -= sizeof(T);
    return true;
}

// Helper: read a string
bool read_str(const uint8_t*& ptr, size_t& remaining, std::string& out) {
    uint32_t len;
    if (!read_val(ptr, remaining, len))
        return false;
    if (remaining < len)
        return false;
    out.assign(reinterpret_cast<const char*>(ptr), len);
    ptr += len;
    remaining -= len;
    return true;
}

// Helper: read a term-frequency map
bool read_tf_map(const uint8_t*& ptr, size_t& remaining,
                 std::unordered_map<std::string, uint32_t>& tf) {
    uint32_t count;
    if (!read_val(ptr, remaining, count))
        return false;
    tf.clear();
    tf.reserve(count);
    for (uint32_t i = 0; i < count; ++i) {
        std::string term;
        uint32_t val;
        if (!read_str(ptr, remaining, term))
            return false;
        if (!read_val(ptr, remaining, val))
            return false;
        tf[std::move(term)] = val;
    }
    return true;
}

} // namespace

auto BM25Index::serialize() const -> std::vector<uint8_t> {
    std::vector<uint8_t> buf;
    buf.reserve(1024 * 1024); // Pre-allocate 1MB

    // Magic + version
    write_val(buf, static_cast<uint32_t>(0x424D3235)); // "BM25"
    write_val(buf, static_cast<uint32_t>(1));          // version 1

    // Parameters
    write_val(buf, k1);
    write_val(buf, b);
    write_val(buf, name_boost);
    write_val(buf, signature_boost);
    write_val(buf, doc_boost);
    write_val(buf, path_boost);

    // Average lengths
    write_val(buf, avg_name_len_);
    write_val(buf, avg_signature_len_);
    write_val(buf, avg_doc_len_);
    write_val(buf, avg_path_len_);
    write_val(buf, built_);

    // Documents
    write_val(buf, static_cast<uint32_t>(documents_.size()));
    for (const auto& doc : documents_) {
        write_val(buf, doc.id);
        write_tf_map(buf, doc.name_tf);
        write_tf_map(buf, doc.signature_tf);
        write_tf_map(buf, doc.doc_tf);
        write_tf_map(buf, doc.path_tf);
        write_val(buf, doc.name_len);
        write_val(buf, doc.signature_len);
        write_val(buf, doc.doc_len);
        write_val(buf, doc.path_len);
    }

    // Doc frequency table
    write_tf_map(buf, doc_freq_);

    // IDF table
    write_val(buf, static_cast<uint32_t>(idf_.size()));
    for (const auto& [term, val] : idf_) {
        write_str(buf, term);
        write_val(buf, val);
    }

    return buf;
}

auto BM25Index::deserialize(const uint8_t* data, size_t len) -> bool {
    const uint8_t* ptr = data;
    size_t remaining = len;

    // Magic + version
    uint32_t magic, version;
    if (!read_val(ptr, remaining, magic) || magic != 0x424D3235)
        return false;
    if (!read_val(ptr, remaining, version) || version != 1)
        return false;

    // Parameters
    if (!read_val(ptr, remaining, k1))
        return false;
    if (!read_val(ptr, remaining, b))
        return false;
    if (!read_val(ptr, remaining, name_boost))
        return false;
    if (!read_val(ptr, remaining, signature_boost))
        return false;
    if (!read_val(ptr, remaining, doc_boost))
        return false;
    if (!read_val(ptr, remaining, path_boost))
        return false;

    // Average lengths
    if (!read_val(ptr, remaining, avg_name_len_))
        return false;
    if (!read_val(ptr, remaining, avg_signature_len_))
        return false;
    if (!read_val(ptr, remaining, avg_doc_len_))
        return false;
    if (!read_val(ptr, remaining, avg_path_len_))
        return false;
    if (!read_val(ptr, remaining, built_))
        return false;

    // Documents
    uint32_t doc_count;
    if (!read_val(ptr, remaining, doc_count))
        return false;
    documents_.clear();
    documents_.reserve(doc_count);
    for (uint32_t i = 0; i < doc_count; ++i) {
        BM25Document doc;
        if (!read_val(ptr, remaining, doc.id))
            return false;
        if (!read_tf_map(ptr, remaining, doc.name_tf))
            return false;
        if (!read_tf_map(ptr, remaining, doc.signature_tf))
            return false;
        if (!read_tf_map(ptr, remaining, doc.doc_tf))
            return false;
        if (!read_tf_map(ptr, remaining, doc.path_tf))
            return false;
        if (!read_val(ptr, remaining, doc.name_len))
            return false;
        if (!read_val(ptr, remaining, doc.signature_len))
            return false;
        if (!read_val(ptr, remaining, doc.doc_len))
            return false;
        if (!read_val(ptr, remaining, doc.path_len))
            return false;
        documents_.push_back(std::move(doc));
    }

    // Doc frequency table
    if (!read_tf_map(ptr, remaining, doc_freq_))
        return false;

    // IDF table
    uint32_t idf_count;
    if (!read_val(ptr, remaining, idf_count))
        return false;
    idf_.clear();
    idf_.reserve(idf_count);
    for (uint32_t i = 0; i < idf_count; ++i) {
        std::string term;
        float val;
        if (!read_str(ptr, remaining, term))
            return false;
        if (!read_val(ptr, remaining, val))
            return false;
        idf_[std::move(term)] = val;
    }

    return true;
}

} // namespace tml::search
