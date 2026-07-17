# MCP Daemon Supervisor — Tasks

**Status:** 12/12 — COMPLETE

## 1. Daemon core
- [x] 1.1 Create `compiler/src/main_daemon.cpp` — McpProcess class with subprocess management
- [x] 1.2 Pipe stdin/stdout/stderr between daemon and tml_mcp.exe subprocess
- [x] 1.3 Notification detection — fire-and-forget for messages without `"id"`
- [x] 1.4 Auto-restart subprocess on crash
- [x] 1.5 Capture subprocess stderr and include in JSON-RPC error response
- [x] 1.6 Register in CMakeLists.txt, build as `tml_daemon.exe`

## 2. MCP server fixes
- [x] 2.1 Remove detached worker threads — synchronous tool execution
- [x] 2.2 Replace fprintf format strings with fputs (format string vulnerability)
- [x] 2.3 Wrap init_call_logger filesystem ops in try/catch
- [x] 2.4 Fix analyze tool crash — file sharing conflict (ifstream vs fopen on same JSONL)

## 3. Integration
- [x] 3.1 Update `.mcp.json` to use `tml_daemon.exe`
- [x] 3.2 Verify all 25 MCP tools work via daemon
