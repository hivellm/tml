# Proposal: Net + Sync Extras — incoming, write_all, Arc::make_mut

## Why

Net is at 90% and Sync at 85%. Missing: `TcpListener::incoming()` iterator, `TcpStream::write_all`/`read_to_end`, `Arc::make_mut` clone-on-write, `Receiver` as Iterator. Small gaps but needed for production-quality networking and concurrency code.

## What Changes

Add missing methods to existing net and sync modules. Pure TML wrappers where possible.

## Impact
- Affected specs: std::net, std::sync
- Affected code: `lib/std/src/net/tcp.tml`, `lib/std/src/sync/Arc.tml`, `lib/std/src/sync/mpsc.tml`
- Breaking change: NO
- User benefit: Accept loop via iterator, reliable write-all, clone-on-write Arc, channel drain via iterator
