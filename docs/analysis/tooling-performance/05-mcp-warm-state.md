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
