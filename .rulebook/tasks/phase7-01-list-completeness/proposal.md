# Proposal: List[T] Completeness — Rust Vec Parity

## Why

List[T] has only 35% coverage vs Rust's Vec<T> (16 of 44 methods). Missing critical operations like `sort`, `insert`, `remove`, `contains`, `reverse` make List unusable for many real programs. This is the single biggest gap in the TML stdlib.

## What Changes

Add 28 missing methods to List[T] in `lib/std/src/collections/list.tml`. List is C-backed, so some methods need C runtime additions while others can be pure TML wrappers using existing `get`/`set`/`push`/`pop`/`len`.

## Impact
- Affected specs: std::collections::List
- Affected code: `lib/std/src/collections/list.tml`, `lib/std/runtime/collections/list.c`
- Breaking change: NO (additive only)
- User benefit: Sort lists, insert/remove at index, search, iterate — fundamental collection operations
