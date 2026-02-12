//! # BM25 Text Index
//!
//! Implements the Okapi BM25 ranking function for full-text search over
//! structured documentation items. Provides TF-IDF scoring with field
//! boosting, tokenization with camelCase/snake_case splitting, and
//! stop word filtering.
//!
//! ## Overview
//!
//! BM25 (Best Matching 25) is the industry-standard probabilistic ranking
//! function for information retrieval. It considers:
//! - **Term Frequency (TF)**: How often a term appears in a document
//! - **Inverse Document Frequency (IDF)**: How rare a term is across all documents
//! - **Document length normalization**: Shorter docs score higher for same TF
//!
//! ## Usage
//!
//! ```cpp
//! BM25Index index;
//! index.add_document(0, "split", "pub func split(s: Str, delim: Str) -> List[Str]",
//!                    "Splits a string by delimiter", "core::str");
//! index.build();
//! auto results = index.search("split string", 10);
//! ```

#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace tml::search {

/// A single search result from the BM25 index.
struct BM25Result {
    /// Document ID (maps back to DocItem).
    uint32_t doc_id;
    /// BM25 relevance score (higher = more relevant).
    float score;
};

/// Represents a single indexed document with per-field term frequencies.
struct BM25Document {
    uint32_t id;
    /// Term frequencies for each field: name, signature, doc, path.
    std::unordered_map<std::string, uint32_t> name_tf;
    std::unordered_map<std::string, uint32_t> signature_tf;
    std::unordered_map<std::string, uint32_t> doc_tf;
    std::unordered_map<std::string, uint32_t> path_tf;
    /// Total token count per field (for length normalization).
    uint32_t name_len = 0;
    uint32_t signature_len = 0;
    uint32_t doc_len = 0;
    uint32_t path_len = 0;
};

/// BM25 full-text search index with field boosting.
///
/// Supports multi-field indexing with configurable boost weights:
/// - Name field: highest boost (exact name matches are most relevant)
/// - Signature field: medium boost
/// - Documentation text: lower boost
/// - Module path: lowest boost
class BM25Index {
public:
    /// BM25 parameters.
    /// k1 controls term frequency saturation (1.2 is standard).
    /// b controls document length normalization (0.75 is standard).
    float k1 = 1.2f;
    float b = 0.75f;

    /// Field boost weights.
    float name_boost = 3.0f;
    float signature_boost = 1.5f;
    float doc_boost = 1.0f;
    float path_boost = 0.5f;

    /// Adds a document to the index.
    ///
    /// Must call `build()` after adding all documents before searching.
    ///
    /// @param doc_id Unique identifier for this document
    /// @param name Item name (e.g., "split")
    /// @param signature Full signature (e.g., "pub func split(s: Str) -> List[Str]")
    /// @param doc_text Documentation text
    /// @param path Module path (e.g., "core::str")
    void add_document(uint32_t doc_id, const std::string& name, const std::string& signature,
                      const std::string& doc_text, const std::string& path);

    /// Builds the IDF table and average document lengths.
    ///
    /// Must be called after all documents are added and before searching.
    void build();

    /// Searches the index and returns ranked results.
    ///
    /// @param query Search query string
    /// @param limit Maximum number of results to return
    /// @return Vector of results sorted by score descending
    auto search(const std::string& query, size_t limit = 10) const -> std::vector<BM25Result>;

    /// Returns the total number of indexed documents.
    auto size() const -> size_t {
        return documents_.size();
    }

    /// Returns the vocabulary (all indexed terms).
    auto vocabulary() const -> const std::unordered_map<std::string, uint32_t>& {
        return doc_freq_;
    }

    /// Returns the IDF value for a term.
    ///
    /// @param term The term to look up
    /// @return IDF score, or 0 if term not in vocabulary
    auto idf(const std::string& term) const -> float;

    /// Tokenizes text into searchable terms.
    ///
    /// Splits on whitespace, punctuation, camelCase boundaries, and
    /// snake_case underscores. Lowercases all tokens. Filters stop words.
    ///
    /// @param text Input text
    /// @return Vector of normalized tokens
    static auto tokenize(const std::string& text) -> std::vector<std::string>;

    /// Returns the set of stop words.
    static auto stop_words() -> const std::unordered_set<std::string>&;

    /// Returns whether the index has been built.
    auto is_built() const -> bool {
        return built_;
    }

    /// Serializes the BM25 index to a binary byte vector.
    /// Only valid after build() has been called.
    auto serialize() const -> std::vector<uint8_t>;

    /// Deserializes a BM25 index from binary data.
    /// Returns true on success, false on invalid/corrupt data.
    auto deserialize(const uint8_t* data, size_t len) -> bool;

private:
    /// All indexed documents.
    std::vector<BM25Document> documents_;

    /// Document frequency for each term (how many docs contain it).
    std::unordered_map<std::string, uint32_t> doc_freq_;

    /// Pre-computed IDF values.
    std::unordered_map<std::string, float> idf_;

    /// Average field lengths (for BM25 normalization).
    float avg_name_len_ = 0.0f;
    float avg_signature_len_ = 0.0f;
    float avg_doc_len_ = 0.0f;
    float avg_path_len_ = 0.0f;

    /// Whether build() has been called.
    bool built_ = false;

    /// Computes BM25 score for a single field.
    auto score_field(const std::unordered_map<std::string, uint32_t>& tf, uint32_t field_len,
                     float avg_field_len, const std::vector<std::string>& query_terms) const
        -> float;
};

} // namespace tml::search
