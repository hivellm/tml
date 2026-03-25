# Tasks: Trie[V] — Prefix Tree

**Status**: Proposed
**Priority**: MEDIUM
**Phase**: 2 — Stdlib Completeness

## Motivation

Tries (prefix trees) are optimal for: URL routing (radix tree variant), autocomplete/typeahead, IP routing tables, dictionary/spell checking, and prefix-based search. TML's HTTP router already uses a radix tree internally — a general-purpose Trie in collections would be reusable.

## Phase 1: Core Implementation (`lib/std/src/collections/trie.tml`)

- [ ] 1.1 Implement `Trie[V]` struct — string-keyed prefix tree with optional values at nodes
- [ ] 1.2 `Trie::new() -> Trie[V]` — empty trie
- [ ] 1.3 `insert(mut this, key: Str, value: V)` — insert key-value pair
- [ ] 1.4 `get(this, key: Str) -> Maybe[ref V]` — exact lookup
- [ ] 1.5 `contains(this, key: Str) -> Bool` — exact key existence
- [ ] 1.6 `remove(mut this, key: Str) -> Maybe[V]` — remove and return value
- [ ] 1.7 `len(this) -> I64` — number of stored keys
- [ ] 1.8 `is_empty(this) -> Bool`
- [ ] 1.9 Write tests: insert/get/remove, overwrite, missing keys

## Phase 2: Prefix Operations

- [ ] 2.1 `starts_with(this, prefix: Str) -> Bool` — any key has this prefix
- [ ] 2.2 `keys_with_prefix(this, prefix: Str) -> List[Str]` — all keys starting with prefix
- [ ] 2.3 `values_with_prefix(this, prefix: Str) -> List[ref V]` — values for prefix matches
- [ ] 2.4 `longest_prefix(this, query: Str) -> Maybe[Str]` — longest key that is a prefix of query
- [ ] 2.5 `autocomplete(this, prefix: Str, limit: I64) -> List[Str]` — top N completions
- [ ] 2.6 Implement `Iterator` — iterate all key-value pairs (DFS order)
- [ ] 2.7 Implement `Display`, `Debug`, `Clone`, `Default`, `Drop`
- [ ] 2.8 Write tests: prefix search, autocomplete, longest prefix, iteration
- [ ] 2.9 Update `collections/mod.tml` to export Trie
- [ ] 2.10 Run collections test suite
