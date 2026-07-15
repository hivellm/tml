# Proposal: phase28b_async-file-hardening

## Why

`lib/std/src/aio/async_file.tml` shipped as an explicit MVP (CHANGELOG
0.3.26–0.3.36 "async-file MVP + design doc"). A WAL-driven application — the
UzDB-shaped use case phase28a re-earns — depends on properties the MVP does
not yet guarantee: fsync completion implying durability of all previously
completed writes, typed errors on short/failed writes, and defined
cancellation semantics with no buffer use-after-free. Analysis F-012 flags
this as the main remaining stdlib maturity gap for real applications.

## What Changes

Harden async file I/O to production grade: documented + tested fsync
ordering contract, partial-write/disk-full/permission errors surfaced as
typed `Outcome`s with byte counts, defined cancellation behavior, and
multi-file/multi-op concurrency stress-tested under the phase25a adversarial
allocator. phase28a's commit log switches to the async WAL path and re-runs
its soak gate as the end-to-end consumer.

## Impact

- Affected specs: `std::aio` module docs / design doc update.
- Affected code: `lib/std/src/aio/async_file.tml` (+ runtime iocp/uring
  bindings it sits on), phase28a commit-log integration.
- Breaking change: NO (semantics tightened, API kept).
- User benefit: durable-write workloads (databases, logs, queues) become
  safely buildable in TML.

## Source

- docs/analysis/tml-table-analysis/05-tooling-stdlib-gaps.md +
  06-execution-plan.md (Phase D3). Analysis finding F-012.
