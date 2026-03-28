# Proposal: Core Net Types — Move IP/Socket Types to Core

## Status: PROPOSED

## Summary

Move `IpAddr`, `Ipv4Addr`, `Ipv6Addr`, `SocketAddr`, `SocketAddrV4`, and `SocketAddrV6` from `lib/std/src/net/` to `lib/core/src/net/`. These types are pure data structures with no OS calls and no heap allocation requirements — they belong in core, not std. The `std/net` module re-exports them for backward compatibility.

## Motivation

Rust made this same move in 1.77 (stabilized `core::net`). The rationale: IP addresses and socket addresses are pure value types — they store integers, format as strings, parse from strings, and compare by value. None of these operations require OS calls, allocation, or any dependency beyond core. Putting them in `std` means `no_std` environments (embedded systems, WASM) cannot use them at all.

TML targets embedded and WASM in its roadmap. Moving these types to core now, before the ecosystem builds up a dependency on `std/net::IpAddr`, avoids a painful migration later.

Additionally, the HTTP server and DNS resolver use IP types internally. Having them in core reduces the dependency chain: a pure-TML DNS library can use `Ipv4Addr` without depending on all of `std`.

## Design

The move is purely structural — no implementation changes to the types themselves. The files are relocated:

- `lib/std/src/net/ip.tml` → split into `lib/core/src/net/ip.tml` (pure types) and keep networking logic in std
- `lib/std/src/net/socket.tml` → `lib/core/src/net/socket.tml` (address types only)

`lib/core/src/mod.tml` gains a `net` module export. The std versions become re-exports: `use core::net::*` at the top of `std/net/ip.tml` and `std/net/socket.tml`.

All parsing, formatting, and comparison logic moves with the types. FFI-using functions (socket creation, bind, connect) stay in `std/net`.

## What Changes

- New: `lib/core/src/net/mod.tml` — module root
- New: `lib/core/src/net/ip.tml` — Ipv4Addr, Ipv6Addr, IpAddr (moved from std)
- New: `lib/core/src/net/socket.tml` — SocketAddrV4, SocketAddrV6, SocketAddr (moved from std)
- Modified: `lib/core/src/mod.tml` — add `net` module export
- Modified: `lib/std/src/net/ip.tml` — replace implementation with `use core::net::*` re-export
- Modified: `lib/std/src/net/socket.tml` — replace implementation with `use core::net::*` re-export
- New: `lib/core/tests/net/ip.test.tml` — core-level tests for IP types
- Existing: `lib/std/tests/net/` tests continue to pass via re-exports

## Dependencies

- Depends on: nothing (all types are pure data — no FFI, no allocation beyond what T: Display requires)
- Enables: `no_std` and WASM targets using IP address types
- Enables: DNS library in core (no std dependency)

## Risks

- Any code that currently uses `std::net::IpAddr` in `use` declarations may need updating; the re-export strategy mitigates this but must be verified with a full test suite run
- The move must preserve all existing behavior implementations (Display, Debug, PartialEq, Hash, FromStr) exactly; byte-for-byte identical formatted output must be verified
