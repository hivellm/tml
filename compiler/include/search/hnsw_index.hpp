//! # HNSW Vector Index
//!
//! Implements Hierarchical Navigable Small World (HNSW) graph for approximate
//! nearest neighbor search. Used for semantic search over documentation items
//! by embedding text into TF-IDF weighted bag-of-words vectors.
//!
//! ## Algorithm
//!
//! HNSW builds a multi-layer graph where:
//! - Layer 0 contains all nodes with many connections (dense)
//! - Higher layers contain fewer nodes with fewer connections (sparse)
//! - Search starts at the top layer and greedily descends
//! - At each layer, a beam search finds the closest neighbors
//!
//! ## Parameters
//!
//! | Parameter | Default | Description |
//! |-----------|---------|-------------|
//! | M | 16 | Max connections per node per layer |
//! | efConstruction | 200 | Beam width during insertion |
//! | efSearch | 50 | Beam width during query |
//! | mL | 1/ln(M) | Level generation factor |
//!
//! ## Embedding Strategy
//!
//! Documents are embedded as TF-IDF weighted bag-of-words vectors.
//! The vocabulary is built at index time from the corpus. Each dimension
//! corresponds to a term, weighted by its IDF. Vectors are L2-normalized
//! so that dot product equals cosine similarity.
//!
//! ## Usage
//!
//! ```cpp
//! HnswIndex index(512);  // 512 dimensions
//! index.set_params(16, 200, 50);
//! index.insert(0, embedding_vector);
//! auto results = index.search(query_vector, 10);
//! ```

#pragma once

#include <cstddef>
#include <cstdint>
#include <mutex>
#include <random>
#include <string>
#include <unordered_map>
#include <vector>

namespace tml::search {

/// A single nearest-neighbor result from the HNSW index.
struct HnswResult {
    /// Document ID.
    uint32_t doc_id;
    /// Distance to query (lower = more similar for L2, higher for cosine).
    float distance;
};

/// Node in the HNSW graph.
struct HnswNode {
    /// Document ID this node represents.
    uint32_t doc_id;
    /// Maximum layer this node appears in.
    int32_t max_layer;
    /// Connections at each layer. neighbors[layer] = vector of node indices.
    std::vector<std::vector<uint32_t>> neighbors;
    /// The embedding vector for this node.
    std::vector<float> embedding;
};

/// TF-IDF Vectorizer for converting text to embeddings.
///
/// Builds a vocabulary from the corpus and converts documents to
/// TF-IDF weighted bag-of-words vectors of fixed dimensionality.
class TfIdfVectorizer {
public:
    /// Creates a vectorizer with the specified maximum dimension count.
    ///
    /// @param max_dims Maximum number of dimensions (top-N IDF terms)
    explicit TfIdfVectorizer(size_t max_dims = 512);

    /// Adds a document to the corpus (call before build()).
    ///
    /// @param doc_id Document identifier
    /// @param text Combined text from all fields
    void add_document(uint32_t doc_id, const std::string& text);

    /// Builds the vocabulary from all added documents.
    ///
    /// Selects the top-N terms by IDF as dimensions.
    void build();

    /// Converts text to a TF-IDF vector.
    ///
    /// @param text Input text
    /// @return L2-normalized TF-IDF vector of size `dims()`
    auto vectorize(const std::string& text) const -> std::vector<float>;

    /// Returns the number of dimensions.
    auto dims() const -> size_t {
        return dim_;
    }

    /// Returns true if the vectorizer has been built.
    auto is_built() const -> bool {
        return built_;
    }

    /// Serializes the vectorizer to binary data.
    auto serialize() const -> std::vector<uint8_t>;

    /// Deserializes the vectorizer from binary data.
    auto deserialize(const uint8_t* data, size_t len) -> bool;

private:
    size_t max_dims_;
    size_t dim_ = 0;
    bool built_ = false;

    /// Per-document term lists (before build).
    std::vector<std::vector<std::string>> doc_terms_;
    size_t total_docs_ = 0;

    /// Document frequency per term.
    std::unordered_map<std::string, uint32_t> doc_freq_;

    /// After build: term -> dimension index.
    std::unordered_map<std::string, uint32_t> term_to_dim_;

    /// After build: IDF per dimension.
    std::vector<float> idf_weights_;
};

/// HNSW (Hierarchical Navigable Small World) approximate nearest neighbor index.
///
/// Provides sub-linear search time for high-dimensional vector similarity.
/// Thread-safe for concurrent search (not concurrent insert).
class HnswIndex {
public:
    /// Creates an HNSW index for vectors of the given dimensionality.
    ///
    /// @param dims Number of dimensions per vector
    explicit HnswIndex(size_t dims);

    /// Sets HNSW construction and search parameters.
    ///
    /// @param M Maximum connections per node per layer
    /// @param ef_construction Beam width during insertion
    /// @param ef_search Beam width during query
    void set_params(int M = 16, int ef_construction = 200, int ef_search = 50);

    /// Inserts a vector into the index.
    ///
    /// @param doc_id Document identifier
    /// @param embedding Vector of size `dims()` (will be copied)
    void insert(uint32_t doc_id, const std::vector<float>& embedding);

    /// Searches for the k nearest neighbors to a query vector.
    ///
    /// Uses cosine similarity (higher = more similar).
    ///
    /// @param query Query vector of size `dims()`
    /// @param k Number of results to return
    /// @return Vector of results sorted by distance ascending
    auto search(const std::vector<float>& query, size_t k) const -> std::vector<HnswResult>;

    /// Returns the number of indexed vectors.
    auto size() const -> size_t {
        return nodes_.size();
    }

    /// Returns the vector dimensionality.
    auto dims() const -> size_t {
        return dims_;
    }

    /// Returns the current maximum layer in the graph.
    auto max_layer() const -> int32_t {
        return max_layer_;
    }

    /// Serializes the HNSW index to binary data.
    auto serialize() const -> std::vector<uint8_t>;

    /// Deserializes the HNSW index from binary data.
    auto deserialize(const uint8_t* data, size_t len) -> bool;

private:
    size_t dims_;
    int M_ = 16;
    int M_max0_ = 32; // Max connections at layer 0 (2*M)
    int ef_construction_ = 200;
    int ef_search_ = 50;
    float mL_ = 0.0f; // 1/ln(M)

    /// All nodes in the graph.
    std::vector<HnswNode> nodes_;

    /// Entry point node index.
    uint32_t entry_point_ = 0;

    /// Maximum layer across all nodes.
    int32_t max_layer_ = -1;

    /// Random number generator for layer assignment.
    mutable std::mt19937 rng_{42};

    /// Generates a random layer for a new node.
    auto random_layer() -> int32_t;

    /// Computes distance between two nodes (1 - cosine_similarity for cosine metric).
    auto distance(const std::vector<float>& a, const std::vector<float>& b) const -> float;

    /// Greedy search at a single layer to find the closest node.
    auto search_layer_greedy(const std::vector<float>& query, uint32_t entry, int layer) const
        -> uint32_t;

    /// Beam search at a single layer.
    /// Returns a max-heap of (distance, node_index) pairs.
    using DistNodePair = std::pair<float, uint32_t>;

    auto search_layer(const std::vector<float>& query, uint32_t entry, int ef, int layer) const
        -> std::vector<DistNodePair>;

    /// Selects M best neighbors from candidates using the simple heuristic.
    auto select_neighbors(const std::vector<DistNodePair>& candidates, int M) const
        -> std::vector<uint32_t>;
};

} // namespace tml::search
