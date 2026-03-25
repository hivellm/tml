# Proposal: Trie[V] — Prefix Tree

## Status: PROPOSED

## Summary

A string-keyed prefix tree (trie) supporting exact lookup, prefix search, autocomplete, and longest-prefix matching. Keys are `Str`; values are generic `V`. The core use cases are URL routing, autocomplete/typeahead, dictionary lookup, and IP prefix matching. TML's HTTP router already uses a radix tree internally — this is a general-purpose reusable version for application code.

## Motivation

Hash maps give O(1) exact lookup but cannot answer "all keys that start with X" without a full scan. Binary search trees give O(log n) prefix queries but share prefixes wastefully. A trie gives O(k) operations (where k is key length, independent of collection size) for both exact and prefix queries.

Autocomplete, command-line tab completion, URL dispatch, and network routing tables all fundamentally require prefix queries. Implementing any of these with a `HashMap` means doing O(n) scans for prefix queries. A trie reduces this to O(k + results).

## Design

`Trie[V]` is an n-ary tree where each node contains:
- A `HashMap[U8, TrieNode[V]]` mapping the next byte to the child node
- A `Maybe[V]` value (present only if this node is a key terminus)
- A `Bool` indicating whether this node terminates a key

This design uses byte-level branching (256-way at each node) which is simple to implement correctly. A radix tree variant (compressing single-child chains) is a future optimization.

`insert`, `get`, `contains`, `remove` all traverse the tree character-by-character from the root. `remove` prunes empty branches on the way back up.

`keys_with_prefix` does a DFS from the node that is the prefix endpoint, collecting all key suffixes and prepending the prefix. `longest_prefix` walks as far as possible and records the last terminus encountered. `autocomplete` is `keys_with_prefix` with a limit.

The `Iterator` implementation does a DFS traversal in lexicographic order (since `HashMap` does not preserve order, DFS order is undefined unless nodes are sorted — the iterator should sort child keys at each level).

## What Changes

- New: `lib/std/src/collections/trie.tml` — Trie[V], TrieNode[V], TrieIter[V]
- Modified: `lib/std/src/collections/mod.tml` — export Trie
- New: `lib/std/tests/collections/trie_basic.test.tml`
- New: `lib/std/tests/collections/trie_prefix.test.tml`

## Dependencies

- Depends on: `HashMap[K,V]`, `List[T]`, `Maybe[T]` from std/core
- Enables: application-level URL routing, autocomplete, command-line tools
- Potentially enables: replacement of HTTP router's internal radix tree with a general-purpose implementation

## Risks

- The byte-level branching factor of 256 means each node has a potentially large `HashMap` — for short or sparse tries this has high memory overhead; a compact trie variant (using sorted `List[(U8, TrieNode)]` for nodes with few children) should be considered
- `remove` that prunes empty branches requires careful recursive logic; the test suite must include removal of a key that is a prefix of another key and vice versa
