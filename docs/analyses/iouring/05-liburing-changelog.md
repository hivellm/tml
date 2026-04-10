# liburing Changelog — 2024 to 2026

Current stable version: **2.14** (February 7, 2026)  
GitHub: https://github.com/axboe/liburing

---

## Version History

### liburing 2.14 — February 7, 2026 ★ Current

**Theme**: Documentation completeness milestone

- **Complete API documentation** — entire liburing public API now has man pages (first time)
- New section 7 man pages describing io_uring concepts (not just functions)
- Documentation for `IORING_SETUP_TASKRUN_FLAG` and `IORING_SETUP_DEFER_TASKRUN`
- Bug fix in `io_uring_alloc_huge` ring memory calculation
- Removal of `const` from `io_uring_prep_files_update` (API fix for C++ compatibility)
- Net zerocopy benchmark updates
- Test suite: enhanced CQE count and user_data verification

**Impact for TML**: DEFER_TASKRUN documentation is relevant — this flag defers task completion  
to the io_uring thread rather than the kernel workqueue, reducing context switches in  
single-threaded event loop architectures.

---

### liburing 2.13 — December 16, 2025

**Theme**: Stability and C++ compatibility

- **Buffer ring enlarged to 2MB** for `recvsend_bundle` tests — fixes ENOBUFS under load
- TSAN (Thread Sanitizer) build support added to CI
- `const` qualifiers added to prep functions (C++ compatibility)
- `noexcept` and `local inline` qualifiers for C++ header compatibility
- Fix 1-byte munmap error in NO_MMAP error path
- Fix buffer ring unmapping when mmap fails
- LSAN (Leak Sanitizer) fixes
- Futex: fix alternation of async cancel requests

**Impact for TML**: The 2MB buffer ring fix directly addresses a production stability bug.  
Under high concurrent load, multishot recv would exhaust the buffer ring and return ENOBUFS,  
causing recv operations to fail. Enlarging the ring buffer prevents this at the cost of ~2MB  
of pre-allocated memory — acceptable for an HTTP server.

---

### liburing 2.12 — August 23, 2025

**Theme**: Correctness and pipe support

- **Pipe operations support** for newer kernels — `IORING_OP_PIPE`
- `smp_load_acquire` in `__io_uring_peek_cqe()` — **fixes memory ordering race** in CQ polling
- Compile assertions for `IORING_OP_LAST` increment (catches opcode table drift)
- `noexcept` specifiers in liburing.h for C++ support
- Sanitizer-safe code improvements
- Fix `__kernel_timespec` compile errors in configure scripts

**Impact for TML**: The `smp_load_acquire` fix is critical for correctness.  
Before this fix, multi-threaded CQ polling had a subtle race where a CQE could be  
read before its data was fully visible (memory ordering bug). Any code using  
`__io_uring_peek_cqe` directly (not via liburing helpers) was affected.  
**Always use liburing helpers, never raw ring access.**

---

### liburing 2.11 — June 16, 2025

**Theme**: Compatibility (musl/Alpine)

- **musl library compatibility fixes** — Alpine Linux Docker images now work
- Fix `io_uring_for_each_cqe` macro compilation error
- Fix `io_uring_queue_init_mem()` memory initialization
- New helpers for determining required ring sizes
- Fix futex test compilation on systems with older headers
- Rename internal `aligned_alloc()` to `t_aligned_alloc()` (avoids naming conflict)

**Impact for TML**: **Most relevant to Docker compatibility.**  
Alpine Linux is the most popular Docker base image. Before 2.11, Swoole + liburing would  
fail to compile on Alpine because liburing had glibc-specific headers.  
This is the primary reason the user's Docker test succeeded — liburing 2.11+ supports Alpine natively.

---

### liburing 2.10 — May 29, 2024

**Theme**: Cleanup and compatibility with older kernels

- SQ and CQ code cleanup (internal refactor)
- Fix: stack use-after-free in `sendmsg_iov_clean` test
- Test fixes for compatibility with older kernels
- Updated `io_uring_enter2()` syscall signature
- Migrated `io_uring_files_update` → `io_uring_rsrc_update` API
- Fix missing `string.h` include for `memcpy`
- Ubuntu 24.04 clang added to CI

**Impact for TML**: Routine stability release. The `io_uring_rsrc_update` migration  
is an API change — if using registered file descriptors, use the new API.

---

### liburing 2.9 — February 2024

**Theme**: Ring resizing and registered waits

- **Ring resizing support** — dynamically resize the SQ/CQ rings at runtime  
  via `io_uring_resize_rings()` (new in kernel 6.8)
- **Registered waits** — register wait parameters once, avoiding per-wait overhead  
  (`io_uring_register_sync_cancel`, `io_uring_register_iowq_*`)
- Bug fix: incomplete ring closure in SQE128 mode when calling `io_uring_queue_exit()`

**Impact for TML**: Ring resizing enables adaptive sizing — start with a small ring,  
scale up under load. Registered waits reduce `io_uring_enter()` overhead for  
high-frequency polling loops. Both are optimization opportunities, not baseline requirements.

---

## Features in Scope Before 2.9 (Already Available)

These features are stable and available since liburing 2.5–2.8 (pre-2024):

| Feature | Since | Relevant for TML |
|---|---|---|
| Multishot accept | 2.4 (kernel 5.19) | Yes — eliminates accept pool |
| Provided buffer rings | 2.4 | Yes — zero-copy recv |
| Incremental buffer consumption | 2.8 | Yes — partial reads without refilling ring |
| Send/recv bundle | 2.7 | Yes — batch multiple send/recv per SQE |
| BIND/LISTEN ops | 2.7 | Yes — async server setup |
| Ring-mapped buffers | 2.6 | Yes — lower-overhead provided buffers |
| Zero-copy network transmit | 2.5 (kernel 6.0) | Optional — significant for large responses |

---

## Minimum Recommended Version

**liburing 2.11** (June 2025) — minimum for:
- Alpine/musl Docker compatibility
- Memory ordering fix in CQ polling
- Stable multishot and provided buffer support

**liburing 2.13** (December 2025) — recommended for production:
- 2MB buffer ring (prevents ENOBUFS under load)
- TSAN-validated (threading bugs caught)
- Full C++ compatibility headers

**liburing 2.14** (February 2026) — current stable, preferred:
- Complete API documentation
- All known bugs fixed
