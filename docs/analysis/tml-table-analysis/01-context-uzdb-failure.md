# 01 — Context: The UzDB Failure and the 0.1.6 → 0.3.52 Timeline

## What UzDB was

A DB-as-server for MMORPG backends (SpacetimeDB-inspired): in-memory store + commit log,
MVCC single-writer-per-shard, reducers as atomic transactions, SQL push subscriptions,
spatial indexes, UzRPC (MessagePack envelope + BSATN-equivalent "UzBIN" rows). It was fully
specced — 14 spec documents, a PRD, a dependency DAG, and a roadmap — but **never reached
implementation**. Its status froze at "Design phase" with only 8 spike programs
(`E:\UzmiGames\UzDB\src\spikes\`) before the decision to rewrite in Rust.

## Timeline

- The feedback letter (`E:\UzmiGames\UzDB\docs\letter-to-tml-dev.md`) dates from **TML 0.1.6**.
  UzDB's git history is 5 commits, all docs/specs — TML implementation never started because
  the phase-0 primitive-verification spikes could not be reliably compiled and run.
- TML is now **0.3.52**. The CHANGELOG (0.3.26–0.3.36) records a "full UzDB feedback response"
  release train that addressed the letter's P0/P1 items.

## F-011 — The original UzDB tooling blockers are RESOLVED (progress, not a current blocker)

**Confidence: High.**

The P0/P1 items in the 0.1.6 letter have been addressed by 0.3.52:

| Letter item (0.1.6) | Status at 0.3.52 | Evidence |
|---------------------|------------------|----------|
| Pipe hangs / stderr routing / `WriteConsole` | Fixed | CHANGELOG 0.3.26–0.3.36 "pipe-hang fix … stderr routing" |
| MCP subprocess timeout | Added | CHANGELOG 0.3.26–0.3.36 "MCP timeout" |
| `match` keyword ICE | Now a diagnostic | `compiler/src/parser/parser_expr.cpp:607` detects `match` and emits "did you mean `when`" instead of panicking |
| `U128`/`I128` `Display` | Present | `lib/core/src/fmt/impls.tml` |
| `fsync`/`fdatasync` | Present | `File::sync` / `File::datasync`, `lib/std/src/file/file.tml:171-181` |
| MessagePack | Present | `lib/std/src/msgpack/` (mod/reader/types/writer) |
| `BTreeMapIter` not iterable | Fixed | implements `Iterator`/`IntoIterator`, `lib/std/src/collections/btreemap.tml:376,395` |
| Integer-literal inference at struct sites | Fixed | CHANGELOG 0.3.26–0.3.36 "int literal inference" |

**Impact:** these are no longer the reason TML is unusable. Do NOT re-litigate them; the
current blockers are lower in the stack (files 02 and 03).

## F-012 — Remaining app-capability gaps are modest and mostly maturity-level

**Confidence: Medium-High.**

What UzDB needed that is still thin at 0.3.52:

- **Async file I/O** exists only as an MVP (`lib/std/src/aio/async_file.tml`; CHANGELOG
  0.3.26–0.3.36 "async-file MVP + design doc"). A WAL-driven database needs this hardened
  (durability ordering, error paths, backpressure).
- **Package registry** is partial (see F-010 in file 05).
- Everything else UzDB specced (MVCC store, commit log, spatial indexes, UzRPC) is application
  code UzDB would author itself — the stdlib primitives (Buffer, HashMap, BTreeMap, crypto,
  msgpack, TCP/WebSocket, HTTP/2, SQLite) are all present.

**The gap is not missing libraries; it is that the ones that exist crash under load**
(files 02 and 03). A database is, structurally, the worst-case stress test for TML's current
memory model: long-lived collections of owned rows, snapshot copies, refcounted shared state,
and tight allocation/free loops.
