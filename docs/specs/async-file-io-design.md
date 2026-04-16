# Async File I/O — Design Note

**Status:** Proposal for a future runtime addition.
**UzDB item:** P3-11 (async file I/O).
**Tracking task:** archived `phase1m_async-file-io` (2026-04-16).

## Why this is not shipped yet

`std::file::File` is entirely synchronous: every method is a thin wrapper over
a blocking C FFI. `std::aio` provides an `EventLoop` + `Poller` for sockets
(epoll / WSAPoll) but file descriptors on most platforms cannot register
with those polling primitives — a proper async file layer therefore needs a
**separate backend per OS**:

| Platform | Required primitive |
|----------|--------------------|
| Windows  | IOCP + `ReadFile`/`WriteFile` with `OVERLAPPED` |
| Linux 5.1+ | `io_uring` (`IORING_OP_READ` / `IORING_OP_WRITE`) |
| Linux ≤5.0 | POSIX AIO or thread-pool fallback |
| macOS    | kqueue + thread-pool fallback |

Each backend requires a new C file under `lib/std/runtime/aio/` plus
integration with the existing `EventLoop` runtime so that file futures
resolve on the same executor as socket futures. UzDB classified this
under P3 polish rather than a blocker, so it was scoped out of the
feedback-response session; the rest of the UzDB letter is fully
resolved.

## Target API

```tml
pub type AsyncFile {
    handle: *Unit,
    runtime: ref EventLoop,
}

impl AsyncFile {
    pub func open(rt: ref EventLoop, path: Str) -> Future[Outcome[AsyncFile, IoError]]
    pub func read_async(this, buf: ref mut Buffer, n: I64) -> Future[Outcome[I64, IoError]]
    pub func write_async(this, buf: ref Buffer, n: I64) -> Future[Outcome[I64, IoError]]
    pub func read_at_async(this, offset: I64, buf: ref mut Buffer, n: I64) -> Future[Outcome[I64, IoError]]
    pub func write_at_async(this, offset: I64, buf: ref Buffer, n: I64) -> Future[Outcome[I64, IoError]]
    pub func sync_async(this) -> Future[Outcome[Unit, IoError]]
    pub func close_async(mut this) -> Future[Outcome[Unit, IoError]]
}
```

- `open` resolves after the OS has actually opened the file (Windows `CreateFile` with
  `FILE_FLAG_OVERLAPPED`; Linux via io_uring or thread-pool `open(2)`).
- Positioned reads/writes (`*_at_async`) are first-class because databases
  (UzDB's use case) read and write page-aligned offsets rather than streaming
  forward.
- Cancellation: dropping the `Future` drops the associated SQE / OVERLAPPED
  registration — important for request timeouts.

## Interim workaround

Users who need concurrent file I/O today can spawn blocking operations on a
worker thread via `std::thread::spawn`:

```tml
use std::thread
use std::file::File

let handle = thread::spawn(do() {
    return File::read_all(path)
})
// ... other work ...
let content = handle.join().unwrap()
```

This is O(N_threads) scalable and blocks each worker on the blocking FFI
call, but gives useful parallelism for a small number of concurrent reads
(e.g. a few dozen connections doing cold page loads).

## Steps to ship

1. Decide canonical backend stack: IOCP (Win) + io_uring (Linux, with POSIX AIO
   fallback for < 5.1) + thread pool (macOS & fallback).
2. Add `lib/std/runtime/aio/file_iocp.c`, `lib/std/runtime/aio/file_iouring.c`,
   `lib/std/runtime/aio/file_threadpool.c`.
3. Wire completion notifications into `EventLoop::poll` so file futures run
   on the same executor as socket futures.
4. Expose the TML API shown above in `lib/std/src/aio/async_file.tml`.
5. Add benchmarks comparing:
   - Sync `File::read_all_bytes` vs async read on a 1 GB file,
   - 1000 concurrent async reads vs `std::thread::spawn` fan-out.
6. Documentation + changelog entry.

Estimated effort: 2–3 engineer-days on Windows + Linux (assuming working
io_uring host), ≈1 day extra for macOS backend and benchmarks.
