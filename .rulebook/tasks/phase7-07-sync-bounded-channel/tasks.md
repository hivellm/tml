# Tasks: Bounded MPSC Channel (sync_channel)

**Status**: Proposed
**Priority**: HIGH
**Phase**: 7 — Rust Parity

## Phase 1: Bounded channel implementation
- [ ] 1.1 `sync_channel[T](bound: I64) -> (SyncSender[T], Receiver[T])`
- [ ] 1.2 `SyncSender[T]` type — blocks when buffer full
- [ ] 1.3 `SyncSender::send(value: T) -> Outcome[Unit, SendError]`
- [ ] 1.4 `SyncSender::try_send(value: T) -> Outcome[Unit, TrySendError]`
- [ ] 1.5 `impl Iterator for Receiver[T]` — consume as iterator
- [ ] 1.6 Tests: bounded channel, full buffer, iterator drain
