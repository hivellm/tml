# 05 — MCP Layer Discards Warm State

**F-017 — RESOLVED (phase40a, 2026-07-17) — Every MCP tool call spawned a fresh cold `tml.exe` subprocess; the daemon was never used.**
Evidence (original): `mcp_tools.cpp` `handle_check` built `tml.exe check <file>` and ran it via `execute_command` → `CreateProcessA`; the comment explicitly chose subprocess over in-process. `handle_compile` and the codegen handlers (`mcp_tools_codegen.cpp`) did the same. None set `TML_DAEMON=1`, so even the existing daemon result cache (F-016) was bypassed. The long-running `tml_mcp.exe` holds no warm compiler state between tool calls.
Impact: **Very High** — this was the #1 reason `check` *felt* slow through the agent workflow. Confidence: **High**.
**Resolution:** `execute_command(cmd, timeout, use_daemon)` now launches daemon-eligible children (`check`, `compile`/`build`, `emit-ir`, `emit-mir`) with `TML_DAEMON=1` in a per-child environment block and auto-starts the compile daemon (throttled, detached `tml daemon start`) when the PID-file probe finds it dead. `run`/`test` intentionally stay cold (`run` would lose program stdout inside the daemon's NUL handles; `test` is not a forwardable command). Measured: `check` on an unchanged file 452.8 ms → **6.6 ms** median at CLI level, **26–31 ms** through the full MCP stack (gate < 50 ms: PASS). Fallback correctness verified: daemon absent → auto-start + cold fall-through (correct result); daemon killed mid-session → transparent restart; DLL rebuilt under a live daemon → restart notice now falls through to direct compile instead of a spurious exit-1 (fix in `main_launcher.cpp` + `cmd_daemon.cpp`); `dispatcher.cpp` ordering bug that made daemon forwarding for `check` unreachable in the DLL dispatcher is fixed. See `01-measurements.md` ("phase40a baseline + warm-path results").

**F-018 — PARTIALLY RESOLVED (phase40a) — Two parallel "warm-server" implementations exist (`tml_daemon.exe` and `tml_mcp.exe`) that don't cooperate.**
Evidence: `build/debug/bin/tml_daemon.exe` (1.08 MB, MCP HTTP proxy/supervisor — NOT the compile daemon; the compile daemon runs as `tml.exe __daemon_server`) + `tml_mcp.exe` (3.56 MB); `build.bat:191–197` kills `tml.exe` (and thereby the compile daemon) plus `tml_daemon.exe`/`tml_mcp.exe` on rebuild. The daemon has the warm LLVM context + result cache; the MCP server has the tool surface but re-shells out.
Impact: **High**. Confidence: **High**.
**Resolution status:** the MCP server now *uses* the compile daemon through the thin-launcher forwarding path (per-call subprocess is retained by design for crash isolation — see proposal.md of phase40a for why in-process `tml_main` was rejected). The two binaries remain separate processes; full unification was deliberately not pursued.

## Residual caveats (phase40a)

- The daemon's mtime result cache stores stdout/stderr + exit code only. A cache-hit
  `build`/`emit-ir`/`emit-mir` does not re-produce on-disk artifacts; they persist from
  the cache-miss run. Deleting `build/debug/<stem>.ll`/`.mir`/exe between identical
  invocations (without touching the source) yields a hit with no artifact regeneration.
  Touch the source or restart the daemon to force regeneration.
- The first request after a daemon (re)start is a cache-miss at normal cold cost; the
  daemon warms transparently from then on.
- A live `mcp__tml__*` session talks to the `tml_mcp.exe` that was running when the MCP
  client connected — after a compiler rebuild the MCP client must reconnect to pick up
  the new server binary (build.bat kills `tml_mcp.exe` when it is part of the build target).

## Staleness correctness (phase42b, v0.3.73)

The phase40a residual caveat "the daemon result cache stores stdout/stderr + exit code
only" was correctness-safe for *artifacts* but had two *diagnostic* staleness holes that
phase42b closed (see `docs/analysis/incremental-cache/02-findings.md` F-028/F-029):

- **Imported-module edits are now seen (F-028).** The result cache previously keyed only on
  argv `.tml` mtimes; editing a transitively-imported module returned the previous
  diagnostics. The daemon now also checks a `universe_epoch` (max mtime over the lib source
  tree + each argv file's sibling directory) before serving a warm hit.
- **Library-source edits under a warm daemon (F-029).** A per-request lib-epoch probe drops
  the process-level module caches (`GlobalModuleCache` + meta preload) when a lib source
  changes, so a warm recompile type-checks against fresh interfaces instead of the ones
  preloaded at daemon start.

Residual caveat introduced by these fixes:

- The lib source tree (~2.3k files) is swept for its max mtime at most **once per 750 ms**
  (to protect the warm-request latency target). A library-source edit followed by a warm
  `check`/`build`/`emit-ir` within that window can still see the pre-edit stdlib interface
  for up to ~750 ms; the next request past the window picks up the change. Project-local
  sibling edits are detected immediately (their scan is un-throttled). Restart the daemon to
  force an immediate refresh.
- The artifact-regeneration caveat above is unchanged: a cache-hit `build`/`emit-ir` still
  does not re-produce on-disk artifacts deleted between identical invocations.
