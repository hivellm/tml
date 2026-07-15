# phase28b — Async File I/O Hardening (Stabilization ERA 0, Phase D3)

> Analysis: `docs/analysis/tml-table-analysis/05-tooling-stdlib-gaps.md` (F-012).
> `lib/std/src/aio/async_file.tml` is an MVP; a WAL-driven application needs
> guaranteed durability ordering and robust failure semantics. Requires phase28a's
> commit-log workload as the consumer that validates it.

## 1. Implementation
- [ ] 1.1 fsync ordering: guarantee that `sync`/`datasync` completion implies all previously-completed async writes are durable (document + test the ordering contract)
- [ ] 1.2 Partial-write and error propagation: short writes surface as typed `Outcome` errors with byte counts, not silent truncation; disk-full and permission errors covered by tests
- [ ] 1.3 Cancellation semantics: dropping/cancelling a pending async op has defined behavior (no use-after-free on the buffer, no orphaned completion)
- [ ] 1.4 Concurrency: multiple in-flight ops on one file + multiple files; stress test under the phase25a adversarial allocator
- [ ] 1.5 Wire into phase28a's commit log as the WAL path (replacing sync `File` writes) and re-run the soak gate

## 2. Tail (docs + tests — check or waive with tailWaiver)
- [ ] 2.1 Update or create documentation covering the implementation
- [ ] 2.2 Write tests covering the new behavior
- [ ] 2.3 Run tests and confirm they pass
