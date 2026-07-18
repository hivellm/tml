# ADR-010: Routing `tml check` through QueryContext (F-019)

**Status:** REJECTED — document-blocked (kept eager, no re-route) · **Date:** 2026-07-18
**Context:** phase42c (incremental-cache), finding F-019 (`docs/analysis/incremental-cache/02-findings.md`)

## Context

`tml check` (`compiler/src/cli/commands/cmd_debug.cpp:244-308`, `run_check`) bypasses the
query system entirely: it does a direct lex → parse → `preload_all_meta_caches` →
`TypeChecker::check_module`, constructing **no** `QueryContext`. So parse and typecheck run
from scratch on every invocation, with no memoization or incremental reuse. Finding F-019
proposed routing `check` through `QueryContext` so it shares the memo/incremental layers the
`build`/`run`/`test` paths use.

## Decision

**Do not route `check` through `QueryContext`.** Keep the direct path. The win is illusory for
the one-shot CLI and the warm workflow is already served — safely — by the daemon result cache.

## Evidence

1. **No cross-process typecheck persistence.** `force<ResultType>()` only attempts previous-session
   (GREEN) reuse for `CodegenUnitResult`; every other result type — including `TypecheckResult` —
   is checked against the **in-memory** cache only (`compiler/include/query/query_context.hpp:236-254`),
   and the incremental writer persists only `save_ir`/`save_link_libs` for `CodegenUnit`
   (`query_context.hpp:318-329`). A one-shot `tml check` starts with an empty in-memory cache and
   nothing to load, so a query route would recompute parse + typecheck anyway — **zero memo win**,
   plus the added cost of constructing a `QueryContext` and loading the incr cache. A net loss.

2. **No diagnostics replay, and a lossy result contract.** `TypecheckResult` carries `errors` as
   flattened strings and drops cascading errors (`query_core.cpp:454-470`), and has **no warnings
   field** at all (`query_key.hpp:201-206`) — warnings are emitted only as a transient `TML_LOG_WARN`
   side effect while the provider runs (`query_core.cpp:474-479`). No diagnostics-replay path exists
   anywhere in `compiler/src/query`. So any attempt to *skip* the typecheck provider off a cached
   result would silently drop errors and warnings — unacceptable for a command whose sole output is
   diagnostics. `run_check` today renders structured errors + warnings through the diagnostic emitter
   (`cmd_debug.cpp:288-295`), fidelity the query result does not preserve.

3. **Redundant with an existing, safer cache.** The daemon result cache
   (`compiler/src/cli/commands/cmd_daemon.cpp`) already memoizes `check` (`is_cacheable` includes
   `check`) keyed by argv CRC + full `.tml` mtime snapshot + `universe_epoch` (the transitive source
   universe, from phase42b F-028/F-029). On a warm hit it replays the **literal captured stdout +
   stderr + exit code**, so there is no diagnostics-replay problem — the exact emitted text is cached
   — and it is invalidated correctly by any edit in the transitive mtime universe and by stdlib
   changes. This covers the realistic warm workflow that an in-process query route (one-shot, nothing
   to reuse) fundamentally cannot.

## Consequences

- `check` stays a direct lex/parse/typecheck; F-019's structural observation is acknowledged but the
  proposed remedy is rejected as no-win (one-shot) or unsafe (drops diagnostics).
- **Unlocking prerequisite if revisited:** a persisted, diagnostics-carrying typecheck query — extend
  `TypecheckResult` with structured errors **and** warnings, add previous-session reuse for
  non-`CodegenUnit` results, and add a diagnostics-replay step. That is a substantially larger change
  than a CLI re-route and remains redundant with the daemon cache for the common workflow.
- The F-036 mtime "nothing changed" fast path (phase42c) already removes the redundant per-module
  source re-hashing that dominated cold `check` startup, capturing the achievable cold-path win
  without touching the query layer.
