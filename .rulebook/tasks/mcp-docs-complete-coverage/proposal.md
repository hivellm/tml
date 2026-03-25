# MCP Documentation Complete Coverage

## Why

The MCP documentation tools (`docs_search`, `docs_get`, `docs_list`) are functional (11866 items indexed, queries in ~30ms) but return **empty descriptions and no examples** for most items. Two root causes identified:

### Root Cause 1: Top-Level Functions Not Linked to Impl Methods

Doc comments exist on **top-level functions** (`pub func len(s: Str) -> I64` with `///` comments) but NOT on the corresponding **impl methods** (`impl Str { func len(this) -> I64 }` without comments). The extractor indexes impl methods (which appear in `docs_search`), but the top-level functions with their docs are either not indexed or in a different path. Result: search finds the method but returns empty description.

**Evidence**: `docs_search("len", module="core::str")` finds `impl_Str::len` (method, no doc) but NOT the top-level `pub func len(s: Str)` which has a 10-line doc comment with examples.

### Root Cause 2: Most Library Modules Lack Doc Comments

Out of ~6000 public functions, only ~600 have `///` doc comments (mostly in `core::str`). Even where comments exist (like `str.tml` with 421 comments), they're on top-level functions, not on the impl methods that the extractor indexes.

### Infrastructure Already Built

- Extractor: `compiler/include/doc/extractor.hpp` — parses `@param`, `@returns`, `@example`, `@see`, `@since`, `@deprecated`, `@throws`
- Doc model: `compiler/include/doc/doc_model.hpp` — `DocParam`, `DocReturn`, `DocExample`, `DocThrows`, `DocDeprecation`
- Doc parser: `compiler/src/doc/doc_parser.cpp` — structured tag parsing
- MCP handler: `compiler/src/mcp/mcp_tools_docs_handlers.cpp` — formats full items with description, examples, params
- Search: BM25 index already indexes `item.doc` text (line 745-747 of `mcp_tools_docs.cpp`)

## What Changes

1. **Add `///` doc comments to all public functions/methods** in `lib/core/` and `lib/std/` (~6000 functions)
2. **Add `@example` blocks** with working code snippets extracted from existing test files
3. **Add `@param` tags** for all parameters with type and purpose
4. **Add `@returns` tags** describing return values and edge cases
5. **Add `@see` cross-references** between related types/functions
6. **Add `@since 0.1.0`/`@since 0.2.0`** version tags
7. **Add `@deprecated` tags** for functions with known codegen bugs or planned removal
8. **Add category tags in descriptions** (`Thread-safe`, `Pure TML`, `FFI wrapper`, `Iterator adapter`)

## Impact

- **AI productivity**: 10x fewer syntax errors per session (estimated from past error logs)
- **Coverage measurement**: doc coverage tracked alongside test coverage
- **User-facing docs**: HTML docs auto-generated from same doc comments
- **No breaking changes**: doc comments are ignored by compiler, zero runtime impact
- **Specs affected**: none (documentation-only change)

## Approach

Auto-generate initial doc comments from:
1. **Test files** — each `@test` function is a usage example
2. **Function signatures** — parameter names/types → `@param` tags
3. **Module structure** — group names → category tags
4. **Existing `///` comments** — preserve and enhance (421 in core/str, 150 in sync/mutex)

Then manual review + enhancement for top 50 most-used types.
