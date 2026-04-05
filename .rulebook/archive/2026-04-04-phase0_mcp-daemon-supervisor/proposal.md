# Proposal: MCP Daemon — stdio proxy + subprocess supervisor

## Why

The MCP server (`tml_mcp.exe`) crashes when tool handlers segfault, leaving Claude Code with "Connection closed" permanently. Rebuilding the compiler while MCP is running locks the DLL files. There is no crash recovery — once the MCP process dies, the entire session is broken until VS Code is restarted.

## What Changes

A new `tml_daemon.exe` binary that acts as a stdio proxy between Claude Code and `tml_mcp.exe`:

1. Claude Code launches `tml_daemon.exe` via stdio (standard `.mcp.json` config)
2. The daemon spawns `tml_mcp.exe` as a subprocess with piped stdin/stdout/stderr
3. Each JSON-RPC request from Claude Code is forwarded to the subprocess stdin
4. Each response from subprocess stdout is forwarded back to Claude Code
5. Notifications (no `"id"` field) are fire-and-forget — no response expected
6. If the subprocess crashes, the daemon captures stderr, returns a JSON-RPC error with the crash details, and auto-restarts the subprocess
7. The subprocess can be rebuilt independently — the daemon stays alive

### Key fixes implemented:
- **File sharing conflict**: `analyze` tool crashed because `std::ifstream` conflicted with `fopen("a")` on the same JSONL file. Fixed by using C `fopen("r")` for reading.
- **Notification handling**: Notifications (JSON-RPC messages without `"id"`) must not wait for a response. The daemon uses `fire()` instead of `send()` for these.
- **Stderr capture**: When subprocess crashes, daemon drains the stderr pipe and includes the last 500 chars in the error response, enabling debugging without external tools.
- **Thread removal**: Removed detached worker threads from `handle_tools_call` — all tool calls run synchronously to avoid lifecycle issues.

## Impact
- Affected code: `compiler/src/main_daemon.cpp` (new), `compiler/src/mcp/mcp_server.cpp`, `compiler/src/mcp/mcp_tools_analyze.cpp`, `.mcp.json`, `compiler/CMakeLists.txt`
- Breaking change: NO (additive — daemon is optional, stdio mode still works)
- User benefit: MCP never disconnects, crashes are recovered automatically, rebuild doesn't lock files
