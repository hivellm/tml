# Proposal: Thread Completeness — park/unpark, Builder, TLS

## Why

Thread module is at 75% coverage. `park`/`unpark` are stubs, `Builder::spawn` returns Err, thread-local storage doesn't exist. These block real concurrent programs that need thread synchronization and per-thread state.

## What Changes

Implement real park/unpark via OS futex/event, fix Builder::spawn, add thread-local storage via C runtime TLS.

## Impact
- Affected specs: std::thread
- Affected code: `lib/std/src/thread/`, `lib/std/runtime/thread.c`
- Breaking change: NO (fixing stubs + additive)
- User benefit: Real thread parking, named thread spawning, per-thread storage
