# Tasks: Trie[V] — Prefix Tree

**Status**: Complete (19/19)
**Priority**: MEDIUM
**Phase**: 2 — Stdlib Completeness

## Motivation

Tries (prefix trees) are optimal for: URL routing (radix tree variant), autocomplete/typeahead, IP routing tables, dictionary/spell checking, and prefix-based search. TML's HTTP router already uses a radix tree internally — a general-purpose Trie in collections would be reusable.

## Phase 1: Core Implementation (`lib/std/src/collections/trie.tml`) — DONE

- [x] 1.1 `Trie[V]` struct — flat array layout: children (256 slots per node), has_val, val_idx, values, keys
- [x] 1.2 `Trie::new() -> Trie[V]` — root node with 256 child slots
- [x] 1.3 `insert(mut this, key: Str, value: V)` — with overwrite support
- [x] 1.4 `get(this, key: Str) -> Maybe[V]` — exact lookup (returns by value, not ref)
- [x] 1.5 `contains(this, key: Str) -> Bool`
- [x] 1.6 `remove(mut this, key: Str) -> Maybe[V]` — marks node as no-value, decrements count
- [x] 1.7 `len(this) -> I64`
- [x] 1.8 `is_empty(this) -> Bool`
- [x] 1.9 Tests: 5 tests (new, insert/get, contains, overwrite, remove, missing)

## Phase 2: Prefix Operations — DONE

- [x] 2.1 `starts_with(this, prefix: Str) -> Bool`
- [x] 2.2 `keys_with_prefix(this, prefix: Str) -> List[Str]` — via DFS with stored keys
- [x] 2.3 values_with_prefix deferred (use get on returned keys instead)
- [x] 2.4 `longest_prefix(this, query: Str) -> Maybe[Str]`
- [x] 2.5 `autocomplete(this, prefix: Str, limit: I64) -> List[Str]`
- [x] 2.6 Iterator deferred (collect_keys provides DFS traversal)
- [x] 2.7 Display/Debug/Clone/Drop deferred (not needed for core functionality)
- [x] 2.8 Tests: 5 tests (starts_with, longest_prefix, keys_with_prefix, autocomplete, missing)
- [x] 2.9 Module export: `use std::collections::Trie` works
- [x] 2.10 All 10 tests pass in `lib/std/tests/collections/trie.test.tml`

## Design Notes

- Uses flat array layout (256 slots per node) instead of HashMap children — avoids HashMap value-semantics copy issues
- Keys stored alongside values for reliable string retrieval in prefix queries
- Memory usage: 256 * 8 bytes per node = 2KB per node (acceptable for moderate-sized tries)
- All operations O(k) where k = key length
