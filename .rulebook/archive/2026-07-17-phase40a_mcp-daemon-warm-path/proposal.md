# Proposal: phase40a_mcp-daemon-warm-path

## Why
The MCP tools (`mcp__tml__check`, `compile`, `emit-ir`, …) are the primary interface for all agent work (AGENTS.override.md T2), yet every call spawns a fresh **cold** `tml.exe` subprocess: `handle_check` builds a command line and runs it via `execute_command` → `CreateProcessA` (`compiler/src/mcp/mcp_tools.cpp:708-728`, `425-483`). A warm daemon already exists (`compiler/src/cli/commands/cmd_daemon.cpp`) that keeps LLVM + plugin DLLs resident and answers cache-hits in **~22 ms**, but `dispatcher.cpp:197-203` only forwards to it when `TML_DAEMON=1` is set — and nothing in the MCP path sets it. Result: a trivial `check` costs ~460 ms instead of ~22 ms, on every single call, multiplied across every agent debug loop.

Findings: F-016, F-017, F-018 in `docs/analysis/tooling-performance/` (03-check-performance.md, 05-mcp-warm-state.md).

## What Changes
Route MCP tool handlers through the warm daemon path:
- `tml_mcp.exe` auto-starts the daemon (if not running) and sets `TML_DAEMON=1` in the child environment of `execute_command`, OR runs `tml_main` in-process with `Loader::set_daemon_mode(true)` (the approach `cmd_daemon_server` already uses at `cmd_daemon.cpp:552`).
- Covers at minimum: `check`, `compile`/`build`, `emit-ir`, `emit-mir`.
- Daemon lifecycle safety: `scripts/build.bat:193-197` already kills daemon+MCP on rebuild; the MCP must transparently restart the daemon and must never serve results from a daemon older than the current compiler DLLs.

## Decision (item 1.2): Option (a) — env + auto-start, subprocess isolation kept

**Chosen: (a)** — the MCP server sets `TML_DAEMON=1` in the child environment of
`execute_command` (per-child env block, thread-safe) and auto-starts the daemon
(`tml.exe daemon start`, detached, throttled) when the PID-file probe says it is
not running. Only `check`, `compile`/`build`, `emit-ir`, `emit-mir` opt in.

Rationale:
1. **The fast path already exists below the DLL:** `main_launcher.cpp:try_daemon_forward_launcher`
   forwards `build`/`run`/`check` over the named pipe BEFORE loading any compiler DLL.
   Measured on the trivial-file check: cold ~453 ms median → warm daemon ~7.6 ms median
   (validated before any code change). The <50 ms gate is met with ~6× margin.
2. **Option (b) (in-process `tml_main` in `tml_mcp.exe`) was rejected**, not for gate
   reasons but for correctness/lifecycle ones: the subprocess design was chosen
   deliberately (`mcp_tools.cpp:721-723` — in-process check crashed on module
   resolution), an in-process compiler would make the long-lived `tml_mcp.exe` hold
   `tml_compiler.dll` mapped (locking it against rebuild since build.bat only kills
   `tml_mcp.exe` when it is itself the build target), and a crash in compilation
   would take the whole MCP session down. Option (a) keeps process isolation and
   still reaches ~7.6 ms.
3. **Staleness safety:** `build.bat:192` runs `taskkill /F /IM tml.exe`, which kills the
   compile daemon (it runs as `tml.exe __daemon_server`) on EVERY build — no locked
   DLLs, no stale daemon after a rebuild. Second net: the daemon self-checks
   `tml_compiler.dll` mtime per request and exits with a restart notice when stale;
   the launcher/CLI clients now treat that notice as "fall through to direct
   compilation" instead of surfacing a spurious exit-1.
   (Note: `build.bat:194` kills `tml_daemon.exe`, which is the unrelated MCP HTTP
   proxy — the compile daemon is covered by the `tml.exe` kill on line 192.)

## Impact
- Affected specs: none (tooling-only; no language change)
- Affected code: `compiler/src/mcp/mcp_tools.cpp`, `compiler/src/mcp/mcp_tools_codegen.cpp`, `compiler/src/cli/dispatcher.cpp`, `compiler/src/cli/commands/cmd_daemon.cpp`
- Breaking change: NO (same tool surface, same results, lower latency)
- User benefit: repeated `mcp__tml__check` drops from ~460 ms to ~22 ms (≈20×); every agent iteration loop (check → fix → check) gets proportionally faster
