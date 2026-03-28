# Proposal: Iterator collect() + FromIterator

## Why
Without `.collect()`, iterator chains cannot be materialized into concrete collections. This makes most iterator pipelines useless in practice since there is no way to gather results into a List, HashMap, or Str.

## What Changes
Add `FromIterator[T]` behavior to `core::iter`, implement it for List, HashMap, and Str, and add a `collect[C]()` method to the Iterator behavior.

## Impact
- Affected specs: core::iter, core::collections
- Affected code: lib/core/src/iter/mod.tml, lib/core/src/collections/list.tml, lib/std/src/collections/hashmap.tml, lib/core/src/str.tml
- Breaking change: NO
- User benefit: Iterator chains can be materialized into List, HashMap, or Str with a single collect() call
