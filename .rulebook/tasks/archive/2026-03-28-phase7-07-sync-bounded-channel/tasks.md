# Tasks: Bounded MPSC Channel (sync_channel)

**Status**: Complete
**Priority**: HIGH
**Phase**: 7 — Rust Parity

## Phase 1: Bounded channel implementation
- [x] 1.1 `sync_channel[T](bound: I64) -> (SyncSender[T], BoundedReceiver[T])` — implemented in lib/std/src/sync/mpsc.tml
- [x] 1.2 `SyncSender[T]` type — blocks when buffer full via Condvar::wait
- [x] 1.3 `SyncSender::send(value: T) -> Outcome[Unit, SendError]` — blocking send with backpressure
- [x] 1.4 `SyncSender::try_send(value: T) -> Outcome[Unit, TrySendError[T]]` — non-blocking, returns TrySendError::Full or TrySendError::Disconnected
- [x] 1.5 `impl Iterator for Receiver[T]` and `impl Iterator for BoundedReceiver[T]` — both delegate to recv()
- [x] 1.6 Tests: sync_channel_basic.test.tml, sync_channel_try_send.test.tml, sync_channel_iter.test.tml — all passing

## Notes
- `TrySendError[T]` enum added: `Full(T)`, `Disconnected(T)`
- `BoundedChannelInner[T]` uses `Mutex[BoundedChannelState[T]]` + two Condvars (not_full, not_empty)
- `BoundedReceiver[T]` is the receiver end for bounded channels (distinct from unbounded `Receiver[T]`)
- Blocking `send` uses Condvar::wait loop; non-blocking `try_send` checks len >= cap immediately
