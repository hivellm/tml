# Tasks: Core Net Types — Move IP/Socket Types to Core

**Status**: Done
**Priority**: LOW
**Phase**: 2 — Stdlib Completeness

## Why

Rust moved `IpAddr`, `Ipv4Addr`, `Ipv6Addr`, `SocketAddr` to `core::net` in Rust 1.77 (2024). These types are pure data (no OS calls, no allocation needed) and should be available in `no_std` contexts. Currently TML has them only in `std/net/ip.tml`. Moving to core enables embedded/WASM targets and reduces dependency chains for DNS/HTTP libraries.

## Phase 1: Move to Core

- [x] 1.1 Create `lib/core/src/net/mod.tml` — module root with pub mod ip, socket + re-exports
- [x] 1.2 Move `Ipv4Addr` implementation to `lib/core/src/net/ip.tml` (pure data, no FFI)
- [x] 1.3 Move `Ipv6Addr` implementation to `lib/core/src/net/ip.tml`
- [x] 1.4 Move `IpAddr` enum to `lib/core/src/net/ip.tml`
- [x] 1.5 Move `SocketAddrV4`, `SocketAddrV6`, `SocketAddr` to `lib/core/src/net/socket.tml`
- [x] 1.6 Update `core/mod.tml` to export `net` module
- [x] 1.7 Update `std/net/ip.tml` to re-export from core (`pub use core::net::ip::*`)
- [x] 1.8 Update `std/net/socket.tml` to re-export from core (`pub use core::net::socket::*`)
- [x] 1.9 Write core-level tests: `lib/core/tests/net/ip.test.tml` (12 tests covering Ipv4/Ipv6/IpAddr)
- [x] 1.10 Run std/net tests — ip_v4, ip_v6, ip_addr, ip_v4_traits all pass via re-exports

## Notes

- Import fix: `Maybe`, `Just`, `Nothing` are builtins — don't import from `core::types::option`
- Import fix: `super::ip` doesn't work in core submodules — use `core::net::ip` instead
- Pre-existing failures in std/net (not caused by this migration):
  - `net_socket`: V6 IpAddr dispatch crash (IpAddr::V6 loopback)
  - `ip_eq_fmt`: LLVM "unsized type" compilation error
