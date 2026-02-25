# TML Standard Library: Search

> `std::search` — Search algorithms for full-text and vector similarity search.

## Overview

Provides two search index implementations and a set of distance functions:

- **BM25** (`std::search::bm25`) — Full-text search using the Okapi BM25 ranking algorithm with automatic tokenization and TF-IDF scoring.
- **HNSW** (`std::search::hnsw`) — Approximate nearest neighbor search using the Hierarchical Navigable Small World graph algorithm for vector similarity.
- **Distance** (`std::search::distance`) — Vector distance and similarity functions (dot product, cosine, Euclidean).

## Import

```tml
use std::search::bm25::Bm25Index
use std::search::hnsw::{HnswIndex, TfIdfVectorizer}
use std::search::distance::{cosine_similarity, dot_product}
```

---

## Bm25Index

Full-text search index with BM25 ranking.

### Construction and Indexing

```tml
func Bm25Index::new() -> Bm25Index
func add_text(mut self, id: I64, text: Str)
func add_document(mut self, id: I64, title: Str, body: Str)
func build(mut self)
```

### Querying

```tml
func search(ref self, query: Str, k: I64) -> List[SearchResult]
```

`SearchResult` contains the document `id` and a relevance `score`.

---

## HnswIndex

Approximate nearest neighbor vector search.

### Construction

```tml
func HnswIndex::create(dims: I64) -> HnswIndex
```

### Configuration

```tml
func set_params(mut self, m: I64, ef_construction: I64, ef_search: I64)
```

- `m` — Max connections per node (default: 16)
- `ef_construction` — Build-time search width (default: 200)
- `ef_search` — Query-time search width (default: 50)

### Indexing and Search

```tml
func insert(mut self, id: I64, vec: List[F64])
func search(ref self, query: List[F64], k: I64) -> List[SearchResult]
```

---

## TfIdfVectorizer

Converts text documents into TF-IDF vectors for use with `HnswIndex`.

```tml
func TfIdfVectorizer::new() -> TfIdfVectorizer
func add_document(mut self, id: I64, text: Str)
func build(mut self)
func vectorize(ref self, text: Str) -> List[F64]
func dims(ref self) -> I64
```

---

## Distance Functions

Vector distance and similarity functions, available in both `F64` and `F32` variants.

```tml
// F64 variants
func dot_product(a: List[F64], b: List[F64]) -> F64
func cosine_similarity(a: List[F64], b: List[F64]) -> F64
func euclidean_distance(a: List[F64], b: List[F64]) -> F64
func normalize(vec: List[F64]) -> List[F64]

// F32 variants
func dot_product_f32(a: List[F32], b: List[F32]) -> F32
func cosine_similarity_f32(a: List[F32], b: List[F32]) -> F32
func euclidean_distance_f32(a: List[F32], b: List[F32]) -> F32
func normalize_f32(vec: List[F32]) -> List[F32]
```

---

## Example

```tml
use std::search::bm25::Bm25Index
use std::search::distance::cosine_similarity

func main() {
    // Full-text search
    var idx = Bm25Index::new()
    idx.add_document(1, "Rust", "Systems programming language")
    idx.add_document(2, "TML", "Language for LLM code generation")
    idx.build()

    let results = idx.search("programming language", 5)
    loop r in results {
        print("id={r.id} score={r.score}\n")
    }

    // Vector similarity
    let a = [1.0, 0.0, 1.0]
    let b = [0.0, 1.0, 1.0]
    let sim = cosine_similarity(a, b)
    print("cosine similarity: {sim}\n")
}
```
