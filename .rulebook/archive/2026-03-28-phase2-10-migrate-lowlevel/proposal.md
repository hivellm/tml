# Proposal: Migrate lowlevel to Typed APIs

## Status: COMPLETE

## Summary

The TML HTTP server, async runtime, and stream layer originally contained ~702 `lowlevel` blocks using raw `ptr_read`/`ptr_write`/`mem_alloc` to implement what should have been typed struct fields. This made the code unsafe, unmaintainable, and opaque to the optimizer. The migration introduced typed accessor functions and proper struct layouts, reducing raw pointer usage to only the legitimate cases (FFI, zero-copy sharing, core primitives).

## Why

The TML HTTP server used raw mem_alloc/ptr_write for hook tables instead of typed List/HashMap collections, making the code unsafe, unmaintainable, and opaque to the optimizer.

## Motivation

Raw `ptr_read`/`ptr_write` at an offset from a base pointer is equivalent to writing C with no struct definitions. Every access requires computing offsets manually, there are no compile-time type checks, and the optimizer cannot reason about aliasing. A codebase with 702 such blocks in a single module is not maintainable.

The migration was triggered by discovering that the HTTP module had 692 unnecessary unsafe blocks. The goal was to reduce this to zero for cases where typed TML APIs already existed.

## Design

Each migration phase followed the same pattern:
1. Define a typed struct (or use an existing TML type) for the data being accessed
2. Write accessor functions (`field_get`/`field_set`) that encapsulate the remaining raw access
3. Replace all call sites with the accessor functions
4. Run tests to verify no regressions

Phases were organized by subsystem: HTTP shared state, HTTP router, stream layer, HTTP parser, async runtime, and miscellaneous. The HTTP `bytes.tml` refcount pattern and `work_stealing.tml` cross-thread struct sharing were explicitly deferred as legitimate uses of raw pointer access.

## What Changes

All changes are complete. Affected files included:
- `lib/std/src/http/shared_state.tml`, `worker.tml`, `dispatch.tml`, `iocp_worker.tml`
- `lib/std/src/http/conn_pool.tml`, `rate_limit.tml`, `agent.tml`
- `lib/std/src/http/parse.tml`
- `lib/std/src/stream/buffered.tml`, `pipe.tml`
- `lib/std/src/runtime/multi_executor.tml`, `aio/timer_wheel.tml`
- `lib/std/src/net/buffer_view.tml`, `events.tml`, `server_response.tml`

Phase 3 (HTTP App table migration) remains open. Phase 3 tasks were blocked by complexity of the `App::new()` initialization sequence and were not completed.

## Dependencies

- Depended on: existing typed TML APIs (Buffer, List, HashMap, Text) in std
- Enabled: readable HTTP/runtime code, optimizer visibility into struct fields, maintainability

## Risks

No remaining risks — task is complete except for Phase 3 (HTTP App tables), which is tracked as an open item in `tasks.md`.
