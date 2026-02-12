//! # HNSW Vector Index — Implementation
//!
//! Implements the HNSW algorithm from:
//! "Efficient and Robust Approximate Nearest Neighbor using Hierarchical
//!  Navigable Small World Graphs" (Malkov & Yashunin, 2018)

#include "search/hnsw_index.hpp"

#include "search/bm25_index.hpp"
#include "search/simd_distance.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <queue>
#include <unordered_set>

namespace tml::search {

// ============================================================================
// TfIdfVectorizer
// ============================================================================

TfIdfVectorizer::TfIdfVectorizer(size_t max_dims) : max_dims_(max_dims) {}

void TfIdfVectorizer::add_document(uint32_t /*doc_id*/, const std::string& text) {
    auto tokens = BM25Index::tokenize(text);

    // Track document frequency (unique terms per document)
    std::unordered_set<std::string> unique_terms(tokens.begin(), tokens.end());
    for (const auto& term : unique_terms) {
        doc_freq_[term]++;
    }

    doc_terms_.push_back(std::move(tokens));
    total_docs_++;
}

void TfIdfVectorizer::build() {
    if (total_docs_ == 0) {
        dim_ = 0;
        built_ = true;
        return;
    }

    // Compute IDF for all terms
    struct TermIdf {
        std::string term;
        float idf;
    };
    std::vector<TermIdf> all_terms;
    all_terms.reserve(doc_freq_.size());

    float n = static_cast<float>(total_docs_);
    for (const auto& [term, df] : doc_freq_) {
        float fdf = static_cast<float>(df);
        float idf_val = std::log((n + 1.0f) / (fdf + 1.0f)) + 1.0f;
        all_terms.push_back({term, idf_val});
    }

    // Sort by IDF descending (rarest terms first — most discriminative)
    std::sort(all_terms.begin(), all_terms.end(),
              [](const TermIdf& a, const TermIdf& b) { return a.idf > b.idf; });

    // Take top-N terms as dimensions
    dim_ = std::min(max_dims_, all_terms.size());
    term_to_dim_.clear();
    idf_weights_.resize(dim_);

    for (size_t i = 0; i < dim_; ++i) {
        term_to_dim_[all_terms[i].term] = static_cast<uint32_t>(i);
        idf_weights_[i] = all_terms[i].idf;
    }

    // Clear document terms (no longer needed after build)
    doc_terms_.clear();
    built_ = true;
}

auto TfIdfVectorizer::vectorize(const std::string& text) const -> std::vector<float> {
    std::vector<float> vec(dim_, 0.0f);
    if (!built_ || dim_ == 0)
        return vec;

    auto tokens = BM25Index::tokenize(text);

    // Count term frequencies
    std::unordered_map<std::string, uint32_t> tf;
    for (const auto& t : tokens) {
        tf[t]++;
    }

    // TF-IDF weighting
    for (const auto& [term, count] : tf) {
        auto it = term_to_dim_.find(term);
        if (it != term_to_dim_.end()) {
            uint32_t dim_idx = it->second;
            float tf_weight = 1.0f + std::log(static_cast<float>(count));
            vec[dim_idx] = tf_weight * idf_weights_[dim_idx];
        }
    }

    // L2 normalize
    normalize_f32(vec.data(), dim_);
    return vec;
}

// ============================================================================
// HnswIndex
// ============================================================================

HnswIndex::HnswIndex(size_t dims) : dims_(dims) {
    mL_ = 1.0f / std::log(static_cast<float>(M_));
}

void HnswIndex::set_params(int M, int ef_construction, int ef_search) {
    M_ = M;
    M_max0_ = 2 * M;
    ef_construction_ = ef_construction;
    ef_search_ = ef_search;
    mL_ = 1.0f / std::log(static_cast<float>(M));
}

auto HnswIndex::random_layer() -> int32_t {
    std::uniform_real_distribution<float> dist(0.0f, 1.0f);
    float r = dist(rng_);
    return static_cast<int32_t>(std::floor(-std::log(r) * mL_));
}

auto HnswIndex::distance(const std::vector<float>& a, const std::vector<float>& b) const -> float {
    // Cosine distance = 1 - cosine_similarity
    // Since vectors are L2-normalized, dot product = cosine similarity
    float sim = dot_product_f32(a.data(), b.data(), dims_);
    return 1.0f - sim;
}

auto HnswIndex::search_layer_greedy(const std::vector<float>& query, uint32_t entry,
                                    int layer) const -> uint32_t {
    float best_dist = distance(query, nodes_[entry].embedding);
    uint32_t best_node = entry;
    bool changed = true;

    while (changed) {
        changed = false;
        const auto& neighbors = nodes_[best_node].neighbors[layer];
        for (uint32_t neighbor : neighbors) {
            float d = distance(query, nodes_[neighbor].embedding);
            if (d < best_dist) {
                best_dist = d;
                best_node = neighbor;
                changed = true;
            }
        }
    }

    return best_node;
}

auto HnswIndex::search_layer(const std::vector<float>& query, uint32_t entry, int ef,
                             int layer) const -> std::vector<DistNodePair> {
    float entry_dist = distance(query, nodes_[entry].embedding);

    // candidates: min-heap (closest first for expansion)
    // result: max-heap (farthest first for pruning)
    std::priority_queue<DistNodePair, std::vector<DistNodePair>, std::greater<>> candidates;
    std::priority_queue<DistNodePair> result;
    std::unordered_set<uint32_t> visited;

    candidates.push({entry_dist, entry});
    result.push({entry_dist, entry});
    visited.insert(entry);

    while (!candidates.empty()) {
        auto [c_dist, c_node] = candidates.top();
        float farthest = result.top().first;

        // If closest candidate is farther than farthest result, stop
        if (c_dist > farthest) {
            break;
        }
        candidates.pop();

        // Expand neighbors
        const auto& neighbors = nodes_[c_node].neighbors[layer];
        for (uint32_t neighbor : neighbors) {
            if (visited.count(neighbor))
                continue;
            visited.insert(neighbor);

            float d = distance(query, nodes_[neighbor].embedding);
            farthest = result.top().first;

            if (d < farthest || static_cast<int>(result.size()) < ef) {
                candidates.push({d, neighbor});
                result.push({d, neighbor});
                if (static_cast<int>(result.size()) > ef) {
                    result.pop();
                }
            }
        }
    }

    // Extract results from the max-heap
    std::vector<DistNodePair> results;
    results.reserve(result.size());
    while (!result.empty()) {
        results.push_back(result.top());
        result.pop();
    }
    return results;
}

auto HnswIndex::select_neighbors(const std::vector<DistNodePair>& candidates, int M) const
    -> std::vector<uint32_t> {
    // Simple selection: take M closest
    auto sorted = candidates;
    std::sort(sorted.begin(), sorted.end());

    std::vector<uint32_t> selected;
    int count = std::min(M, static_cast<int>(sorted.size()));
    selected.reserve(count);
    for (int i = 0; i < count; ++i) {
        selected.push_back(sorted[i].second);
    }
    return selected;
}

void HnswIndex::insert(uint32_t doc_id, const std::vector<float>& embedding) {
    uint32_t node_idx = static_cast<uint32_t>(nodes_.size());
    int32_t node_layer = random_layer();

    HnswNode node;
    node.doc_id = doc_id;
    node.max_layer = node_layer;
    node.embedding = embedding;
    node.neighbors.resize(node_layer + 1);
    nodes_.push_back(std::move(node));

    // First node: just set as entry point
    if (nodes_.size() == 1) {
        entry_point_ = 0;
        max_layer_ = node_layer;
        return;
    }

    uint32_t ep = entry_point_;

    // Phase 1: Greedily descend from top layer to node_layer + 1
    for (int32_t layer = max_layer_; layer > node_layer; --layer) {
        ep = search_layer_greedy(nodes_[node_idx].embedding, ep, layer);
    }

    // Phase 2: Insert at layers [node_layer, 0]
    for (int32_t layer = std::min(node_layer, max_layer_); layer >= 0; --layer) {
        auto candidates = search_layer(nodes_[node_idx].embedding, ep, ef_construction_, layer);

        int max_conn = (layer == 0) ? M_max0_ : M_;
        auto selected = select_neighbors(candidates, max_conn);

        // Connect new node to selected neighbors
        nodes_[node_idx].neighbors[layer] = selected;

        // Connect selected neighbors back to new node (bidirectional)
        for (uint32_t neighbor_idx : selected) {
            auto& neighbor_connections = nodes_[neighbor_idx].neighbors[layer];
            neighbor_connections.push_back(node_idx);

            // Prune if exceeded max connections
            if (static_cast<int>(neighbor_connections.size()) > max_conn) {
                // Re-evaluate connections for this neighbor
                std::vector<DistNodePair> conn_dists;
                conn_dists.reserve(neighbor_connections.size());
                for (uint32_t c : neighbor_connections) {
                    float d = distance(nodes_[neighbor_idx].embedding, nodes_[c].embedding);
                    conn_dists.push_back({d, c});
                }
                neighbor_connections = select_neighbors(conn_dists, max_conn);
            }
        }

        // Use closest candidate as entry point for next layer
        if (!candidates.empty()) {
            auto closest = std::min_element(candidates.begin(), candidates.end());
            ep = closest->second;
        }
    }

    // Update entry point if this node has a higher layer
    if (node_layer > max_layer_) {
        entry_point_ = node_idx;
        max_layer_ = node_layer;
    }
}

auto HnswIndex::search(const std::vector<float>& query, size_t k) const -> std::vector<HnswResult> {
    if (nodes_.empty())
        return {};

    uint32_t ep = entry_point_;

    // Phase 1: Greedy descent from top to layer 1
    for (int32_t layer = max_layer_; layer > 0; --layer) {
        ep = search_layer_greedy(query, ep, layer);
    }

    // Phase 2: Beam search at layer 0
    int ef = std::max(ef_search_, static_cast<int>(k));
    auto candidates = search_layer(query, ep, ef, 0);

    // Sort by distance ascending and take top-k
    std::sort(candidates.begin(), candidates.end());

    std::vector<HnswResult> results;
    size_t count = std::min(k, candidates.size());
    results.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        results.push_back({nodes_[candidates[i].second].doc_id, candidates[i].first});
    }

    return results;
}

// ============================================================================
// Binary Serialization Helpers (shared with BM25)
// ============================================================================

namespace {

template <typename T> void write_val(std::vector<uint8_t>& buf, T val) {
    const auto* p = reinterpret_cast<const uint8_t*>(&val);
    buf.insert(buf.end(), p, p + sizeof(T));
}

void write_str(std::vector<uint8_t>& buf, const std::string& s) {
    write_val(buf, static_cast<uint32_t>(s.size()));
    buf.insert(buf.end(), s.begin(), s.end());
}

template <typename T> bool read_val(const uint8_t*& ptr, size_t& remaining, T& out) {
    if (remaining < sizeof(T))
        return false;
    std::memcpy(&out, ptr, sizeof(T));
    ptr += sizeof(T);
    remaining -= sizeof(T);
    return true;
}

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

} // namespace

// ============================================================================
// TfIdfVectorizer Serialization
// ============================================================================

auto TfIdfVectorizer::serialize() const -> std::vector<uint8_t> {
    std::vector<uint8_t> buf;
    buf.reserve(256 * 1024);

    write_val(buf, static_cast<uint32_t>(0x54464944)); // "TFID"
    write_val(buf, static_cast<uint32_t>(1));          // version

    write_val(buf, static_cast<uint32_t>(max_dims_));
    write_val(buf, static_cast<uint32_t>(dim_));
    write_val(buf, built_);
    write_val(buf, static_cast<uint32_t>(total_docs_));

    // term_to_dim map
    write_val(buf, static_cast<uint32_t>(term_to_dim_.size()));
    for (const auto& [term, idx] : term_to_dim_) {
        write_str(buf, term);
        write_val(buf, idx);
    }

    // idf_weights
    write_val(buf, static_cast<uint32_t>(idf_weights_.size()));
    for (float w : idf_weights_) {
        write_val(buf, w);
    }

    // doc_freq map
    write_val(buf, static_cast<uint32_t>(doc_freq_.size()));
    for (const auto& [term, count] : doc_freq_) {
        write_str(buf, term);
        write_val(buf, count);
    }

    return buf;
}

auto TfIdfVectorizer::deserialize(const uint8_t* data, size_t len) -> bool {
    const uint8_t* ptr = data;
    size_t remaining = len;

    uint32_t magic, version;
    if (!read_val(ptr, remaining, magic) || magic != 0x54464944)
        return false;
    if (!read_val(ptr, remaining, version) || version != 1)
        return false;

    uint32_t max_dims, dim, total_docs;
    if (!read_val(ptr, remaining, max_dims))
        return false;
    if (!read_val(ptr, remaining, dim))
        return false;
    if (!read_val(ptr, remaining, built_))
        return false;
    if (!read_val(ptr, remaining, total_docs))
        return false;

    max_dims_ = max_dims;
    dim_ = dim;
    total_docs_ = total_docs;

    // term_to_dim map
    uint32_t ttd_count;
    if (!read_val(ptr, remaining, ttd_count))
        return false;
    term_to_dim_.clear();
    term_to_dim_.reserve(ttd_count);
    for (uint32_t i = 0; i < ttd_count; ++i) {
        std::string term;
        uint32_t idx;
        if (!read_str(ptr, remaining, term))
            return false;
        if (!read_val(ptr, remaining, idx))
            return false;
        term_to_dim_[std::move(term)] = idx;
    }

    // idf_weights
    uint32_t idf_count;
    if (!read_val(ptr, remaining, idf_count))
        return false;
    idf_weights_.resize(idf_count);
    for (uint32_t i = 0; i < idf_count; ++i) {
        if (!read_val(ptr, remaining, idf_weights_[i]))
            return false;
    }

    // doc_freq map
    uint32_t df_count;
    if (!read_val(ptr, remaining, df_count))
        return false;
    doc_freq_.clear();
    doc_freq_.reserve(df_count);
    for (uint32_t i = 0; i < df_count; ++i) {
        std::string term;
        uint32_t count;
        if (!read_str(ptr, remaining, term))
            return false;
        if (!read_val(ptr, remaining, count))
            return false;
        doc_freq_[std::move(term)] = count;
    }

    doc_terms_.clear(); // Not needed after deserialization
    return true;
}

// ============================================================================
// HnswIndex Serialization
// ============================================================================

auto HnswIndex::serialize() const -> std::vector<uint8_t> {
    std::vector<uint8_t> buf;
    // Estimate size: each node has embedding + neighbors
    buf.reserve(nodes_.size() * (dims_ * 4 + 256) + 1024);

    write_val(buf, static_cast<uint32_t>(0x484E5357)); // "HNSW"
    write_val(buf, static_cast<uint32_t>(1));          // version

    // Parameters
    write_val(buf, static_cast<uint32_t>(dims_));
    write_val(buf, static_cast<int32_t>(M_));
    write_val(buf, static_cast<int32_t>(M_max0_));
    write_val(buf, static_cast<int32_t>(ef_construction_));
    write_val(buf, static_cast<int32_t>(ef_search_));
    write_val(buf, mL_);
    write_val(buf, entry_point_);
    write_val(buf, max_layer_);

    // Nodes
    write_val(buf, static_cast<uint32_t>(nodes_.size()));
    for (const auto& node : nodes_) {
        write_val(buf, node.doc_id);
        write_val(buf, node.max_layer);

        // Neighbors per layer
        write_val(buf, static_cast<uint32_t>(node.neighbors.size()));
        for (const auto& layer_neighbors : node.neighbors) {
            write_val(buf, static_cast<uint32_t>(layer_neighbors.size()));
            for (uint32_t n : layer_neighbors) {
                write_val(buf, n);
            }
        }

        // Embedding vector
        write_val(buf, static_cast<uint32_t>(node.embedding.size()));
        for (float v : node.embedding) {
            write_val(buf, v);
        }
    }

    return buf;
}

auto HnswIndex::deserialize(const uint8_t* data, size_t len) -> bool {
    const uint8_t* ptr = data;
    size_t remaining = len;

    uint32_t magic, version;
    if (!read_val(ptr, remaining, magic) || magic != 0x484E5357)
        return false;
    if (!read_val(ptr, remaining, version) || version != 1)
        return false;

    // Parameters
    uint32_t dims;
    if (!read_val(ptr, remaining, dims))
        return false;
    dims_ = dims;

    int32_t m, m_max0, ef_c, ef_s;
    if (!read_val(ptr, remaining, m))
        return false;
    if (!read_val(ptr, remaining, m_max0))
        return false;
    if (!read_val(ptr, remaining, ef_c))
        return false;
    if (!read_val(ptr, remaining, ef_s))
        return false;
    M_ = m;
    M_max0_ = m_max0;
    ef_construction_ = ef_c;
    ef_search_ = ef_s;

    if (!read_val(ptr, remaining, mL_))
        return false;
    if (!read_val(ptr, remaining, entry_point_))
        return false;
    if (!read_val(ptr, remaining, max_layer_))
        return false;

    // Nodes
    uint32_t node_count;
    if (!read_val(ptr, remaining, node_count))
        return false;
    nodes_.clear();
    nodes_.reserve(node_count);

    for (uint32_t i = 0; i < node_count; ++i) {
        HnswNode node;
        if (!read_val(ptr, remaining, node.doc_id))
            return false;
        if (!read_val(ptr, remaining, node.max_layer))
            return false;

        // Neighbors per layer
        uint32_t num_layers;
        if (!read_val(ptr, remaining, num_layers))
            return false;
        node.neighbors.resize(num_layers);
        for (uint32_t l = 0; l < num_layers; ++l) {
            uint32_t num_neighbors;
            if (!read_val(ptr, remaining, num_neighbors))
                return false;
            node.neighbors[l].resize(num_neighbors);
            for (uint32_t j = 0; j < num_neighbors; ++j) {
                if (!read_val(ptr, remaining, node.neighbors[l][j]))
                    return false;
            }
        }

        // Embedding
        uint32_t emb_size;
        if (!read_val(ptr, remaining, emb_size))
            return false;
        node.embedding.resize(emb_size);
        // Read embedding as a block for efficiency
        size_t emb_bytes = emb_size * sizeof(float);
        if (remaining < emb_bytes)
            return false;
        std::memcpy(node.embedding.data(), ptr, emb_bytes);
        ptr += emb_bytes;
        remaining -= emb_bytes;

        nodes_.push_back(std::move(node));
    }

    return true;
}

} // namespace tml::search
