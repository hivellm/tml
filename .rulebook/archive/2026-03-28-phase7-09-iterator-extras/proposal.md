# Proposal: Iterator Extra Methods — max, min, partition, unzip

## Why

Iterator is at 85% coverage but missing `max`/`min` (which are the most common terminal operations after `collect`), plus `partition` and `unzip` which are frequently used patterns.

## What Changes

Add 7 methods to Iterator trait/adapters in `lib/core/src/iter/`. All pure TML — no compiler changes needed.

## Impact
- Affected specs: core::iter
- Affected code: `lib/core/src/iter/traits/iterator.tml`
- Breaking change: NO
- User benefit: Find max/min of sequences, split iterators by predicate, separate pair iterators
