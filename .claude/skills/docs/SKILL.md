---
name: docs
description: Search and browse TML documentation. Use when the user asks about TML APIs, functions, types, behaviors, or says "docs", "documentation", "how does X work in TML".
user-invocable: true
allowed-tools: mcp__tml__docs_search, mcp__tml__docs_get, mcp__tml__docs_list, mcp__tml__docs_resolve
argument-hint: <search query or module path>
---

## TML Documentation Lookup

Determine user intent from `$ARGUMENTS`:

### Search (default)
If the argument is a search query (e.g., "split string", "HashMap methods"):
- Use `mcp__tml__docs_search` with `query: $ARGUMENTS`
- Use `mode: "hybrid"` for best results
- Optionally filter with `kind` (function, method, struct, enum, behavior) or `module`

### Get Specific Item
If the argument looks like a qualified path (e.g., "core::str::split", "std::collections::HashMap"):
- Use `mcp__tml__docs_get` with `id: $ARGUMENTS`

### List Module Contents
If the argument looks like a module path (e.g., "core::str", "std::json"):
- Use `mcp__tml__docs_list` with `module: $ARGUMENTS`

### Resolve Name
If the argument is a short name (e.g., "HashMap", "split"):
- Use `mcp__tml__docs_resolve` with `name: $ARGUMENTS`
- Then use `mcp__tml__docs_get` on the resolved path for full details

Present results clearly with signatures, descriptions, and examples when available.