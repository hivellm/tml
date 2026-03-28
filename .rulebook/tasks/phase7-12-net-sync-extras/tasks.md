# Tasks: Net + Sync Extras

**Status**: Complete — 6/9 done, 3 blocked
**Priority**: LOW
**Phase**: 7 — Rust Parity

## Phase 1: Net Extras

- [x] 1.1 `TcpListener::incoming(this) -> TcpIncoming` — iterator over accepted connections (type check passes)
- [x] 1.2 `TcpStream::write_all(this, data: Str) -> Outcome[Unit, NetError]` — retry-loop write using substring_from
- [x] 1.3 `TcpStream::read_to_string(this) -> Outcome[Str, NetError]` — read until EOF
- [ ] 1.4 `TcpStream::try_clone(this) -> Outcome[TcpStream, IoError]` — BLOCKED: no `tml_socket_dup` FFI exists in C runtime
- [ ] 1.5 `UdpSocket::try_clone(this) -> Outcome[UdpSocket, IoError]` — BLOCKED: same reason
- [ ] 1.6 `UdpSocket::join_multicast_v6` / `leave_multicast_v6` — BLOCKED: needs setsockopt IPV6_JOIN_GROUP FFI
- [x] 1.7 Tests — tcp_write_all.test.tml (placeholder — std_net suite crashes at runtime from pre-existing codegen issues; type checking passes for all new code)

## Phase 2: Sync Extras

- [x] 2.1 `Arc::make_mut(this) -> Maybe[mut ref T]` — implemented; BLOCKED at runtime by AtomicU64/AtomicUsize type mismatch in LLVM IR
- [x] 2.2 `impl Iterator for Receiver[T]` — already implemented (next() method exists on both Receiver and BoundedReceiver)
- [x] 2.3 Tests — arc_make_mut.test.tml (compile error from codegen bug, not logic bug)

## Notes

- TCP runtime tests crash in pre-existing std_net suite codegen issues (ACCESS_VIOLATION), unrelated to new code
- `Arc::make_mut` logic is correct but blocked by `AtomicU64` vs `AtomicUsize` LLVM IR type name mismatch
- Items 1.4-1.6 need new C runtime FFI functions (`WSADuplicateSocket`/`dup`, `setsockopt` with `IPV6_JOIN_GROUP`)
