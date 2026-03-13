# Proposal: complete-async-coverage

## Why
Async modules account for 82 of 273 uncovered functions (37.7%). The net/async_tcp, net/async_udp, future, async_iter, task, and pin modules have significant coverage gaps. Reaching 100% async coverage is critical for validating the async runtime before building higher-level features on top of it.

## What Changes
- Write tests for `pin` module (6 functions): Pin::new, get_ref, get_mut, get_unchecked_mut, deref, deref_mut
- Write tests for `future` module (6 functions): Ready::poll, Pending::poll, Map::new, poll_fn, Fuse::new, Fuse::is_terminated
- Write tests for `async_iter` module (8 functions): poll_next variants, from_iter, Take::new
- Write tests for `task` module (3 functions): Poll::map_ok, Poll::map_err, Poll::eq
- Fix Waker implementation (3 functions): wire to C runtime TmlWaker via FFI
- Write loopback tests for `net/async_tcp` (28 functions): bind, connect, accept, read, write, etc.
- Write loopback tests for `net/async_udp` (28 functions): bind, send_to, recv_from, etc.

## Impact
- Affected specs: core::pin, core::future, core::async_iter, core::task, std::net
- Affected code: lib/core/tests/, lib/std/tests/, lib/core/src/task/mod.tml (Waker fix)
- Breaking change: NO
- User benefit: Validated async stack, confidence in async networking primitives
