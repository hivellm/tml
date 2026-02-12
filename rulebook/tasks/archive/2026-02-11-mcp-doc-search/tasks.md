# Tasks: MCP Documentation Search with BM25 + HNSW Vector Index

**Status**: Complete (100%)

## Phase 1: Documentation Loading on MCP Startup

- [x] 1.1.1 Direct AST extraction via doc::Extractor (no docs.json needed)
- [x] 1.1.2 Lazy index building on first query (not startup)
- [x] 1.1.3 Cache DocIndex in static global with mutex
- [x] 1.1.4 File mtime tracking for automatic invalidation
- [x] 1.1.5 Auto-discover TML root by walking up from cwd

## Phase 2: BM25 Text Index

- [x] 2.1.1 Create `compiler/include/search/bm25_index.hpp` with BM25Index class
- [x] 2.1.2 Implement tokenizer (whitespace, camelCase, snake_case splitting)
- [x] 2.1.3 Implement stop word filtering (English + TML keywords)
- [x] 2.1.4 Implement per-field term frequency (name, signature, doc, path)
- [x] 2.1.5 Implement inverse document frequency (IDF) with Okapi variant
- [x] 2.1.6 Implement BM25 scoring (k1=1.2, b=0.75)
- [x] 2.1.7 Index DocItem fields with per-field length normalization
- [x] 2.1.8 Implement field boosting (name=3x, signature=1.5x, doc=1x, path=0.5x)
- [x] 2.1.9 Create `compiler/src/search/bm25_index.cpp` implementation
- [x] 2.1.10 TML std library tests for BM25 (16 tests passing)

## Phase 3: SIMD-Optimized Distance Functions

- [x] 3.1.1 Create `compiler/runtime/search/search_engine.cpp` with C FFI
- [x] 3.1.2 Implement dot_product (F32 + F64)
- [x] 3.1.3 Implement cosine_similarity (F32 + F64)
- [x] 3.1.4 Implement euclidean_distance (F32 + F64)
- [x] 3.1.5 Implement l2_squared_f32 for fast comparison
- [x] 3.1.6 Implement norm and normalize (F32 + F64)
- [x] 3.1.7 Auto-vectorized loops for SIMD performance
- [x] 3.1.8 TML std library wrappers in `lib/std/src/search/distance.tml`
- [x] 3.1.9 TML std library tests for distance functions (26 tests passing)

## Phase 4: HNSW Vector Index

- [x] 4.1.1 Create `compiler/include/search/hnsw_index.hpp` with HnswIndex + TfIdfVectorizer
- [x] 4.1.2 Implement TF-IDF vectorization with top-512 IDF terms as dimensions
- [x] 4.1.3 Implement L2 normalization for cosine distance via dot product
- [x] 4.1.4 Implement HNSW graph construction (M=16, efConstruction=200)
- [x] 4.1.5 Implement HNSW layer selection (exponential distribution)
- [x] 4.1.6 Implement HNSW greedy descent + beam search
- [x] 4.1.7 Implement HNSW insert with bidirectional connections + pruning
- [x] 4.1.8 Create `compiler/src/search/hnsw_index.cpp` implementation
- [x] 4.1.9 TML std library tests for HNSW + TF-IDF (17 tests passing)

## Phase 5: MCP Search Integration

- [x] 5.1.1 Replace `handle_docs_search` with BM25 + HNSW hybrid search
- [x] 5.1.2 Add `mode` parameter: text, semantic, hybrid (default)
- [x] 5.1.3 Implement Reciprocal Rank Fusion with noise filtering
- [x] 5.1.4 Add `kind` parameter to filter by item type
- [x] 5.1.5 Add `module` parameter to filter by module path
- [x] 5.1.6 Structured result formatting (signature, module, source location, docs)
- [x] 5.1.7 Over-fetch 3x limit to compensate for post-filtering
- [x] 5.1.8 RRF tuning: BM25 2x weight, HNSW distance cutoffs (0.5 standalone, 0.8 boost)
- [x] 5.1.9 Link tml_doc to tml_mcp in CMakeLists.txt
- [x] 5.1.10 Add `docs/get` tool for full DocItem by qualified name
- [x] 5.1.11 Add `docs/list` tool to list items in a module

## Phase 6: Index Persistence and Cache

- [x] 6.1.1 BM25 binary serialization (magic "BM25", v1, docs, IDF, TF maps)
- [x] 6.1.2 HNSW binary serialization (magic "HNSW", v1, nodes, embeddings, graph)
- [x] 6.1.3 TfIdf binary serialization (magic "TFID", v1, term_to_dim, idf_weights)
- [x] 6.1.4 Serialize to `build/debug/.doc-index/` (bm25.bin, hnsw.bin, tfidf.bin)
- [x] 6.1.5 FNV fingerprint of source file sizes + mtimes for cache validation
- [x] 6.1.6 Load persisted indices on startup if fingerprint matches
- [x] 6.1.7 Automatic cache invalidation when sources change

## Phase 7: CMake and Build Integration

- [x] 7.1.1 Add search/*.cpp to CMakeLists.txt (tml_search library)
- [x] 7.1.2 Add search runtime to linker for TML programs
- [x] 7.1.3 Build and verify all code compiles
- [x] 7.1.4 Index builds 6157 items from core+std libraries
- [x] 7.1.5 Performance instrumentation: build time + query latency in search header
- [x] 7.1.6 Query latency verified < 10ms (3-9ms measured in debug build)

## Phase 8: Query Processing (inspired by Vectorizer)

- [x] 8.1.1 Query-side stop word removal before search
- [x] 8.1.2 Query expansion for TML domain terms (65+ synonym mappings)
- [x] 8.1.3 Synonym map for TML concepts (e.g. "error" -> "Outcome Err", "optional" -> "Maybe")
- [x] 8.1.4 Multi-query fusion: expand to max 8 queries, merge with best-score-wins

## Phase 9: Result Diversification (inspired by Vectorizer MMR)

- [x] 9.1.1 Implement MMR (Maximal Marginal Relevance) post-ranking
- [x] 9.1.2 Jaccard word-set similarity for content deduplication
- [x] 9.1.3 Configurable lambda (default 0.7) for relevance vs diversity tradeoff
- [x] 9.1.4 Deduplicate near-identical results (Jaccard > 0.8 threshold)

## Phase 10: Multi-Signal Ranking (inspired by Vectorizer)

- [x] 10.1.1 Boost pub items over private/internal items
- [x] 10.1.2 Boost well-documented items (has doc comments, params, examples)
- [x] 10.1.3 Boost top-level module items over deeply nested ones
- [x] 10.1.4 Score breakdown in results (BM25 contribution vs HNSW contribution)

## Validation

- [x] V.1 MCP server lazy-loads documentation on first query
- [x] V.2 BM25 search returns ranked results with field-boosted scoring
- [x] V.3 HNSW search finds semantically related documentation
- [x] V.4 Hybrid mode fuses BM25 + HNSW with tuned RRF
- [x] V.5 SIMD distance functions tested (26 tests)
- [x] V.6 Index persistence saves/loads from build/debug/.doc-index/ (BM25=1.1MB, HNSW=13.5MB)
- [x] V.7 Claude Code queries docs via MCP docs_search tool
- [x] V.8 Query expansion improves recall (e.g. "optional" finds Maybe, "vector" finds List)
- [x] V.9 MMR diversification avoids redundant results from same module
- [x] V.10 Score breakdown shows BM25/HNSW/boost contributions per result
