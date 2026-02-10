# Tasks: MCP Documentation Search with BM25 + HNSW Vector Index

**Status**: Pending (0%)

## Phase 1: Documentation Loading on MCP Startup

- [ ] 1.1.1 Generate docs.json via `tml doc --format=json` on MCP startup
- [ ] 1.1.2 Parse docs.json into DocIndex in-memory
- [ ] 1.1.3 Fall back to direct AST extraction if JSON missing/stale
- [ ] 1.1.4 Cache loaded DocIndex in MCP server state
- [ ] 1.1.5 Add source file hash checking for invalidation

## Phase 2: BM25 Text Index

- [ ] 2.1.1 Create `compiler/include/search/bm25_index.hpp` with BM25Index class
- [ ] 2.1.2 Implement tokenizer (whitespace, camelCase, snake_case splitting)
- [ ] 2.1.3 Implement stop word filtering (English + TML common keywords)
- [ ] 2.1.4 Implement term frequency (TF) computation per document
- [ ] 2.1.5 Implement inverse document frequency (IDF) across corpus
- [ ] 2.1.6 Implement BM25 scoring (k1=1.2, b=0.75)
- [ ] 2.1.7 Index DocItem fields: name, path, signature, doc, params, return type
- [ ] 2.1.8 Implement field boosting (name > signature > doc text)
- [ ] 2.1.9 Create `compiler/src/search/bm25_index.cpp` implementation
- [ ] 2.1.10 Write unit tests for BM25 tokenizer and scoring

## Phase 3: SIMD-Optimized Distance Functions

- [ ] 3.1.1 Create `compiler/include/search/simd_distance.hpp`
- [ ] 3.1.2 Implement `dot_product_f32` with SSE2 baseline
- [ ] 3.1.3 Implement `dot_product_f32` with AVX2 acceleration
- [ ] 3.1.4 Implement `cosine_similarity_f32` using dot product
- [ ] 3.1.5 Implement `euclidean_distance_f32` for alternative metric
- [ ] 3.1.6 Add runtime CPUID detection for AVX2 vs SSE2 fallback
- [ ] 3.1.7 Add scalar fallback for non-x86 targets
- [ ] 3.1.8 Create `compiler/src/search/simd_distance.cpp` implementation
- [ ] 3.1.9 Write unit tests for distance functions (correctness + SIMD vs scalar)

## Phase 4: HNSW Vector Index

- [ ] 4.1.1 Create `compiler/include/search/hnsw_index.hpp` with HnswIndex class
- [ ] 4.1.2 Implement TF-IDF weighted bag-of-words embedding generation
- [ ] 4.1.3 Build vocabulary from corpus at index time (top-N IDF terms)
- [ ] 4.1.4 Implement HNSW graph construction (M=16, efConstruction=200)
- [ ] 4.1.5 Implement HNSW layer selection (ln(N) layers)
- [ ] 4.1.6 Implement HNSW greedy search with SIMD distance
- [ ] 4.1.7 Implement HNSW insert with neighbor selection heuristic
- [ ] 4.1.8 Create `compiler/src/search/hnsw_index.cpp` implementation
- [ ] 4.1.9 Write unit tests for HNSW (insert, search, recall@10)

## Phase 5: Enhanced MCP Search Tools

- [ ] 5.1.1 Replace `handle_docs_search` with BM25 + HNSW hybrid search
- [ ] 5.1.2 Add `mode` parameter: "text" (BM25 only), "semantic" (HNSW only), "hybrid" (fused)
- [ ] 5.1.3 Implement score fusion for hybrid mode (reciprocal rank fusion)
- [ ] 5.1.4 Return structured JSON results (item ID, path, signature, score, snippet)
- [ ] 5.1.5 Add `docs/get` tool — get full DocItem by ID
- [ ] 5.1.6 Add `docs/list` tool — list items in a module
- [ ] 5.1.7 Add `docs/resolve` tool — resolve short name to full path(s)
- [ ] 5.1.8 Update `mcp_tools.hpp` with new tool declarations
- [ ] 5.1.9 Register new tools in `register_compiler_tools()`
- [ ] 5.1.10 Update `.mcp.json` configuration with new tool descriptions

## Phase 6: Index Persistence and Cache

- [ ] 6.1.1 Define binary serialization format for BM25 index
- [ ] 6.1.2 Define binary serialization format for HNSW index
- [ ] 6.1.3 Serialize indices to `build/debug/.doc-index/`
- [ ] 6.1.4 Load persisted indices on startup if source hashes match
- [ ] 6.1.5 Extend `cache/invalidate` to invalidate doc index entries
- [ ] 6.1.6 Implement incremental re-indexing for changed documents only

## Phase 7: Integration and CMake

- [ ] 7.1.1 Add `compiler/src/search/*.cpp` to CMakeLists.txt
- [ ] 7.1.2 Add `compiler/include/search/` to include paths
- [ ] 7.1.3 Build and verify all new code compiles
- [ ] 7.1.4 Integration test: MCP startup loads docs and builds indices
- [ ] 7.1.5 Integration test: `docs/search` returns ranked results for known queries
- [ ] 7.1.6 Integration test: `docs/get` returns full item by ID
- [ ] 7.1.7 Integration test: semantic search finds related items by intent
- [ ] 7.1.8 Performance test: index build time < 1s for full stdlib
- [ ] 7.1.9 Performance test: query latency < 10ms for typical queries

## Validation

- [ ] V.1 MCP server auto-loads documentation on startup
- [ ] V.2 BM25 search returns ranked results with TF-IDF scoring
- [ ] V.3 HNSW search finds semantically related documentation
- [ ] V.4 Hybrid mode fuses BM25 + HNSW results correctly
- [ ] V.5 SIMD distance functions match scalar reference implementation
- [ ] V.6 Index persistence avoids re-indexing on restart
- [ ] V.7 Claude Code can query docs via enhanced MCP tools
- [ ] V.8 `docs/get` returns complete item documentation
- [ ] V.9 `docs/list` enumerates module contents
- [ ] V.10 `docs/resolve` finds items by short name
