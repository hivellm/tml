# Tasks: Net + Sync Extras

**Status**: Proposed
**Priority**: LOW
**Phase**: 7 — Rust Parity

## Phase 1: Net Extras

- [ ] 1.1 `TcpListener::incoming(this) -> TcpIncoming` — iterator over accepted connections
- [ ] 1.2 `TcpStream::write_all(this, data: Buffer) -> Outcome[Unit, IoError]` — retry-loop write
- [ ] 1.3 `TcpStream::read_to_end(this) -> Outcome[Buffer, IoError]` — read entire stream
- [ ] 1.4 `TcpStream::try_clone(this) -> Outcome[TcpStream, IoError]` — duplicate handle
- [ ] 1.5 `UdpSocket::try_clone(this) -> Outcome[UdpSocket, IoError]`
- [ ] 1.6 `UdpSocket::join_multicast_v6` / `leave_multicast_v6` — IPv6 multicast
- [ ] 1.7 Tests

## Phase 2: Sync Extras

- [ ] 2.1 `Arc::make_mut(this) -> mut ref T` — clone-on-write exclusive access
- [ ] 2.2 `impl Iterator for Receiver[T]` — drain channel via iterator
- [ ] 2.3 Tests
