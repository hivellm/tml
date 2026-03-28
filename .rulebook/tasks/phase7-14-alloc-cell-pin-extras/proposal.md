# Proposal: Alloc, Cell, Pin Extras — Weak[T], get_or_init, as_ref/as_mut

## Why

Heap[T] missing as_ref/as_mut. Shared[T]/Sync[T] have no Weak[T] or downgrade. RefCell missing into_inner/swap. OnceCell missing get_or_init. Pin missing safe into_inner/as_ref/as_mut. These gaps block real ownership and interior mutability patterns.

## What Changes

Add missing methods to alloc, cell, and pin modules. Weak[T] type needs new implementation for Shared and Sync.

## Impact
- Affected specs: core::alloc, core::cell, core::runtime::pin
- Affected code: `lib/core/src/alloc/`, `lib/core/src/cell/`, `lib/core/src/runtime/pin.tml`
- Breaking change: NO
- User benefit: Weak references, lazy init, safe pin operations
