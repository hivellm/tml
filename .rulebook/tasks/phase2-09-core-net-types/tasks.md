# Tasks: Core Net Types — Move IP/Socket Types to Core

**Status**: Proposed
**Priority**: LOW
**Phase**: 2 — Stdlib Completeness

## Motivation

Rust moved `IpAddr`, `Ipv4Addr`, `Ipv6Addr`, `SocketAddr` to `core::net` in Rust 1.77 (2024). These types are pure data (no OS calls, no allocation needed) and should be available in `no_std` contexts. Currently TML has them only in `std/net/ip.tml`.

## Phase 1: Move to Core

- [ ] 1.1 Create `lib/core/src/net/mod.tml`
- [ ] 1.2 Move `Ipv4Addr` implementation to `lib/core/src/net/ip.tml` (pure data, no FFI)
- [ ] 1.3 Move `Ipv6Addr` implementation to `lib/core/src/net/ip.tml`
- [ ] 1.4 Move `IpAddr` enum to `lib/core/src/net/ip.tml`
- [ ] 1.5 Move `SocketAddrV4`, `SocketAddrV6`, `SocketAddr` to `lib/core/src/net/socket.tml`
- [ ] 1.6 Update `core/mod.tml` to export `net` module
- [ ] 1.7 Update `std/net/ip.tml` to re-export from core (backward compatibility)
- [ ] 1.8 Update `std/net/socket.tml` to re-export from core
- [ ] 1.9 Write core-level tests: `lib/core/tests/net/ip.test.tml`
- [ ] 1.10 Run full net test suite to verify no regressions
