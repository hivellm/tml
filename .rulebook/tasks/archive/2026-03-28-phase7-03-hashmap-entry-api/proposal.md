# Proposal: HashMap Entry API + Missing Methods

## Why
Without the Entry API there is no safe get-or-insert pattern, forcing callers to do two separate lookups. `is_empty()` and bulk operations like `keys()`, `values()`, `retain()`, and `drain()` are also missing, blocking common use cases.

## What Changes
Add an `Entry[K,V]` enum with `Occupied`/`Vacant` variants, the `entry()` method on HashMap, all Entry combinators, and the missing bulk/query methods.

## Impact
- Affected specs: std::collections
- Affected code: lib/std/src/collections/hashmap.tml
- Breaking change: NO
- User benefit: Idiomatic get-or-insert patterns and full HashMap API parity with Rust's std::collections::HashMap
