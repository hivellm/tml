# Proposal: Bounded MPSC Channel (sync_channel)

## Why
Only unbounded channels exist in `std::sync::mpsc`. Bounded channels are essential for backpressure and memory control in producer-consumer pipelines; without them callers must implement their own blocking coordination.

## What Changes
Add `sync_channel[T](bound: I64) -> (SyncSender[T], Receiver[T])`, the `SyncSender[T]` type with blocking and non-blocking send, and an `Iterator` implementation for `Receiver[T]`.

## Impact
- Affected specs: std::sync::mpsc
- Affected code: lib/std/src/sync/mpsc.tml, compiler/runtime/concurrency/
- Breaking change: NO
- User benefit: Producers can be rate-limited by receiver capacity without external synchronization primitives
