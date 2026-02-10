# Proposal: MCP Documentation Search with BM25 + HNSW Vector Index

## Why

The MCP server's current `docs/search` tool performs naive substring matching over raw `.md` and `.tml` files. This produces low-quality results with no relevance ranking. Meanwhile, the `tml doc` command already generates structured JSON documentation (DocIndex with modules, items, signatures, examples). We need to:

1. **Load this structured documentation on MCP startup** so it's always available
2. **Build a BM25 text index** for keyword-based search with proper TF-IDF ranking
3. **Build an HNSW vector index** for semantic similarity search (embedding doc items into vectors)
4. **Leverage existing SIMD infrastructure** (`compiler/include/mir/passes/vectorization.hpp`) for optimized distance computations

**Current problems:**
- `docs/search` does naive substring grep across files with no ranking
- No structured data loaded; every search re-scans the filesystem
- No semantic search capability (can't find "how to iterate a list" when docs say "loop over elements")
- No term frequency or inverse document frequency scoring
- Results are raw file snippets, not structured DocItem references

**Benefits:**
- High-quality ranked results via BM25 (industry standard text retrieval)
- Semantic search via HNSW nearest neighbor lookup
- Sub-millisecond query times (in-memory indices)
- Auto-loaded on MCP startup; no cold-start filesystem scanning
- SIMD-accelerated vector distance computation for HNSW
- Structured results with DocItem IDs, signatures, paths, and relevance scores

## What Changes

### 1. Documentation Loading on MCP Startup

- On `tml mcp` startup, run `tml doc --format=json` to generate `docs.json`
- Parse the JSON into DocIndex in-memory
- Fall back to extracting docs directly from AST if JSON is stale/missing
- Cache the loaded index; invalidate when source files change

### 2. BM25 Text Index

- **New file**: `compiler/include/search/bm25_index.hpp`
- **New file**: `compiler/src/search/bm25_index.cpp`
- Tokenizer: split on whitespace, punctuation, camelCase boundaries, snake_case underscores
- Stop words: common English + TML keywords that appear everywhere (func, let, var, pub)
- Term frequency (TF) per document, inverse document frequency (IDF) across corpus
- BM25 scoring with k1=1.2, b=0.75 parameters (tunable)
- Index fields: name, path, signature, doc text, parameter names, return type
- Field boosting: name matches rank higher than doc text matches

### 3. HNSW Vector Index

- **New file**: `compiler/include/search/hnsw_index.hpp`
- **New file**: `compiler/src/search/hnsw_index.cpp`
- Embedding generation: TF-IDF weighted bag-of-words vectors (no external ML model needed)
- Vocabulary built from corpus at index time
- HNSW construction: M=16, efConstruction=200 (standard parameters)
- HNSW search: efSearch=50 (tunable via query parameter)
- SIMD-accelerated cosine similarity using SSE2/AVX2 intrinsics
- Vector dimensionality: top-N IDF terms (e.g., 512 or 1024 dimensions)

### 4. SIMD-Optimized Distance Functions

- **New file**: `compiler/include/search/simd_distance.hpp`
- **New file**: `compiler/src/search/simd_distance.cpp`
- `cosine_similarity_f32(a, b, dim)` — SSE2 baseline, AVX2 accelerated
- `euclidean_distance_f32(a, b, dim)` — for alternative metrics
- `dot_product_f32(a, b, dim)` — core inner product
- Runtime CPU feature detection (CPUID) for AVX2 fallback to SSE2
- Scalar fallback for non-x86 targets

### 5. Enhanced MCP Search Tools

Replace the current `docs/search` handler and add new tools:

- **`docs/search`** (enhanced) — Combined BM25 + HNSW search with relevance fusion
  - Parameters: `query` (string), `limit` (int), `mode` ("text" | "semantic" | "hybrid")
  - Returns: ranked DocItems with scores, signatures, paths, and context snippets
- **`docs/get`** — Get full documentation for an item by ID
  - Parameters: `id` (string, e.g., "core::slice::Slice::get")
  - Returns: complete DocItem with all fields
- **`docs/list`** — List all items in a module
  - Parameters: `module` (string, e.g., "std::net::tcp"), `kind` (optional filter)
  - Returns: array of DocItems in that module
- **`docs/resolve`** — Resolve a short name to full path(s)
  - Parameters: `name` (string, e.g., "HashMap"), `limit` (int)
  - Returns: matching full paths and item summaries

### 6. Index Persistence

- Serialize BM25 + HNSW indices to binary format in `build/debug/.doc-index/`
- On startup: if index file exists and source hashes match, load directly (skip re-indexing)
- On source change: invalidate and rebuild affected documents only
- `cache/invalidate` tool extended to also invalidate doc index entries

## Impact

### Specs Affected
- `compiler/mcp` — Enhanced docs tools
- `compiler/search` — NEW: search index infrastructure

### Code Affected
- `compiler/src/mcp/mcp_tools.cpp` — Replace `handle_docs_search`, add new handlers
- `compiler/include/mcp/mcp_tools.hpp` — Add new tool declarations
- `compiler/src/mcp/mcp_server.cpp` — Load doc index on startup
- NEW: `compiler/include/search/` — BM25, HNSW, SIMD distance headers
- NEW: `compiler/src/search/` — Implementation files
- `CMakeLists.txt` — Add new source files

### Dependencies
- No external dependencies; BM25 and HNSW implemented from scratch in C++
- SIMD intrinsics via `<immintrin.h>` (already available in the build)

### Breaking Changes
- `docs/search` response format changes from raw text to structured JSON
- Existing callers (Claude Code MCP) will get better results automatically

### User Benefits
- Claude Code can find relevant documentation with natural language queries
- Sub-millisecond search latency (vs current filesystem scanning)
- Semantic understanding: find docs by intent, not just exact keywords
- Structured results enable precise code generation from documentation
