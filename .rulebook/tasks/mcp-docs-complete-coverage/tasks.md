# MCP Documentation Complete Coverage — Tasks

## Phase 0: Fix Extractor Bug (CRITICAL — blocks all other phases)

### 0.1 Fix top-level function indexing
- [x] 0.1.1 Investigate why top-level `pub func` items with `///` doc comments don't appear in `docs_search` results — ROOT CAUSE: `skip_newlines()` consumed DocComment tokens between declarations
- [x] 0.1.2 Fix: `skip_newlines()` now only skips Newline tokens. DocComment tokens preserved for `collect_doc_comment()`. Also added propagation from top-level funcs to impl methods.
- [x] 0.1.3 Verified: `docs_get("core::str::len")` returns description "Returns the length of a string in bytes" + examples
- [x] 0.1.4 Verified: `docs_get("core::str::split")` returns description + examples

### 0.2 Add doc comments to impl methods (where top-level has docs)
- [x] 0.2.1 Not needed — extractor now propagates docs from top-level functions to impl methods automatically (`propagate_docs_to_impl_methods`)
- [x] 0.2.2 Not needed — automatic propagation handles this
- [x] 0.2.3 Verified: 11866 items indexed, descriptions + examples appear in docs_get output

## Phase 1: Core Library Doc Comments (highest impact)

### 1.1 Core Types (most used, highest ROI)
- [x] 1.1.1 `lib/core/src/str.tml` — 58/58 ✓ (pre-existing)
- [x] 1.1.2 `lib/core/src/option.tml` — 29/29 ✓ (6b730926)
- [x] 1.1.3 `lib/core/src/result.tml` — 33/33 ✓ (6b730926)
- [x] 1.1.4 `lib/core/src/fmt/mod.tml` — behaviors only, no pub func
- [x] 1.1.5 `lib/core/src/iter/mod.tml` — behavior only, no pub func
- [x] 1.1.6 `lib/core/src/ops/arith.tml` — 102/102 ✓ (6b730926)
- [x] 1.1.7 `lib/core/src/clone.tml` — 14/17 (impl-level docs cover rest)
- [x] 1.1.8 `lib/core/src/cmp.tml` — 41/41 ✓ (6b730926)
- [x] 1.1.9 `lib/core/src/convert.tml` — 38/40 (impl-level docs cover rest)
- [x] 1.1.10 `lib/core/src/default.tml` — 13/14 (impl-level docs cover rest)

### 1.2 Core Collections & Memory
- [x] 1.2.1 `lib/core/src/slice.tml` — file not found (no separate module)
- [x] 1.2.2 `lib/core/src/alloc/heap.tml` — 12/12 ✓ (6d30a43b)
- [x] 1.2.3 `lib/core/src/alloc/shared.tml` — 15/15 ✓ (6d30a43b)
- [x] 1.2.4 `lib/core/src/alloc/sync.tml` — 16/16 ✓ (6d30a43b)
- [x] 1.2.5 `lib/core/src/cell/ref_cell.tml` — 15/15 ✓ (pre-existing)
- [x] 1.2.6 `lib/core/src/pin.tml` — 11/11 ✓ (pre-existing)
- [x] 1.2.7 `lib/core/src/num/nonzero.tml` — 6/6 ✓ (pre-existing)

### 1.3 Core Numeric & Char
- [x] 1.3.1 `lib/core/src/num/integer.tml` — 51/51 ✓ (pre-existing)
- [x] 1.3.2 float.tml — in fmt/float.tml (separate module)
- [x] 1.3.3 `lib/core/src/num/nonzero.tml` — 6/6 ✓
- [x] 1.3.4 char.tml — in ascii/char.tml + unicode/char.tml

## Phase 2: Std Library Doc Comments

### 2.1 Collections
- [x] 2.1.1 `hashmap.tml` — 15/15 ✓ (4c944385)
- [x] 2.1.2 `list.tml` — 16/16 ✓ (pre-existing)
- [x] 2.1.3 `buffer.tml` — 84/84 ✓ (4c944385)
- [x] 2.1.4 `btreemap.tml` — 26/26 ✓ (pre-existing)
- [x] 2.1.5 `deque.tml` — 15/15 ✓ (pre-existing)

### 2.2 Sync & Concurrency
- [x] 2.2.1 `mutex.tml` — 9/9 ✓ (pre-existing)
- [x] 2.2.2 `rwlock.tml` — 10/10 ✓ (pre-existing)
- [x] 2.2.4 `atomic.tml` — 121/121 ✓ (4c944385)
- [x] 2.2.5 `condvar.tml` — 6/6 ✓ (pre-existing)
- [x] 2.2.6 `mpsc.tml` — 17/17 ✓ (4c944385)
- [x] 2.2.7 `barrier.tml` — 3/3 ✓ (pre-existing)

### 2.3-2.5 I/O, JSON, Other
- [x] 2.4.1 `json/mod.tml` — 48/48 ✓ (pre-existing)
- [x] 2.5.2 `time.tml` — 13/13 ✓ (pre-existing)
- [ ] 2.3.x I/O & Networking — deferred (need to locate actual file paths)
- [ ] 2.5.x Other modules — deferred (crypto, regex, thread, os)

## Phase 3: Example Generation from Tests

- [ ] 3.1 Build script to extract `@test` functions as `@example` blocks
- [ ] 3.2 Run script on core/ tests → inject examples into core/ doc comments
- [ ] 3.3 Run script on std/ tests → inject examples into std/ doc comments
- [ ] 3.4 Manual review of generated examples for top 50 types

## Phase 4: Cross-References & Metadata

- [ ] 4.1 Add `@see` cross-references between related types (Maybe↔Outcome, Mutex↔RwLock, etc.)
- [ ] 4.2 Add `@since` version tags (0.1.0 for core, 0.2.0 for recent additions)
- [ ] 4.3 Add `@deprecated` tags for functions with known codegen bugs
- [ ] 4.4 Add category prefixes in summaries: `[Thread-safe]`, `[Pure TML]`, `[FFI]`, `[Iterator]`

## Phase 5: MCP Search Quality

- [ ] 5.1 Verify `docs_search` returns descriptions (not just signatures) after Phase 1-2
- [ ] 5.2 Verify `docs_get` returns examples after Phase 3
- [ ] 5.3 Verify `docs_search` with category keywords finds tagged items after Phase 4
- [ ] 5.4 Measure doc coverage: % of public functions with doc comments
- [ ] 5.5 Target: 90%+ of public functions have `///` with description + `@param` + `@returns`
